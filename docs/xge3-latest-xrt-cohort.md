# Matched latest-XRT candidates

The current qualified core header is
`1567b04ec597aa0183ad198c413610dda09075704209edccf515a8c91add5726`.
It is regenerated from the committed XRT tree at 6305aa54, with regenerated
xruntime declarations. The consumed core/runtime sources are unchanged from
a8e51e86. Concurrent uncommitted WebSocket work is excluded from this immutable
snapshot. XGE's vendored source update is committed as dc9352d2.

`tools/package/sync_xrt.py` in XGE now exports the complete declared extension
dependency closure and uses deterministic LF archive bytes and path ordering.
The normal XRT export also regenerates xruntime_decl.h rather than accepting
the stale live generated copy. Core APIs increase from 3459 to 3481, with no
removed or changed existing function declaration in this comparison. The
xruntime implementation remains the same thin 682392-byte source; its corrected
declaration header is 306921 bytes. These counts do not certify every API's
language integration.

The external candidates under `F:/xlang-validation/xge-full-20261110/` are:

- Complete O2 compiler/TCC/XRT/VFS builds:
  `compiler-latest-xrt-{win,linux}-full-r1`.
- Complete default+3D host and static SDK rebuilds:
  `latest-native-sdk-{win,linux}-r2`.
- Independent product/header/SDK/host-test verification:
  `latest-xrt-cohort-verified-{win,linux}-r1.json`.

The actual xl and xlc runtime images are tested in separate processes on both
OSes. All 213 required Windows and 211 Linux host symbols resolve. Function
addresses identify the same host runtime; allocation observation and four
Context/Widget creation/cleanup cycles balance; embedded TCC invokes the real
native functions. These are offline CPU tests. The static libraries are rebuilt,
but static language execution and full GPU/backend acceptance are separate.

The host gate reads `runtime_image` from the matched build report, accepting
xge3_host.dll/libxge3_host.so without manufacturing a legacy xge.dll copy. Its
public helper is built for the actual platform; the generated helper's source
and image hashes are checked along with the original inputs.

These complete builds predate the integer_min contract. The follow-up
`compiler-integer-min-{win,linux}-full-r2` candidates combine that committed
contract with the new committed producer metadata and are qualified separately.
Neither this record nor the SDK builds install runtime bytes into release,
replace an IDE, change the current delivery index, or close the full integration
task. Root xlang runtime/source-snapshot promotion remains part of that matched
delivery; it must not be confused with the isolated successful candidates.
