# Platform SDK resources

Linux runtime/compiler VFS manifests resolve public SDK files from
`release/linux_x64`; Windows manifests use `release/win_x64`. The private-header
staging tool selects the current host's compiler manifest by default. A caller
may still pass an explicit manifest to its Python API.

This matters when updating XRT: the active SDK's `xrt_decl.h`, snapshot metadata
and embedded `include/__xl_runtime/xrt_decl.h` must describe the same producer.
Reading an inactive target's older header can silently lose newly declared
types or functions, even though compiling the implementation itself succeeds.
The normal full build's snapshot check rejects such a mismatch.

`tools/check_runtime_sdk_resources.py` stages the real manifest-selected
resources, checks that every SDK input belongs to the active target, and matches
the private declaration hash to `lib/xrt/xrt.snapshot.json`. Its negative control
explicitly selects the other target: an absent SDK must be rejected, or a
different older declaration must fail the same byte check. It does not replace
the loader with a test implementation. Reports retain selected source hashes.

The accompanying full compiler cohorts additionally rebuild TinyCC, XRT,
xruntime, precompiled providers and VFS resources. This source gate alone does
not prove their generated binaries, nested compilation or an installed release.

When refreshing the separate XRT catalogs, generate
`src/compile/xl6_artifact_native_symbols.inc` first, then regenerate
`src/runtime/xl6_host_xrt_symbols.h` from it. Check both before freezing build
inputs. The package stager independently rejects a stale host resolver catalog.
