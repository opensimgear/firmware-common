#!/usr/bin/env python3
"""Enforce shared portable-core line coverage after the native CTest suite."""
import argparse
import pathlib
import re
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=pathlib.Path, required=True)
parser.add_argument("--minimum", type=float, default=70)
parser.add_argument("--gcov-command", default="gcov")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
build = args.build.resolve()
objects = sorted((build / "CMakeFiles/osg_core.dir/src").rglob("*.gcno"))
if not objects:
    parser.error("no instrumented osg_core objects; configure OSG_TEST_COVERAGE=ON")
output = build / "coverage"
output.mkdir(exist_ok=True)
executed = total = 0
for obj in objects:
    source = root / "src" / obj.relative_to(build / "CMakeFiles/osg_core.dir/src")
    source = source.with_suffix("")  # file.c.gcno -> file.c
    result = subprocess.run(
        [*shlex.split(args.gcov_command), "-b", "-o", str(obj), str(source)],
        cwd=output, text=True, capture_output=True, check=True,
    )
    report = output / (source.name + ".gcov")
    if not report.is_file():
        sys.exit(f"missing coverage report for {source}: {result.stdout} {result.stderr}")
    covered = lines = 0
    for line in report.read_text().splitlines():
        match = re.match(r"\s*([^:]+):\s*([0-9]+):", line)
        if not match or match[2] == "0" or match[1].strip() == "-":
            continue
        lines += 1
        if match[1].strip() not in ("#####", "=====", "0"):
            covered += 1
    if not lines:
        sys.exit(f"empty coverage report for {source}")
    percent = 100 * covered / lines
    print(f"{source.relative_to(root)}: {covered}/{lines} lines ({percent:.2f}%)")
    executed += covered
    total += lines
percent = 100 * executed / total
print(f"Portable core: {executed}/{total} lines ({percent:.2f}%); required >{args.minimum:g}%")
if percent <= args.minimum:
    sys.exit(1)
