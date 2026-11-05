# Protected nullable Buffer spans

`@c_span` accepts `Buffer?` for readonly or writable pointer/count inputs.
An absent language object passes exactly NULL and zero without opening a
Buffer scope. A present empty Buffer opens and ends the ordinary protected
scope, even when its data pointer is NULL and its logical length is zero.
This distinction prevents a reentrant mutation from bypassing protection.
Nullable string, bytes, POD records and arbitrary native-pointer classes do
not become span storage.

Nonnull readonly string, bytes and Buffer declarations may share a C target
when they prove the same exact pointer type, element type, count ABI, header,
remaining parameters, results, effects and ownership. Each keeps its own
logical type and validated preparation ABI. Nullable and nonnullable signatures
are not made physically compatible by this rule. Writable strings and bytes
remain invalid.

Strings pass their existing counted byte storage, including embedded NUL;
there is no implicit UTF-8 re-encoding, terminator scan or temporary bytes
conversion. Bytes use their normal owned value lifetime. Buffers use the
existing read/write span pin through the Native call and cleanup tail.
Only a null Buffer skips that pin. Typed element size, alignment and count
range are checked before the business Native function. A readonly descriptor
pack may use the same prepared span through paired `span_data`/`span_count`
fields; nullable Buffer fields produce NULL and zero together.

Writing addresses the Buffer's already initialized logical length. It neither
extends nor truncates that length. Native failure may leave a modified prefix,
and normal checked error handling reports that failure. Reentrant mutation of
a pinned Buffer is checked and rejected. No raw pointer or borrowing syntax is
added to the language, and no business C forwarding function is generated.

`test/cases/foreign_nullable_span.xl` and
`tools/check_foreign_nullable_span.py` exercise null/empty/nonempty inputs,
readonly overloads and packed spans, direct/bound/protocol calls, NUL data,
source replacement during a callback, last-source release, reentrant mutation,
write failure and logical-length preservation. Forced allocation failure also
covers the allocation-free null/empty call path and range rejection without
calling the business Native function. C11/C++11/C++17 proofs, malformed
declarations, sealed IR mutations and O0/O2 CPU execution are separate gates.
