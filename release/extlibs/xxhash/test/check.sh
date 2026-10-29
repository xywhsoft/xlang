#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"

for pair in private_update_negative:updateNative private_string_length_negative:updateStrNative; do
    case=${pair%%:*}
    method=${pair#*:}
    output=$(mktemp "${TMPDIR:-/tmp}/xlang-xxhash-access-XXXXXX")
    if timeout 90s release/xl check "release/extlibs/xxhash/test/$case.xl" > "$output" 2>&1; then
        printf 'xxhash: %s must be rejected\n' "$case" >&2
        rm -f -- "$output"
        exit 1
    fi
    if ! grep -Fq "$method" "$output" || ! grep -Fq 'access' "$output"; then
        cat "$output" >&2
        rm -f -- "$output"
        exit 1
    fi
    rm -f -- "$output"
done

bash tools/check_extlib_xxhash.sh
bash tools/check_extlib_linux_xl.sh xxhash
diff -u release/examples/extlibs/xxhash_basic.expected.txt \
    <(timeout 90s release/xl run release/extlibs/xxhash/examples/basic.c)
timeout 90s release/xl run release/extlibs/xxhash/test/seeded_boundaries.xl
timeout 90s release/xl run release/extlibs/xxhash/test/stream_limit.xl
central=$(timeout 90s release/xl run release/examples/extlibs/xxhash.xl)
if [[ ${central##*$'\n'} != 'xxhash example: PASS' ]]; then
    printf 'xxhash central example failed: %s\n' "$central" >&2
    exit 1
fi
printf 'xxhash package C ASan/UBSan and XLang gate: PASS\n'
