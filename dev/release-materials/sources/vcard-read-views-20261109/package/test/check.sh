#!/usr/bin/env bash
set -Eeuo pipefail
runtime=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
root=$(cd -- "$runtime/../.." && pwd)
exec "${PYTHON:-python3}" "$root/tools/check_vcard_read_views.py" --runtime "$runtime" --sanitize "$@"
