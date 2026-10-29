#!/usr/bin/env bash
# B13-B17b Regex/Pattern snapshots, Builder transactions and checked callbacks.
set -euo pipefail
SCRIPT="$(realpath "${BASH_SOURCE[0]}")"
ROOT="$(cd "${3:-$(dirname "$SCRIPT")/..}" && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
GROUP="${2:-all}"
[[ "$GROUP" = all || "$GROUP" = pattern ]] || { printf 'Unknown validation group\n' >&2; exit 1; }
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve regex evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
mkdir -p "$OUT/include"
CC="$(command -v gcc)"; CLI="$ROOT/release/xl"; checks=0
trap 'status=$?; if ((status)); then printf "{\"Complete\":false,\"Checks\":%d,\"Exit\":%d}\n" "$checks" "$status" >"$OUT/results.json"; fi' EXIT
run() {
    local name="$1"; shift
    local seconds=120
    # One full registry has over a million dependency pairs. Keep its complete
    # check once per cohort, with a separate bound from individual fixtures.
    [[ "$name" != artifact-registry ]] || seconds=1800
    if ! timeout "${seconds}s" "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 35 "$OUT/$name.log" >&2; printf 'Regex check failed: %s\n' "$name" >&2; exit 1
    fi
    checks=$((checks+1))
}
cases=(stdlib_regex_core_only stdlib_regex_match stdlib_regex_errors stdlib_regex_iterators
    stdlib_regex_replace_only stdlib_regex_replace_buffer stdlib_regex_replace_buffer_errors stdlib_regex_split_only stdlib_regex_set_only stdlib_regex_extended_errors stdlib_regex_byte_boundaries
    stdlib_regex_replace_func stdlib_regex_replace_func_buffer stdlib_regex_replace_func_errors stdlib_regex_error_metadata
    stdlib_pattern_core_only stdlib_pattern_options stdlib_pattern_options_only stdlib_pattern_match stdlib_pattern_errors stdlib_pattern_tags stdlib_pattern_error_modes
    stdlib_pattern_builder stdlib_pattern_builder_lifetime stdlib_pattern_builder_cleanup)
negative=(xlir_regex_bad_result xlir_regex_bad_owner xlir_regex_bad_constructor xlir_regex_bad_nullable
    xlir_regex_bad_set_items xlir_regex_bad_set_hits xlir_regex_bad_iterator_owner
    xlir_regex_bad_callback_arity xlir_regex_bad_callback_result xlir_regex_bad_callback_raw xlir_regex_bad_error_input
    xlir_pattern_bad_result xlir_pattern_bad_tag xlir_pattern_bad_options xlir_pattern_bad_builder_tags
    xlir_pattern_bad_edit_items xlir_pattern_bad_edit_payload xlir_pattern_bad_key_owner)
privacy=(xlir_regex_private_consume xlir_regex_private_split_consume xlir_pattern_private_code xlir_pattern_private_builder)
undeclared=(sem_native_class_extern_bad_type sem_native_class_extern_return_bad)
if [[ "$GROUP" = pattern ]]; then
    cases=(stdlib_pattern_core_only stdlib_pattern_options stdlib_pattern_options_only stdlib_pattern_match stdlib_pattern_errors stdlib_pattern_tags stdlib_pattern_error_modes
        stdlib_pattern_builder stdlib_pattern_builder_lifetime stdlib_pattern_builder_cleanup)
    negative=(xlir_pattern_bad_result xlir_pattern_bad_tag xlir_pattern_bad_options xlir_pattern_bad_builder_tags
        xlir_pattern_bad_edit_items xlir_pattern_bad_edit_payload xlir_pattern_bad_key_owner)
    privacy=(xlir_pattern_private_code xlir_pattern_private_builder)
    undeclared=()
fi
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_stdlib_regex.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' tools/gen_tcc_vfs_resources.py
    printf '%s\0' "$SCRIPT"
    printf '%s\0' test/support/xlir_resident_closure.c
    printf '%s\0' test/cases/xlir_regex_host.c test/support/regex_native.c test/cases/xlir_pattern_host.c test/support/pattern_native.c
    printf '%s\0' test/cases/xlir_pattern_builder_host.c test/support/pattern_builder_native.c
    printf '%s\0' test/cases/xlir_regex_extended_host.c test/cases/xlir_regex_callback_host.c
    printf '%s\0' test/cases/xlir_text_value_contract.c test/cases/xlir_artifact_runtime_contract.c
    printf '%s\0' test/cases/xlir_error_projection_contract.c test/cases/xlir_continuation_owner_test.h test/cases/xlir_synthetic_owner_edit.h
    printf '%s\0' test/cases/xlir_string_interpolation_host.c
    for fixture in "${cases[@]}" "${negative[@]}" "${privacy[@]}" "${undeclared[@]}"; do printf '%s\0' "test/cases/$fixture.xl"; done
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
python3 - "$ROOT" "$OUT" <<'PY'
from pathlib import Path
import hashlib,json,shutil,sys
root,out=map(Path,sys.argv[1:]);sys.path.insert(0,str(root/'tools'))
from gen_tcc_vfs_resources import load_resources
manifest=out/'runtime-headers.manifest'
rows=[]
for line in (root/'tools/tcc_vfs_manifest_linux_xlc.txt').read_text(encoding='utf-8').splitlines():
    parts=line.strip().split('|')
    if len(parts)>=3 and (parts[1]=='include/__xl_runtime' or parts[1].startswith('include/__xl_runtime/')):rows.append(line)
assert rows
manifest.write_text('\n'.join(rows)+'\n',encoding='utf-8')
records=[]
for resource in load_resources(manifest,root,root,root,out):
    target=out/resource.virtual
    assert target.resolve().is_relative_to((out/'include/__xl_runtime').resolve())
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(resource.source,target)
    digest=hashlib.sha256(resource.source.read_bytes()).hexdigest()
    assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
    records.append(dict(Source=str(resource.source),Path=str(target),Sha256=digest))
assert records
(out/'runtime-headers.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
PY
find "$OUT/include" -type f -print0 | sort -zu | xargs -0 sha256sum >>"$OUT/inputs.sha256"
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -Wno-pointer-sign -Wno-format-truncation -I. -Irelease -Irelease/include -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
# The actual artifact registry owns transitive resident dependencies. Native
# tests must use the same closure as Windows and generated artifacts; a short
# hand-maintained source list misses cleanup/handle policy dependencies.
run resident-closure-build "$CC" "${strict[@]}" -O0 -ffunction-sections -fdata-sections \
    -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables \
    test/support/xlir_resident_closure.c src/compile/xl6_compile_unit.c \
    -Wl,--gc-sections -o "$OUT/resident-closure"
close_sources() {
    local name="$1"; shift
    local args=(); local source
    for source in "$@"; do args+=(--source "$source"); done
    run "$name.closure" "$OUT/resident-closure" "${args[@]}"
    run "$name.sources" python3 -c 'import json,sys; r=json.load(open(sys.argv[1],encoding="utf-8")); s=r["Sources"]; assert s and len(s)==len(set(s)); print("\n".join(s))' "$OUT/$name.closure.log"
}
if [[ "$GROUP" = all ]]; then
close_sources native src/runtime/xl6_regex_core.c src/runtime/xl6_regex_match.c
mapfile -t native_sources <"$OUT/native.sources.log"
run native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_regex_host.c \
    test/support/regex_native.c "${native_sources[@]}" \
    -pthread -ldl -lm -o "$OUT/native"
run native "$OUT/native"
grep -q '68 actual failures, 5 first no-hit completions' "$OUT/native.log"
close_sources extended src/runtime/xl6_regex_core.c src/runtime/xl6_regex_match.c src/runtime/xl6_regex_iterator.c \
    src/runtime/xl6_regex_replace.c src/runtime/xl6_regex_replace_buffer.c src/runtime/xl6_regex_split.c src/runtime/xl6_regex_set.c src/runtime/xl6_array_input.c src/runtime/xl6_sequence.c
mapfile -t extended_sources <"$OUT/extended.sources.log"
run extended-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_regex_extended_host.c \
    test/support/regex_native.c "${extended_sources[@]}" \
    -pthread -ldl -lm -o "$OUT/extended-native"
run extended-native "$OUT/extended-native"
grep -Eq '[1-9][0-9]* actual failures, 9 first no-hit completions' "$OUT/extended-native.log"
fi
close_sources pattern src/runtime/xl6_pattern_core.c src/runtime/xl6_pattern_options.c src/runtime/xl6_pattern_match.c
mapfile -t pattern_sources <"$OUT/pattern.sources.log"
run pattern-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_pattern_host.c \
    test/support/pattern_native.c "${pattern_sources[@]}" \
    -pthread -ldl -lm -o "$OUT/pattern-native"
run pattern-native "$OUT/pattern-native"
grep -q '60 actual failures, 5 first no-hit completions' "$OUT/pattern-native.log"
close_sources builder src/runtime/xl6_pattern_builder.c src/runtime/xl6_pattern_core.c src/runtime/xl6_pattern_options.c src/runtime/xl6_array_input.c src/runtime/xl6_sequence.c
mapfile -t builder_sources <"$OUT/builder.sources.log"
run pattern-builder-native-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_pattern_builder_host.c \
    test/support/pattern_builder_native.c "${builder_sources[@]}" \
    -pthread -ldl -lm -o "$OUT/pattern-builder-native"
run pattern-builder-native "$OUT/pattern-builder-native"
grep -Eq '[1-9][0-9]* actual failures, 10 first no-hit completions' "$OUT/pattern-builder-native.log"
if [[ "$GROUP" = all ]]; then
run callback-native-build "$CC" "${strict[@]}" test/cases/xlir_regex_callback_host.c -lxl "${libs[@]}" -o "$OUT/callback-native"
run callback-native "$OUT/callback-native"
grep -q 'regex callback native OOM: 133 actual failures, 3 first no-hit completions, balanced' "$OUT/callback-native.log"
fi
run contract-build "$CC" "${strict[@]}" test/cases/xlir_text_value_contract.c -lxlc "${libs[@]}" -o "$OUT/contract"
run artifact-build "$CC" "${strict[@]}" -O2 test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
run artifact-registry "$OUT/artifact" --registry
run error-projection-contract-build "$CC" "${strict[@]}" test/cases/xlir_error_projection_contract.c -lxlc "${libs[@]}" -o "$OUT/error-projection-contract"
run error-projection-contract "$OUT/error-projection-contract" test/cases/stdlib_pattern_tags.xl "$OUT/error-projection.closed.c"
run error-projection-modes-contract "$OUT/error-projection-contract" test/cases/stdlib_pattern_error_modes.xl "$OUT/error-projection-modes.closed.c"
for fixture in "${cases[@]}"; do
    source="test/cases/$fixture.xl"
    run "$fixture.cgen" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.c"
    if [[ "$fixture" = stdlib_pattern_tags ]]; then
        run "$fixture.error-projection-equality" cmp "$OUT/$fixture.c" "$OUT/error-projection.closed.c"
    elif [[ "$fixture" = stdlib_pattern_error_modes ]]; then
        run "$fixture.error-projection-equality" cmp "$OUT/$fixture.c" "$OUT/error-projection-modes.closed.c"
    fi
    run "$fixture.repeat" "$CLI" cgen "$source" --inc-path "$OUT/include" --emit "$OUT/$fixture.repeat.c"
    run "$fixture.determinism" cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
    run "$fixture.contract" "$OUT/contract" "$source" "$OUT/$fixture.contract.c"
    run "$fixture.contract-equality" cmp "$OUT/$fixture.c" "$OUT/$fixture.contract.c"
    if grep -Eq 'xl6_xrt_take_cstr|xrtRegexMatcher(Find|Capture)[(]|xrtPattern(CompileManyConfig|Match|Lookup)[(]|^(static )?(void[*]|xstrview|int64_t|bool) +xl6(Regex|Pattern).*[{]' "$OUT/$fixture.c"; then
        printf 'Regex implementation in Cgen\n' >&2; exit 1
    fi
    run "$fixture.strict-build" "$CC" "${strict[@]}" -include "$OUT/$fixture.c" \
        test/cases/xlir_string_interpolation_host.c -lxl "${libs[@]}" -o "$OUT/$fixture.strict"
    run "$fixture.strict" "$OUT/$fixture.strict"
    if [[ "$fixture" = stdlib_regex_match || "$fixture" = stdlib_regex_iterators || "$fixture" = stdlib_regex_replace_only ||
          "$fixture" = stdlib_regex_replace_buffer || "$fixture" = stdlib_regex_split_only || "$fixture" = stdlib_regex_set_only ||
          "$fixture" = stdlib_regex_replace_func || "$fixture" = stdlib_regex_replace_func_buffer ||
          "$fixture" = stdlib_pattern_core_only || "$fixture" = stdlib_pattern_options || "$fixture" = stdlib_pattern_options_only ||
          "$fixture" = stdlib_pattern_match || "$fixture" = stdlib_pattern_tags || "$fixture" = stdlib_pattern_builder_lifetime ]]; then
        for ((point=0;point<2048;++point)); do
            name="$fixture.oom-$point"; run "$name" "$OUT/$fixture.strict" "$point"
            grep -q 'hit=0 balanced' "$OUT/$name.log" && break
        done
        ((point<2048))
    fi
    run "$fixture.jit" "$CLI" run "$source" --inc-path "$OUT/include"
    run "$fixture.aot-build" "$CLI" build "$source" --inc-path "$OUT/include" --save-temps -o "$OUT/$fixture.aot"
    run "$fixture.aot" "$OUT/$fixture.aot"
    run "$fixture.native-plan" "$OUT/artifact" --emit-native "$source" "$OUT/$fixture.native.c"
    saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$fixture.aot-build.log")"
    [[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]]
    run "$fixture.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.native.c"
    run "$fixture.native-build" "$CC" "${strict[@]}" -Ilib/xruntime -c "$OUT/$fixture.native.c" -o "$OUT/$fixture.native.o"
    run "$fixture.generated-build" "$CC" "${strict[@]}" -c "$OUT/$fixture.c" -o "$OUT/$fixture.generated.o"
    run "$fixture.selection" python3 -c 'import json,sys; p=next(json.loads(s) for s in open(sys.argv[1]) if s.startswith("{\"Fixture\"")); name=sys.argv[2]
if name.startswith("stdlib_pattern_"):
    expected={"stdlib_pattern_core_only":["core"],"stdlib_pattern_options":["core","options"],"stdlib_pattern_options_only":["options"],"stdlib_pattern_match":["core","match"],"stdlib_pattern_errors":["core","options","match"],"stdlib_pattern_tags":["core","match"],"stdlib_pattern_error_modes":["core"],
        "stdlib_pattern_builder":["builder","core","match"],"stdlib_pattern_builder_lifetime":["builder","core","options","match"],"stdlib_pattern_builder_cleanup":["builder","core"]}[name]
    assert sorted(s for s in p["Sources"] if "src/runtime/xl6_pattern_" in s)==sorted("src/runtime/xl6_pattern_"+s+".c" for s in expected)
    assert not any("src/runtime/xl6_regex_" in s for s in p["Sources"])
    assert "XRT_MODULE_PATTERN" in p["NativeModules"] and "XRT_MODULE_REGEX_CORE" not in p["NativeModules"] and "XRT_MODULE_BUFFER" not in p["NativeModules"]
else:
    expected={"stdlib_regex_core_only":["core"],"stdlib_regex_match":["core","match"],"stdlib_regex_errors":["core","match"],"stdlib_regex_iterators":["core","match","iterator"],"stdlib_regex_replace_only":["core","replace"],"stdlib_regex_replace_buffer":["core","replace_buffer"],"stdlib_regex_replace_buffer_errors":["core","replace_buffer"],"stdlib_regex_split_only":["core","split"],"stdlib_regex_set_only":["core","set"],"stdlib_regex_extended_errors":["core","match","iterator","replace","split","set"],"stdlib_regex_byte_boundaries":["core","match","replace","split"],"stdlib_regex_replace_func":["core","match","replace_func"],"stdlib_regex_replace_func_buffer":["core","match","replace_func","replace_func_buffer"],"stdlib_regex_replace_func_errors":["core","match","replace_func","replace_func_buffer"],"stdlib_regex_error_metadata":["core","set"]}[name]; assert sorted(s for s in p["Sources"] if "src/runtime/xl6_regex_" in s)==sorted("src/runtime/xl6_regex_"+s+".c" for s in expected); assert "XRT_MODULE_REGEX_CORE" in p["NativeModules"]; assert ("XRT_MODULE_BUFFER" in p["NativeModules"])==(name in ("stdlib_regex_replace_buffer","stdlib_regex_replace_buffer_errors","stdlib_regex_replace_func_buffer","stdlib_regex_replace_func_errors")); assert "XRT_MODULE_PATTERN" not in p["NativeModules"]' \
        "$OUT/$fixture.native-plan.log" "$fixture"
done
for fixture in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' "$OUT/$fixture.log"
    checks=$((checks+1))
done
for fixture in "${privacy[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -Eq 'unknown class/union field|private' "$OUT/$fixture.log"
    checks=$((checks+1))
done
for fixture in "${undeclared[@]}"; do
    if "$CLI" cgen "test/cases/$fixture.xl" --emit "$OUT/$fixture.c" >"$OUT/$fixture.log" 2>&1; then exit 1; fi
    [[ ! -e "$OUT/$fixture.c" ]]
    grep -q 'does not support typed container direct C ABI' "$OUT/$fixture.log"
    checks=$((checks+1))
done
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
printf '{"Complete":true,"Checks":%d,"PositiveFixtures":%d,"AbiNegativeFixtures":%d,"PrivacyFixtures":%d,"UndeclaredArrayFixtures":%d,"ActualNativeClosures":%d,"InputsUnchanged":true,"Group":"%s","SanitizedScope":"Selected native XRT/adapter hosts; generated libxl hosts use logical OOM","Excludes":"Isolated deployment, whole-task acceptance"}\n' "$checks" "${#cases[@]}" "${#negative[@]}" "${#privacy[@]}" "${#undeclared[@]}" "${#cases[@]}" "$GROUP" >"$OUT/results.json"
printf 'Regex/Pattern B13-B17b Linux: %d checks passed\n' "$checks" | tee "$OUT/summary.txt"
