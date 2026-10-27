#!/usr/bin/env python3
"""Check that the Flatpak manifest stages every file Kazarov's CMake downloads for the PC build.

    flatpak_sources.py [--kazarov tools/kazarov] [--manifest port/flatpak/org.themoonpeople.Chronicle.yml]
                       [--cmake port/CMakeLists.txt] [--includers port/CMakeLists.txt tools/dcdata/CMakeLists.txt]

flatpak-builder downloads a module's sources before it enters the build sandbox, and the sandbox has
no network, so a download CMake tries during the build fails (after its retries). Kazarov's CMake
downloads three kinds of dependency for the port, read here from every Kazarov module Chronicle's CMake
includes (cmake/KazarovPort.cmake for the game, cmake/KazarovRust.cmake for dcdata) and the modules they
include in turn:

- a library FetchContent_Declare names with URL and URL_HASH. The manifest stages it as an archive
  at the same SHA-256 with dest flatpak-deps/<name>, which port/CMakeLists.txt hands FetchContent as
  FETCHCONTENT_SOURCE_DIR_<NAME>. One declared with FIND_PACKAGE_ARGS may instead come from a
  manifest module that installs it (nlohmann/json): any source at its SHA-256 will do.
- a single file _kz_port_vendor takes from KZ_PORT_VENDOR_DIRS (port/CMakeLists.txt) and otherwise
  downloads. The manifest stages it as a file at the same SHA-256 in one of those directories.
- a crate Kazarov's Cargo.lock names, which cargo would fetch from crates.io. The manifest takes them
  all from port/flatpak/cargo-sources.json, which port/flatpak/cargo_sources.py writes from the lock.

A dependency Kazarov declares that the manifest does not stage at its hash is missing, with the
manifest entry to add printed; a flatpak-deps/ entry or vendored file whose hash Kazarov no longer
declares is stale. Either fails the check (exit 1), so a Kazarov pin that adds or bumps a download
is caught in its own pull request rather than by the next Flatpak build. Needs only the standard
library: the manifest's sources are read line by line.
"""

import argparse
import json
import os
import re
import sys
import tomllib

FETCH = re.compile(r"FetchContent_Declare\(\s*(\w+)(.*?)\)", re.S)
VENDOR = re.compile(r"_kz_port_vendor\(\s*\w+\s+(\S+)\s+(\S+)\s+([0-9a-f]{64})\s*\)")
INCLUDE = re.compile(r"include\(\s*\"?\$\{CMAKE_CURRENT(?:_FUNCTION)?_LIST_DIR\}/([\w.]+\.cmake)\"?\s*\)")
PROJECT_INCLUDE = re.compile(r"include\(\s*\"?\$\{(?:KAZAROV_SOURCE_DIR|KZ_ROOT)\}/cmake/([\w.]+\.cmake)\"?\s*\)")
REGISTRY = "registry+https://github.com/rust-lang/crates.io-index"
VENDOR_DIRS = re.compile(r"set\(KZ_PORT_VENDOR_DIRS([^)]*)\)")
SOURCE_KEY = re.compile(r"^\s*(?:- )?([\w-]+):\s*(.*?)\s*$")


def strip_comments(text):
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def included_modules(includers):
    """The Kazarov CMake modules Chronicle's own CMake files include."""
    modules = []
    for path in includers:
        with open(path, encoding="utf-8") as stream:
            modules.extend(PROJECT_INCLUDE.findall(strip_comments(stream.read())))
    return modules


def kazarov_downloads(cmake_dir, roots):
    """The FetchContent and _kz_port_vendor downloads of the root modules and what they include."""
    fetches, vendored, seen = [], [], set()
    pending = list(roots)
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        path = os.path.join(cmake_dir, name)
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as stream:
            text = strip_comments(stream.read())
        pending.extend(INCLUDE.findall(text))
        for dependency, body in FETCH.findall(text):
            url = re.search(r"\bURL\s+(\S+)", body)
            sha = re.search(r"\bURL_HASH\s+SHA256=([0-9a-fA-F]{64})", body)
            if url and sha:
                fetches.append({"name": dependency, "url": url.group(1), "sha256": sha.group(1).lower(),
                                "found_package": "FIND_PACKAGE_ARGS" in body, "where": name})
        for file_name, url, sha in VENDOR.findall(text):
            vendored.append({"file": file_name, "url": url, "sha256": sha, "where": name})
    return fetches, vendored


def manifest_sources(path):
    """Every source of every module: the keys under each '- type:' item."""
    sources, current, indent = [], None, None
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            text = line.split(" #", 1)[0].rstrip()
            if not text.strip() or text.lstrip().startswith("#"):
                continue
            depth = len(text) - len(text.lstrip())
            item = re.match(r"^(\s*)- type:\s*(\S+)", text)
            if item:
                current = {"type": item.group(2)}
                indent = len(item.group(1))
                sources.append(current)
                continue
            if current is not None and depth > indent:
                key = SOURCE_KEY.match(text)
                if key and not text.lstrip().startswith("- "):
                    current[key.group(1)] = key.group(2).strip("'\"")
                    continue
            current = None
    return sources


def vendor_dirs(cmake_path):
    with open(cmake_path, encoding="utf-8") as stream:
        found = VENDOR_DIRS.search(strip_comments(stream.read()))
    if not found:
        return set()
    return {entry.replace("${CMAKE_SOURCE_DIR}/", "").strip("/") for entry in found.group(1).split()}


def locked_crates(lock_path):
    """{dest: sha256} for each crates.io package in Cargo.lock, as cargo_sources.py stages it."""
    with open(lock_path, "rb") as stream:
        packages = tomllib.load(stream)["package"]
    return {"cargo/vendor/%s-%s" % (p["name"], p["version"]): p["checksum"]
            for p in packages if p.get("source") == REGISTRY}


def staged_crates(json_path):
    with open(json_path, encoding="utf-8") as stream:
        return {s["dest"]: s["sha256"] for s in json.load(stream) if s.get("type") == "archive"}


def archive_entry(fetch):
    lines = ["      - type: archive",
             "        url: " + fetch["url"],
             "        sha256: " + fetch["sha256"]]
    if not re.search(r"\.(tar\.gz|tgz|tar\.xz|tar\.bz2|zip)$", fetch["url"]):
        lines.append("        archive-type: " + ("zip" if "/zip/" in fetch["url"] else "tar-gzip"))
    lines.append("        dest: flatpak-deps/" + fetch["name"])
    return "\n".join(lines)


def file_entry(vendor, directory):
    return "\n".join(["      - type: file",
                      "        url: " + vendor["url"],
                      "        sha256: " + vendor["sha256"],
                      "        dest: " + directory])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--kazarov", default="tools/kazarov")
    parser.add_argument("--manifest", default="port/flatpak/org.themoonpeople.Chronicle.yml")
    parser.add_argument("--cmake", default="port/CMakeLists.txt")
    parser.add_argument("--includers", nargs="+", default=["port/CMakeLists.txt", "tools/dcdata/CMakeLists.txt"])
    options = parser.parse_args()

    cmake_dir = os.path.join(options.kazarov, "cmake")
    if not os.path.isfile(os.path.join(cmake_dir, "KazarovPort.cmake")):
        return "flatpak_sources.py: %s has no cmake/KazarovPort.cmake (git submodule update --init)" % options.kazarov

    fetches, vendored = kazarov_downloads(cmake_dir, included_modules(options.includers))
    sources = manifest_sources(options.manifest)
    dirs = vendor_dirs(options.cmake)
    problems = []

    by_sha = {}
    for source in sources:
        if "sha256" in source:
            by_sha.setdefault(source["sha256"].lower(), []).append(source)

    for fetch in fetches:
        staged = [s for s in by_sha.get(fetch["sha256"], []) if s.get("dest") == "flatpak-deps/" + fetch["name"]]
        if staged or (fetch["found_package"] and fetch["sha256"] in by_sha):
            continue
        problems.append("missing: %s (FetchContent_Declare in %s) is not staged at its hash. Add to the "
                        "chronicle module's sources:\n%s" % (fetch["name"], fetch["where"], archive_entry(fetch)))

    for vendor in vendored:
        staged = [s for s in by_sha.get(vendor["sha256"], [])
                  if s.get("dest") in dirs and (s.get("dest-filename") or s.get("url", "").rsplit("/", 1)[-1]) == vendor["file"]]
        if staged:
            continue
        directory = sorted(dirs)[0] if dirs else "<a KZ_PORT_VENDOR_DIRS directory>"
        problems.append("missing: %s (_kz_port_vendor in %s) is not staged at its hash. Add to the chronicle "
                        "module's sources:\n%s" % (vendor["file"], vendor["where"], file_entry(vendor, directory)))

    declared = {f["sha256"] for f in fetches} | {v["sha256"] for v in vendored}
    for source in sources:
        dest = source.get("dest", "")
        if dest.startswith("flatpak-deps/") or dest in dirs:
            if source.get("sha256", "").lower() not in declared:
                problems.append("stale: %s in %s is not a download Kazarov declares at this hash; update or "
                                "remove it" % (source.get("url", "?"), dest))

    crates = 0
    lock = os.path.join(options.kazarov, "Cargo.lock")
    if os.path.isfile(lock):
        cargo_json = os.path.join(os.path.dirname(options.manifest), "cargo-sources.json")
        locked = locked_crates(lock)
        crates = len(locked)
        with open(options.manifest, encoding="utf-8") as stream:
            listed = re.search(r"^\s*- cargo-sources\.json\s*$", strip_comments(stream.read()), re.M)
        if not listed:
            problems.append("missing: the chronicle module does not list cargo-sources.json among its sources")
        if not os.path.isfile(cargo_json) or staged_crates(cargo_json) != locked:
            problems.append("stale: %s does not stage exactly the crates %s locks; run port/flatpak/cargo_sources.py"
                            % (cargo_json, lock))

    for problem in problems:
        print(problem + "\n")
    if problems:
        print("flatpak_sources.py: %d problem(s); the Flatpak build cannot configure offline until they are fixed"
              % len(problems))
        return 1
    print("flatpak_sources.py: the manifest stages all %d of Kazarov's downloads and %d crates"
          % (len(fetches) + len(vendored), crates))
    return 0


if __name__ == "__main__":
    sys.exit(main())
