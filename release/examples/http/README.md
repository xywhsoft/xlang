# HTTP/1 examples

Run from the demo6 directory with the rebuilt compiler:

```text
release/xl run release/examples/http/parse.xl
release/xl run release/examples/http/tcp.xl
```

`parse.xl` constructs and parses an owned message containing an embedded NUL.
`tcp.xl` runs an ordinary local TCP exchange, reads a chunked body and trailers,
handles a pipelined request, and hands the retained stream and surplus bytes to
the next protocol. It binds only a literal loopback address and an ephemeral port.

Both examples are registered in `tools/check_stdlib_http.py` and run through
JIT, module relocation and independent AOT. The Windows standard-library suite
and `test_linux.sh` invoke this gate automatically; receipts pin both sources.

For TLS, construct `Http1TlsTransport` from an established `TlsStream`; its
read methods have the same names and limits as the TCP transport. The complete
TLS test is `test/cases/stdlib_http1_tls_transport.xl`, using the project's
existing offline certificate fixtures and a local listener.

`readRequestHead[Async]` / `readResponseHead[Async]` return `Http1Head?`: null is
EOF between messages. Truncation throws `IOError`. Read or discard a body before
the next head. The `Async` methods use Future continuations; synchronous methods
must run on a thread where blocking is allowed. One read operation at a time is
allowed per transport. The caller must give it exclusive use of the stream's
receive side. Send operations can continue through the original managed stream.

`detach()` is explicit and accepts only an HTTP message boundary or an upgraded
connection. Its handoff owns both `stream` and `buffered`; process `buffered`
before receiving more from `stream`. The old transport then rejects further use.
The original stream is still reference counted; dropping the transport alone
does not override other owners' lifetimes.

The default cumulative body and decoded-output limit is 16 MiB. Larger or
unlimited values require an explicit configuration. `timeoutMillis` budgets the
whole read operation, including every partial receive. A pre-cancelled call
leaves the state intact; an error after a read begins marks the reader failed.
Explicitly close a failed transport. Streamed output already delivered to a
Buffer is retained when a later error occurs.

Higher-level client/server policy, pooling, routing and cookies remain separate
extension work. WebSocket uses its own standard-library module and the transport
handoff API. This API exposes the 18 XRT HTTP protocol
modules and composes them with managed TCP/TLS.
