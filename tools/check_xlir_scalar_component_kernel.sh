#!/usr/bin/env bash
# Linux x86_64 B10: scalar constants and the actual artifact component graph.
# Build build_linux.sh first. This gate does not claim full 53-module completion.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:?Provide a fresh evidence directory}"
[[ "$OUT" = /* ]] || OUT="$ROOT/$OUT"
[[ ! -e "$OUT" ]] || { printf 'Preserve prior kernel evidence\n' >&2; exit 1; }
[[ "$(uname -m)" = x86_64 ]] && getconf GNU_LIBC_VERSION >/dev/null
[[ -x release/xl && -f release/libxl.so && -f release/libxlc.so ]]
mkdir -p "$OUT/include"
CLI="$ROOT/release/xl"
CC="$(command -v gcc)"
checks=0
run() {
    local name="$1"; shift
    if ! "$@" >"$OUT/$name.log" 2>&1; then
        tail -n 35 "$OUT/$name.log" >&2
        printf 'Scalar/component kernel check failed: %s\n' "$name" >&2; exit 1
    fi
    checks=$((checks + 1))
}
fixture=cgen_signed_integer_constants
negative=(sem_int64_positive_overflow sem_int_out_of_range_literal sem_auto_int_overflow sem_var_int_overflow)
regressions=(cgen_numeric_literals operators_int_literal_forms type_int_narrowing_wrap
    xlir_integer_conversion_source_abi xlir_checked_integer_arithmetic_abi
    xlir_checked_integer_compound_error_abi xlir_checked_integer_continuation_abi)
{
    find src lib release/include -type f -print0
    printf '%s\0' "$CC" release/xl release/libxl.so release/libxlc.so build_linux.sh
    printf '%s\0' tools/check_xlir_scalar_component_kernel.sh tools/tcc_vfs_manifest_linux_xlc.txt
    printf '%s\0' test/cases/xlir_component_set.c test/cases/xlir_signed_integer_constants_contract.c
    printf '%s\0' test/cases/xlir_artifact_runtime_contract.c test/cases/xlir_generated_module_host.c
    printf '%s\0' test/cases/xlir_native_future_map_contract.c test/cases/xlir_network_service_contract.c
    printf '%s\0' test/cases/xlir_continuation_owner_test.h test/cases/xlir_synthetic_owner_edit.h
    printf '%s\0' test/cases/stdlib_string_support.c
    for name in "$fixture" "${negative[@]}" "${regressions[@]}"; do printf '%s\0' "test/cases/$name.xl"; done
    for name in stdlib_net_fast_path stdlib_net_send_file stdlib_tls_server_loopback \
        xlir_net_resolver_standalone xlir_net_datagram_boundaries xlir_net_buffer_command_lifetime; do
        printf '%s\0' "test/cases/$name.xl"
    done
} | sort -zu | xargs -0 sha256sum >"$OUT/inputs.sha256"
python3 tools/stage_runtime_headers.py --root "$ROOT" --out "$OUT"
strict=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -DXL_STATIC_EMBED
    -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter
    -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers
    -Wno-pointer-sign -Wno-format-truncation -I. -Irelease -Irelease/include -I"$OUT/include")
libs=(-Lrelease -Wl,-rpath,"$ROOT/release" -pthread -ldl -lm)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
run component-set-build "$CC" "${strict[@]}" "${sanitize[@]}" test/cases/xlir_component_set.c -o "$OUT/component-set"
run component-set "$OUT/component-set"
run component-set.tcc "$CLI" run test/cases/xlir_component_set.c --inc-path "$ROOT"
run signed-contract-build "$CC" "${strict[@]}" test/cases/xlir_signed_integer_constants_contract.c -lxlc "${libs[@]}" -o "$OUT/signed-contract"
run artifact-build "$CC" "${strict[@]}" test/cases/xlir_artifact_runtime_contract.c -lxlc "${libs[@]}" -o "$OUT/artifact"
run artifact-contract "$OUT/artifact" "test/cases/$fixture.xl"
run catalog "$OUT/artifact" --catalog
run component-catalog python3 -c 'import json,sys; p=[json.loads(s) for s in open(sys.argv[1])]; assert len(p)>=60; assert [x["Id"] for x in p]==list(range(len(p))); assert all(x["Id"] in x["Closure"] for x in p); assert all(isinstance(x["Dependencies"],list) and isinstance(x["Closure"],list) for x in p); assert all(0<=n<len(p) for x in p for n in x["Dependencies"]+x["Closure"])' "$OUT/catalog.log"
for contract in xlir_native_future_map_contract xlir_network_service_contract; do
    run "$contract.build" "$CC" "${strict[@]}" "test/cases/$contract.c" -lxlc "${libs[@]}" -o "$OUT/$contract"
    run "$contract" "$OUT/$contract"
done
run "$fixture.emit" "$CLI" cgen "test/cases/$fixture.xl" --inc-path "$OUT/include" --emit "$OUT/$fixture.c"
run "$fixture.repeat" "$CLI" cgen "test/cases/$fixture.xl" --inc-path "$OUT/include" --emit "$OUT/$fixture.repeat.c"
run "$fixture.deterministic" cmp "$OUT/$fixture.c" "$OUT/$fixture.repeat.c"
run "$fixture.contract" "$OUT/signed-contract" "test/cases/$fixture.xl" "$OUT/$fixture.contract.c"
run "$fixture.contract-equality" cmp "$OUT/$fixture.c" "$OUT/$fixture.contract.c"
run "$fixture.strict-build" "$CC" "${strict[@]}" "${sanitize[@]}" -include "$OUT/$fixture.c" \
    -DXLIR_GENERATED_MODULE_PREINCLUDED test/cases/xlir_generated_module_host.c -lxl "${libs[@]}" -o "$OUT/$fixture.strict"
run "$fixture.strict" "$OUT/$fixture.strict"
run "$fixture.jit" "$CLI" run "test/cases/$fixture.xl" --inc-path "$OUT/include"
run "$fixture.aot-build" "$CLI" build "test/cases/$fixture.xl" --inc-path "$OUT/include" --save-temps -o "$OUT/$fixture.aot"
run "$fixture.aot" "$OUT/$fixture.aot"
run "$fixture.native-plan" "$OUT/artifact" --emit-native "test/cases/$fixture.xl" "$OUT/$fixture.native.c"
saved="$(sed -n 's/^temporary files kept: //p' "$OUT/$fixture.aot-build.log")"
[[ -n "$saved" && -f "$saved/xl6_xrt_impl.c" ]]
run "$fixture.actual-native-equality" cmp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.native.c"
cp "$saved/xl6_xrt_impl.c" "$OUT/$fixture.actual-native.c"
for name in "${negative[@]}"; do
    if "$CLI" cgen "test/cases/$name.xl" --emit "$OUT/$name.c" >"$OUT/$name.log" 2>&1; then
        printf 'Invalid integer literal accepted: %s\n' "$name" >&2; exit 1
    fi
    [[ ! -e "$OUT/$name.c" ]]
    grep -Fq "$(printf '\u6574\u6570\u5b57\u9762\u91cf\u8d85\u51fa')" "$OUT/$name.log"
    checks=$((checks + 1))
done
for name in "${regressions[@]}"; do
    run "$name.regression" "$CLI" run "test/cases/$name.xl" test/cases/stdlib_string_support.c --inc-path "$OUT/include"
done
run inputs-unchanged sha256sum --check "$OUT/inputs.sha256"
population="$(wc -l <"$OUT/catalog.log")"
printf '{"Complete":true,"Checks":%d,"SignedFixtures":1,"IntegerNegativeFixtures":4,"IntegerRegressions":7,"ComponentPopulations":10,"MaxSyntheticPopulation":257,"ActualComponentCount":%d,"ActualDependencyPairs":%d,"InputsUnchanged":true,"SanitizedScope":"component set and signed generated C host only; linked shared libraries are not instrumented"}\n' "$checks" "$population" "$((population*population))" >"$OUT/results.json"
printf 'Scalar/component kernel Linux: %s checks passed.\n' "$checks" | tee "$OUT/summary.txt"
