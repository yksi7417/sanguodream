"""Checks the rows of docs/europe/E9-CANDIDATES.tsv against the English Wikipedia.

Usage:
    python -X utf8 -I misc/e9_wiki_check.py [docs/europe/E9-CANDIDATES.tsv]

The tool asks the MediaWiki API for the article of each row. It reads the
categories "NNNN births" and "NNNN deaths" of the article. A category of a
decade, for example "1190s births", holds a year of that decade. The tool
writes these columns into the table, and it keeps the other columns:
- wiki_title: the title of the article after the redirects, or -;
- wiki_birth and wiki_death: the years of the categories, with a bar
  between two values, or -;
- wiki_check: ok, or a list of the findings: missing, disambiguation,
  birth differs, death differs, no birth category, no death category.

The API refuses a fast client with 429. The tool waits as the answer says.
The User-Agent names the repository, as the API policy asks.
"""
import csv
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

API = "https://en.wikipedia.org/w/api.php"
USER_AGENT = "hexdom-e9-source-check/1.0 (https://github.com/yksi7417/sanguodream)"
BATCH = 20
PAUSE_SECONDS = 3
RESULT_COLUMNS = ["wiki_title", "wiki_birth", "wiki_death", "wiki_check"]


def ask(titles):
    params = {"action": "query", "format": "json", "redirects": 1, "prop": "categories|pageprops",
              "cllimit": "max", "titles": "|".join(titles)}
    request = urllib.request.Request(API + "?" + urllib.parse.urlencode(params),
                                     headers={"User-Agent": USER_AGENT, "Api-User-Agent": USER_AGENT})
    for attempt in range(8):
        try:
            with urllib.request.urlopen(request, timeout=60) as answer:
                return json.load(answer)["query"]
        except urllib.error.HTTPError as error:
            if error.code != 429:
                raise
            time.sleep(min(int(error.headers.get("Retry-After") or 10 * (attempt + 1)), 120))
    sys.exit("e9_wiki_check: the API refuses each request with 429")


def title_of(url):
    match = re.match(r"https://en\.wikipedia\.org/wiki/(.+)$", url.strip())
    return urllib.parse.unquote(match.group(1)).replace("_", " ") if match else None


def category_years(categories, word):
    years = []
    for category in categories:
        match = re.match(r"Category:(\d{3,4}s?) " + word + "$", category["title"])
        if match:
            years.append(match.group(1))
    return years


def first_year(text):
    match = re.search(r"\d{3,4}", text or "")
    return int(match.group(0)) if match else None


def agrees(year, values):
    for value in values:
        if value.endswith("s") and year is not None and int(value[:-1]) <= year < int(value[:-1]) + 10:
            return True
        if not value.endswith("s") and year == int(value):
            return True
    return False


def main(path):
    with open(path, encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    columns = [name for name in rows[0].keys() if name not in RESULT_COLUMNS] + RESULT_COLUMNS
    titles = sorted({title_of(row["wikipedia_url"]) for row in rows if title_of(row["wikipedia_url"])})
    pages = {}
    for start in range(0, len(titles), BATCH):
        batch = titles[start:start + BATCH]
        result = ask(batch)
        normal = {pair["from"]: pair["to"] for pair in result.get("normalized", [])}
        moved = {pair["from"]: pair["to"] for pair in result.get("redirects", [])}
        by_title = {page["title"]: page for page in result["pages"].values()}
        for title in batch:
            name = normal.get(title, title)
            final = moved.get(name, name)
            pages[title] = (final, by_title.get(final))
        time.sleep(PAUSE_SECONDS)
    for row in rows:
        title = title_of(row["wikipedia_url"])
        final, page = pages.get(title, (None, None)) if title else (None, None)
        if not page or "missing" in page:
            row.update(wiki_title="-", wiki_birth="-", wiki_death="-", wiki_check="missing")
            continue
        categories = page.get("categories", [])
        births = category_years(categories, "births")
        deaths = category_years(categories, "deaths")
        findings = []
        if "disambiguation" in page.get("pageprops", {}):
            findings.append("disambiguation")
        for field, values in (("birth", births), ("death", deaths)):
            if not values:
                findings.append("no %s category" % field)
            elif not agrees(first_year(row[field]), values):
                findings.append("%s differs" % field)
        row.update(wiki_title=final, wiki_birth="|".join(births) or "-", wiki_death="|".join(deaths) or "-",
                   wiki_check="; ".join(findings) or "ok")
    with open(path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows({name: row.get(name, "") for name in columns} for row in rows)
    checks = {}
    for row in rows:
        checks[row["wiki_check"]] = checks.get(row["wiki_check"], 0) + 1
    for check, count in sorted(checks.items(), key=lambda item: -item[1]):
        print("%4d %s" % (count, check))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "docs/europe/E9-CANDIDATES.tsv")
