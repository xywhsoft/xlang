xws xlang deferred assets

This directory preserves the high-level WebSocket client/server language
layer removed from the xlang core product. The saved generators, stdlib
source, tests, and HTTP upgrade bridge are migration input for the xws
extension package.

XRT core WebSocket frame, message, handshake, compression, TCP/TLS stream,
and HTTP upgrade primitives remain the foundation. The extension must use
those public APIs and must not restore the previous xws object layer to XRT
core.
