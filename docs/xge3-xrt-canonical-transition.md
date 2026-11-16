# Canonical latest-XRT source and staged SDKs

The canonical `lib/xrt` source is pinned to committed XRT
`4641cd2e4da09d8b1a66872cabe085c263d8b5ba`, matching the XGE native producer at
`466a1b003db896bbeb1dec10d624619ca34a7b45`. The full header SHA-256 is
`369f884c1c1fdc9621f654c81bb86850e85d992dc56b9cebf395e20ea03dfe7b`.
The xruntime implementation is unchanged and continues to use this one core.
Source snapshot metadata, ordinary/artifact symbol tables and host XRT symbol
catalogs are synchronized together.

All 3,482 previous public C declarators are preserved; three APIs are added:
`xrtTrySwapErrorTake`, `xrtNetAddrTryParse` and `xrtTlsClientResumeCapacity`.
The opaque `xtlsresume` identity is declared once for all relevant TLS
configurations. These source APIs have already entered the immutable complete
Windows/Linux O2 compiler cohorts and matching native SDKs used by the host,
callback, output and value-ABI qualification. No second XRT implementation is
introduced into the native host/static products.

Source verification can select the declaration headers of an external SDK:

```text
python tools/sync_xrt_snapshot.py --root . --check --sdk-directory PATH_TO_SDK
```

This flag only affects read-only checking. It cannot combine with `--sync`.
Without the flag, checking retains the normal platform `release/` lookup.
Altered declaration bytes are rejected against the source snapshot metadata.
The capability matrix is also required to be current.

Source promotion is distinct from replacing the installed SDK and binaries.
Until the matched runtime/compiler/native/IDE bundle is installed, the normal
release tree retains its existing bytes; checking newer source against its
older declaration headers is expected to report a mismatch. Do not refresh an
installed header alone to hide this condition. Final delivery hashes belong in
the development provenance index, and release payloads remain outside Git.

The receipt is
`dev/release-materials/provenance/xge3-canonical-xrt-20261117.json`.
