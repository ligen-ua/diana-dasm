"""One-line summary of a pytest JUnit XML report, for run_tests.cmd / cmake-test.sh.

    python tests/cli/junit_summary.py tests/cli/_out/junit.xml
    -> 70 passed, 11 known bugs: B1 B2 B6 B7 B9 B11 B12 B13 B14 B17

Known bugs are the xfail tests; their reason starts with the bug ID from
testing/cmdline-exploratory-test-report.md ("B7: ...").
"""
import re
import sys
import xml.etree.ElementTree as ET


def summarize(path: str) -> str:
    counts = {"passed": 0, "failed": 0, "errors": 0, "skipped": 0}
    bugs = set()
    xfails = 0
    for case in ET.parse(path).getroot().iter("testcase"):
        if case.find("failure") is not None:
            counts["failed"] += 1
        elif case.find("error") is not None:
            counts["errors"] += 1
        elif (skipped := case.find("skipped")) is not None:
            if skipped.get("type") == "pytest.xfail":
                xfails += 1
                m = re.match(r"(B\d+)\b", skipped.get("message", ""))
                bugs.add(m.group(1) if m else "?")
            else:
                counts["skipped"] += 1
        else:
            counts["passed"] += 1

    parts = [f"{n} {name}" for name, n in counts.items() if n or name == "passed"]
    if xfails:
        ids = sorted(bugs, key=lambda b: (len(b), b))
        parts.append(f"{len(bugs)} known bugs ({xfails} tests): {' '.join(ids)}")
    return ", ".join(parts)


if __name__ == "__main__":
    try:
        print(summarize(sys.argv[1]))
    except Exception as e:  # the summary is informational, never fail the run over it
        print(f"no summary: {e}")
