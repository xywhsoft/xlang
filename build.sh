#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"

if [ "$(uname -s 2>/dev/null || echo unknown)" = "Linux" ]; then
    exec bash "$script_dir/build_linux.sh"
fi

if command -v cmd.exe >/dev/null 2>&1; then
    exec cmd.exe /d /c build.bat
fi

echo "DEMO6 build failed: unsupported host. Use Linux or Windows MinGW/MSYS." >&2
exit 1
