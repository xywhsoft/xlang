# Matched XRT804 native SDKs

The complete O2 compiler cohorts for strict data snapshot arguments and the
XGE default-plus-3D SDKs now use the same XRT core revision:
`804cc769de3030118c2b46ef7af69543d1b82d9d`. Native synchronization is committed
as `d8b2914242b0582422fe47d7d87a3f6400fa9ca3` in the XGE repository. Only four
vendored core files changed; native model implementations and selected
extension dependencies remain unchanged.

Windows and Linux each built a fresh hosted SDK and direct-external static
SDK at O2: 164 + 166 compilation units, 660 units across four builds, with no
reused production objects. The hosted SDK resolves the compiler's one XRT;
the static SDK embeds neither an XRT implementation nor host-import slots.
Both match the qualified compiler's core header exactly.

Each OS passed 43 source-unit steps and 48 actual hosted/static client steps
at O0/O2. Coverage includes the original baseline reproduction, all 99 integer
model Busy boundaries, Shared identity, Clone/history isolation, checked
lifetimes, allocation failures, schema/XSON regressions and binary imports.
Independent verification rechecked 7,659 Windows and 7,451 Linux files,
including frozen build inputs, objects, dependency records, logs and clients.
The SDK checker accepts the native freeze's `overrides` spelling as well as
the earlier `overlays` spelling without relaxing hash verification.

This is a matched dependency qualification. It does not qualify the public
language model API or imply a final runtime/IDE installation. Graph/Workflow
snapshot wiring and complete delivery remain active work. No network,
graphics-device or security tests were run in this batch. Small evidence is
indexed in `dev/release-materials/provenance/xrt804-matched-xge-sdk-20261117.json`;
raw builds and logs remain at their recorded external paths.
