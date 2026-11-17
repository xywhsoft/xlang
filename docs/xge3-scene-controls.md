# Scene queries, appearance and incremental deletion

`xge3d.Scene` adds ten declarations mapped directly to nine existing native
functions. The scene's previously qualified shared ownership and per-call
native access scope are reused. No library-side C forwarding functions or new
native implementation are added.

| Language method | Behavior |
| --- | --- |
| `bounds(node, descendants=0)` | Native `(status, Aabb)`; can include the subtree and ignores visibility |
| `raycast(ray, maxDistance)` | Native `(status, Hit)` for the nearest visible triangle |
| `raycast(ray, maxDistance, filter)` | Selects query layers and optionally hidden ancestors |
| `prepareSpatial()` | Builds/refits the native spatial cache; checked failure |
| `setLight(node, light)` / `clearLight(node)` | Copies a validated light descriptor or passes typed NULL |
| `setShadowFlags(node, flags)` / `shadowFlags(node)` | Preserves the native CAST/RECEIVE bits and validation |
| `setColor(node, rgba)` | Takes exactly four fixed `c_float` values in [0,1] |
| `destroyStep(root, budget)` | Returns the native completion integer after bounded subtree deletion |

Query methods preserve native status codes, including NOT_FOUND and invalid
geometry; check status before reading a result. The native query functions
clear outputs before failure. Managed lifetime/admission and conversion errors
still use checked errors. Ordinary configuration and cache-maintenance
methods check native status instead of returning it silently.

`destroyStep` logically retires the whole subtree immediately, even when its
node budget is zero. Retiring handles are unavailable to ordinary methods.
Continue with the original root until completion becomes one. Nodes are
removed from leaves upward. The native `microseconds` field remains a soft
limit checked between nodes; zero disables that time limit. GPU-containing
nodes still require their owning graphics context thread; this CPU fixture
does not qualify those separate GPU resources.

The beginner `scene_controls.xl` example needs no window, graphics context,
external library loader or custom class. The separate acceptance fixture uses
a CPU triangle created by a controlled C provider, because public Mesh
ownership is still a separate integration task. Test-only private fields are
read solely through the exact private header frozen with the SDK; these reads
are not exposed by the language package.

Windows/Linux each pass 39 build/run/import/header steps: JIT, hosted/static
standalone programs, a shared library with checked retirement and physical
unload, O0/O2 real-SDK C clients, and both prior Scene/ray lessons. Tests cover
positive hits, miss/cleared output, query layers, inherited visibility,
descendant bounds, light normalization without source mutation, invalid
light/color/shadow rollback, closed views, retiring child rejection and 32
allocation budgets followed by successful spatial-cache retries. C11/C++11/
C++17 generated public headers compile strictly.

The warm comparison executes 1000 iterations of the same ray, shadow read and
color write. C and language paths both give every operation its own native
access scope and observe zero XRT allocation/reallocation/free operations.
Calling the C comparison from language adds one outer foreign argument scope;
the independent C clients have no such outer view. This is allocation evidence,
not a CPU-equivalence claim. Managed view/code-owner/error checks still cost
work. Fixed records/arrays have value semantics and can incur stack/register
copies; they are not boxed as language containers.

On both x64 platforms the measured C sizes are: Node 16, Ray 24, Hit 72,
Aabb 24, LightDesc 48, WorkBudget 16, QueryFilter 8 and RGBA array 16 bytes.
The native Scene scope is 8 bytes. The status/Hit and status/Aabb product
layouts are 80 and 28 bytes. Generated query calls write status and records
directly into their result product fields, without an intermediate native
output buffer or heap object. These layout sizes do not measure optimized
machine-code copy counts.

The matching normal full-3D SDKs are the already qualified Native `497121af`
cohort. No new SDK build is counted by this batch. Customized disabled feature
profiles and the remaining GPU/model/animation/collision capabilities remain
in the full goal. Both source audits report 5312 native functions/991 types and
3936 directly declared names; inventory presence is not a functional completion
percentage. Storage review passes 399/400 checks. Two unchanged Proxy field
header witnesses are rebased only after proving the native header added five
Shared declarations and changed no other text modulo line endings.

The small receipt
`dev/release-materials/provenance/xge3-scene-controls-20261117.json` indexes
exact inputs, packages, logs, layout and independent verification. No shipping
release/IDE/help tree is installed by this batch. The now-approved generic
`@c_data_snapshot` rule and safe Graph/Workflow APIs remain subsequent work.
