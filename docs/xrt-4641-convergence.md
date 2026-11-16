# Latest XRT convergence candidates

The Native repository now pins committed producer
`4641cd2e4da09d8b1a66872cabe085c263d8b5ba` (Native commit `466a1b003d`).
The complete canonical header SHA-256 is
`369f884c1c1fdc9621f654c81bb86850e85d992dc56b9cebf395e20ea03dfe7b`.
All 3,482 old function signatures match; three APIs are additive. There is no
microsecond-time compatibility layer.

Windows/Linux isolated full O2 compiler, TinyCC, XRT/xruntime, VFS and private
SDK builds pass with no changed frozen inputs. Default/debug hosted and static
Native SDKs, plus Linux EGL variants, stage against that same producer. The
root's canonical `lib/xrt` and installed runtime/IDE have not been promoted to
these candidates; the final matched delivery step remains open.

`tools/check_xrt_snapshot_additions.py` verifies qualified complete compiler
products, SDK declaration hash and the three registration entries, then runs
the C fixture through actual JIT and standalone AOT. The generated executable
must not import a compiler/runtime DLL/DSO. Each platform passes four commands.
The fixture uses only owning error-slot exchange, counted numeric address
parsing and NULL-session argument checking. No socket, DNS, certificate or TLS
handshake is performed. Receipt: `dev/release-materials/provenance/xrt-4641-candidates-20261117.json`.

The broader current candidates also pass native HostSession O0/O2, public
package WGL/EGL/GLES, callback errors, actual acyclic shared-image unload and
live debug checks. These results do not close the whole XGE/XUI/Document goal.

The EGL global-context gate exposed an independent ownership issue: language
global init keeps a separate module-code lease, while a module global context
prevents Unit, so neither side retires. Sixteen nonwaiting passes stay Busy.
The same global-context/module graph retires with a C-resident initializer and
an explicit OS image pin. A diagnostic plan confirms one remaining external
module reference. An owning Engine controller decision was requested; the old
manual init/unit contract is unchanged. Claimed callback finalization is also
pending. The failed gates and diagnostic sources/logs remain in external QA;
they are not relabeled as passing release acceptance.
