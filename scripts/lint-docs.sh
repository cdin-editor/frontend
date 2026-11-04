#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"
if command -v markdownlint-cli2 >/dev/null 2>&1; then
    markdownlint-cli2
else
    echo "markdownlint-cli2 is not installed; skipping local Markdown lint (CI docs job installs it)"
fi
if git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$repo_root" diff --check
else
    # Source archives and the initial ZIP may not contain a .git directory.
    python3 - "$repo_root" <<'PYCODE'
from pathlib import Path
import sys
root = Path(sys.argv[1])
problems = []
for path in root.rglob("*"):
    if not path.is_file() or any(part in {".git", ".deps", "build", "dist"} or part.startswith("build-") for part in path.parts):
        continue
    try:
        content = path.read_text(encoding="utf-8")
    except (UnicodeDecodeError, OSError):
        continue
    for number, line in enumerate(content.splitlines(), 1):
        if line.endswith((" ", "\t")):
            problems.append(f"{path.relative_to(root)}:{number}: trailing whitespace")
if problems:
    print("\n".join(problems), file=sys.stderr)
    raise SystemExit(1)
PYCODE
fi
