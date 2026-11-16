# Shared modules and dependency initialization

Shared output rejects a user `main` and user top-level executable statements.
It accepts a source dependency's initialization program. Top-level variable
declaration initializers keep their existing checked module initialization
semantics. Both explicit host initialization and exported entrypoints use the
same idempotent initialization state and checked failure path.

Import expansion now removes repeated initialization statements from a shared
dependency, alongside its repeated declarations. The identity includes the
import namespace, physical source path and original statement byte range.
Two identical statements deliberately written at different offsets both run.
Separate module instances and different source files retain their own state.
Unknown origins/ranges are preserved rather than guessed equal.

The AST source owns an immutable namespace string and shares it through the
existing source reference count. Failed namespace allocation leaves the source
unchanged. Rebinding the same identity allocates nothing; changing a bound
identity is rejected. Replacing/freeing the last source releases the namespace.
On the tested 64-bit layouts this adds eight bytes per parsed source and one
namespace string allocation per imported source parse. It adds no generated
runtime call, allocation or wrapper.

`tools/check_shared_dependency_init.py` qualifies a complete compiler, tests
dependency diamonds, deliberate repeated statements, source/module instances,
initialization ordering and checked initialization failure, and rejects user
program entries. It also builds the AST source lifetime/OOM fixture at O0/O2.
The separate Graph gates exercise the actual hosted/static SDK through JIT,
executables and checked shared modules. Native SDK and release installation are
separate from compiler source qualification.
