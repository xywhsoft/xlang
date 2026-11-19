# Watched native record fields

`@c_output_field(ordinal, "member", "owner_view")` admits a child returned
inside a borrowed native output record. The source is the method receiver.
The receiver must belong to a managed root with a native call scope. Every
declared child result must specify that same root and a watch policy.

```xl
extern "getNativeNode" @no_raise @c_output_record(2,"NativeNodeInfo","borrowed") @c_status(0) @c_output_field(0,"widget","owner_view") @c_output_field(1,"bounds") func getNode(c_int index) as (Widget,Rect)
```

The C header proves the field's pointer type. The producer keeps the child
valid in the active receiver scope until the watcher is attached. For a
nullable result, a NULL field produces null. A required field with a NULL
pointer raises a checked state error. Creator-thread and native-destruction
checks use the same rules as an ordinary `owner_view` result.

The compiler calls the producer once, checks its status when declared, and
admits the watched children before invoking retained-output hooks. A failed status
skips the record fields. A failed field conversion releases any acquired
language references and leaves the destination tuple untouched. A watcher
already cached by the root can be reused by subsequent calls. The root
removes cached watchers during native destruction.

The first exposure can allocate the managed view and the owner's identity
directory entry. Repeated exposure retains the cached view. Stack records,
the lifetime scope, identity lookup, and reference tracking are still part
of each call.
