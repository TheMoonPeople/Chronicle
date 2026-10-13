#!/usr/bin/env python3
"""Generate the objdiff report for one release, as decomp.dev reads it."""

import argparse
import json
import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
REGIONS = ("NTSC", "PAL")


def load_region(region):
    path = ROOT / ("objdiff.json" if region == "NTSC" else "ps2/build/pal/objdiff.json")
    config = json.loads(path.read_text(encoding="utf-8"))
    for unit in config["units"]:
        for key in ("target_path", "base_path"):
            if key in unit:
                unit[key] = str(ROOT / unit[key])
    return config


def print_summary(report):
    red, green, yellow, gray, reset = "\033[91m", "\033[92m", "\033[93m", "\033[90m", "\033[0m"
    categories = {entry["id"]: entry["measures"] for entry in report["categories"]}
    for section, image in (("game", "SCUS_971.11"), ("title", "TITLE.BIN"),
                           ("dun", "DUN.BIN")):
        measures = categories[section]
        units = [unit for unit in report["units"] if section in
                 unit.get("metadata", {}).get("progress_categories", ())]
        fuzzy = sum(1 for unit in units for function in unit.get("functions", ())
                    if function.get("fuzzy_match_percent", 100.0) < 100.0)
        perfect = measures["matched_functions"]
        unmatched = measures["total_functions"] - perfect - fuzzy
        passed = float(measures["matched_code_percent"]) == 100.0
        status = f"{green}OK{reset}" if passed else f"{red}FAILED{reset}"
        print(f"{image}: {status} {gray}({perfect} perfect, {fuzzy} fuzzy, "
              f"{unmatched} unmatched){reset}")

    measures = report["measures"]
    units = report["units"]
    fuzzy = sum(1 for unit in units for function in unit.get("functions", ())
                if function.get("fuzzy_match_percent", 100.0) < 100.0)
    perfect = measures["matched_functions"]
    unmatched = measures["total_functions"] - perfect - fuzzy
    perfect_share = float(measures["matched_code_percent"])
    fuzzy_share = float(measures["fuzzy_match_percent"]) - perfect_share
    unmatched_share = 100.0 - float(measures["fuzzy_match_percent"])
    print("\nCode, by byte")
    for label, color, share, count in (
        ("Perfect", green, perfect_share, perfect),
        ("Fuzzy", yellow, fuzzy_share, fuzzy),
        ("Unmatched", red, unmatched_share, unmatched),
    ):
        print(f"  {color}{label:<11}{reset} {share:6.2f}%    {count:4d} functions")
    # Each unit's target carries the data the unit defines, at retail's bytes
    # and sizes (scripts/build/objdiff_data.py); what objdiff leaves unmatched
    # is a datum the source sizes or places apart from retail's symbol table.
    # It is not a byte-for-byte linked-image comparison, so do not call its
    # gap a diff.
    total_data = int(measures.get("total_data", 0))
    if total_data:
        unmatched_data = total_data - int(measures.get("matched_data", 0))
        print(f"  Data       {yellow}{unmatched_data} bytes not matched by objdiff{reset}")
    differences = [(unit["name"], function["name"],
                    "fuzzy" if "fuzzy_match_percent" in function else "unmatched")
                   for unit in units for function in unit.get("functions", ())
                   if function.get("fuzzy_match_percent", 0.0) < 100.0]
    differences.sort(key=lambda entry: entry[2] != "unmatched")
    for unit, function, status in differences[:10]:
        color = yellow if status == "fuzzy" else red
        print(f"    {color}{status}{reset} {unit}/{function}")
    if len(differences) > 10:
        print(f"    ... and {len(differences) - 10} more")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--region", choices=REGIONS,
                        default=os.environ.get("REGION", "PAL").upper())
    args = parser.parse_args()

    config = load_region(args.region)
    project = ROOT / "build" / "report_project" / args.region.lower()
    project.mkdir(parents=True, exist_ok=True)
    (project / "objdiff.json").write_text(json.dumps(config, indent=2) + "\n",
                                          encoding="utf-8")
    output = ROOT / "progress" / args.region.lower() / "report.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["objdiff-cli", "report", "generate", "--project", str(project),
                    "--output", str(output)], cwd=ROOT, check=True)
    print_summary(json.loads(output.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()
