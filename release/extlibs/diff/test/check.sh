#!/usr/bin/env bash
set -euo pipefail
exec python3 -B -X utf8 "$(dirname -- "${BASH_SOURCE[0]}")/check.py" "$@"
