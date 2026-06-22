import os
import re
import subprocess
import sys

def get_color(pct):
    if pct >= 90: return "brightgreen"
    if pct >= 80: return "green"
    if pct >= 70: return "yellowgreen"
    if pct >= 60: return "yellow"
    return "red"

def get_doc_coverage():
    try:
        with open("include/cfs/cfs.h", "r") as f:
            lines = [line.strip() for line in f.readlines()]
        total = 0
        documented = 0
        for i, line in enumerate(lines):
            if line.startswith("CFS_API "):
                total += 1
                j = i - 1
                while j >= 0 and lines[j] == "":
                    j -= 1
                if j >= 0 and lines[j].endswith("*/"):
                    documented += 1
        if total == 0: return 0.0
        return (documented / total) * 100.0
    except Exception as e:
        print(f"Error calculating doc coverage: {e}")
        return 0.0

def get_test_coverage():
    try:
        if os.name == 'nt':
            return None
        cmd = ["gcovr", "-r", "..", ".", "-f", r".*/cfs\.h$", "-f", r".*/cfs\.c$",
               "--gcov-ignore-parse-errors=negative_hits.warn", "--print-summary"]
        res = subprocess.run(cmd, cwd="build_precommit", capture_output=True, text=True)
        if res.returncode != 0:
            print("gcovr failed:\n" + res.stderr)
            return None

        match = re.search(r"lines:\s+([0-9.]+)%", res.stdout)
        if match:
            return float(match.group(1))
        return None
    except Exception as e:
        print(f"Error calculating test coverage: {e}")
        return None

def main():
    doc_cov = get_doc_coverage()
    test_cov = get_test_coverage()

    print(f"Doc Coverage: {doc_cov:.1f}%")
    if test_cov is not None:
        print(f"Test Coverage: {test_cov:.1f}%")
    else:
        print("Test Coverage: N/A")
        test_cov = 0.0 # Default for failure or missing

    try:
        with open("README.md", "r") as f:
            readme = f.read()

        # Remove existing shields and any trailing newlines from them
        readme = re.sub(r"\[!\[Doc Coverage\]\(.*?\)\]\(.*?\)\n?", "", readme)
        readme = re.sub(r"\[!\[Test Coverage\]\(.*?\)\]\(.*?\)\n?", "", readme)

        doc_color = get_color(doc_cov)
        doc_shield = f"[![Doc Coverage](https://img.shields.io/badge/docs-{doc_cov:.0f}%25-{doc_color}.svg)](#)"

        test_shield = ""
        if test_cov is not None:
            test_color = get_color(test_cov)
            test_shield = f"[![Test Coverage](https://img.shields.io/badge/coverage-{test_cov:.0f}%25-{test_color}.svg)](#)"

        # Find license shield and insert right after it
        license_regex = r"(\[!\[License\].*?\]\(.*?\)\n?)"

        insert_str = r"\1" + doc_shield + "\n"
        if test_shield:
            insert_str += test_shield + "\n"

        readme = re.sub(license_regex, insert_str, readme, count=1)

        with open("README.md", "w") as f:
            f.write(readme)

    except Exception as e:
        print(f"Error updating README.md: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
