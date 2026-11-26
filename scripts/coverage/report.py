#!/usr/bin/env python3
"""The coverage report's own pages, from what llvm-cov wrote.

scripts/coverage.sh runs it once llvm-cov has written its pages to <out>/files
and its summary to <out>/summary.json:

    python3 scripts/coverage/report.py <out> [<llvm-cov's version>]

It writes the summary page, <out>/index.html: the totals, a trend across
versions, and each top folder with a bar, which opens to its files, each
linking to its page from llvm-cov. And <out>/badge.svg, for the README, and
<out>/history.json, the trend's points. It puts llvm-cov's pages in the same
look, with this folder's coverage.css and a bar on top that leads back.

The trend has a point for each version measured: the seed's, in this folder's
history.json, which is committed, the points of <out>/history.json that the
seed doesn't have, which CI fetches from the published report first, and this
run's, for the version in CMakeLists.txt, in place of any other of its version.
"""

import datetime
import html
import json
import math
import os
import pathlib
import re
import shutil
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
REPOSITORY = "https://github.com/nenuadrian/nanoMMO"
KINDS = ("lines", "functions", "branches", "regions")
NAMES = {"lines": "Lines", "functions": "Functions", "branches": "Branches",
         "regions": "Regions"}


def version():
    found = re.search(r"project\(nanommo\s+VERSION\s+([0-9.]+)",
                      (ROOT / "CMakeLists.txt").read_text())
    if not found:
        sys.exit("report.py: no version in CMakeLists.txt's project()")
    return found.group(1)


def share(part):
    return part["covered"] / part["count"] if part["count"] else None


def percent(part):
    value = share(part)
    return "–" if value is None else f"{100 * value:.1f}%"


# The history.

def key(point):
    """A point's place on the trend: its version's numbers, or before there
    were versions (episode 59), its episode's, where 0.N would be."""
    if "version" in point:
        return tuple(int(n) for n in point["version"].split("."))
    return (0, int(re.match(r"ep(\d+)/", point["ref"]).group(1)), -1)


def label(point):
    return point.get("version") or point["ref"].split("/")[0]


def read_points(path):
    try:
        points = json.loads(path.read_text())["points"]
        for point in points:
            key(point)
            numbers = [point[k][n] for k in KINDS for n in ("covered", "count")]
            if not all(isinstance(n, int) for n in numbers) or not point["measured"]:
                raise ValueError(f"{label(point)} has a number or its day missing")
    except (ValueError, KeyError, TypeError, AttributeError) as error:
        sys.exit(f"report.py: {path} isn't a history: {error!r}")
    return points


def history_json(points):
    """The history as JSON, a point a line."""
    lines = ",\n".join("  " + json.dumps(point) for point in points)
    return f'{{"points": [\n{lines}\n]}}\n'


def history(out, current):
    seed = read_points(HERE / "history.json")
    published = out / "history.json"
    earlier = read_points(published) if published.exists() else []
    points = {}
    for point in earlier + seed + [current]:
        points[label(point)] = point
    return sorted(points.values(), key=key)


# The summary page.

def meter(part):
    value = share(part) or 0
    return (f'<span class="meter" aria-hidden="true">'
            f'<span style="width:{100 * value:.2f}%"></span></span>')


def tile(kind, part):
    lead = " lead" if kind == "lines" else ""
    return (f'<div class="tile{lead}"><div class="label">{NAMES[kind]}</div>'
            f'<div class="value">{percent(part)}</div>{meter(part)}'
            f'<div class="of">{part["covered"]:,} of {part["count"]:,} run</div></div>')


def nice_step(value):
    """A round step, 1, 2, 2.5 or 5 times a power of ten, at or over the value."""
    magnitude = 10 ** math.floor(math.log10(value)) if value > 0 else 1
    for step in (1, 2, 2.5, 5, 10):
        if step * magnitude >= value:
            return step * magnitude
    return 10 * magnitude


def compact(n):
    return f"{n / 1000:g}K" if n >= 10000 else f"{n:,.0f}"


def chart(points, values, ticks, as_line, show):
    """The trend of one number as SVG: a line, or columns. A point's hover
    gives its version and its lines run."""
    width, height = 480, 220
    left, right, upper, lower = 46, 18, 24, 28
    plot_w, plot_h = width - left - right, height - upper - lower
    top = ticks[-1]
    n = len(points)
    step = plot_w / n
    xs = [left + step * (i + 0.5) for i in range(n)]

    def y(v):
        return upper + plot_h * (1 - v / top)

    svg = [f'<svg viewBox="0 0 {width} {height}" aria-hidden="true">']
    for tick in ticks:
        ty = y(tick)
        svg.append(f'<line class="grid" x1="{left}" x2="{width - right}" '
                   f'y1="{ty:.1f}" y2="{ty:.1f}"/>'
                   f'<text class="axis" x="{left - 8}" y="{ty + 4:.1f}" '
                   f'text-anchor="end">{show(tick)}</text>')
    # The versions along the bottom: as many as fit, and always the last.
    every = max(1, math.ceil(46 / step))
    for i, point in enumerate(points):
        if (n - 1 - i) % every == 0:
            svg.append(f'<text class="axis" x="{xs[i]:.1f}" y="{height - 8}" '
                       f'text-anchor="middle">{html.escape(label(point))}</text>')
    if as_line:
        line = " ".join(f"{x:.1f},{y(v):.1f}" for x, v in zip(xs, values))
        svg.append(f'<polygon class="area" points="{xs[0]:.1f},{y(0):.1f} {line} '
                   f'{xs[-1]:.1f},{y(0):.1f}"/><polyline class="line" points="{line}"/>')
    bar = min(24, step * 0.6)
    for x, point, v in zip(xs, points, values):
        lines = point["lines"]
        tip = (f'{label(point)}: {percent(lines)} of {lines["count"]:,} lines run '
               f'({lines["covered"]:,})')
        svg.append(f'<g class="point"><title>{html.escape(tip)}</title>'
                   f'<rect class="hit" x="{x - step / 2:.1f}" y="{upper}" '
                   f'width="{step:.1f}" height="{plot_h}"/>')
        if as_line:
            svg.append(f'<circle class="dot" cx="{x:.1f}" cy="{y(v):.1f}" r="4"/>')
        else:
            # A column with its top corners rounded, square on the baseline.
            top_y, base = y(v), y(0)
            r = min(4, (base - top_y) / 2)
            x0, x1 = x - bar / 2, x + bar / 2
            svg.append(
                f'<path class="bar" d="M{x0:.1f},{base:.1f} V{top_y + r:.1f} '
                f'Q{x0:.1f},{top_y:.1f} {x0 + r:.1f},{top_y:.1f} H{x1 - r:.1f} '
                f'Q{x1:.1f},{top_y:.1f} {x1:.1f},{top_y + r:.1f} V{base:.1f} Z"/>')
        svg.append("</g>")
    svg.append(f'<text class="value" x="{xs[-1]:.1f}" y="{y(values[-1]) - 10:.1f}" '
               f'text-anchor="middle">{show(values[-1], last=True)}</text></svg>')
    return "".join(svg)


def trend(points):
    shares = [100 * share(p["lines"]) for p in points]
    measured = [p["lines"]["count"] for p in points]
    step = nice_step(max(measured) / 4)
    count_ticks = [step * i for i in range(math.ceil(max(measured) / step) + 1)]

    def show_share(v, last=False):
        return f"{v:.1f}%" if last else f"{v:g}%"

    def show_count(v, last=False):
        return f"{v:,}" if last else compact(v)

    rows = []
    for point in reversed(points):
        lines = point["lines"]
        cells = "".join(f"<td>{percent(point[k])}</td>" for k in KINDS)
        rows.append(
            f'<tr><td><a href="{REPOSITORY}/tree/{html.escape(point["ref"])}">'
            f'{html.escape(label(point))}</a></td>'
            f'<td>{lines["covered"]:,} of {lines["count"]:,}</td>{cells}'
            f'<td>{html.escape(point["measured"])}</td></tr>')
    heads = "".join(f"<th>{NAMES[k]}</th>" for k in KINDS)
    return (
        '<div class="charts">'
        '<figure class="chart"><figcaption>Lines run '
        '<span class="muted">· the share the tests run</span></figcaption>'
        f'{chart(points, shares, [0, 25, 50, 75, 100], True, show_share)}</figure>'
        '<figure class="chart"><figcaption>Lines measured '
        '<span class="muted">· all there is to run</span></figcaption>'
        f'{chart(points, measured, count_ticks, False, show_count)}</figure>'
        '</div>'
        '<details class="table"><summary>The numbers, newest first</summary>'
        '<table class="numbers"><thead><tr><th>Version</th><th>Lines run</th>'
        f'{heads}<th>Measured</th></tr></thead><tbody>{"".join(rows)}</tbody>'
        '</table></details>')


def cells(summary):
    lines = summary["lines"]
    return (f'<span class="bar">{meter(lines)}</span>'
            f'<span class="r num">{percent(lines)}</span>'
            f'<span class="r num wide">{lines["covered"]:,} / {lines["count"]:,}</span>'
            f'<span class="r num wide">{percent(summary["functions"])}</span>'
            f'<span class="r num wide">{percent(summary["branches"])}</span>')


def source_root(files):
    """Where the paths llvm-cov names start: the repo's top, as in Docker, or
    for a report made elsewhere, the folder all of them are in."""
    names = [entry["filename"] for entry in files]
    if all(name.startswith(f"{ROOT}/") for name in names):
        return str(ROOT)
    return os.path.commonpath(names)


def folders(out, files):
    """Each top folder with its sums, which opens to its files."""
    root = source_root(files)
    groups = {}
    for entry in files:
        path = os.path.relpath(entry["filename"], root)
        top, _, rest = path.partition("/")
        group = groups.setdefault(
            top, {"files": [], **{k: {"covered": 0, "count": 0} for k in KINDS}})
        group["files"].append((rest or top, path, entry))
        for kind in KINDS:
            group[kind]["covered"] += entry["summary"][kind]["covered"]
            group[kind]["count"] += entry["summary"][kind]["count"]

    rows = ['<div class="row head"><span>Folder</span><span>Lines run</span>'
            '<span class="r">Lines</span><span class="r wide">Run / all</span>'
            '<span class="r wide">Functions</span><span class="r wide">Branches</span></div>']
    for top in sorted(groups):
        group = groups[top]
        files = []
        for name, path, entry in sorted(group["files"]):
            page = f'files/coverage/{entry["filename"].lstrip("/")}.html'
            shown = html.escape(name)
            if (out / page).exists():
                shown = f'<a href="{html.escape(page)}">{shown}</a>'
            files.append(f'<div class="row file"><span class="name" '
                         f'title="{html.escape(path)}">{shown}</span>'
                         f'{cells(entry["summary"])}</div>')
        count = len(group["files"])
        rows.append(
            f'<details id="{html.escape(top)}"><summary class="row">'
            f'<span class="name">{html.escape(top)}/ <span class="muted">'
            f'{count} file{"s" if count != 1 else ""}</span></span>{cells(group)}'
            f'</summary><div class="files">{"".join(files)}</div></details>')
    return "".join(rows), sorted(groups)


def page(out, summary, points, number, now, llvm):
    data = summary["data"][0]
    rows, tops = folders(out, data["files"])
    side_folders = "".join(
        f'<a class="sub" href="#{html.escape(t)}">{html.escape(t)}/</a>' for t in tops)
    tiles = "".join(tile(k, data["totals"][k]) for k in KINDS)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>nanoMMO: Test coverage</title>
<link rel="stylesheet" href="coverage.css">
</head>
<body class="summary">
<aside class="side"><div class="stick">
<div class="project"><a href="../index.html"><span class="name">nanoMMO</span>
<span class="number">{number}</span></a>
<div class="brief">Building an MMO: The Engineering Series</div></div>
<nav>
<a href="../index.html">API docs</a>
<a href="../md_docs_2checks.html">Code style and checks</a>
<a class="current" href="index.html">Test coverage</a>
{side_folders}
<a href="files/index.html">Every file</a>
<a href="{REPOSITORY}">Code on GitHub</a>
</nav>
</div></aside>
<div class="page">
<header><h1>Test coverage</h1></header>
<main class="contents">
<p>This is how much of nanoMMO {number}'s code the tests run, as Clang's source-based coverage
measures it. All of the tests run against MySQL, Redis and Consul, and the code of the servers,
the bot and the client that no test reaches counts too, at zero. A branch is each way an
<code>if</code> or a <code>&amp;&amp;</code> can go, and a region is a stretch of code that runs
as one.</p>
<section class="totals" aria-label="Totals">{tiles}</section>
<h2>Across versions</h2>
<p>Each version's coverage as it was measured, the newest on the right.</p>
{trend(points)}
<h2>By folder</h2>
<p>Each folder opens to its files, and each file to its code, with how many times each line
ran.</p>
<div class="folders">{rows}</div>
</main>
<footer>Measured {now:%Y-%m-%d %H:%M} UTC with {html.escape(llvm)}</footer>
</div>
</body>
</html>
"""


# The badge, drawn as shields.io draws one, in the report's blue, with
# Verdana's widths at 11 pixels so the text fits its boxes.

WIDTHS = {**dict.fromkeys("0123456789", 7.0), ".": 4.0, "%": 11.8, "–": 7.0,
          "c": 5.7, "o": 6.7, "v": 6.5, "e": 6.6, "r": 4.7, "a": 6.6, "g": 6.9}


def badge(lines):
    left, right = "coverage", percent(lines)
    lw = sum(WIDTHS.get(c, 7.0) for c in left) + 10
    rw = sum(WIDTHS.get(c, 7.0) for c in right) + 10
    width = round(lw + rw)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="20" role="img" aria-label="{left}: {right}">
<title>{left}: {right}</title>
<linearGradient id="s" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>
<clipPath id="r"><rect width="{width}" height="20" rx="3" fill="#fff"/></clipPath>
<g clip-path="url(#r)"><rect width="{lw:.1f}" height="20" fill="#555"/><rect x="{lw:.1f}" width="{width - lw:.1f}" height="20" fill="#1779c4"/><rect width="{width}" height="20" fill="url(#s)"/></g>
<g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
<text x="{lw / 2:.1f}" y="15" fill="#010101" fill-opacity=".3">{left}</text><text x="{lw / 2:.1f}" y="14">{left}</text>
<text x="{lw + rw / 2:.1f}" y="15" fill="#010101" fill-opacity=".3">{right}</text><text x="{lw + rw / 2:.1f}" y="14">{right}</text>
</g>
</svg>
"""


# llvm-cov's pages: this folder's stylesheet after theirs, a title, and a bar
# on top in place of their heading, which leads back to the summary.

HEADING = re.compile(r"<h1>[^<]*</h1><h2>Coverage Report</h2>")
SOURCE = re.compile(r"<div class='source-name-title'><pre>([^<]*)</pre>")


def restyle(out, number, root):
    files = out / "files"
    for path in files.rglob("*.html"):
        text = path.read_text()
        if "<body class='llvm-cov'>" in text:
            continue
        up = os.path.relpath(out, path.parent)
        if path == files / "index.html":
            title, crumbs = "every file", "Every file"
        else:
            source = SOURCE.search(text)
            shown = (os.path.relpath(html.unescape(source.group(1)), root) if source
                     else path.relative_to(files).as_posix()[:-5])
            title = shown
            crumbs = (f'<a href="{up}/files/index.html">Every file</a> / '
                      f'<span class="here">{html.escape(shown)}</span>')
        bar = (f'<div class="bar"><a href="{up}/index.html"><span class="name">nanoMMO'
               f'</span> <span class="number">{number}</span></a><span class="crumbs">'
               f'<a href="{up}/index.html">Test coverage</a> / {crumbs}</span></div>')
        text = text.replace(
            "</head>",
            f"<link rel='stylesheet' type='text/css' href='{up}/coverage.css'>"
            "<meta name='color-scheme' content='light dark'>"
            f"<title>nanoMMO: test coverage of {html.escape(title)}</title></head>", 1)
        text = text.replace("<body>", f"<body class='llvm-cov'>{bar}", 1)
        text = HEADING.sub("", text, count=1)
        path.write_text(text)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    out = pathlib.Path(sys.argv[1])
    llvm = sys.argv[2] if len(sys.argv) == 3 else "llvm-cov"
    summary = json.loads((out / "summary.json").read_text())
    totals = summary["data"][0]["totals"]
    now = datetime.datetime.now(datetime.timezone.utc)
    number = version()
    current = {"version": number, "ref": f"v{number}", "measured": f"{now:%Y-%m-%d}",
               **{k: {"covered": totals[k]["covered"], "count": totals[k]["count"]}
                  for k in KINDS}}
    points = history(out, current)
    (out / "history.json").write_text(history_json(points))
    shutil.copy(HERE / "coverage.css", out / "coverage.css")
    restyle(out, number, source_root(summary["data"][0]["files"]))
    (out / "index.html").write_text(page(out, summary, points, number, now, llvm))
    (out / "badge.svg").write_text(badge(totals["lines"]))
    # The README's badge, which shields.io draws from this, as it draws the
    # checks' badges beside it.
    (out / "badge.json").write_text(json.dumps({
        "schemaVersion": 1, "label": "coverage", "message": percent(totals["lines"]),
        "color": "1779c4"}) + "\n")
    print(f"report.py: {out / 'index.html'}, {percent(totals['lines'])} of the lines run, "
          f"{len(points)} versions on the trend")


if __name__ == "__main__":
    main()
