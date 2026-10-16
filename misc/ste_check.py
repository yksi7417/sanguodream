"""Check a Markdown document against the STE rule in .claude/rules/ste-writing.md.

Usage: python -I misc/ste_check.py [--quiet] [--skip HEADING ...] FILE ...

The checker reads each file and reports, for the prose only:
- sentences with more than 25 words
- passive forms, for example "is read" or "was found"
- words that end in -ing (the -ing form of a verb)
- words from the replacement table of the rule

The checker skips fenced code blocks, table rows and headings. It also treats
inline code and text in double quotes as names, not as prose: file names,
identifiers, commands and test names are not sentences. A sentence ends at a
period, an exclamation mark or a question mark. A colon or a semicolon does
not end a sentence, so the words after it count against the limit.

With --skip HEADING the checker does not read the section under a heading
with that exact text, for example a section that a job tells you to keep as
it is. The last line for each file is a summary that a commit message can
quote:

    SUMMARY FILE long=N passive=N ing=N words=N

With --quiet the checker prints only the summary lines. The exit code is 0
when every file has long=0 and words=0, and 1 otherwise.
"""

import re
import sys

MAX_WORDS = 25

# Words and phrases from the replacement table of the rule, as regexes.
REPLACEMENT_PATTERNS = [
    r"\butili[sz]e[sd]?\b",
    r"\bemploy(s|ed|ing)?\b",
    r"\bleverage[sd]?\b",
    r"\bperform(s|ed|ing)?\b",
    r"\bcarr(y|ies|ied) out\b",
    r"\bexecute[sd]?\b",
    r"\bensure[sd]?\b",
    r"\bin order to\b",
    r"\bprior to\b",
    r"\bsubsequent(ly)?\b",
    r"\bfollowing\b",
    r"\bcommence[sd]?\b",
    r"\binitiate[sd]?\b",
    r"\bterminate[sd]?\b",
    r"\bassist(s|ed|ance)?\b",
    r"\brequire[sd]?\b",
    r"\badequate(ly)?\b",
    r"\benough\b",
    r"\babout\b",
    r"\baround\b",
    r"\bshould\b",
    r"\bcould\b",
    r"\bwould\b",
    r"\bmay\b",
    r"\bshall\b",
    r"\be\.g\.",
    r"\bi\.e\.",
    r"\betc\b",
    r"\band so on\b",
    r"\band/or\b",
    r"\bvia\b",
    r"\bin case of\b",
    r"\bonce\b",
    r"\bvery\b",
    r"\bquite\b",
    r"\brather\b",
    # "which" without a comma before it.
    r"(?<![,;]\s)(?<![,;])\bwhich\b",
]
REPLACEMENT_RE = re.compile("|".join(REPLACEMENT_PATTERNS), re.IGNORECASE)

PASSIVE_AUX = r"(?:is|are|was|were|be|been|being|get|gets|got)"
PASSIVE_IRREGULAR = (
    "read|found|built|written|made|done|given|kept|held|left|put|set|shown|"
    "taken|run|sent|lost|hit|cut|split|seen|known|drawn|thrown|chosen|"
    "driven|hidden|bound|begun|won|told|sold|meant|spent|caught|bought|"
    "brought|fed|led|met|paid|said|laid|lit|shot|understood"
)
PASSIVE_RE = re.compile(
    r"\b" + PASSIVE_AUX + r"\s+(?:(?:not|also|then|never|only|still|always)\s+)?"
    r"((?:\w{2,}ed)|" + PASSIVE_IRREGULAR + r")\b",
    re.IGNORECASE,
)
# Words that end in -ed but are not past participles.
NOT_PARTICIPLE = {
    "red", "need", "indeed", "hundred", "bed", "feed", "seed", "speed",
    "shed", "wed", "fixed",
}

ING_RE = re.compile(r"\b[A-Za-z]+ing\b")
# -ing words that are nouns or technical names, not verb forms.
ING_ALLOWED = {
    "thing", "things", "nothing", "something", "anything", "everything",
    "during", "string", "strings", "ring", "spring", "king", "kingdoms",
    "bring", "wing", "sing", "swing", "being", "evening", "morning",
    "pathfinding", "setting", "settings", "building", "buildings",
    "ping", "sling", "fling", "cling", "sting", "sibling", "ceiling",
}

INLINE_CODE_RE = re.compile(r"`[^`]*`")
QUOTED_RE = re.compile(r'"[^"\n]*"')
LINK_RE = re.compile(r"\[([^\]]*)\]\([^)]*\)")
# A token with a path separator, an extension or an underscore is a name.
PATH_TOKEN_RE = re.compile(r"(?:[/\\_]|\.[A-Za-z]{1,4}\b)")
TOKEN_RE = re.compile(r"(\S+?)([.,;:)\]]*)(?=\s|$)")
LIST_ITEM_RE = re.compile(r"^\s*(?:[-*+]|\d+\.)\s+")
HEADING_RE = re.compile(r"^\s*#+\s*(.*?)\s*#*\s*$")
SENTENCE_END_RE = re.compile(r"(?<=[.!?])\s+(?=[A-Z`\"(])|(?<=[.!?])\s*$")


def prose_paragraphs(lines, skip_headings):
    """Yield (first_line_number, text) for each prose paragraph.

    A paragraph ends at a blank line, a heading, a table row, a code fence or
    the start of a new list item. The sections under the headings in
    skip_headings are not read.
    """
    in_code = False
    skipping = False
    current = []
    start = 0
    for number, raw in enumerate(lines, start=1):
        line = raw.rstrip("\n")
        stripped = line.strip()
        if stripped.startswith("```") or stripped.startswith("~~~"):
            if current:
                yield start, " ".join(current)
                current = []
            in_code = not in_code
            continue
        if in_code:
            continue
        heading = HEADING_RE.match(line)
        if heading:
            skipping = heading.group(1) in skip_headings
        if skipping:
            continue
        is_break = (
            stripped == ""
            or stripped.startswith("#")
            or stripped.startswith("|")
            or LIST_ITEM_RE.match(line)
        )
        if is_break and current:
            yield start, " ".join(current)
            current = []
        if stripped == "" or stripped.startswith("#") or stripped.startswith("|"):
            continue
        text = LIST_ITEM_RE.sub("", line, count=1).strip()
        if not current:
            start = number
        current.append(text)
    if current:
        yield start, " ".join(current)


def mask_names(text):
    """Replace inline code, quoted names, links and path tokens with a token."""
    text = INLINE_CODE_RE.sub("NAME", text)
    text = QUOTED_RE.sub("NAME", text)
    text = LINK_RE.sub(r"\1", text)
    return TOKEN_RE.sub(mask_token, text)


def mask_token(match):
    """Replace one path-like token with NAME and keep its trailing punctuation."""
    core, tail = match.group(1), match.group(2)
    if PATH_TOKEN_RE.search(core):
        return "NAME" + tail
    return core + tail


def split_sentences(text):
    parts = [p.strip() for p in SENTENCE_END_RE.split(text)]
    return [p for p in parts if p]


def word_count(sentence):
    return len([w for w in re.split(r"\s+", sentence) if re.search(r"\w", w)])


def check_file(path, quiet, skip_headings):
    with open(path, encoding="utf-8") as handle:
        lines = handle.readlines()
    long_count = passive_count = ing_count = words_count = 0
    for number, paragraph in prose_paragraphs(lines, skip_headings):
        masked = mask_names(paragraph)
        for sentence in split_sentences(masked):
            count = word_count(sentence)
            if count > MAX_WORDS:
                long_count += 1
                report(quiet, path, number, "LONG %d words: %s" % (count, sentence))
            for match in PASSIVE_RE.finditer(sentence):
                if match.group(1).lower() in NOT_PARTICIPLE:
                    continue
                passive_count += 1
                report(quiet, path, number, "PASSIVE: %s" % match.group(0))
            for match in ING_RE.finditer(sentence):
                word = match.group(0)
                if word.lower() in ING_ALLOWED:
                    continue
                ing_count += 1
                report(quiet, path, number, "ING: %s" % word)
            for match in REPLACEMENT_RE.finditer(sentence):
                words_count += 1
                report(quiet, path, number, "WORD: %s" % match.group(0))
    print("SUMMARY %s long=%d passive=%d ing=%d words=%d"
          % (path, long_count, passive_count, ing_count, words_count))
    return long_count == 0 and words_count == 0


def report(quiet, path, number, message):
    if not quiet:
        print("%s:%d: %s" % (path, number, message))


def main(argv):
    quiet = False
    skip_headings = set()
    paths = []
    arguments = list(argv)
    while arguments:
        argument = arguments.pop(0)
        if argument == "--quiet":
            quiet = True
        elif argument == "--skip" and arguments:
            skip_headings.add(arguments.pop(0))
        else:
            paths.append(argument)
    if not paths:
        print(__doc__)
        return 2
    ok = True
    for path in paths:
        if not check_file(path, quiet, skip_headings):
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
