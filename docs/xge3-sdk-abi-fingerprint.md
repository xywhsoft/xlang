# Complete native SDK ABI fingerprint

The Native builder and package stager use fingerprint schema 2. Its inputs are
the generated feature configuration, every sorted public `include/*.h` file,
the single XRT header, compiler version and debug mode. Paths and content
boundaries are included. Text line endings are normalized by UTF-8 text reads.

This includes the transfer service/owner structures and Document/Document UI
records. Previously only six named files contributed, so changes confined to
these additional public headers could evade the build compatibility check.
The stager rejects older build reports before creating output. Rebuild the
engine with its matching source rather than editing an old report or copying
a new header over an older binary.

`tools/check_xge3_sdk_fingerprint.py` compares both implementations and mutates
every public header in an isolated tree, checks newly added empty headers and
line endings, and verifies rejection of the old report format. It does not
build/load a Native engine or qualify a delivered SDK. Final delivery still
requires the normal builder, resolver/import checks and headless execution.
