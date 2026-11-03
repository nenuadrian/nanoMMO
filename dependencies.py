#!/usr/bin/env python3
"""Lists the libraries CMakeLists.txt fetches from GitHub, the version each is
pinned to, and the newest one there is."""

import argparse
import json
import os
import re
import urllib.error
import urllib.request

CMAKELISTS = "CMakeLists.txt"

# A FetchContent_Declare of a GitHub archive, by tag or commit, or of a file
# from a release.
DECLARE = re.compile(
    r"FetchContent_Declare\(\s*(?P<name>[\w-]+)\s+"
    r"URL (?P<url>https://github\.com/(?P<repo>[^/\s]+/[^/\s]+)/"
    r"(?:archive/(?:refs/tags/)?(?P<archive>\S+?)\.tar\.gz"
    r"|releases/download/(?P<release>[^/\s]+)/\S+))"
)
COMMIT = re.compile(r"[0-9a-f]{40}")
# A tag that's a version, such as v1.18.0, 20250512.1 or asio-1-38-2, and
# not a release candidate, such as v37.0-rc1.
VERSION_TAG = re.compile(r"[a-z-]*\d+(?:[.-]\d+)*")


def github(path):
    request = urllib.request.Request(f"https://api.github.com/{path}")
    # A token, as CI has, gets more requests an hour than none.
    if token := os.environ.get("GITHUB_TOKEN"):
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def numbers(version):
    return [int(number) for number in re.findall(r"\d+", version)]


def newest(repo, pinned):
    """The repo's newest version, the way it's pinned: a commit, or a tag."""
    if COMMIT.fullmatch(pinned):
        return github(f"repos/{repo}/commits/HEAD")["sha"]
    try:
        return github(f"repos/{repo}/releases/latest")["tag_name"]
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
    # No releases, only tags.
    tags = [tag["name"] for tag in github(f"repos/{repo}/tags?per_page=100")]
    return max((tag for tag in tags if VERSION_TAG.fullmatch(tag)), key=numbers)


def is_newer(latest, pinned):
    if COMMIT.fullmatch(pinned):
        return latest != pinned
    return numbers(latest) > numbers(pinned)


def short(version):
    return version[:7] if COMMIT.fullmatch(version) else version


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    with open(CMAKELISTS) as file:
        text = file.read()
    for match in DECLARE.finditer(text):
        pinned = match["archive"] or match["release"]
        latest = newest(match["repo"], pinned)
        status = f"-> {short(latest)}" if is_newer(latest, pinned) else "newest"
        print(f"{match['name']:<16} {short(pinned):<12} {status}")


if __name__ == "__main__":
    main()
