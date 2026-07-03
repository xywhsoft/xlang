#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "DEMO6 build.sh currently targets Windows MinGW/MSYS and requires cmd.exe." >&2
    exit 1
fi

cd "$script_dir"
cmd.exe /d /c build.bat
