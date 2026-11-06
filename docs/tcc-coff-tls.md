# Windows AMD64 static TLS in the native COFF linker

`IMAGE_REL_AMD64_SECTION` stores the final one-based PE output-section index.
`IMAGE_REL_AMD64_SECREL` stores the symbol offset from that output section's
start, including its signed in-place addend. Input contributions retain their
original COFF key length separately from the linker's uniqueness suffix.

After section GC, the PE linker sorts and merges live `.tls` / `.tls$*`
contributions and `.CRT$XL*` / `.CRT$XD*` pointer arrays by their original keys.
A live template or callback table roots the actual `_tls_used` directory when
present. Its VA relocations and ordinary writable `_tls_index` storage are
preserved. TLS directory alignment covers all retained template inputs. The
template must begin at the TLS output-section base, so standard SECREL offsets
match the loader template. Its index address is four-byte aligned. The
four directory addresses and callback pointers use normal `DIR64` image-base
relocations, including EXE output without an explicit ASLR option.

When no CRT supplies a directory, the linker synthesizes only the OS template,
index, null-terminated callbacks and 40-byte TLS directory. Dynamic `.CRT$XD*`
initialization requires its actual CRT descriptor/callbacks. This does not
synthesize CRT startup, `atexit`, or arbitrary `.CRT$XI*`/`.CRT$XC*` behavior.
Unsupported live CRT families fail after GC. Mis-sized or over-aligned TLS
callback-array contributions and reserved alignment encodings are rejected.
Legacy prioritized `.ctors.N` / `.dtors.N` remain outside this input contract.

Live Windows static TLS and live COFF image-section-relative relocations are
not supported by the memory/JIT linker. They fail after GC, so an unreferenced
TLS function/data graph can still be discarded. `TlsAlloc` is not used to
imitate the Windows loader's static-template ABI. TCC's existing ELF-style
`__thread` instruction generation is likewise rejected on this PE path; this
feature supports GCC-produced Windows AMD64 COFF static TLS, not a claim that
TCC's Windows C frontend now emits that ABI.

The linker adds no per-call TLS wrapper. It adds the necessary per-thread OS
TLS storage and PE directory/index/callback/relocation data. Merging copies
link-time template contributions and rewrites their symbol/relocation metadata.
The exact original-name length uses four existing bytes of `Section` tail
padding: Win64 `sizeof(Section)` remains 128, while its name offset becomes 124.
No extra runtime allocation or private TLS-key pool is introduced.

The CPU tool is `tools/check_tcc_coff_tls.py`. Its GCC-built fixture includes
production libtcc and runs the real COFF reader, GC, PE preparation, address
assignment and relocation in memory. No TinyCC-produced PE is written or run.
Real MinGW `tlssup`, `tlsthrd`, `tlsmcrt`, `tls_atexit` objects and the actual XGE
MD4C object are structural inputs. Unrelated CRT/MD4C imports are inert test
stubs in that structural test, so it is not an executable integration proof.
A separate GCC-built Windows program validates the source fixture's OS TLS
initialization/isolation and is inspected with GNU objdump. The original blocked reports remain unchanged. After the user restored
execution authorization, `tools/check_tcc_windows_tls_pe.py --execute-pe`
qualified fresh TinyCC-produced PE images from the exact final R7 body and
subsequent canonical-pdata R9 body: the
synthetic OS directory and real MinGW C static-TLS directory/thread callbacks,
fixed and ASLR images, initialized/zero/aligned TLS, two concurrent isolated
threads, and process/thread attach/detach ordering. A dedicated OS file handle
records the final process-detach outcome after the C main function returns.
C++ `tls_atexit`/UCRT registration is outside this C fixture. This source/CLI
qualification is separate from matched shipping compiler and whole-XGE AOT
acceptance. No security-product settings are changed by either tool.

The format contract follows Microsoft's PE/COFF specification:
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
