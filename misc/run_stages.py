"""Runs the stages of docs/EUROPE.md as headless Claude Code sessions, in parallel.

    python misc/run_stages.py --list
    python misc/run_stages.py --prepare e3_native_names
    python misc/run_stages.py e3_native_names e4_officer_panel bio_check
    python misc/run_stages.py --status

A stage is a brief in misc/stages/<stage>.md. The tool puts misc/stages/common.md
before the brief. For each stage, the tool makes the worktree ../hexdom-<stage> on
the branch stage/<stage> from origin/master, with the submodules. Then it starts
"claude -p" in that worktree with the brief on stdin. The session bills the API key
of the player. The tool takes the key from ANTHROPIC_API_KEY or CLAUDE_API_KEY: first
from the environment, then from the user environment of Windows (the registry). It
gives the key to the session as ANTHROPIC_API_KEY. It never prints the key or writes
it to a file.

The output of a session goes to build/stage.jsonl of its worktree (stream-json).
--status prints the end of each session: its result, its cost and its turns.

Options:
    --model NAME      the model of the sessions (default claude-sonnet-5-5)
    --budget USD      the largest cost of one session (default 20)
    --prepare         make the worktrees, start no session
    --dry-run         print the commands, change nothing
"""
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STAGES = os.path.join(ROOT, "misc", "stages")
ALLOWED = "Bash,Read,Edit,Write,Glob,Grep,WebFetch,WebSearch"
DENIED = "Bash(git push --force:*),Bash(git push -f:*),Bash(git reset --hard:*),Bash(rm -rf:*),Bash(git stash:*)"
LIBRARIES = ("third_party/imgui", "third_party/imgui_test_engine")


def fail(message):
    sys.exit("run_stages: " + message)


def git(*args, cwd=ROOT, check=True):
    result = subprocess.run(["git"] + list(args), cwd=cwd, capture_output=True, text=True)
    if check and result.returncode != 0: fail("git %s: %s" % (" ".join(args), result.stderr.strip()))
    return result


def api_key():
    """The key of the player, or "". The user environment of Windows is the fallback,
    because a process that started before the player set the variable does not have it."""
    for name in ("ANTHROPIC_API_KEY", "CLAUDE_API_KEY"):
        if os.environ.get(name): return os.environ[name]
    if os.name != "nt": return ""
    import winreg
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment") as key:
        for name in ("ANTHROPIC_API_KEY", "CLAUDE_API_KEY"):
            try: return winreg.QueryValueEx(key, name)[0]
            except OSError: continue
    return ""


def stage_names():
    return sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(STAGES, "*.md"))
                  if os.path.basename(p) != "common.md")


def worktree_of(stage):
    return os.path.join(os.path.dirname(ROOT), "hexdom-" + stage.replace("_", "-"))


def checkout_spec(tree):
    """The pinned commit of spec-src. The remote can lack it: then a sibling checkout gives it."""
    pinned = git("ls-tree", "HEAD", "spec-src", cwd=tree).stdout.split()[2]
    if git("submodule", "update", "--init", "spec-src", cwd=tree, check=False).returncode == 0: return
    spec = os.path.join(tree, "spec-src")
    for candidate in glob.glob(os.path.join(os.path.dirname(ROOT), "hexdom*", "spec-src")):
        if os.path.abspath(candidate) == os.path.abspath(spec): continue
        if git("cat-file", "-e", pinned + "^{commit}", cwd=candidate, check=False).returncode != 0: continue
        git("fetch", "-q", candidate, pinned, cwd=spec)
        git("checkout", "-q", pinned, cwd=spec)
        return
    fail("no checkout has the spec-src commit %s" % pinned)


def prepare(stage, dry_run):
    tree, branch = worktree_of(stage), "stage/" + stage
    if os.path.isdir(tree):
        print("%s: the worktree %s exists" % (stage, tree))
        return tree, branch
    print("%s: new worktree %s on %s" % (stage, tree, branch))
    if dry_run: return tree, branch
    has_branch = git("rev-parse", "--verify", "-q", branch, check=False).returncode == 0
    if has_branch: git("worktree", "add", tree, branch)
    else:          git("worktree", "add", "-b", branch, tree, "origin/master")
    for library in LIBRARIES:
        git("submodule", "update", "--init", "--reference", os.path.join(ROOT, library), library, cwd=tree)
    checkout_spec(tree)
    return tree, branch


def prompt_of(stage, tree, branch):
    with open(os.path.join(STAGES, "common.md"), encoding="utf-8") as f: common = f.read()
    with open(os.path.join(STAGES, stage + ".md"), encoding="utf-8") as f: brief = f.read()
    return common.replace("{worktree}", tree).replace("{branch}", branch) + "\n\n" + brief


def start(stage, tree, branch, model, budget, dry_run, key):
    command = ["claude", "-p", "--model", model, "--max-budget-usd", str(budget),
               "--permission-mode", "acceptEdits", "--allowedTools", ALLOWED, "--disallowedTools", DENIED,
               "--output-format", "stream-json", "--verbose"]
    print("%s: %s" % (stage, " ".join(command)))
    if dry_run: return
    build = os.path.join(tree, "build")
    os.makedirs(build, exist_ok=True)
    out = open(os.path.join(build, "stage.jsonl"), "w", encoding="utf-8")
    err = open(os.path.join(build, "stage.err"), "w", encoding="utf-8")
    flags = 0
    if os.name == "nt": flags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    environment = dict(os.environ, ANTHROPIC_API_KEY=key)
    process = subprocess.Popen(command, cwd=tree, stdin=subprocess.PIPE, stdout=out, stderr=err, env=environment,
                               text=True, encoding="utf-8", creationflags=flags)
    process.stdin.write(prompt_of(stage, tree, branch))
    process.stdin.close()
    with open(os.path.join(build, "stage.pid"), "w") as f: f.write(str(process.pid))
    print("%s: started, pid %d, output %s" % (stage, process.pid, out.name))


def status():
    for stage in stage_names():
        path = os.path.join(worktree_of(stage), "build", "stage.jsonl")
        if not os.path.exists(path):
            print("%-20s not started" % stage)
            continue
        result = None
        with open(path, encoding="utf-8") as f:
            for line in f:
                try: event = json.loads(line)
                except ValueError: continue
                if event.get("type") == "result": result = event
        if not result:
            print("%-20s running (%d bytes of output)" % (stage, os.path.getsize(path)))
            continue
        text = (result.get("result") or "").replace("\n", " ")
        print("%-20s %s, %s turns, %.2f USD: %s" % (stage, result.get("subtype"), result.get("num_turns"),
                                                     result.get("total_cost_usd") or 0.0, text[:400]))


def main():
    args, stages = sys.argv[1:], []
    model, budget, dry_run, only_prepare = "claude-sonnet-5-5", 20.0, False, False
    while args:
        arg = args.pop(0)
        if arg in ("-h", "--help"): sys.exit(__doc__)
        elif arg == "--list":    print("\n".join(stage_names())); return
        elif arg == "--status":  status(); return
        elif arg == "--model":   model = args.pop(0)
        elif arg == "--budget":  budget = float(args.pop(0))
        elif arg == "--dry-run": dry_run = True
        elif arg == "--prepare": only_prepare = True
        elif arg in stage_names(): stages.append(arg)
        else: fail("unknown stage or option %s; --list shows the stages" % arg)
    if not stages: sys.exit(__doc__)
    key = "" if dry_run or only_prepare else api_key()
    if not dry_run and not only_prepare and not key:
        fail("set CLAUDE_API_KEY or ANTHROPIC_API_KEY in the user environment first")
    if not dry_run: git("fetch", "-q", "origin")
    for stage in stages:
        tree, branch = prepare(stage, dry_run)
        if not only_prepare: start(stage, tree, branch, model, budget, dry_run, key)


main()
