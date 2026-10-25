#!/usr/bin/env python3

"""生成并校验 XRT/xruntime 与 xlang 的能力对应矩阵。"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sys


IMPLEMENTATION_MARKERS = (
    "#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRUNTIME_IMPLEMENTATION_ONCE)",
    "#if defined(XRT_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION_ONCE)",
    "#if defined(XACME_IMPLEMENTATION) && !defined(XACME_IMPLEMENTATION_ONCE)",
)
# Resident adapters may consume a separately packaged native product. Their
# exact public declarations and providers are not part of the XRT ABI and must
# not be counted as missing core symbols or admitted by a name-prefix waiver.
EXTERNAL_API_PRODUCTS = {
    "xacme": {
        "declarations": "release/extlibs/xacme/upstream/xacme.h",
        "provider": "release/extlibs/xacme/native/xacme_impl.c",
        "consumers": ("src/runtime/xl6_xacme.c",),
    },
}
PUBLIC_SECTION = re.compile(r"/\* public: ([^*\r\n]+?) \*/")
FUNCTION = re.compile(
    r"\bXRT_API\s+[^;{}]*?\b(xrt[A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;",
    re.DOTALL,
)
# Header-defined helpers are callable C, but have no external TCC symbol.
# Keep that distinction explicit instead of declaring a missing-name exception.
INLINE_FUNCTION = re.compile(
    r"\bstatic\s+inline\s+[^;{}]*?\b(xrt[A-Za-z0-9_]*)\s*\([^;{}]*\)\s*\{",
    re.DOTALL,
)
# A function-pointer declarator starts with its RETURN TYPE, e.g.
# `struct xrtownershipref (*Ownership)(...)`; that type is not an API call.
# Preserve ordinary calls (including dereferenced arguments) and unknown APIs.
CALL = re.compile(r"\b(xrt[A-Za-z0-9_]*)\s*\((?!\s*\*\s*[A-Za-z_]\w*\s*\)\s*\()")
# Resident role/operation tables bind API addresses without spelling a call.
# Only known function symbols in aggregate initializers count here; strings,
# comments, type declarators and unknown data identifiers are not witnesses.
C_NONCODE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)
API_INITIALIZER = re.compile(r'(?:^|[,={])\s*(?:\.[A-Za-z_]\w*\s*=\s*)?&?\s*(xrt[A-Za-z0-9_]*)\s*(?=[,}])', re.MULTILINE)
SYMBOL = re.compile(r"(?m)^\s*XL_XRT_SYMBOL\((xrt[A-Za-z0-9_]+)\)\s*$")
STDLIB_BINDING = re.compile(
    r'\{"([^"]*)",\s*"([^"]+)",\s*"[^"]+",\s*"(xrt[A-Za-z0-9_]+)"'
)

XRUNTIME_CAPABILITY_CONTRACT = {
    "extlibs/xruntime/include/xrt/runtime_call.h": {
        "capability": "function values, call frames, and dynamic invocation",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_function_named_value.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_convert.h": {
        "capability": "checked runtime conversion for is/as and var",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_runtime_is_as.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_field.h": {
        "capability": "DynamicFields and class dynamic-field storage",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/xruntime_dynamic_fields.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_object.h": {
        "capability": "class instance identity, payload, and weak ownership",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": (
            "test/cases/weak_class_lifecycle.xl",
            "test/cases/weak_async_lifecycle.xl",
            "test/cases/async_class_receiver_lifetime.xl",
        ),
    },
    "extlibs/xruntime/include/xrt/runtime_object_graph.h": {
        "capability": "custom runtime object graph traversal",
        "disposition": "explicit-low-level",
        "backendStatus": "public-extern",
        "witnesses": ("docs/xruntime语言桥接契约.md",),
    },
    "extlibs/xruntime/include/xrt/runtime_type.h": {
        "capability": "type identity, descriptors, registry, and protocols",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_runtime_type_baseline.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_type_future.h": {
        "capability": "future<T> runtime type identity",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_future_task_wait.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_type_string.h": {
        "capability": "stable language type names",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_runtime_type_baseline.xl",),
    },
    "extlibs/xruntime/include/xrt/runtime_value.h": {
        "capability": "var lifecycle, boxing, comparison, and cloning",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/runtime_var_type_storage_matrix.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_array.h": {
        "capability": "array<T>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_typed_container_functions.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_dict.h": {
        "capability": "dict<T>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_typed_container_functions.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_list.h": {
        "capability": "list<T>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_typed_container_functions.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_queue.h": {
        "capability": "typed SPSC/MPSC/MPMC queues",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/stdlib_special_collections.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_set.h": {
        "capability": "set<T>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_typed_container_functions.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_stack.h": {
        "capability": "Stack<T>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/stdlib_special_collections.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_tree.h": {
        "capability": "OrderedMap<K,V>",
        "disposition": "language-type",
        "backendStatus": "direct",
        "witnesses": ("test/cases/stdlib_special_collections.xl",),
    },
    "extlibs/xruntime/include/xrt/typed_value.h": {
        "capability": "typed values used by generic containers",
        "disposition": "kernel",
        "backendStatus": "direct",
        "witnesses": ("test/cases/cgen_typed_container_functions.xl",),
    },
}


def _declaration_region(path: Path) -> str:
    """读取单头文件的公共声明区域。"""

    text = path.read_text(encoding="utf-8")
    positions = [text.find(marker) for marker in IMPLEMENTATION_MARKERS]
    positions = [position for position in positions if position >= 0]
    if not positions:
        raise ValueError(f"implementation marker is missing: {path}")
    return text[:min(positions)]


def _api_headers(path: Path, pattern: re.Pattern[str] = FUNCTION, *,
    allow_identical_redeclarations: bool = False,
) -> dict[str, str]:
    """将每个公共函数映射到其原始模块头。"""

    text = _declaration_region(path)
    sections = [(match.start(), match.group(1).strip()) for match in PUBLIC_SECTION.finditer(text)]
    positions = [position for position, _ in sections]
    result: dict[str, str] = {}
    prototypes: dict[str, str] = {}
    for match in pattern.finditer(text):
        index = bisect_right(positions, match.start()) - 1
        if index < 0:
            raise ValueError(f"public API has no source section: {match.group(1)}")
        name = match.group(1)
        header = sections[index][1]
        prototype = re.sub(r"\s+", "", C_NONCODE.sub(" ", match.group(0)))
        if name in result:
            # C permits an identical repeated declaration in one origin header.
            # Core exports remain strict; conflicting types/origins still fail.
            if (allow_identical_redeclarations and result[name] == header
                    and prototypes[name] == prototype):
                continue
            raise ValueError(f"duplicate public API declaration: {name}")
        result[name] = header
        prototypes[name] = prototype
    return result


def _runtime_symbols(root: Path) -> set[str]:
    """读取嵌入式 TCC 实际注册的运行时符号。"""

    path = root / "src" / "api" / "xl_xrt_symbols.inc"
    text = path.read_text(encoding="utf-8")
    symbols = SYMBOL.findall(text)
    if len(symbols) != len(set(symbols)):
        raise ValueError("runtime symbol table contains duplicate entries")
    return set(symbols)


def _source_files(root: Path) -> list[Path]:
    """返回体现语言内核和标准库包装关系的源码。"""

    paths = [
        path
        for path in (root / "src").rglob("*")
        if path.is_file() and path.suffix in {".c", ".h"}
    ]
    paths.append(root / "release" / "include" / "xl_runtime.h")
    return sorted(set(paths))


def _native_address_references(text: str, runtime_apis: set[str]) -> set[str]:
    code = C_NONCODE.sub(" ", text)
    return set(API_INITIALIZER.findall(code)) & runtime_apis


def _source_references(
    root: Path,
    runtime_apis: set[str],
    external_apis: set[str] | None = None,
) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """收集调用及驻留操作表地址绑定；绑定不等于公开 API 已经验收。"""

    references: dict[str, set[str]] = defaultdict(set)
    missing: dict[str, set[str]] = defaultdict(set)
    known = runtime_apis | (external_apis or set())
    for path in _source_files(root):
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="strict")
        for name in set(CALL.findall(text)) | _native_address_references(text, known):
            target = references if name in runtime_apis else missing
            target[name].add(relative)
    return (
        {name: sorted(paths) for name, paths in references.items()},
        {name: sorted(paths) for name, paths in missing.items()},
    )


def _external_api_catalog(root: Path, runtime_apis: set[str]) -> dict[str, dict[str, object]]:
    """Resolve exact declarations of active external-product adapters."""

    catalog: dict[str, dict[str, object]] = {}
    for product, contract in EXTERNAL_API_PRODUCTS.items():
        consumers = tuple(contract["consumers"])
        if not any((root / path).is_file() for path in consumers):
            continue
        for path in (contract["declarations"], contract["provider"], *consumers):
            if not (root / path).is_file():
                raise ValueError(f"external API product {product} is missing: {path}")
        declarations = _api_headers(root / contract["declarations"],
            allow_identical_redeclarations=True)
        if not declarations:
            raise ValueError(f"external API product {product} has no declarations")
        for name, header in declarations.items():
            if name in runtime_apis or name in catalog:
                raise ValueError(f"external API ownership collision: {name}")
            catalog[name] = dict(product=product, header=header,
                declarations=contract["declarations"], provider=contract["provider"],
                consumers=consumers)
    return catalog


def _partition_external_references(
    references: dict[str, list[str]], catalog: dict[str, dict[str, object]],
) -> tuple[dict[str, list[str]], list[dict[str, object]]]:
    """Keep unknown calls fail-closed and validate each external consumer."""

    unresolved: dict[str, list[str]] = {}
    external: list[dict[str, object]] = []
    for name, paths in sorted(references.items()):
        if name not in catalog:
            unresolved[name] = paths
            continue
        contract = catalog[name]
        unexpected = sorted(set(paths) - set(contract["consumers"]))
        if unexpected:
            raise ValueError(f"external API consumer mismatch: {name}: {unexpected}")
        external.append(dict(name=name, product=contract["product"],
            header=contract["header"], declarations=contract["declarations"],
            provider=contract["provider"], references=paths))
    return unresolved, external


def _stdlib_bindings(root: Path) -> dict[str, list[str]]:
    """收集直接绑定到 XRT C 符号的公开 xlang 标准库入口。"""

    path = root / "src" / "stdlib" / "xl6_stdlib_data.c"
    text = path.read_text(encoding="utf-8")
    result: dict[str, set[str]] = defaultdict(set)
    for match in STDLIB_BINDING.finditer(text):
        namespace, name, symbol = match.groups()
        public_name = f"{namespace}.{name}" if namespace else name
        if not namespace.startswith("__"):
            result[symbol].add(public_name)
    return {name: sorted(bindings) for name, bindings in result.items()}


def _xruntime_capabilities(
    root: Path,
    owned_headers: set[str],
) -> dict[str, dict[str, object]]:
    """Validate the semantic capability contract for every xruntime-owned module."""

    contracts = XRUNTIME_CAPABILITY_CONTRACT
    missing = sorted(owned_headers - set(contracts))
    extra = sorted(set(contracts) - owned_headers)
    if missing or extra:
        raise ValueError(
            f"xruntime capability contract mismatch: missing={missing}, extra={extra}"
        )
    for header, contract in contracts.items():
        disposition = contract["disposition"]
        if disposition not in {"kernel", "language-type", "explicit-low-level"}:
            raise ValueError(f"invalid capability disposition for {header}: {disposition}")
        witnesses = contract["witnesses"]
        if not witnesses:
            raise ValueError(f"xruntime capability has no witness: {header}")
        for witness in witnesses:
            if not (root / witness).is_file():
                raise ValueError(f"xruntime capability witness is missing: {witness}")
    return contracts


def _deferred_product(name: str) -> str:
    """将迁出核心后的旧引用归入明确的扩展产品。"""

    if name.startswith("xrtRegex"):
        return "xregex"
    if name.startswith(("xrtWs", "xrtWebSocket")):
        return "xws"
    if name.startswith((
        "xrtHttp",
        "xrtUrl",
        "xrtCookie",
        "xrtSetCookie",
        "xrtForm",
        "xrtMime",
        "xrtMultipart",
        "xrtQuery",
    )):
        return "xhttp"
    raise ValueError(f"unclassified API reference outside xruntime: {name}")


def _deferred_content(missing: dict[str, list[str]]) -> str:
    """生成迁出扩展 API 的精确允许清单。"""

    groups: dict[str, list[str]] = defaultdict(list)
    for name in missing:
        groups[_deferred_product(name)].append(name)
    lines = [
        "# Generated by tools/gen_xrt_xlang_matrix.py. Do not edit manually.",
        "# These references move with their extension product and must reach zero after adaptation.",
    ]
    for product in ("xhttp", "xws", "xregex"):
        lines.extend(("", f"[{product}]"))
        lines.extend(sorted(groups[product]))
    return "\n".join(lines) + "\n"


def _matrix(
    root: Path,
) -> tuple[dict[str, object], str, str]:
    """构建完整逐 API 数据、文档矩阵和延期清单。"""

    core_headers = _api_headers(root / "lib" / "xrt" / "xrt.h")
    runtime_headers = _api_headers(root / "lib" / "xruntime" / "xruntime.h")
    core_apis = set(core_headers)
    runtime_apis = set(runtime_headers)
    symbols = _runtime_symbols(root)
    if symbols != runtime_apis:
        missing = sorted(runtime_apis - symbols)
        extra = sorted(symbols - runtime_apis)
        raise ValueError(f"runtime symbol mismatch: missing={missing[:5]}, extra={extra[:5]}")
    if not core_apis < runtime_apis:
        raise ValueError("xruntime must be a strict superset of the XRT core product")
    runtime_owned_apis = runtime_apis - core_apis
    runtime_owned_headers = {runtime_headers[name] for name in runtime_owned_apis}
    capabilities = _xruntime_capabilities(root, runtime_owned_headers)

    inline_headers = _api_headers(root / "lib" / "xruntime" / "xruntime.h", INLINE_FUNCTION)
    external_catalog = _external_api_catalog(root, runtime_apis)
    references, missing_references = _source_references(
        root, runtime_apis | set(inline_headers), set(external_catalog))
    missing_references, external_references = _partition_external_references(
        missing_references, external_catalog)
    bindings = _stdlib_bindings(root)
    unknown_bindings = sorted(set(bindings) - runtime_apis)
    if unknown_bindings:
        raise ValueError(f"stdlib binds APIs outside xruntime: {unknown_bindings[:5]}")
    for name in missing_references:
        _deferred_product(name)

    rows = []
    module_rows: dict[tuple[str, str], dict[str, int]] = defaultdict(
        lambda: {"api": 0, "direct": 0, "abstracted": 0, "lowLevelOnly": 0}
    )
    abstracted_apis: set[str] = set()
    direct_apis: set[str] = set()
    for name in sorted(runtime_apis):
        product = "xrt" if name in core_apis else "xruntime"
        header = runtime_headers[name]
        direct_status = "none"
        if name in bindings:
            direct_status = "stdlib"
        elif name in references:
            direct_status = "kernel"
        if direct_status != "none":
            direct_apis.add(name)

        if product == "xruntime":
            capability = capabilities[header]
            disposition = str(capability["disposition"])
            capability_name = str(capability["capability"])
            backend_status = str(capability["backendStatus"])
            witnesses = list(capability["witnesses"])
        else:
            disposition = "kernel-or-stdlib" if direct_status != "none" else "explicit-low-level"
            capability_name = ""
            backend_status = "direct" if direct_status != "none" else "public-extern"
            witnesses = []
        if disposition != "explicit-low-level":
            abstracted_apis.add(name)
        row = {
            "name": name,
            "product": product,
            "header": header,
            "directIntegration": direct_status,
            "capabilityDisposition": disposition,
            "capability": capability_name,
            "backendStatus": backend_status,
            "witnesses": witnesses,
            "kernelReferences": references.get(name, []),
            "stdlibBindings": bindings.get(name, []),
            "lowLevelSymbol": True,
        }
        rows.append(row)
        summary = module_rows[(product, header)]
        summary["api"] += 1
        if direct_status != "none":
            summary["direct"] += 1
        if disposition != "explicit-low-level":
            summary["abstracted"] += 1
        else:
            summary["lowLevelOnly"] += 1

    deferred_counts = Counter(_deferred_product(name) for name in missing_references)
    report: dict[str, object] = {
        "schema": 2,
        "coreApiCount": len(core_apis),
        "runtimeApiCount": len(runtime_apis),
        "runtimeOwnedApiCount": len(runtime_apis - core_apis),
        "directIntegratedApiCount": len(direct_apis),
        "abstractedApiCount": len(abstracted_apis),
        "lowLevelOnlyApiCount": len(runtime_apis - abstracted_apis),
        "xruntimeCapabilityCount": len(capabilities),
        "xruntimeUnclassifiedModuleCount": 0,
        "xruntimeCapabilities": [
            {
                "header": header,
                "capability": contract["capability"],
                "disposition": contract["disposition"],
                "backendStatus": contract["backendStatus"],
                "witnesses": list(contract["witnesses"]),
                "apiCount": module_rows[("xruntime", header)]["api"],
                "directIntegratedApiCount": module_rows[("xruntime", header)]["direct"],
            }
            for header, contract in sorted(capabilities.items())
        ],
        "deferredApiCount": len(missing_references),
        "externalProductReferences": external_references,
        "headerInlineReferences": [
            {"name": name, "header": inline_headers[name], "references": references[name],
             "lowLevelSymbol": False}
            for name in sorted(set(inline_headers) & set(references))
        ],
        "deferredProducts": dict(sorted(deferred_counts.items())),
        "apis": rows,
        "deferredApis": [
            {
                "name": name,
                "product": _deferred_product(name),
                "references": missing_references[name],
            }
            for name in sorted(missing_references)
        ],
    }

    lines = [
        "# XRT 与 xlang 能力矩阵",
        "",
        "此文件由 `tools/gen_xrt_xlang_matrix.py` 从仓内快照、TCC 符号表和当前语言实现生成，请勿手工修改。",
        "",
        "## 契约结论",
        "",
        f"- XRT 核心公共函数：{len(core_apis)}。",
        f"- xruntime 产品公共函数：{len(runtime_apis)}，其中扩展自有 {len(runtime_apis - core_apis)}。",
        f"- 低层能力：{len(symbols)} 个函数全部进入 TCC 符号面，可通过显式 `extern \"xrt...\"` 使用。",
        "- 头文件内联辅助函数单独记录于 `headerInlineReferences`，不增加外部符号数量；未声明的新引用仍报错。",
        f"- 显式扩展产品的 {len(external_references)} 项 API 引用单列于 `externalProductReferences`，核对真实公共声明、提供方和指定使用方，不增加核心 API／符号或延期数量；引用不代表功能验收。",
        f"- 语言内核或标准库已直接使用：{len(abstracted_apis)} 个函数。其余 {len(runtime_apis - abstracted_apis)} 个保留为低层能力，不视为功能丢失。",
        f"- 已迁出核心但尚待扩展库适配的旧引用：{len(missing_references)}，仅包含 xhttp={deferred_counts['xhttp']}、xws={deferred_counts['xws']}、xregex={deferred_counts['xregex']}。",
        "",
        "逐 API 详情可执行 `python tools/gen_xrt_xlang_matrix.py --report build/reports/xrt_xlang_api_matrix.json` 生成。",
        "",
        "## 模块矩阵",
        "",
        "| 产品 | 公共头 | API | 语言抽象/内核使用 | 仅低层 |",
        "| --- | --- | ---: | ---: | ---: |",
    ]
    for (product, header), summary in sorted(module_rows.items()):
        lines.append(
            f"| {product} | `{header}` | {summary['api']} | "
            f"{summary['abstracted']} | {summary['lowLevelOnly']} |"
        )
    lines.extend((
        "",
        "## xruntime 语义能力契约",
        "",
        "直接出现 C 函数名只表示实现证据，不再等同于语言抽象。每个 xruntime 模块必须归入语言类型、内核语义或显式底层能力，并绑定可执行测试或边界文档。",
        "",
        "| 公共头 | 语言能力 | 归属 | 后端状态 | API | 直接调用 | 见证 |",
        "| --- | --- | --- | --- | ---: | ---: | --- |",
    ))
    for header, contract in sorted(capabilities.items()):
        summary = module_rows[("xruntime", header)]
        witnesses = ", ".join(f"`{item}`" for item in contract["witnesses"])
        lines.append(
            f"| `{header}` | {contract['capability']} | {contract['disposition']} | "
            f"{contract['backendStatus']} | {summary['api']} | {summary['direct']} | {witnesses} |"
        )
    lines.extend((
        "",
        "## 扩展延期边界",
        "",
        "延期清单保存在 `tools/xrt_xlang_deferred_apis.txt`。该清单是精确快照，不按前缀宽泛放行；xhttp、xws、xregex 分组当前均为空，出现新的旧 API 引用会直接失败。xregex 已正式接回，xhttp/xws 只延期非快速路径的高级语言包装，不允许旧符号回流核心快照。",
        "",
    ))
    return report, "\n".join(lines), _deferred_content(missing_references)


def _write(path: Path, content: str) -> None:
    """以稳定 UTF-8 和 LF 写入生成文件。"""

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def sync(root: Path) -> dict[str, object]:
    """同步模块矩阵和扩展延期精确清单。"""

    report, document, deferred = _matrix(root.resolve())
    _write(root / "docs" / "XRT与xlang能力矩阵.md", document)
    _write(root / "tools" / "xrt_xlang_deferred_apis.txt", deferred)
    return report


def check(root: Path) -> tuple[bool, dict[str, object]]:
    """只读验证模块矩阵和扩展延期清单没有漂移。"""

    root = root.resolve()
    report, document, deferred = _matrix(root)
    document_path = root / "docs" / "XRT与xlang能力矩阵.md"
    deferred_path = root / "tools" / "xrt_xlang_deferred_apis.txt"
    actual_document = document_path.read_text(encoding="utf-8") if document_path.is_file() else ""
    actual_deferred = deferred_path.read_text(encoding="utf-8") if deferred_path.is_file() else ""
    return actual_document == document and actual_deferred == deferred, report


def main() -> int:
    """执行矩阵同步、只读检查或逐 API 报告导出。"""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sync", action="store_true")
    mode.add_argument("--report", type=Path)
    arguments = parser.parse_args()

    root = arguments.root.resolve()
    if arguments.check:
        current, report = check(root)
        if not current:
            print("XRT/xlang capability matrix is stale")
            return 1
    elif arguments.sync:
        report = sync(root)
    else:
        report, _, _ = _matrix(root)
        output = arguments.report
        if not output.is_absolute():
            output = root / output
        _write(output, json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print(f"XRT/xlang per-API report generated: {output}")
        return 0

    print(
        "XRT/xlang capability matrix is current: "
        f"{report['runtimeApiCount']} low-level, "
        f"{report['directIntegratedApiCount']} directly integrated, "
        f"{report['abstractedApiCount']} semantically abstracted, "
        f"{report['deferredApiCount']} extension-deferred"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
