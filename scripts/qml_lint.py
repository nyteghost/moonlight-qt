#!/usr/bin/env python3
"""Runs qmllint over Vibelight's QML files and fails on real errors.

The app's C++ types (StreamingPreferences, SystemProperties, ...) are only registered when the
app runs, so qmllint can't resolve them and reports lots of "unresolved" warnings. Those are
expected and ignored here. What fails the check is anything qmllint reports as a syntax error
or critical message, which would break the page when it loads.
"""

import glob
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QML_DIR = os.path.join(ROOT, "app", "gui")

# Message ids qmllint uses for things that are expected to be unresolved in this setup
IGNORED_IDS = {"import", "unqualified", "unresolved-type", "missing-type", "incompatible-type"}


def find_qmllint():
    for name in ("qmllint", "qmllint6", "qmllint-qt6"):
        path = shutil.which(name)
        if path:
            return path
    return None


def main():
    qmllint = find_qmllint()
    if not qmllint:
        print("qmllint was not found on PATH")
        return 2

    files = sorted(glob.glob(os.path.join(QML_DIR, "*.qml")))
    if not files:
        print("no QML files found in", QML_DIR)
        return 2

    print("Using", qmllint)
    print("Checking", len(files), "QML files")

    failures = 0
    for path in files:
        rel = os.path.relpath(path, ROOT)
        proc = subprocess.run([qmllint, "--json", "-", "-I", QML_DIR, path],
                              capture_output=True, text=True)
        text = proc.stdout.strip() or proc.stderr.strip()

        try:
            report = json.loads(proc.stdout)
        except ValueError:
            # Older/newer qmllint without JSON output to stdout: fall back to the plain text
            lowered = text.lower()
            bad = [line for line in text.splitlines()
                   if "syntax" in line.lower() or line.lower().startswith("error")]
            if bad:
                failures += 1
                print("FAIL", rel)
                for line in bad:
                    print("   ", line)
            else:
                print("ok  ", rel, "(plain output)")
            continue

        problems = []
        for entry in report.get("files", []):
            for warning in entry.get("warnings", []):
                wid = warning.get("id", "")
                wtype = warning.get("type", "")
                if wtype in ("critical", "error") or wid == "syntax":
                    if wid not in IGNORED_IDS:
                        problems.append(warning)

        if problems:
            failures += 1
            print("FAIL", rel)
            for p in problems:
                print("   line %s: %s (%s)" % (p.get("line", "?"), p.get("message", ""), p.get("id", "")))
        else:
            print("ok  ", rel)

    print()
    print("%d of %d files have errors" % (failures, len(files)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
