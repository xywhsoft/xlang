#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
: "${NETWORK_ABI_DIR:?Set NETWORK_ABI_DIR to the Windows generated ABI directory}"
work=${NETWORK_ABI_WORK:-"$root/build/test/extlib_protocol_abi_linux"}
mkdir -p "$work"
cc=${CC:-gcc}
# 使用已通过门禁且未被改写的生成头，不把手写或失败批次当 ABI 依据。
python3 - "$NETWORK_ABI_DIR" <<'PY'
import hashlib
import json
from pathlib import Path, PureWindowsPath
import sys

directory = Path(sys.argv[1])
record = json.loads((directory / "results.json").read_text())
assert record["outcome"] == "passed-scoped"
assert record["coverage"]
for package in record["coverage"]:
    for field, digest in [("header", "headerSha256"), ("negativeHeader", "negativeSha256")]:
        path = directory / PureWindowsPath(package[field]).name
        assert hashlib.sha256(path.read_bytes()).hexdigest().upper() == package[digest], path
PY
mapfile -t packages < <(python3 -c 'import json,sys; print("\n".join(p["package"] for p in json.load(open(sys.argv[1]))["coverage"]))' "$NETWORK_ABI_DIR/results.json")
mapfile -t input_packages < <(python3 -c 'import json,sys; r=json.load(open(sys.argv[1])); print("\n".join("release/extlibs/"+p for p in r.get("dependencyPackages",[p["package"] for p in r["coverage"]])))' "$NETWORK_ABI_DIR/results.json")
snapshot() {
    find "${input_packages[@]}" -type f -print0 |
        sort -z | xargs -0 sha256sum
    sha256sum lib/xrt/xrt.h tools/check_extlib_protocol_abi.sh "$NETWORK_ABI_DIR"/*-abi.h "$NETWORK_ABI_DIR"/*-negative.h \
        test/support/extlib_http_object_runtime.c test/cases/extlib_protocol_native_contract.c
}
snapshot > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
sanitizers=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
http_objects=()
for package in "${packages[@]}"; do
    pkg="release/extlibs/$package"
    flags=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime -Irelease/include)
    mapfile -t extra < <(python3 -c 'import json,sys; p=sys.argv[1]; m=json.load(open(p+"/xlang.package.json"))["artifact"]; print("\n".join(["-I"+p+"/"+s for s in m.get("includeDirs",[])]+["-D"+s for s in m.get("xrtModules",[])]))' "$pkg")
    flags+=("${extra[@]}")
    mapfile -t sources < <(python3 -c 'import json,sys; print("\n".join(json.load(open(sys.argv[1]+"/xlang.package.json"))["artifact"]["cSources"]))' "$pkg")
    for source in "${sources[@]}"; do
        "$cc" "${flags[@]}" -include "$NETWORK_ABI_DIR/$package-abi.h" -fsyntax-only "$pkg/$source"
        if [[ "$package" == xhttp.protocol || "$package" == xhttp.body ]]; then
            object="$work/$package-$(basename "${source%.c}").o"
            "$cc" "${flags[@]}" "${sanitizers[@]}" -include "$NETWORK_ABI_DIR/$package-abi.h" -c "$pkg/$source" -o "$object"
            http_objects+=("$object")
        fi
    done
    mapfile -t control < <(python3 -c 'import json,sys; p=next(p for p in json.load(open(sys.argv[1]))["coverage"] if p["package"]==sys.argv[2]); print(p["native"]); print(p["destructor"])' "$NETWORK_ABI_DIR/results.json" "$package")
    native=${control[0]}; symbol=${control[1]}
    if "$cc" "${flags[@]}" -include "$NETWORK_ABI_DIR/$package-negative.h" -fsyntax-only "$pkg/$native" > "$work/$package-negative.log" 2>&1; then
        printf 'ABI negative control unexpectedly passed: %s\n' "$package" >&2
        exit 1
    fi
    grep -q 'conflicting types' "$work/$package-negative.log"
    grep -q "$symbol" "$work/$package-negative.log"
    printf '%s LP64 generated ABI and negative control: PASS\n' "$package"
done
if [[ -f "$NETWORK_ABI_DIR/xhttp.protocol-abi.h" && -f "$NETWORK_ABI_DIR/xhttp.body-abi.h" ]]; then
base=(-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt)
"$cc" "${base[@]}" "${sanitizers[@]}" -c test/support/extlib_http_object_runtime.c -o "$work/runtime.o"
"$cc" "${base[@]}" "${sanitizers[@]}" -DXRT_MODULE_IO_BUFFER -DXRT_MODULE_MEMORY_DEBUG \
    -include "$NETWORK_ABI_DIR/xhttp.protocol-abi.h" -include "$NETWORK_ABI_DIR/xhttp.body-abi.h" \
    test/cases/extlib_protocol_native_contract.c "$work/runtime.o" "${http_objects[@]}" -lm -ldl -pthread -o "$work/http-native"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 timeout 90s "$work/http-native" | tee "$work/http-native.log"
fi
snapshot > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
