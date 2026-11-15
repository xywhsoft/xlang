# Public constant parity

XGE and XUI constants are compile-time declarations. They introduce no C
forwarder, dispatch table or native function call. Import initialization and
ordinary string concatenation retain their existing costs; the shader-source
example does not claim that string construction or a complete import is free.

The binding sources now include the exact multi-literal native
`XGE_GLSL_MATERIAL_UV` preamble, the toast close reason `XUI_TOAST_CLOSE_API`,
the public header revisions `XUI_CODE_TOKEN_STYLE_RESOLVER` and
`XUI_CODE_DOCUMENT_BATCH`, and the previously omitted result-enum members
`XGE_STATUS_DEFERRED`, `XUI_STATUS_DEFERRED` and `XGE_ERROR_BUSY`.
`XUI_PROXY_VERSION` is corrected from the stale value 17 to the matched SDK's 18.
The revision constants describe the pinned header protocol. They do not
install callbacks, implement batch editing or grant an object thread safety.

The generator no longer treats every name ending in `_API` as a DLL decoration.
It explicitly recognizes `XGE_API` and `XUI_API`. Its ordinary UTF-8 C string
reader joins adjacent literals and implements byte escapes according to C,
including the different hexadecimal-escape consumption rule. Wide literals,
unsupported escapes, out-of-byte-range escapes, invalid UTF-8 and nonliteral
tails are rejected instead of exporting a silently changed string.

`tools/check_xge3_constants.py` reads all manifest-selected binding sources and
all four public XGE/XUI/Document headers, including typedef-enum members. It
generates a QA-only C oracle whose expectations use the actual macro/enum
names under the matched SDK configuration. It compares integer values without
losing unsigned width, the floating value, and every text byte and byte length.
The C expectations do not reuse Python's parsed numeric/string literals.
Separate O0/O2 oracle self-checks intentionally corrupt values, counts, text
bytes and lengths and require rejection. The generated fixture is external
development material, not an extension-library wrapper or shipped payload.

The same language fixture is intended for Windows/Linux JIT, dynamic AOT and
static AOT, with C11/C++11/C++17 public-header checks and binary import-table
inspection. Only the selected full-feature SDK configuration is exercised;
this does not certify every possible feature-disabled or debug build.

The original monolithic QA function exposed a separate Windows AOT limitation:
its large generated local frame overflowed the executable's default 1 MiB
reserved stack. The same original binary passed all 2,319 comparisons in a
diagnostic copy with only the PE stack-reserve field changed to 16 MiB.
The original binary/logs are retained. This diagnostic does not fix or change
the compiler's large-frame/stack policy. The bounded fixture uses functions
with at most 128 numeric values each, preserving every comparison under the
ordinary default settings. It does not present the larger-stack copy as a
passing default-configuration executable.

Eleven distinct native names remain outside the ordinary constant API:

| Names | Reason |
| --- | --- |
| `XGE_BUILD_ABI_FINGERPRINT` | SDK-generated fingerprint; exporting the header fallback `0` would misidentify the built binary. |
| `XGE_RELEASE`, `XGE_DEBUG`, `XGE_DEBUGMODE`, `XGE_HAS_DEBUGMODE` | Build selection and conditional feature configuration. |
| `XGE_API`, `XUI_API` | Declaration decorations, not values. |
| `XUI_TEXT_PAINT_OWNERSHIP_V1_TAG` | Native ownership-protocol tag. |
| `XUI_PROXY_OWNED_ANYTHREAD_FINALIZE` | Native retirement capability; not a language thread-safety promise. |
| `XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR` | Native allocator ABI marker. |
| `XGE3D_TRANSFORM_IDENTITY` | C aggregate initializer; a safe language value-construction equivalent remains open. |

This constant audit does not certify function coverage, callbacks, resource
ownership, GPU behavior or the final installed compiler/IDE ecosystem.

Qualified on 2026-11-15 against the pinned full O2 compiler cohorts and matched
hosted/static Provider-owned SDKs on Windows and Linux. Each platform passed
18 constant-oracle/language process steps plus 11 shader-source lesson steps
(58 total); the surface inventory adds one separate process step. All 2,319
values passed JIT/dynamic/static execution on each platform. The Windows
bounded dynamic/static executables retain their ordinary 1 MiB stack reserve.
C11/C++11/C++17 headers, O0/O2 oracle rejection probes, 17 positive/negative
string parser cases and replay of all eight repaired declarations are pinned
in `dev/release-materials/provenance/xge3-constant-parity-20261115.json`.
The full ecosystem goal and final matched runtime/IDE delivery remain open.
