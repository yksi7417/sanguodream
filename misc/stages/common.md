# The rules of a stage run

You are one stage of the plan in docs/EUROPE.md. misc/run_stages.py started
you without a user, in the worktree {worktree} on the branch {branch}.
Other stages run at the same time in other worktrees.

## How to work
- Read CLAUDE.md, docs/EUROPE.md and the brief of your stage first.
- Follow CLAUDE.md: TDD, the Handmade style, and Simplified Technical
  English for documents, comments and commit messages.
- Work only in {worktree}. Do not edit another worktree.
- Nobody can answer a question. If you cannot continue without a decision,
  add the question to "The open questions" of docs/EUROPE.md. Then commit
  what works, and stop with a report.
- The Bash tool halves the backslashes of a heredoc. Use Write and Edit for
  code and for any text with a backslash.
- A hook can refuse the first edit of a file. State the facts that it asks
  for, then send the same edit again.

## Build and test
- Build: `cmd //c "cd /d {worktree} && build.bat"`
- All tests: `cmd //c "cd /d {worktree} && run_tests.bat"`
- One ui test: `cmd //c "cd /d {worktree}\build && ui_tests.exe <part of the name>"`
- The submodules are ready. Do not change the commit of a submodule.

## Commit and push
1. Commit on {branch} after the tests pass.
2. Run `git fetch origin` and `git rebase origin/master`.
3. If the rebase has a conflict, keep the work of both sides. Then build
   and run all tests again.
4. Run `git push origin HEAD:master`. Never force a push.
5. If the push fails because master moved, go back to step 2.

## The report
Your last message is the report. Give, in this order:
- the commit of the push, or the reason for no push;
- the result of run_tests.bat;
- the questions that you added to docs/EUROPE.md.
