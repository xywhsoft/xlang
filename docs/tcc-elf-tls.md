# AMD64 ELF thread-local storage

The ELF linker must keep shared-library TLS relative to the loader-assigned
module, not relative to the main executable's thread pointer. This is native
thread-local storage; it is unrelated to the TLS network protocol.

The nested compiler shared-library acceptance on Linux exposed an existing
linker defect. A standalone C host returned successfully, while a Python 3.14
host detected damaged thread state on return. A separate three-variable native
fixture reproduced incorrect initial values with the same GCC PIC object; the
GCC-linked reference returned the correct values. Passing a C caller alone did
not establish memory safety.

The correction preserves general-dynamic and local-dynamic calls to
`__tls_get_addr` in a DSO. It emits real module-ID/offset GOT pairs, preserves
TLS symbol offsets in the dynamic table, and handles initial-exec GOT entries
with loader relocations. Executables retain local-exec relaxation with offsets
computed over the complete TLS layout. The TLS template starts at the maximum
alignment of all its sections, including `.tbss`; `PT_TLS` distinguishes stored
initializer bytes from zero-filled memory. Public TLS imports retain normal ELF
symbol resolution. No per-access language wrapper is added.

The current ELF AMD64 implementation adds four 32-bit fields to TinyCC's
internal per-symbol attribute record (`16 -> 32` bytes); Windows retains the
old layout. This is compiler/linker memory, not a language-object or per-call
allocation. A general-dynamic symbol needs a 16-byte GOT pair, local-dynamic
uses one such module pair, and initial-exec uses an 8-byte GOT slot. Loader TLS
storage and alignment padding are additional native costs. These changes do
not establish a total zero-overhead or CPU-performance claim.

Unsafe combinations are rejected before producing an image: a local-exec object
cannot be linked into a DSO, and native ELF TLS input cannot be JIT-relocated by
the current in-memory loader. The latter has no per-module thread-data allocator;
using executable offsets in an arbitrary host is not a fallback. This does not
change the resident runtime's existing portable thread-key APIs.

`tools/check_tcc_elf_tls.py` freezes its source inputs and exercises O0/O2
producers, initialized and zero-only templates, 64-byte alignment, general- and
initial-exec objects, imported host TLS, independent copies, concurrent threads,
host TLS/errno/key sentinels, unload/reload, Python CDLL callers, executable
output, and both refusal paths. It is a source preflight, not proof that a full
shipping or embedded compiler has been rebuilt. The source gate has 59 command
checks and 14 inspected templates. Existing relocation metadata gates pass on
Windows and Linux; the Windows native producer also compiles successfully.

The immutable evidence index is
`dev/release-materials/provenance/tcc-elf-tls-preflight-20261112.json`.
Complete compiler/VFS/embedded-source rebuild and the original failing nested
shared-library scenario remain required before qualified delivery.

Windows r29 complete O2/TCC/XRT/fixed-object/VFS/embedded-source acceptance
subsequently passed all 38 top-level commands, including owned-byte and scalar
transactions inside actual parent executables and Python-loaded shared-library
callers. Image, particle, literal-nested and source-free delivery gates passed.
Linux r21 subsequently passed all 41 top-level commands, including the original
Python CDLL owned-byte scenario and an independent native C dlopen caller.
Both full cohorts include VFS, embedded compiler sources and fixed runtime
objects; neither is an installation of the final XGE delivery. They predate
the new checked C integer integration. See the immutable evidence indices
`tcc-elf-tls-scalar-windows-20261112.json` and
`tcc-elf-tls-scalar-linux-20261112.json`.
