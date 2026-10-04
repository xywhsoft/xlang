#ifndef XL6_IR_H
#define XL6_IR_H

/*
 * XLIR is the compiler's canonical backend contract.  It deliberately owns
 * its text, type arena and source spans: finalized XLIR never points at an
 * XL6_AstNode.
 */
#include "src/common/xl6_output.h"
#include "src/diag/xl6_diag.h"
#include "src/ast/xl6_ast.h"
#include "src/type/xl6_type_id.h"

typedef uint32 XL6_IrNodeId;
typedef uint32 XL6_IrBlockId;
typedef uint32 XL6_IrValueId;
typedef uint32 XL6_IrNominalIdentitySetId;

#define XL6_IR_NODE_INVALID 0u
#define XL6_IR_BLOCK_INVALID 0u
#define XL6_IR_VALUE_INVALID 0u
#define XL6_IR_NOMINAL_IDENTITY_SET_INVALID 0u

typedef XL6_TypeId XL6_IrTypeId;
#define XL6_IR_TYPE_INVALID 0u

typedef enum XL6_IrTypeKind {
    XL6_IR_TYPE_BUILTIN = 1,
    XL6_IR_TYPE_CLASS,
    XL6_IR_TYPE_UNION,
    XL6_IR_TYPE_FUNCTION,
    XL6_IR_TYPE_TUPLE,
    XL6_IR_TYPE_GENERIC_INSTANCE,
    XL6_IR_TYPE_TYPE_PARAM,
    XL6_IR_TYPE_NULLABLE,
    XL6_IR_TYPE_WEAK,
    XL6_IR_TYPE_PROTOCOL,
    XL6_IR_TYPE_NUMERIC_ENUM,
    XL6_IR_TYPE_TAGGED_ENUM,
    /* A reference-counted lexical storage cell.  It has exactly one type
     * parameter: the element ABI.  Cells are compiler-internal XLIR values,
     * never source-level aliases reconstructed by Cgen. */
    XL6_IR_TYPE_CELL,
    /* Unique, compiler-owned cursor over an XRT value-container snapshot.
     * Its sole parameter is the exact source collection TypeId.  Source code
     * cannot name this type and Cgen must not collapse it to a raw pointer. */
    XL6_IR_TYPE_VALUE_ITERATOR,
    /* Compiler-owned reference to one addressable XLIR storage slot.  This is
     * the only value admitted for INOUT/OUT call operands; its sole parameter
     * is the exact pointee TypeId, so an erased C pointer never loses the
     * language-level storage contract. */
    XL6_IR_TYPE_STORAGE_REF
} XL6_IrTypeKind;

/* Canonical collection identity.  All current containers share the erased
 * xvalue C ABI, so this discriminator prevents lowering and verification from
 * recovering array/list/set/dict semantics from diagnostic type names. */
typedef enum XL6_IrCollectionAbi {
    XL6_IR_COLLECTION_NONE = 0,
    XL6_IR_COLLECTION_ARRAY,
    XL6_IR_COLLECTION_LIST,
    XL6_IR_COLLECTION_SET,
    XL6_IR_COLLECTION_DICT
} XL6_IrCollectionAbi;

/* Canonical identity for language-visible runtime resources that share the
 * owning xvalue handle representation.  This is deliberately orthogonal to
 * C ABI: lowering/verifiers select operations from this closed identity and
 * never infer thread/channel/synchronization semantics from a type spelling. */
typedef enum XL6_IrRuntimeResourceAbi {
    XL6_IR_RUNTIME_RESOURCE_NONE = 0,
    XL6_IR_RUNTIME_RESOURCE_THREAD,
    XL6_IR_RUNTIME_RESOURCE_CHANNEL,
    XL6_IR_RUNTIME_RESOURCE_MUTEX,
    XL6_IR_RUNTIME_RESOURCE_COND,
    XL6_IR_RUNTIME_RESOURCE_RWLOCK,
    XL6_IR_RUNTIME_RESOURCE_SEM,
    XL6_IR_RUNTIME_RESOURCE_STACK,
    XL6_IR_RUNTIME_RESOURCE_AVL_TREE,
    XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE,
    XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE,
    XL6_IR_RUNTIME_RESOURCE_MPMC_QUEUE,
    XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE
} XL6_IrRuntimeResourceAbi;

/* A source constructor/member spelling is converted once into this exact
 * resource operation in Typed XLIR.  The resource TypeId remains independently
 * available through operand/result types, so one operation can never be
 * applied to another xvalue-backed resource family. */
typedef enum XL6_IrRuntimeResourceOp {
    XL6_IR_RUNTIME_RESOURCE_OP_INVALID = 0,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_WAIT,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_WAIT_TIMEOUT,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_EXIT_CODE,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_STATE,
    XL6_IR_RUNTIME_RESOURCE_OP_THREAD_STOP,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_SEND,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_TRY_SEND,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECV_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_TRY_RECV_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECV_TIMEOUT_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_CLOSE,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_IS_CLOSED,
    XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_COUNT,
    XL6_IR_RUNTIME_RESOURCE_OP_MUTEX_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_MUTEX_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_MUTEX_TRY_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_MUTEX_UNLOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_WAIT,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_TRY_WAIT,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_WAIT_TIMEOUT,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_POST,
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_POST_MULTIPLE,
    XL6_IR_RUNTIME_RESOURCE_OP_COND_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_COND_WAIT,
    XL6_IR_RUNTIME_RESOURCE_OP_COND_WAIT_TIMEOUT,
    XL6_IR_RUNTIME_RESOURCE_OP_COND_SIGNAL,
    XL6_IR_RUNTIME_RESOURCE_OP_COND_BROADCAST,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_READ_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_TRY_READ_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_READ_UNLOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_WRITE_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_TRY_WRITE_LOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_WRITE_UNLOCK,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_DOWNGRADE,
    XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_UPGRADE,
    /* A declaration-only `sem` has language defaults (0, INT32_MAX).  Keep
     * this distinct from the explicit two-argument constructor so lowering
     * never invents absent operands or backend-owned constants. */
    XL6_IR_RUNTIME_RESOURCE_OP_SEM_CREATE_DEFAULT,
    /* Generic concurrent collections use one payload transfer contract while
     * preserving their exact nominal resource family in the resource TypeId.
     * The operation identity fixes blocking/timeout behavior before Cgen. */
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_TRY_PUSH,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_TRY_POP_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_TIMEOUT_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CLOSE,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_COUNT,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CAPACITY,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_RESET,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_CLOSED,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_DRAINED,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_PUSH,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_RESERVE,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_CLONE,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_EQUALS,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_TRIM,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_CLEAR,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_COUNT,
    /* Exact owned/value results, never writable caller arguments. */
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_POP,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_AT,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_TRY_POP,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_TRY_PEEK,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_TRY_PEEK_AT,
    XL6_IR_RUNTIME_RESOURCE_OP_STACK_CAPACITY,
    /* AvlTree<K,V> is deliberately not encoded as a one-payload resource.
     * Every operation carries independent key/value transfer contracts plus
     * a closed key ordering ABI, so lowering and Cgen never recover either
     * generic argument or comparison semantics from source/type names. */
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CREATE,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_SET,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_GET_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_TAKE_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CONTAINS,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_REMOVE,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_FIRST_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_LAST_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_LOWER_BOUND_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_UPPER_BOUND_INTO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_TRIM_TO,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_EQUALS,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_MERGE,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CLEAR,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_COUNT,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_KEYS,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_VALUES,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_TRIM,
    XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CLONE,
    /* Append-only batch identities.  They borrow an exact array<T> and
     * return the processed prefix length as i64; POP additionally borrows
     * the mutable output array and takes one i64 capacity. */
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_PUSH_BATCH,
    XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_BATCH_INTO
} XL6_IrRuntimeResourceOp;

/* Runtime resources with generic payloads carry an explicit transfer ABI in
 * addition to the exact payload TypeId.  TRIVIAL_COPY is restricted by the
 * verifier to closed scalar C representations and canonical tagged nullable
 * scalar carriers; managed strings are copied and
 * xvalues are retained on send, then moved into addressable storage on receive. */
typedef enum XL6_IrRuntimeResourcePayloadAbi {
    XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID = 0,
    XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY,
    XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY,
    XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN
} XL6_IrRuntimeResourcePayloadAbi;

/* Closed ordering selected for AvlTree keys at the Typed XLIR boundary.
 * SIGNED_I64 also covers time; TYPE_ID is kept distinct despite its u64 C
 * representation because it is a different language comparison contract.
 * Floating order is the runtime's canonical total order, including NaN. */
typedef enum XL6_IrRuntimeResourceKeyOrderAbi {
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID = 0,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_BOOL,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_SIGNED_I64,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_UNSIGNED_U64,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_FLOAT_TOTAL,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_STRING,
    XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_TYPE_ID
} XL6_IrRuntimeResourceKeyOrderAbi;

/* A member call on the open `var` xvalue cannot name one static collection
 * family: the runtime tag deliberately selects array/list/set/dict behavior.
 * Typed XLIR still resolves the language operation once, so lowering and Cgen
 * never inspect the source member spelling.  Every operation consumes zero,
 * one, or two freshly owned boxed arguments and returns one owned xvalue. */
typedef enum XL6_IrDynamicCollectionOp {
    XL6_IR_DYNAMIC_COLLECTION_INVALID = 0,
    XL6_IR_DYNAMIC_COLLECTION_PUSH,
    XL6_IR_DYNAMIC_COLLECTION_INSERT,
    XL6_IR_DYNAMIC_COLLECTION_REMOVE,
    XL6_IR_DYNAMIC_COLLECTION_REMOVE_VALUE,
    XL6_IR_DYNAMIC_COLLECTION_POP,
    XL6_IR_DYNAMIC_COLLECTION_TAKE,
    XL6_IR_DYNAMIC_COLLECTION_CLEAR,
    XL6_IR_DYNAMIC_COLLECTION_RESERVE,
    XL6_IR_DYNAMIC_COLLECTION_SWAP,
    XL6_IR_DYNAMIC_COLLECTION_ADD,
    XL6_IR_DYNAMIC_COLLECTION_CONTAINS,
    XL6_IR_DYNAMIC_COLLECTION_INDEX_OF
} XL6_IrDynamicCollectionOp;

/* Statically typed collection members share one closed semantic family.  The
 * frontend fixes both this operation and collectionAbi; lowering and Cgen may
 * inspect the result/operand TypeIds only to select their physical ABI. */
typedef enum XL6_IrTypedCollectionOp {
    XL6_IR_TYPED_COLLECTION_INVALID = 0,
    XL6_IR_TYPED_COLLECTION_POP,
    XL6_IR_TYPED_COLLECTION_TAKE,
    XL6_IR_TYPED_COLLECTION_CLEAR,
    XL6_IR_TYPED_COLLECTION_CAPACITY,
    XL6_IR_TYPED_COLLECTION_RESERVE,
    XL6_IR_TYPED_COLLECTION_RESIZE,
    XL6_IR_TYPED_COLLECTION_TRIM,
    XL6_IR_TYPED_COLLECTION_TRIM_TO,
    XL6_IR_TYPED_COLLECTION_SWAP,
    XL6_IR_TYPED_COLLECTION_INSERT,
    XL6_IR_TYPED_COLLECTION_INDEX_OF,
    XL6_IR_TYPED_COLLECTION_REVERSE,
    XL6_IR_TYPED_COLLECTION_IS_DISJOINT
} XL6_IrTypedCollectionOp;

/* Frontend-fixed C ABI category.  Cgen maps this field mechanically and
 * must never classify types by name, inference, or source representation. */
typedef enum XL6_IrCAbi {
    XL6_IR_C_ABI_UNSUPPORTED = 0,
    XL6_IR_C_ABI_VOID,
    XL6_IR_C_ABI_BOOL,
    XL6_IR_C_ABI_I64,
    XL6_IR_C_ABI_F64,
    /* Exact ISO C scalar representations.  Language integers deliberately
     * keep the normalized I64 execution ABI above; these categories are for
     * source `c_*` values and therefore preserve the foreign boundary width,
     * signedness and platform model all the way through verified XLIR. */
    XL6_IR_C_ABI_C_CHAR,
    XL6_IR_C_ABI_C_SCHAR,
    XL6_IR_C_ABI_C_UCHAR,
    XL6_IR_C_ABI_C_SHORT,
    XL6_IR_C_ABI_C_USHORT,
    XL6_IR_C_ABI_C_INT,
    XL6_IR_C_ABI_C_UINT,
    XL6_IR_C_ABI_C_LONG,
    XL6_IR_C_ABI_C_ULONG,
    XL6_IR_C_ABI_C_LONGLONG,
    XL6_IR_C_ABI_C_ULONGLONG,
    XL6_IR_C_ABI_C_SIZE,
    XL6_IR_C_ABI_C_SSIZE,
    XL6_IR_C_ABI_C_PTRDIFF,
    XL6_IR_C_ABI_C_INTPTR,
    XL6_IR_C_ABI_C_UINTPTR,
    XL6_IR_C_ABI_C_BOOL,
    XL6_IR_C_ABI_C_FLOAT,
    XL6_IR_C_ABI_C_DOUBLE,
    XL6_IR_C_ABI_C_WCHAR,
    /* Owned, NUL-terminated allocation.  Its construction and destruction
     * are explicit runtime operations in XLIR, never Cgen conventions. */
    XL6_IR_C_ABI_MANAGED_STRING,
    /* Opaque, reference-counted runtime value.  Lowering selects it from the
     * canonical frontend ABI; Cgen only emits the already fixed xvalue calls. */
    XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
    /* A pure native-backed class is an opaque C handle.  It is deliberately
     * not an xvalue: factory and destruction linkage are resolved call ABI
     * facts in XLIR, and Cgen may only pass the resulting void pointer. */
    XL6_IR_C_ABI_NATIVE_HANDLE,
    /* Frontend `point` is an untyped, non-owning foreign pointer. It is not
     * interchangeable with a native class handle. */
    XL6_IR_C_ABI_RAW_POINTER,
    /* Owned XRT error object.  Catch payload lowering takes ownership from
     * the current error context; Cgen never infers this from catch syntax. */
    XL6_IR_C_ABI_RUNTIME_ERROR_PTR,
    /* Reference-counted XRT future handle.  Its completed-value ABI remains
     * a type parameter of future<T>; awaiting it is a separate lowered CFG
     * operation, never a Cgen type-name special case. */
    XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,
    /* Reference-counted XLIR closure.  The closure target, capture frame and
     * typed invoke contract are all lowered data below; a C backend must not
     * reconstruct a lambda from AST syntax or a source function type. */
    XL6_IR_C_ABI_CLOSURE_PTR,
    /* Compiler-owned structural product.  The ordered element TypeIds live in
     * the tuple type's params slice; only products whose elements all have a
     * verified trivial value ABI are admitted initially. */
    XL6_IR_C_ABI_PRODUCT,
    /* Opaque reference-counted cell pointer.  Its element TypeId is carried
     * by XL6_IR_TYPE_CELL and is validated by every CELL_* instruction. */
    XL6_IR_C_ABI_RUNTIME_CELL_PTR,
    /* Lossless execution ABI for language uint64.  This is deliberately not
     * the foreign C_ULONGLONG category and is never routed through signed i64
     * or an xvalue unless a dedicated unsigned boxing ABI is added. */
    XL6_IR_C_ABI_U64,
    /* Language `type` values are stable 64-bit semantic identities.  Keep
     * them distinct from U64 so arithmetic/conversion admission can never
     * accidentally turn a type token into an integer, even though both use
     * uint64_t at the final C boundary. */
    XL6_IR_C_ABI_TYPE_ID,
    /* Trivial nullable scalar/product with an explicit presence tag and one
     * payload whose TypeId is the nullable type's sole parameter. */
    XL6_IR_C_ABI_TAGGED_NULLABLE,
    /* Unique compiler-owned wrapper around xvalueiter.  Its current key/item
     * state is observed only through verified iterator RuntimeOps and its
     * lifetime ends through an explicit LoopResource destroy edge. */
    XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR,
    /* Compiler-owned, nominal, untagged plain union.  Its ordered field layout
     * lives in XL6_IrUnionLayout; only verified trivial field ABIs are admitted.
     * Appended to preserve every established CAbi numeric identity. */
    XL6_IR_C_ABI_UNION,
    /* Erased physical pointer used only by XL6_IR_TYPE_STORAGE_REF.  The
     * pointee TypeId remains in that type's parameter list and every consumer
     * must validate it before emitting a typed C pointer boundary. */
    XL6_IR_C_ABI_STORAGE_REF_PTR,
    /* Language `time` keeps a distinct semantic/physical ABI even though
     * XRT currently represents xtime as a signed 64-bit microsecond count.
     * Keeping it separate prevents integer arithmetic/boxing from silently
     * erasing the runtime time kind; explicit scalar conversion families
     * describe the few source-language conversions that are legal. */
    XL6_IR_C_ABI_TIME_I64,
    /* Unique, compiler-owned synchronous generator handle.  The yielded item
     * TypeId is the sole parameter of generator<T>; frame layout, resume
     * program and close/drop glue live in XL6_IrGeneratorMachine. */
    XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR,
    /* Language `float32` preserves its IEEE single-precision value ABI.  It
     * is distinct from both the language F64 execution ABI and foreign
     * `c_float`; append it so every established CAbi numeric identity remains
     * stable while calls, storage and exports can carry the exact width. */
    XL6_IR_C_ABI_F32,
    /* Language `char` is one validated Unicode scalar.  Keep it distinct from
     * both integer arithmetic ABIs and every foreign C character type even
     * though its final carrier is an exact uint32_t. */
    XL6_IR_C_ABI_CHAR_U32,
    /* Language `bytes` is an owned XRT byte value.  The pointer-shaped carrier
     * deliberately remains distinct from an erased `var` xvalue: its runtime
     * tag, indexing element ABI and mutation rules are statically closed. */
    XL6_IR_C_ABI_MANAGED_BYTES
} XL6_IrCAbi;

/* Frontend-fixed ownership category.  Lowering must not rediscover whether a
 * type needs a destruction edge from its diagnostic spelling. */
typedef enum XL6_IrOwnershipAbi {
    XL6_IR_OWNERSHIP_ABI_INVALID = 0,
    XL6_IR_OWNERSHIP_ABI_TRIVIAL,
    XL6_IR_OWNERSHIP_ABI_MANAGED
} XL6_IrOwnershipAbi;

/* The null representation is a property of the canonical value ABI, not of
 * a source literal and not of Cgen.  Pointer-shaped values use either a C
 * NULL sentinel or XRT's distinguished xvalue null.  A nullable scalar needs
 * a tagged product; that representation is recorded even while its lowering
 * remains outside the currently supported backend subset. */
typedef enum XL6_IrNullAbi {
    XL6_IR_NULL_ABI_NONE = 0,
    XL6_IR_NULL_ABI_POINTER_SENTINEL,
    XL6_IR_NULL_ABI_XVALUE_SENTINEL,
    XL6_IR_NULL_ABI_TAGGED_VALUE
} XL6_IrNullAbi;

/* Persistence transforms operate on boxed XRT values, but their recursive
 * schema is still a frontend decision.  Every canonical TypeId is classified
 * once here so a backend never rediscovers class/nullable/collection meaning
 * from source spellings or from a convenient runtime representation. */
typedef enum XL6_IrPersistenceValueAbi {
    XL6_IR_PERSISTENCE_VALUE_NONE = 0,
    /* XRT already owns the complete value representation; clone it verbatim. */
    XL6_IR_PERSISTENCE_VALUE_XRT_CLONE,
    /* Project/reconstruct through the exact ClassLayout for this TypeId. */
    XL6_IR_PERSISTENCE_VALUE_CLASS_SCHEMA,
    /* Preserve the xvalue null sentinel, otherwise recurse into payloadType. */
    XL6_IR_PERSISTENCE_VALUE_NULLABLE,
    /* Preserve order and recurse element-wise into payloadType. */
    XL6_IR_PERSISTENCE_VALUE_ARRAY,
    /* Project the physical int-map list into a persistence sequence and
     * reconstruct contiguous integer keys while restoring. */
    XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE,
    /* Project stable set iteration into a persistence sequence and rebuild
     * set membership while restoring. */
    XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE,
    /* Preserve string keys while recursively projecting/restoring values. */
    XL6_IR_PERSISTENCE_VALUE_DICT_OBJECT
} XL6_IrPersistenceValueAbi;

/* Parameter ownership and mutability belong to the callable signature, not to
 * a backend's interpretation of a pointer-shaped C ABI.  INOUT/OUT are
 * physical storage-reference boundaries: their call operands must be explicit
 * STORAGE_ADDRESS values and never an rvalue which Cgen traces backwards. */
typedef enum XL6_IrParameterAbi {
    XL6_IR_PARAMETER_ABI_INVALID = 0,
    XL6_IR_PARAMETER_ABI_VALUE,
    XL6_IR_PARAMETER_ABI_BORROW,
    XL6_IR_PARAMETER_ABI_INOUT,
    XL6_IR_PARAMETER_ABI_OUT,
    /* The caller transfers one independent managed owner into the callee.
     * Rebinding affects only the callee slot; every terminal edge releases
     * the slot's current value.  This is the managed `byval` contract. */
    XL6_IR_PARAMETER_ABI_OWNED
} XL6_IrParameterAbi;

/* Source-language mode and physical call ABI are related, but they are not
 * the same identity.  In particular, DEFAULT and BYVAL can both use VALUE
 * for a trivial type while remaining distinct callable type spellings. */
typedef enum XL6_IrParameterMode {
    XL6_IR_PARAMETER_MODE_INVALID = 0,
    XL6_IR_PARAMETER_MODE_DEFAULT,
    XL6_IR_PARAMETER_MODE_BYVAL,
    XL6_IR_PARAMETER_MODE_BYREF,
    XL6_IR_PARAMETER_MODE_OUT
} XL6_IrParameterMode;

/* Canonical function-frame shape.  OPEN_ERASED is the source `function`
 * existential stored as an XRT callable; every other value describes an
 * exact compiler closure signature. */
typedef enum XL6_IrCallableErasureAbi {
    XL6_IR_CALLABLE_ERASURE_INVALID = 0,
    XL6_IR_CALLABLE_ERASURE_OPEN,
    XL6_IR_CALLABLE_ERASURE_FIXED,
    XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC,
    XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC,
    XL6_IR_CALLABLE_ERASURE_C_VARIADIC
} XL6_IrCallableErasureAbi;

/* Canonical integer width/signedness carried by builtin integer TypeIds and
 * explicit conversion operations.  INVALID means the type is not an integer
 * normalization domain. */
typedef enum XL6_IrIntegerTarget {
    XL6_IR_INTEGER_TARGET_INVALID = 0,
    XL6_IR_INTEGER_TARGET_SIGNED_8,
    XL6_IR_INTEGER_TARGET_UNSIGNED_8,
    XL6_IR_INTEGER_TARGET_SIGNED_16,
    XL6_IR_INTEGER_TARGET_UNSIGNED_16,
    XL6_IR_INTEGER_TARGET_SIGNED_32,
    XL6_IR_INTEGER_TARGET_UNSIGNED_32,
    XL6_IR_INTEGER_TARGET_SIGNED_64,
    XL6_IR_INTEGER_TARGET_UNSIGNED_64,
    /* Unicode scalar values are not an arbitrary uint32 domain: surrogate
     * code points and values above U+10FFFF are excluded.  Keep this as an
     * explicit checked-conversion target instead of approximating char as
     * uint32 and asking a backend to recover the language invariant. */
    XL6_IR_INTEGER_TARGET_UNICODE_SCALAR
} XL6_IrIntegerTarget;

/* Conservative effect of releasing one storage owner. Trivial storage,
 * strings and error objects cannot invoke user destruction. Erased values,
 * callable captures and continuation owners may do so even when their value
 * payload is scalar. Products join their element effects. */
typedef enum XL6_IrReleaseEffect {
    XL6_IR_RELEASE_EFFECT_INVALID = 0,
    XL6_IR_RELEASE_EFFECT_NO_RAISE,
    XL6_IR_RELEASE_EFFECT_MAY_RAISE
} XL6_IrReleaseEffect;

/* Closed native product profiles. Semantic types remain ordered tuple TypeIds;
 * this derived, verified representation fact only fixes the C value carrier.
 * NONE preserves compiler-private products, including managed elements. */
typedef enum XL6_IrNativeProductAbi {
    XL6_IR_NATIVE_PRODUCT_NONE = 0,
    XL6_IR_NATIVE_PRODUCT_I64X3_V1,
    XL6_IR_NATIVE_PRODUCT_TIMEX2_V1,
    XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1
} XL6_IrNativeProductAbi;

typedef struct XL6_IrType {
    XL6_IrTypeKind kind;
    XL6_IrCAbi cAbi;
    XL6_IrCollectionAbi collectionAbi;
    XL6_IrRuntimeResourceAbi runtimeResourceAbi;
    XL6_IrOwnershipAbi ownershipAbi;
    XL6_IrNullAbi nullAbi;
    XL6_IrPersistenceValueAbi persistenceValueAbi;
    /* Exact recursive edge for NULLABLE and typed collection ABIs; INVALID
     * for terminal ABIs. */
    XL6_IrTypeId persistencePayloadType;
    /* Exact integer storage-normalization domain; C ABI I64 alone is not
     * enough to distinguish int8/uint8/.../int64 semantics. */
    XL6_IrIntegerTarget integerTarget;
    XL6_IrNativeProductAbi nativeProductAbi;
    /* Diagnostics/provenance only, except for TYPE_PARAM identity.  Concrete
     * aliases with the same ABI and shape share one canonical XLIR type. */
    XL6_TypeId sourceTypeId;
    XL6_TypeAbiId abiId;
    /* Stable source-language `type` value identity.  This intentionally
     * follows the public type-value hash contract rather than abiId: abiId
     * identifies compiler/runtime representation families, while this value
     * preserves exact generic and callable identity observed by user code. */
    uint64 typeValueId;
    /* Canonical declaration identity for nominal types.  Receiver lowering
     * resolves through this symbol and never searches for a `self` spelling. */
    XL6_SymbolId nominalSymbol;
    /* Diagnostic spelling only; never part of XLIR type identity. */
    char* name;
    /* Canonical source-language spelling returned by `string(type(value))`.
     * Generic arguments are expanded here so Cgen never reconstructs a
     * user-visible type name from diagnostic or ABI fields. */
    char* displayName;
    /* Exact, verified backend spelling for nominal public and compiler-owned
     * structural ABIs. Empty for source/runtime types fixed by cAbi. */
    char cName[64];
    /* Canonical C result-carrier spelling for a MAY_RAISE callable returning
     * this type.  Empty means that no error-result ABI is admitted.  This is
     * a type-pool fact so Cgen never reconstructs carriers from cAbi. */
    char errorResultCName[48];
    XL6_IrTypeId* params;
    /* Present exactly for function parameters.  Generic type arguments use
     * params too, but are not callable boundaries and must leave this NULL. */
    XL6_IrParameterMode* parameterModes;
    XL6_IrParameterAbi* parameterAbis;
    size_t paramCount;
    XL6_IrTypeId* returns;
    size_t returnCount;
    /* Canonical physical result at a callable boundary.  Void uses INVALID,
     * one source result uses that TypeId, and multiple source results use the
     * matching tuple TypeId.  Backends consume this field and never collapse
     * or reconstruct `returns` themselves. */
    XL6_IrTypeId callableResultType;
    XL6_IrCallableErasureAbi callableErasureAbi;
    uint64 hash;
} XL6_IrType;

typedef struct XL6_IrTypePool {
    XL6_IrType* items;
    size_t count;
    size_t capacity;
    size_t* slots;
    size_t slotCapacity;
} XL6_IrTypePool;

XL6_IrNativeProductAbi xl6IrNativeProductAbiForShape(const XL6_IrTypePool* pool, const XL6_IrType* type);
int xl6IrDeclarationAllowsForeignContract(XL6_AstKind sourceKind, unsigned int flags);
const char* xl6IrNativeProductAbiCName(XL6_IrNativeProductAbi abi);
const char* xl6IrNativeProductAbiGuard(XL6_IrNativeProductAbi abi);

typedef enum XL6_IrNodeKind {
    XL6_IR_NODE_MODULE = 1,
    XL6_IR_NODE_LIST,
    XL6_IR_NODE_IMPORT,
    XL6_IR_NODE_FUNC_DECL,
    XL6_IR_NODE_TYPE_DECL,
    XL6_IR_NODE_ENUM_DECL,
    XL6_IR_NODE_ENUM_VARIANT,
    XL6_IR_NODE_FIELD_DECL,
    XL6_IR_NODE_PARAM_DECL,
    XL6_IR_NODE_VALUE_DECL,
    XL6_IR_NODE_BLOCK,
    XL6_IR_NODE_RETURN,
    XL6_IR_NODE_YIELD,
    XL6_IR_NODE_THROW,
    XL6_IR_NODE_ASSERT,
    XL6_IR_NODE_BREAK,
    XL6_IR_NODE_CONTINUE,
    XL6_IR_NODE_IF,
    XL6_IR_NODE_LOOP,
    XL6_IR_NODE_TRY,
    XL6_IR_NODE_MATCH,
    XL6_IR_NODE_MATCH_CASE,
    XL6_IR_NODE_PATTERN,
    XL6_IR_NODE_NAME,
    XL6_IR_NODE_LITERAL,
    XL6_IR_NODE_CALL,
    XL6_IR_NODE_MEMBER,
    XL6_IR_NODE_INDEX,
    XL6_IR_NODE_ASSIGN,
    XL6_IR_NODE_CONDITIONAL,
    XL6_IR_NODE_BINARY,
    XL6_IR_NODE_UNARY,
    XL6_IR_NODE_AWAIT,
    XL6_IR_NODE_COLLECTION,
    /* Typed interpolation container.  Its ordered children distinguish raw
     * text parts from already-resolved expressions; lowering must replace it
     * with explicit conversion/concat ownership instructions before Cgen. */
    XL6_IR_NODE_INTERPOLATED_STRING,
    XL6_IR_NODE_OTHER
} XL6_IrNodeKind;

typedef enum XL6_IrValueCategory {
    XL6_IR_VALUE_NONE = 0,
    XL6_IR_VALUE_RVALUE,
    XL6_IR_VALUE_LVALUE
} XL6_IrValueCategory;

typedef enum XL6_IrOwnership {
    XL6_IR_OWNERSHIP_NONE = 0,
    XL6_IR_OWNERSHIP_COPY,
    XL6_IR_OWNERSHIP_BORROW,
    XL6_IR_OWNERSHIP_MOVE,
    XL6_IR_OWNERSHIP_SHARED
} XL6_IrOwnership;

/* Lowered operand effects are independent of result ownership. OBSERVE may
 * copy/retain internally but never takes the supplied owner; CONSUME transfers
 * it on every completion edge. CONSUME_FIELD transfers only elementIndex of
 * a product; its other fields remain independently available. ALIAS preserves
 * identity, DETACH acquires the exact LOAD's storage owner. Unknown effects
 * fail closed. */
typedef enum XL6_IrOperandOwnership {
    XL6_IR_OPERAND_OWNERSHIP_INVALID = 0,
    XL6_IR_OPERAND_OWNERSHIP_OBSERVE,
    XL6_IR_OPERAND_OWNERSHIP_CONSUME,
    XL6_IR_OPERAND_OWNERSHIP_ALIAS,
    XL6_IR_OPERAND_OWNERSHIP_DETACH,
    XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD
} XL6_IrOperandOwnership;

typedef enum XL6_IrStage {
    XL6_IR_STAGE_NONE = 0,
    XL6_IR_STAGE_TYPED,
    XL6_IR_STAGE_LOWERED
} XL6_IrStage;

typedef enum XL6_IrInstructionKind {
    XL6_IR_INSTR_INVALID = 0,
    XL6_IR_INSTR_CONST,
    /* Constructs the exact null value selected by the result type's NullAbi.
     * It has no payload or operands, so Cgen cannot reinterpret a source
     * `null` spelling as an arbitrary C expression. */
    XL6_IR_INSTR_NULL,
    /* Tests one already-lowered value using its exact NullAbi and returns the
     * instruction's canonical boolean type. */
    XL6_IR_INSTR_NULL_TEST,
    XL6_IR_INSTR_LOAD,
    XL6_IR_INSTR_STORE,
    XL6_IR_INSTR_CALL,
    /* Invokes a verified class-dispatch slot.  targetSymbol is deliberately
     * unused: dispatchSlot is a one-based index into classDispatchSlots, so
     * no backend can reinterpret it as a statically selected source method. */
    XL6_IR_INSTR_VIRTUAL_CALL,
    /* Invokes a verified user-protocol requirement slot.  As with virtual
     * calls, dispatchSlot names a frontend-built table entry rather than a
     * declaration or spelling that a backend may reinterpret. */
    XL6_IR_INSTR_PROTOCOL_CALL,
    XL6_IR_INSTR_BINARY,
    XL6_IR_INSTR_UNARY,
    XL6_IR_INSTR_VALUE,
    /* Explicit non-owning observation of an otherwise unused value.  Trivial
     * values are always admissible; a managed value is admissible only when
     * it is a verified borrowed LOAD (or the existing catch-error observer).
     * DISCARD never releases or transfers an owner. */
    XL6_IR_INSTR_DISCARD,
    /* Produce one additional shared owner.  Nullable managed handles preserve
     * NULL without invoking a runtime retain operation. */
    XL6_IR_INSTR_RETAIN,
    /* Consumes storage ownership by detaching and zeroing the storage BEFORE
     * invoking destruction. Reentrant code observes an empty owner; a new
     * owner installed during destruction must not be cleared afterwards.
     * The same order applies to aggregate owners and continuation init bits. */
    XL6_IR_INSTR_RELEASE,
    XL6_IR_INSTR_MOVE,
    XL6_IR_INSTR_DROP,
    /* Constructs an owned closure from a verified XLIR closure target. */
    XL6_IR_INSTR_CLOSURE_MAKE,
    /* Invokes a closure value through its already fixed function signature.
     * Operand zero is the closure; the remaining operands are positional
     * arguments in the signature order.  Every first-class callable uses one
     * effect-erased C thunk ABI: bool success plus an optional out-result.
     * Consequently even a void invocation defines a carrier token consumed
     * by ERROR_TEST; the backend never guesses whether the stored target can
     * raise from its source declaration. */
    XL6_IR_INSTR_CLOSURE_CALL,
    /* Converts one already-evaluated trivial scalar to the instruction's
     * exact result type.  This represents both semantic C-scalar casts and
     * resolved scalar adaptation at a foreign call boundary; Cgen may not
     * infer a conversion from mismatching call operands. */
    XL6_IR_INSTR_SCALAR_CONVERT,
    /* Rebinds one raw foreign pointer to an exact nominal native-handle type.
     * The matching Typed XLIR NativeHandleBind fixes whether this is a borrow
     * or ownership take; any source-storage clear is a separate NULL+STORE. */
    XL6_IR_INSTR_NATIVE_HANDLE_BIND,
    /* Projects the language-visible native `point` field from an exact pure
     * native class receiver.  Both C representations are opaque pointers,
     * but the distinct TypeIds and non-owning direction are verifier-visible;
     * Cgen only emits the already validated pointer assignment. */
    XL6_IR_INSTR_NATIVE_HANDLE_PROJECT,
    /* Constructs one structural product from operands in canonical element
     * order. GET observes a trivial element. TAKE moves one managed element
     * out and invalidates that field; DROP destroys one managed field without
     * producing a value.  A verified managed product must transfer the whole
     * aggregate or consume every managed field exactly once. */
    XL6_IR_INSTR_PRODUCT_MAKE,
    XL6_IR_INSTR_PRODUCT_GET,
    XL6_IR_INSTR_PRODUCT_TAKE,
    XL6_IR_INSTR_PRODUCT_DROP,
    XL6_IR_INSTR_RUNTIME_CALL,
    /* Explicit fatal edge for a checked runtime operation whose failure
     * cannot be represented by the source function's return ABI yet. */
    XL6_IR_INSTR_TRAP_IF_FALSE,
    /* Raises the canonical XLIR error state.  immediateText is the quoted
     * error kind and the optional operand is an owned managed-string message.
     * ERROR_RAISE consumes that owner after the runtime error object has copied
     * the text; the following THROW owns control transfer and cleanup. */
    XL6_IR_INSTR_ERROR_RAISE,
    /* Tests the result carrier of a may-raise CALL and transfers to its
     * already-lowered success or propagation-cleanup edge. */
    XL6_IR_INSTR_ERROR_TEST,
    /* Takes the owned XRT error from the current context into an explicit
     * runtime-error value.  Catch storage then owns its lifetime. */
    XL6_IR_INSTR_ERROR_TAKE,
    /* Transfers an owned runtime-error value into the XRT error context.
     * The source storage must already have been moved/cleared, so a later
     * cleanup edge cannot double-free a rethrown payload. */
    XL6_IR_INSTR_ERROR_SET_TAKE,
    /* Clears an unobserved XRT error after a lowered catch entry has taken
     * the exceptional CFG edge.  Payload handlers use ERROR_TAKE instead. */
    XL6_IR_INSTR_ERROR_CLEAR,
    XL6_IR_INSTR_BRANCH,
    XL6_IR_INSTR_JUMP,
    XL6_IR_INSTR_SWITCH,
    /* An owned managed result commits from TEMPORARY storage via the
     * immediately preceding LOAD/MOVE pair. All lexical, iterator and
     * parameter cleanup precedes this pair; RETURN itself has no cleanup
     * effects. The same storage owns the pending value on failure edges. */
    XL6_IR_INSTR_RETURN,
    XL6_IR_INSTR_THROW,
    XL6_IR_INSTR_AWAIT,
    /* Starts an already-modelled async worker through its selected runtime
     * dispatcher.  This is deliberately distinct from CALL: its result is
     * the public future<T>, while the worker has its own result ABI. */
    XL6_IR_INSTR_ASYNC_START,
    /* Mutable lexical capture is deliberately represented as a separate
     * ownership protocol.  LOAD/STORE/RELEASE retain their ordinary storage
     * meaning; no backend may infer cell operations from a capture field. */
    /* Allocate an empty typed cell as an independent nullable owning result.
     * No element is consumed and target storage remains inactive. The result
     * must immediately feed ERROR_TEST, then CELL_INIT on its success edge. */
    XL6_IR_INSTR_CELL_NEW,
    XL6_IR_INSTR_CELL_LOAD,
    XL6_IR_INSTR_CELL_STORE,
    XL6_IR_INSTR_CELL_RETAIN,
    XL6_IR_INSTR_CELL_RELEASE,
    /* Nullable boundaries are explicit value operations.  WRAP turns the
     * exact element into a present tagged value or relabels an exact
     * same-representation pointer/xvalue sentinel; UNWRAP projects the
     * element only on CFG edges already proven non-null.  Neither operation
     * changes ownership. */
    XL6_IR_INSTR_NULLABLE_WRAP,
    XL6_IR_INSTR_NULLABLE_UNWRAP,
    /* Plain-union values are immutable SSA aggregates. ZERO materializes the
     * language default, GET projects one verified field and SET returns a copy
     * whose selected field is replaced. elementIndex is the exact module union
     * field index fixed by Typed XLIR. */
    XL6_IR_INSTR_UNION_ZERO,
    XL6_IR_INSTR_UNION_GET,
    XL6_IR_INSTR_UNION_SET,
    /* Produces a compiler-owned storage-ref value for one exact targetSymbol.
     * It never reads the slot, which is essential for definite-assignment of
     * OUT arguments.  The result type's sole parameter equals storage->type. */
    XL6_IR_INSTR_STORAGE_ADDRESS,
    /* Constructs a generator activation without executing its body.  The
     * resolved INTERNAL_GENERATOR target owns capture/frame semantics. */
    XL6_IR_INSTR_GENERATOR_START,
    /* Resumes one owned generator handle. The physical result carries both
     * ok and yielded/end; ERROR_TEST observes ok, independently of a nullable
     * diagnostic. The bool SSA result is valid only on the success edge. */
    XL6_IR_INSTR_GENERATOR_NEXT,
    /* Projects the current yielded value. GET is valid only for trivial item
     * ABIs; TAKE transfers the unique managed item owner out of the machine. */
    XL6_IR_INSTR_GENERATOR_ITEM_GET,
    XL6_IR_INSTR_GENERATOR_ITEM_TAKE,
    /* Generator-worker suspension boundary.  Its sole operand has the exact
     * generator item TypeId and is consumed according to generatorYieldAbi. */
    XL6_IR_INSTR_YIELD,
    /* Transfers one complete managed product SSA value into exact temporary
     * storage.  Unlike STORE this is an ownership sink; the matching later
     * LOAD+MOVE or RELEASE makes the destination lifetime explicit. */
    XL6_IR_INSTR_PRODUCT_STORE_MOVE,
    /* Consumes an owned foreign CALL result on its ERROR_TEST failure edge.
     * The sole operand is the physical raw result, type is its exact field
     * TypeId, and targetSymbol is the resolved native destructor. No wrapper
     * allocation is permitted. Detach before invoking the destructor, skip
     * NULL, isolate callback/cleanup failures and preserve the original failed
     * outcome (including a NULL diagnostic). This operation cannot raise.
     * The verifier proves the exclusive failure-edge rollback / success-edge
     * handle-TAKE partition; arbitrary raw pointers and borrowed results are
     * not rollback operands. A skipped foreign CALL initializes its result to
     * NULL, including when a loop reuses the physical SSA variable. */
    XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK,
    /* An exceptional cleanup region owns the already-failed outcome while
     * every crossed owner is released. BEGIN captures the original diagnostic
     * (which may be NULL) in a shared-runtime scope with failed=true. END
     * discards secondary cleanup errors and restores that exact owned error;
     * it never publishes past the language handler selected by the CFG.
     * errorScope identifies BEGIN by its one-based instruction index. Regions
     * cannot nest, suspend, loop, or cross a function/handler boundary. */
    XL6_IR_INSTR_ERROR_UNWIND_BEGIN,
    XL6_IR_INSTR_ERROR_UNWIND_END,
    /* Normal cleanup is a non-suspending control region. BEGIN isolates its
     * errors; END restores the diagnostic and defines a bool outcome consumed
     * only by its immediately following ERROR_TEST. All owners in the region
     * are released before the first failure is routed to language CFG. */
    XL6_IR_INSTR_CLEANUP_BEGIN,
    XL6_IR_INSTR_CLEANUP_END,
    /* Finalization inherits the incoming escaping outcome, including an
     * immediate parent scope's failed-with-NULL state. Shares CLEANUP_END's
     * exclusive bool outcome, but is only valid in a finalization program. */
    XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN,
    /* Accumulate an available native diagnostic after an individual cleanup
     * step, without inventing failure from a NULL pointer. Explicit failed-
     * with-NULL notifications are already latched by the shared scope. */
    XL6_IR_INSTR_CLEANUP_CAPTURE,
    /* Consume one explicitly failed cleanup step on its ERROR_TEST failure
     * edge. Latches failure even when no diagnostic could be allocated. */
    XL6_IR_INSTR_CLEANUP_FAIL,
    /* Publishes an independently owned payload into an existing cell and
     * returns its previous payload as an independent owner. No allocation,
     * destructor, callback or failure is hidden in this operation. Lowering
     * explicitly destroys the returned owner and routes any cleanup failure.
     * Publishing precedes destruction so reentry observes the new payload.
     * CELL_STORE is reserved for trivial payloads that need no destruction. */
    XL6_IR_INSTR_CELL_EXCHANGE,
    /* Atomically extracts the owned payload of an active non-cell lexical or
     * temporary slot. targetSymbol names that slot; there are no operands.
     * The exact-type result is a new independent owner. The slot and any
     * continuation init bit become empty before any later use. This operation
     * neither allocates nor destroys and cannot fail. Storage activation CFG
     * proves an owning installation before each take and no stale loan use.
     * LOAD remains a borrowed observation; ownership is never retroactively
     * changed by finding a later MOVE or by examining the source expression. */
    XL6_IR_INSTR_STORAGE_TAKE,
    /* Consume [fresh cell, independently owned initial element] and activate
     * targetSymbol. Installation does not allocate, destroy or fail. */
    XL6_IR_INSTR_CELL_INIT,
    /* Read the generator activation's explicit close request. This bool has
     * no operands or failure; its immediate BRANCH selects a verified close
     * exit route or normal continuation. Never a source-language exception. */
    XL6_IR_INSTR_GENERATOR_CLOSE_TEST,
    /* Consume one independently owned SSA value, detach its physical owner
     * before destruction, and invalidate every borrowed view. Unlike RELEASE,
     * this operation has no storage symbol. The exact type fixes destruction;
     * potentially raising destruction requires a checked cleanup scope. */
    XL6_IR_INSTR_VALUE_RELEASE
} XL6_IrInstructionKind;

typedef enum XL6_IrGeneratorClosePointAbi {
    XL6_IR_GENERATOR_CLOSE_POINT_INVALID = 0,
    XL6_IR_GENERATOR_CLOSE_POINT_YIELD_UNWIND,
    /* A paused finally must finish its body and preserve its pending outcome.
     * Cleanup-produced items are disposed by the closing consumer, not exposed
     * as additional source-loop iterations. */
    XL6_IR_GENERATOR_CLOSE_POINT_YIELD_DRAIN_FINALLY,
    XL6_IR_GENERATOR_CLOSE_POINT_AFTER_TRY,
    /* A loop exit leaving the active cleanup region must not resume ordinary
     * source control flow while the suspended activation is closing. */
    XL6_IR_GENERATOR_CLOSE_POINT_ABRUPT_UNWIND
} XL6_IrGeneratorClosePointAbi;

typedef enum XL6_IrGeneratorCloseAbi {
    XL6_IR_GENERATOR_CLOSE_INVALID = 0,
    XL6_IR_GENERATOR_CLOSE_UNWIND_DRAIN_FINALLY
} XL6_IrGeneratorCloseAbi;

typedef enum XL6_IrRuntimeOp {
    XL6_IR_RUNTIME_NONE = 0,
    /* Each operation has a fixed operand and result ABI validated by XLIR.
    * Cgen emits these contracts mechanically; it never recognizes strings
    * from source type spellings or binary operator text. */
    XL6_IR_RUNTIME_STRING_DUP,
    /* Materialize an owned duplicate of a borrowed managed string before a
     * mutable storage overwrite can release its previous value. NULL is the
     * failed-acquisition carrier; immediate ERROR_TEST is mandatory before
     * cleanup or any consumer. Nullable copying uses its independent status. */
    XL6_IR_RUNTIME_STRING_COPY,
    XL6_IR_RUNTIME_STRING_CONCAT,
    XL6_IR_RUNTIME_STRING_EQUAL,
    /* Lexicographic ordering is a language/runtime operation.  Distinct
     * predicates preserve the selected semantic operation in XLIR so Cgen
     * never reconstructs it from an AST opcode or an untyped mode value. */
    XL6_IR_RUNTIME_STRING_ORDER_LT,
    XL6_IR_RUNTIME_STRING_ORDER_LE,
    XL6_IR_RUNTIME_STRING_ORDER_GT,
    XL6_IR_RUNTIME_STRING_ORDER_GE,
    /* Language `string(value)` conversion produces a newly owned managed
     * string.  Scalar and dynamic operands remain separate operations so the
     * verifier fixes the complete source ABI before Cgen sees the instruction. */
    XL6_IR_RUNTIME_BOOL_TO_STRING,
    XL6_IR_RUNTIME_I64_TO_STRING,
    XL6_IR_RUNTIME_F64_TO_STRING,
    XL6_IR_RUNTIME_XVALUE_TO_STRING,
    /* Stable language type values are formatted through the module-owned
     * TypePool identity/name table.  The input is an exact TYPE_ID value and
     * the result is a newly owned managed string; Cgen never reconstructs a
     * source type expression or hashes a spelling at the call site. */
    XL6_IR_RUNTIME_TYPE_ID_TO_STRING,
    /* Compiler-derived class string conversion is a structural operation on
     * one exact ClassLayout. payloadType selects that layout and its explicit
     * @string(skip) field partition.  User-defined __toString__ bodies remain
     * ordinary CALL instructions selected by the ClassLayout contract. */
    XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING,
    /* Structural class equality is selected by the exact ClassLayout.  Both
     * operands borrow values of payloadType and the result is bool.  The
     * layout's @compare(skip) partition is the complete comparison plan. */
    XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL,
    /* Checked managed-string to F64 conversion.  This is a raising semantic
     * operation rather than a C scalar cast: lowering must immediately test
     * the runtime error channel and route failure through the verified exit
     * plan before either edge releases an anchored string rvalue. */
    XL6_IR_RUNTIME_STRING_TO_F64_CHECKED,
    /* Checked time conversions preserve the source ownership partition.  The
     * string and xvalue forms are separate runtime operations, both followed
     * immediately by ERROR_TEST.  Formatting returns one owned string. */
    XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED,
    XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED,
    XL6_IR_RUNTIME_TIME_TO_STRING,
    /* Checked integer conversion selected by semantic lowering.  The explicit
     * integerSourceAbi and integerTarget jointly determine the complete
     * conversion contract; Cgen must not recover the source ABI by tracing an
     * operand value definition.  Lowering must immediately place ERROR_TEST
     * and route failure through the planned exception cleanup. */
    XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED,
    /* Dynamic values are a separate runtime ABI, not pointer-shaped strings.
     * Their construction and retain/release semantics remain explicit in IR. */
    XL6_IR_RUNTIME_XVALUE_NULL,
    XL6_IR_RUNTIME_XVALUE_BOOL,
    XL6_IR_RUNTIME_XVALUE_I64,
    XL6_IR_RUNTIME_XVALUE_F64,
    /* Boxes a foreign C float/double without erasing its concrete semantic
     * TypeValueId.  runtimeIdentityType is the exact source C scalar type. */
    XL6_IR_RUNTIME_XVALUE_FOREIGN_FLOAT,
    XL6_IR_RUNTIME_XVALUE_TIME,
    /* Copies a managed string into a newly owned dynamic string value. */
    XL6_IR_RUNTIME_XVALUE_STRING_COPY,
    /* Borrows one exact managed Future<T> and returns an owned xvalue that
     * retains its own Future reference.  A null Future sentinel becomes the
     * canonical xvalue null sentinel; consumers such as task aggregates can
     * then diagnose the invalid source at their own semantic boundary.
     * Transfer/take remains a distinct future operation; collection
     * construction may therefore release an anchored Future rvalue after this
     * box has been appended. */
    XL6_IR_RUNTIME_XVALUE_FUTURE_REF,
    /* Transfers one unique pure-native handle into an owning xvalue handle.
     * targetSymbol names the verified extern destructor used by the static
     * XRT handle policy; a borrowed pointer bridge remains a separate op.
     * The registered native void destructor is an inherited shared cleanup
     * boundary: it records native diagnostics and explicit missing failures
     * before another contained owner can run its destructor.
     * Detach and clear the source BEFORE allocation or fallback destruction.
     * Allocation failure still consumes the old owner exactly once; a new
     * owner written by destructor reentry must never be cleared afterward. */
    XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE,
    XL6_IR_RUNTIME_XVALUE_RETAIN,
    /* Creates one owned runtime weak-value wrapper from a borrowed exact
     * nominal xvalue.  The result TypeId is weak<T>; this is not a generic
     * xvalue rebind and therefore cannot be inferred by lowering or Cgen. */
    XL6_IR_RUNTIME_XVALUE_WEAK_CREATE,
    /* Borrows an exact weak<T> wrapper and returns one owned nullable<T>
     * strong reference.  Expiration is the nullable sentinel, not an error. */
    XL6_IR_RUNTIME_XVALUE_WEAK_LOCK,
    /* Shallow and graph-deep copy are distinct ownership-producing runtime
     * operations.  Both borrow one xvalue and return a newly owned value of
     * the exact same XLIR type; class protocol selection is fixed in the
     * class layout before lowering chooses either operation. */
    XL6_IR_RUNTIME_XVALUE_CLONE,
    XL6_IR_RUNTIME_XVALUE_DEEP_CLONE,
    /* Consumes one owned erased xvalue, verifies only its runtime container
     * tag, and exposes the exact untyped array/list/set/dict result TypeId on
     * success.  collectionAbi carries that tag; lowering immediately emits
     * ERROR_TEST so a failed cast cannot enter user storage. */
    XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST,
    /* Imports one owned erased container into a concrete typed collection.
     * The operation consumes its source on success and failure, validates the
     * runtime container tag plus every element tag, and returns the same owned
     * handle rebound to the result TypeId only on success.  payloadType is the
     * exact element TypeId; lowering immediately observes failure through
     * ERROR_TEST before the value may reach storage. */
    XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND,
    /* Observes container cardinality and normalizes it to the XLIR i64 ABI. */
    XL6_IR_RUNTIME_XVALUE_COUNT,
    XL6_IR_RUNTIME_XVALUE_ARRAY_NEW,
    /* Returns a borrowed item from an xvalue array after resolving a signed
     * index.  A later owned boundary must materialize RETAIN explicitly. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64,
    /* A statically proven array<int> read consumes the same runtime array
     * handle and signed index, but normalizes its checked element ABI to XLIR
     * i64.  It is deliberately separate from the borrowed dynamic-item op. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER,
    /* A statically proven array<bool> read checks the runtime boolean tag and
     * materializes the canonical XLIR bool ABI. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL,
    /* A statically proven array<float> read checks the runtime float tag and
     * materializes the canonical XLIR f64 ABI. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64,
    /* A statically proven array<string> read copies the borrowed runtime
     * text into an owned managed string before it leaves the array boundary. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY,
    /* Consumes its second, owned xvalue operand on both success and failure. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW,
    /* Resolves a signed array index, then consumes its owned value operand on
     * both success and failure.  This is distinct from a generic dynamic
     * member/object write: lowering chooses the container operation. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW,
    /* A list is an integer-keyed XRT map, never an array with inferred holes. */
    XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW,
    /* Integer-keyed list reads are distinct from array indexing.  The first
     * form returns a borrowed erased item; the typed forms check and copy the
     * exact element ABI before leaving the runtime boundary. */
    XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64,
    XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER,
    XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL,
    XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64,
    XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY,
    /* Consumes the owned xvalue item on both success and failure. */
    XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW,
    XL6_IR_RUNTIME_XVALUE_SET_NEW,
    /* Consumes the owned xvalue item on both success and failure. */
    XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW,
    /* Borrows one erased set and one already boxed item.  The frontend has
     * validated the set element type before lowering selects this operation;
     * Cgen observes only the fixed xvalue/xvalue -> bool runtime ABI. */
    XL6_IR_RUNTIME_XVALUE_SET_HAS,
    /* Removes one index/key/element from the explicitly recorded collection
     * ABI.  Operand 1 is i64 for array/list, xvalue for set, and managed
     * string for dict; the verifier rejects every other shape. */
    XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE,
    /* Removes the first equal element from one statically verified array.
     * Both operands are borrowed xvalues and the result is canonical bool. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE,
    /* Closed collection-algebra operations.  Each identity fixes the exact
     * language operation after Typed XLIR has combined a canonical opcode with
     * exact collection TypeIds.  Both operands are borrowed.  Value-producing
     * forms return a newly owned collection; relation forms return bool. */
    XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT,
    XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT,
    XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY,
    XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT,
    XL6_IR_RUNTIME_XVALUE_DICT_WITHOUT_STRING_KEY,
    XL6_IR_RUNTIME_XVALUE_SET_UNION,
    XL6_IR_RUNTIME_XVALUE_SET_INTERSECTION,
    XL6_IR_RUNTIME_XVALUE_SET_DIFFERENCE,
    XL6_IR_RUNTIME_XVALUE_SET_SYMMETRIC_DIFFERENCE,
    XL6_IR_RUNTIME_XVALUE_SET_SUBSET,
    XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUBSET,
    XL6_IR_RUNTIME_XVALUE_SET_SUPERSET,
    XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUPERSET,
    /* Borrows a statically typed collection receiver.  typedCollectionOp,
     * collectionAbi and the instruction TypeId form the complete result and
     * operand contract; no source member spelling reaches Cgen. */
    XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD,
    /* Borrows operand 0 (the open xvalue receiver), consumes every remaining
     * boxed xvalue operand on success and failure, and returns one owned
     * xvalue.  dynamicCollectionOp fixes the exact language operation and its
     * arity; runtime tag dispatch is the only dynamic decision left. */
    XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD,
    XL6_IR_RUNTIME_XVALUE_OBJECT_NEW,
    /* Constructs class storage with declaration-order field access and
     * reverse-declaration-order owned-field destruction. */
    XL6_IR_RUNTIME_XVALUE_CLASS_NEW,
    /* The key is a canonical C string immediate owned by XLIR; consumes the
     * owned item operand on both success and failure. */
    XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW,
    /* Reads a literal-key object item as a borrowed xvalue.  A missing key is
     * the borrowed xvalue null singleton, never a C NULL result. */
    XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL,
    /* Reads an object item through an already lowered managed-string key.
     * This remains distinct from the literal-key form so Cgen never needs to
     * inspect source syntax or infer a key representation. */
    XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING,
    /* Writes an owned item through an already lowered managed-string key.
     * The runtime consumes the item on both success and failure. */
    XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW,
    /* Reads a literal class/object field and checks its exact scalar ABI
     * before materializing the canonical XLIR i64 result.  This is separate
     * from dynamic-object access: Cgen receives neither a member name nor an
     * implicit unbox rule to recover. */
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64,
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY,
      /* Typed class function fields are stored through the runtime callable
       * bridge, but remain first-class XLIR closures at every typed use.  The
       * complete signature is carried by calleeType; neither operation asks
       * Cgen to recover a source member type. GET returns one owned closure
       * (or the empty NULL closure for an uninitialized field); SET borrows
       * a closure and stores its own reference, including that empty state.
       * Empty default function storage is not a source nullable-type test. */
      XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE,
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE,
    /* Materializes a non-owning dynamic pointer value from a pure native class
     * handle. Native destruction remains with its typed owner. */
    XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW,
    /* Observes a borrowed xerror message and materializes an owned string.
     * The catch storage retains sole ownership of the error object. */
     XL6_IR_RUNTIME_ERROR_MESSAGE_COPY,
     /* Observes the scalar code of borrowed catch error storage.  The result
      * is normalized to XLIR's signed i64 scalar ABI; no ownership moves. */
     XL6_IR_RUNTIME_ERROR_CODE,
     /* These borrowed XRT text fields are copied into separate owned strings.
      * They remain distinct so Cgen never infers a getter from a member name. */
     XL6_IR_RUNTIME_ERROR_DOMAIN_COPY,
     XL6_IR_RUNTIME_ERROR_OPERATION_COPY,
     XL6_IR_RUNTIME_ERROR_DATA_COPY,
     XL6_IR_RUNTIME_ERROR_SYSTEM_CODE,
     /* Reads borrowed XRT cause storage then retains it as an owned XLIR
      * error value.  The returned null is a valid owned-null error pointer. */
     XL6_IR_RUNTIME_ERROR_CAUSE_REF,
     /* Materializes the language-level kind name selected by the runtime
      * error bridge into an owned string. */
     XL6_IR_RUNTIME_ERROR_KIND_COPY,
     /* Tests a borrowed runtime error against an XLIR-owned canonical kind
      * token and returns bool for an explicit handler CFG branch. */
     XL6_IR_RUNTIME_ERROR_KIND_IS,
      /* Borrows one owned-or-null language error pointer and creates an
       * independently retained xvalue handle.  runtimeIdentityType fixes the
       * concrete `error` TypeValueId and the handle owns one xerror ref. */
      XL6_IR_RUNTIME_XVALUE_ERROR_REF,
      /* Dynamic class membership compares an xvalue's XLIR-owned concrete
       * runtime tag with a target class layout tag fixed during lowering. */
      XL6_IR_RUNTIME_XVALUE_CLASS_IS,
      /* Checked downcast: success returns an owned retain with the target
       * concrete class type; failure is an explicit runtime trap. */
      XL6_IR_RUNTIME_XVALUE_CLASS_AS,
      /* Borrows one typed XLIR closure and returns an owned runtime callable
       * xvalue.  calleeType fixes the complete closure signature; lowering
       * immediately follows the operation with ERROR_TEST. */
      XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE,
      /* Borrows operand zero as a runtime callable and consumes every remaining
       * owned xvalue argument on both success and failure.  The owned dynamic
       * result is the explicit success carrier consumed by ERROR_TEST.  An XRT
       * callable-entry wrapper projects its owned cause back into the XLIR
       * error slot, preserving the source callable's error kind and payload. */
      XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL,
      /* Borrows one erased receiver and transfers positional/keyword xvalues
       * according to the linked verified DynamicCallSite. METHOD invokes a
       * method; MEMBER_GET invokes a getter or retains a stored field. The
       * site kind distinguishes these identities, both with an owned success
       * result and an immediate checked error outcome. */
      XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL,
      /* Borrows one exact future<T>/task<T> handle and requests cooperative
       * cancellation.  The boolean result reports whether the pending
       * producer accepted the request; the Future ownership is unchanged. */
      XL6_IR_RUNTIME_FUTURE_CANCEL,
      /* Language-level Future observation keeps the operation identity and
       * millisecond time ABI explicit through Lowered XLIR.  Every operation
       * borrows operand zero as Future<T>; timeout/deadline operations take one
       * additional I64 value and every operation returns bool. */
      XL6_IR_RUNTIME_FUTURE_WAIT,
      XL6_IR_RUNTIME_FUTURE_WAIT_TIMEOUT,
      XL6_IR_RUNTIME_FUTURE_WAIT_UNTIL,
      XL6_IR_RUNTIME_FUTURE_IS_DONE,
      XL6_IR_RUNTIME_FUTURE_IS_PENDING,
      XL6_IR_RUNTIME_FUTURE_IS_CANCELLED,
      XL6_IR_RUNTIME_FUTURE_IS_FAILED,
      /* Exact non-boolean Future inspection.  STATE exposes the stable runtime
       * state number, STATUS exposes the language compatibility projection and
       * ERROR_REF returns an owned retain of the structured failure or null. */
      XL6_IR_RUNTIME_FUTURE_STATE,
      XL6_IR_RUNTIME_FUTURE_STATUS,
      XL6_IR_RUNTIME_FUTURE_ERROR_REF,
      /* Typed task aggregation receives a flattened, non-empty list of
       * borrowed Future<payload> operands.  payloadType and asyncResultAbi
       * fix the source completion representation before C emission.  ALL
       * produces Future<array<payload>> through the future bridge; RACE
       * preserves the winning Future<payload> result without consuming any
       * source handle.  WHEN_ANY_INDEX accepts heterogeneous Future payloads
       * and produces the winning zero-based index as Future<I64>. */
      XL6_IR_RUNTIME_FUTURE_ALL_XVALUES,
      XL6_IR_RUNTIME_FUTURE_RACE,
      /* Named/expression array variants borrow one exact xvalue-backed
       * array<Future<T>>. Their payload and public Future result remain
       * explicit, so Cgen never confuses a container with a flattened list. */
      XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY,
      XL6_IR_RUNTIME_FUTURE_RACE_ARRAY,
      XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX,
      /* Borrows one exact xvalue-backed array<Future<T>> and produces the
       * winning zero-based index as Future<I64>. The container representation
       * is fixed here rather than rediscovered by Cgen. */
      XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY,
      /* Borrows one exact typed closure plus its canonical physical argument
       * slice and returns an owned Future<payload>. calleeType fixes the
       * callable signature; payloadType/asyncResultAbi fix worker result
       * storage.  Runtime frame capture is therefore mechanical and never
       * reconstructed from task.spawn syntax by Cgen. */
      XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE,
      /* Registers one verified language closure as a Future continuation.
       * futureContinuationKind selects then/catch/finally; sourcePayloadType
       * and sourceAsyncResultAbi describe the borrowed input result while
       * payloadType and asyncResultAbi describe the owned output Future.
       * Cgen receives no member spelling and never reconstructs a callback
       * signature from the producer or consumer expression. */
      XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE,
      /* Borrows one canonical language u64 and returns an owned string. */
      XL6_IR_RUNTIME_U64_TO_STRING,
      /* Append-only U64 dynamic bridges.  These remain separate from the
       * signed integer operations so neither the verifier nor Cgen can infer
       * signedness from a source type spelling or an XRT runtime tag. */
      XL6_IR_RUNTIME_XVALUE_U64,
      XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64,
      XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64,
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64,
      /* Exact typed class-field projections.  Each operation borrows the
       * xvalue class storage, validates one literal-key field tag and returns
       * the already-selected physical ABI.  Pointer/handle results remain
       * non-owning views of storage owned by the enclosing class object. */
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL,
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64,
       XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER,
       XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW,
       XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW,
       /* Consumes the resource stored in one exact native-handle field while
        * leaving the shared managed object and field wrapper valid but empty.
        * Explicit native destroy lowering uses this before its physical C call. */
       XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE,
       /* Borrows one language raw pointer and materializes an owned xvalue
        * pointer tag.  This is distinct from a pure-native class handle bridge
        * even though both render through xrtValuePointer at the C boundary. */
       XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW,
       /* Direct dynamic arithmetic is a runtime semantic operation, never a
       * permissive scalar BINARY.  Separate identities keep result promotion
       * and the supported operator partition verifier-visible. */
      XL6_IR_RUNTIME_XVALUE_BINARY_ADD,
      XL6_IR_RUNTIME_XVALUE_BINARY_SUB,
      XL6_IR_RUNTIME_XVALUE_BINARY_MUL,
      XL6_IR_RUNTIME_XVALUE_BINARY_DIV,
      XL6_IR_RUNTIME_XVALUE_BINARY_MOD,
      XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND,
      XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR,
      XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR,
      XL6_IR_RUNTIME_XVALUE_BINARY_SHL,
      XL6_IR_RUNTIME_XVALUE_BINARY_SHR,
      /* Dynamic unary arithmetic returns a fresh owned xvalue.  Separate
       * identities keep numeric plus, numeric negation and integer bit-not
       * explicit after source syntax has been discarded. */
      XL6_IR_RUNTIME_XVALUE_UNARY_POS,
      XL6_IR_RUNTIME_XVALUE_UNARY_NEG,
      XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT,
      /* Approximate equality and erased comparisons are language semantic
       * operations, not C operators.  The scalar variants fix the numeric
       * algorithm selected by lowering; F64 accepts already-resolved numeric
       * scalar operands and converts them to double at the runtime boundary.
       * Every xvalue comparison borrows two boxed operands and returns bool.
       * Separate ordering identities prevent Cgen from recovering an
       * operator mode from source syntax or an unverified immediate. */
      XL6_IR_RUNTIME_I64_APPROX,
      XL6_IR_RUNTIME_U64_APPROX,
      XL6_IR_RUNTIME_F64_APPROX,
      XL6_IR_RUNTIME_STRING_APPROX,
      XL6_IR_RUNTIME_XVALUE_EQUAL,
      XL6_IR_RUNTIME_XVALUE_ORDER_LT,
      XL6_IR_RUNTIME_XVALUE_ORDER_LE,
      XL6_IR_RUNTIME_XVALUE_ORDER_GT,
      XL6_IR_RUNTIME_XVALUE_ORDER_GE,
      XL6_IR_RUNTIME_XVALUE_APPROX,
      /* Borrows one erased value and applies the language truth predicate.
       * Short-circuit CFG consumes only this bool result; it never branches
       * directly on an xvalue pointer. */
      XL6_IR_RUNTIME_XVALUE_TRUTHY,
      /* A tagged nullable scalar crosses erased class/dynamic storage only
       * through these exact TypeId operations.  BOX maps absent to xvalue
       * null and present to the scalar runtime value.  GET reconstructs the
       * same tagged carrier and traps when a present field has the wrong
       * runtime scalar tag. */
      XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX,
      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE,
      /* Borrows one erased collection and one erased key.  Runtime i64 keys
       * address arrays/integer maps; runtime strings address objects.  A
       * missing item, unsupported key tag or incompatible receiver is the
       * borrowed xvalue null singleton.  The result is never a C NULL and is
       * retained only by an explicit later ownership boundary. */
      XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY,
      /* Borrows stable erased collection/key storage and consumes one owned
       * item on every path.  Runtime i64 keys select array/integer-map
       * mutation; runtime strings select object mutation.  The bool result is
       * followed by an explicit trap/error edge chosen by lowering. */
      XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW,
      /* Borrows one erased container and one already boxed item/key.  Runtime
       * tags select array/integer-map value search, set membership, object key
       * membership, or string substring search.  Incompatible tag pairs are
       * a deterministic false result; neither operand is consumed. */
      XL6_IR_RUNTIME_XVALUE_CONTAINS,
       /* Borrows one managed string and returns its UTF-8 code-point count as
        * canonical i64.  This is the language `string.length` semantic ABI. */
        XL6_IR_RUNTIME_STRING_UTF8_COUNT,
       /* Borrows one managed string plus one canonical i64 code-point index
        * and returns a newly owned managed string containing exactly that
        * Unicode scalar.  Bounds are established by the loop CFG before this
        * operation; Cgen never reconstructs byte offsets or iteration shape. */
       XL6_IR_RUNTIME_STRING_UTF8_AT,
      /* Borrows a managed-string haystack and managed-string needle in that
       * order and returns bool.  This remains separate from erased xvalue
       * membership so no runtime tag or source operator is inferred by Cgen. */
       XL6_IR_RUNTIME_STRING_CONTAINS,
      /* Consumes no ownership: item/start/stop/step are four canonical i64
       * scalar values and the result is bool.  Lowering obtains the descriptor
       * from a verified RANGE intrinsic and materializes omitted bounds. */
      XL6_IR_RUNTIME_I64_RANGE_CONTAINS,
      /* Stable xvalue-container traversal.  CREATE borrows a statically shaped
       * source while taking its own backing snapshot. CREATE_CHECKED consumes
       * a borrowed physical `var` carrier plus payloadType, the Typed
       * expression's admitted logical collection TypeId (or the same `var`
       * TypeId for a fully open loop).  It validates that exact collection
       * family when one is known and establishes a TypeError on mismatch; the
       * result is immediately observed by ERROR_TEST. ADVANCE mutates only the
       * cursor and reports item/end as bool. Destroy is represented by RELEASE
       * of the compiler-owned VALUE_ITERATOR type, never inferred by Cgen. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64,
      /* Array cursors expose a zero-based size_t ordinal (runtime key tag 1),
       * while list cursors expose a signed integer key (tag 2).  Keep those
       * physical contracts distinct even though both lower to language i64. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64,
       /* Borrows the cursor's current xvalue item and returns one independent,
        * owned managed-string copy.  Its per-iteration target lifetime is a
        * separate Typed LoopTargetLifetimeAbi, not a Cgen convention. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY,
      /* Borrows the cursor's current managed xvalue item and returns one
       * independently retained owner with the collection element TypeId. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN,
      /* Copies the current object/dict key view into independent managed
       * string ownership.  The borrowed runtime key never escapes ADVANCE. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY,
      /* Open dynamic iteration has runtime-dependent key/value layout.  A
       * two-target loop boxes array/list/set ordinals or dict string keys into
       * one owned dynamic xvalue.  A one-target loop returns the dict key but
       * otherwise retains the current item.  These are separate closed ops so
       * Cgen never rediscovers source container semantics. Key boxing can fail
       * even for an integer key. Both ops use NULL_RESULT/MAY_RAISE and an
       * immediate ERROR_TEST; the target's construction failure plan owns the
       * already-initialized iteration prefix and its cursor. */
      XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE,
      XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE,
        /* Runtime type identity is an explicit XLIR contract.  BIND consumes
         * one unbound owned xvalue; REBIND_OWNED consumes one uniquely owned
         * wrapper after a verified representation-preserving conversion.
         * Both return that owned handle with runtimeIdentityType attached.
         * TYPE_ID borrows one xvalue and returns the canonical TYPE_ID selected
         * by immutable XRT metadata or the module runtimeTypeMap. */
        XL6_IR_RUNTIME_XVALUE_TYPE_BIND,
        /* Consumes and returns one owned exact class xvalue after installing
         * the ClassLayout's verified reference or structural hash/equality
         * callbacks.  It is emitted only for a complete identity pair. */
        XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND,
        /* Consumes one fully initialized owned class object on BOTH outcomes.
         * Success returns it with the exact derived-to-base destroy plan;
         * failure releases that owner and returns NULL, preserving the binding
         * error across rollback. MAY_RAISE/NULL_RESULT requires an immediate
         * ERROR_TEST. The verified finalizer CFG runs before field release. */
        XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT,
        XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED,
        XL6_IR_RUNTIME_XVALUE_TYPE_ID,
       /* Non-raising integer-to-integer conversion with language modulo/bit
        * truncation semantics.  Source representation and target width are
        * explicit instruction fields; this is never a checked conversion. */
       XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING,
       /* Typed nullable projection is deliberately orthogonal to collection
        * mechanics.  ITERATOR_ITEM_BORROW exposes only the current canonical
        * dynamic xvalue; the three PROJECT operations then reconstruct one
        * exact nullable TypeId.  TAGGED copies an explicit presence/product
        * value and carries its scalar element TypeId in payloadType.  STRING
        * preserves the pointer-null sentinel while copying a present managed
        * string.  CLOSURE preserves the pointer-null sentinel while building
        * an independent closure whose exact signature is calleeType. */
       XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW,
       XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY,
       XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY,
       XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY,
       /* Borrows one canonical callable xvalue and returns an independent,
        * non-null closure of the exact calleeType signature.  Unlike the
        * nullable projection above, a null runtime value violates the typed
       * container invariant instead of becoming a valid result. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY,
       /* Borrows one canonical xvalue pointer carrier whose immutable runtime
        * identity is the exact pure-native class TypeId, then projects its
        * non-owning native handle.  runtimeIdentityType fixes the nominal
        * class; the result never transfers or synthesizes native ownership. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW,
       /* The inverse erased boundary.  Both operations borrow an exact
        * pointer-sentinel nullable and return one owned canonical xvalue:
        * absent becomes a retained xvalue-null, while present string/closure
        * payloads are copied using the verified physical/signature ABI. */
       XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX,
       XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX,
       /* Borrows an exact pointer-sentinel nullable string and returns one
        * owned value of the same TypeId.  Absent remains C NULL; present is a
        * distinct managed-string allocation. */
       XL6_IR_RUNTIME_STRING_NULLABLE_COPY,
       /* Safe integer indexing is distinct from direct indexing.  Both
        * operations borrow the collection and key, but they normalize a
        * missing/null receiver result to the process-lifetime xvalue-null
        * singleton.  The result therefore still satisfies the canonical
        * dynamic-xvalue TypeId instead of leaking a C NULL through XLIR. */
       XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL,
       XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL,
       /* Direct literal-key reads from an exact typed dict are not class-field
        * projections.  The receiver's dict payload TypeId must equal the
        * result TypeId, while the operation identity fixes both the physical
        * scalar/string ABI and the owned-copy rule for strings.  Keeping this
        * family distinct prevents Cgen from recovering container semantics
        * from a generic object helper or from the source member spelling. */
       XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64,
       XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_U64,
       XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_BOOL,
       XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_F64,
       XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY,
       /* Checked scalar integer arithmetic is an explicit raising operation.
        * integerTarget fixes signedness and language width; both operands and
        * the result have the exact same integer TypeId.  Lowering immediately
        * emits ERROR_TEST, so Cgen never decides whether `/`, `%` or a shift
        * needs a zero/overflow/range check from source syntax or a C type. */
       XL6_IR_RUNTIME_INTEGER_DIV_CHECKED,
       XL6_IR_RUNTIME_INTEGER_MOD_CHECKED,
       XL6_IR_RUNTIME_INTEGER_SHL_CHECKED,
       XL6_IR_RUNTIME_INTEGER_SHR_CHECKED,
       /* Language integer power is not a C binary operator.  This first
        * admitted partition is exact signed language i64: both operands and
        * the result share one TypeId, negative exponents deterministically
        * produce zero, and the operation does not use the error channel. */
      XL6_IR_RUNTIME_I64_POW,
      /* Time approximate equality is a fixed runtime predicate over exact
       * language time values.  The tolerance belongs to this semantic
       * identity; Cgen does not recover it from the source operator. */
      XL6_IR_RUNTIME_TIME_APPROX,
      /* Frontend builtin float constants are semantic values, not arbitrary
       * C expressions.  Keep NaN and positive infinity as closed zero-operand
       * identities so their target spelling is selected only by Cgen. */
      XL6_IR_RUNTIME_F64_NAN,
      XL6_IR_RUNTIME_F64_POSITIVE_INFINITY,
      /* Borrows an exact class xvalue plus one canonical bool pretty flag and
       * returns an owned managed JSON string.  payloadType names the verified
       * class persistence schema consumed mechanically by Cgen. */
       XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY,
       /* Borrows one managed JSON string and one fully initialized instance of
        * payloadType.  The verified schema overlays present serialized fields
        * onto that default instance and returns an owned retained class value. */
       XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE,
       /* XLON uses the same verified class-schema/default-instance contract as
        * JSON, but remains a distinct RuntimeOp so Cgen selects the wire format
        * mechanically instead of recovering it from a callee name. */
       XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY,
       XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE,
       /* File persistence remains distinct from text conversion.  WRITE
        * borrows path/class/pretty and returns bool; READ borrows path plus an
        * explicitly lowered default instance and returns an owned class. */
       XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE,
       XL6_IR_RUNTIME_PERSISTENCE_JSON_READ,
       XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE,
       XL6_IR_RUNTIME_PERSISTENCE_XLON_READ,
       /* Copies one verified trivial union value into an owned runtime bytes
        * value and binds the union's stable TypeId. payloadType is the exact
        * XL6_IR_C_ABI_UNION source type, so neither layout nor identity is
        * recovered by Cgen from a source spelling. */
       XL6_IR_RUNTIME_XVALUE_UNION_BOX,
       /* Borrows one runtime bytes value, verifies the exact stable union
        * TypeId and byte width, then copies into a verified trivial union
        * result. payloadType is identical to the result type. */
       XL6_IR_RUNTIME_XVALUE_UNION_UNBOX,
       /* Borrows one verified class object field whose declared physical ABI
        * is language time and returns its exact signed-microsecond value.
        * The runtime tag check is distinct from integer field projection. */
       XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME,
       /* Safe `.length` is a closed semantic operation over an erased XRT
        * value, not a dynamic object-field lookup.  The tagged form borrows a
        * statically shaped collection and returns nullable<int>; the open form
        * borrows `var` and returns one owned null-or-int xvalue. */
       XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE,
       XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL,
       /* Source location is part of the immutable runtime error ABI.  Text
        * projection returns an owned managed string; scalar projection widens
        * XRT's int32 location to the language i64 representation. */
       XL6_IR_RUNTIME_ERROR_FILE_COPY,
       XL6_IR_RUNTIME_ERROR_LINE,
       XL6_IR_RUNTIME_ERROR_COLUMN,
       /* Safe collection lookup borrows a null-or-present canonical xvalue.
        * UNION copies a verified trivial union payload into the exact tagged
        * nullable result and carries that union TypeId in payloadType. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY,
       /* Borrows a canonical null-or-value xvalue and returns one retained
        * owner. payloadType fixes the static nullable element. Concrete class
        * payloads additionally require runtimeIdentityType == payloadType;
        * verified polymorphic payloads leave runtimeIdentityType invalid. This
        * preserves presence for a native class whose handle may itself be NULL
        * without rejecting a protocol value implemented by a concrete class. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN,
       /* Non-null counterpart for an xvalue-represented nominal result.
        * payloadType is the exact static result type; concrete classes also
        * repeat it in runtimeIdentityType so Cgen mechanically checks the
       * stable TypeId before retaining the borrowed carrier. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN,
       /* Borrows one canonical non-null xvalue and copies one verified
        * trivial scalar payload. payloadType equals the result TypeId and
        * fixes the exact bool/char/i64/u64/f64/time physical getter; a missing or
        * mismatched runtime value is an invariant violation, not a dynamic
        * language conversion or a recoverable error edge. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY,
       /* Borrows one canonical pointer-tag xvalue and projects the exact
        * pointer-sentinel static result recorded in payloadType.  The pointer
        * value is non-owning and may itself be C NULL; a non-pointer runtime
        * carrier is an invariant violation.  Generic point parameters remain
        * a static XLIR fact because XRT's raw-pointer tag intentionally carries
        * no nominal runtime identity. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW,
       /* Borrows one canonical non-null string-tag xvalue and returns an
        * independently owned managed-string copy. payloadType equals the
        * exact static string result TypeId; null or another runtime tag is an
        * invariant violation after a verified type test or typed lookup. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY,
       /* Future metadata is maintained by the language runtime bridge rather
        * than XRT's completion payload.  These append-only identities keep
        * mutation, borrowed observation, owned text projection and signed
        * language counters distinct through lowering and C generation. */
       XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME,
       XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY,
       XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS,
       XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS,
       XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS,
       /* Recoverable source-language `as` over one borrowed open xvalue.
        * castAbi fixes the exact runtime identity, physical result getter and
        * ownership rule; payloadType is the target TypeId.  Failure sets the
        * language error channel and must be followed immediately by
        * ERROR_TEST. */
       XL6_IR_RUNTIME_XVALUE_CAST_CHECKED,
       /* Recoverable dynamic `as T?` families.  A runtime type mismatch is a
        * successful absent nullable, never a TypeError.  TAGGED materializes
        * an exact scalar payload without allocation; STRING copies a present
        * managed string and may report allocation failure.  payloadType is
        * the non-null element TypeId and castAbi fixes the physical family. */
       XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED,
       XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING,
       /* Nominal tests/casts consume one precomputed identity set keyed by
        * payloadType.  TEST returns bool, CHECKED returns an owned retain and
        * reports TypeError, NULLABLE returns an owned retain or null without
        * raising.  Cgen must never rebuild inheritance or protocol witnesses. */
       XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST,
       XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED,
       XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE,
       /* Owning xvalue-backed thread resources.  CREATE borrows an exact
        * func() as int closure and may raise; every member borrows the verified
        * thread handle.  The result/operand contracts are closed by the
        * verifier before Cgen selects XRT helpers. */
       XL6_IR_RUNTIME_THREAD_CREATE,
       XL6_IR_RUNTIME_THREAD_WAIT,
       XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT,
       XL6_IR_RUNTIME_THREAD_EXIT_CODE,
       XL6_IR_RUNTIME_THREAD_STATE,
       XL6_IR_RUNTIME_THREAD_STOP,
       /* Generic owning channel resources.  Every operation repeats the exact
        * channel TypeId, payload TypeId and transfer ABI on the instruction;
        * receive targets are explicit STORAGE_ADDRESS values. */
       XL6_IR_RUNTIME_CHANNEL_CREATE,
       XL6_IR_RUNTIME_CHANNEL_SEND,
       XL6_IR_RUNTIME_CHANNEL_TRY_SEND,
       XL6_IR_RUNTIME_CHANNEL_RECV_INTO,
       XL6_IR_RUNTIME_CHANNEL_TRY_RECV_INTO,
       XL6_IR_RUNTIME_CHANNEL_RECV_TIMEOUT_INTO,
       XL6_IR_RUNTIME_CHANNEL_CLOSE,
       XL6_IR_RUNTIME_CHANNEL_IS_CLOSED,
       XL6_IR_RUNTIME_CHANNEL_COUNT,
       /* Owning synchronization resources.  Constructors produce one exact
        * resource TypeId; members borrow only operands whose resource family
        * is fixed by Typed XLIR.  Conditional waits carry an independently
        * verified borrowed mutex instead of recovering its ABI in Cgen. */
       XL6_IR_RUNTIME_MUTEX_CREATE,
       XL6_IR_RUNTIME_MUTEX_LOCK,
       XL6_IR_RUNTIME_MUTEX_TRY_LOCK,
       XL6_IR_RUNTIME_MUTEX_UNLOCK,
       XL6_IR_RUNTIME_SEM_CREATE,
       XL6_IR_RUNTIME_SEM_WAIT,
       XL6_IR_RUNTIME_SEM_TRY_WAIT,
       XL6_IR_RUNTIME_SEM_WAIT_TIMEOUT,
       XL6_IR_RUNTIME_SEM_POST,
       XL6_IR_RUNTIME_SEM_POST_MULTIPLE,
       XL6_IR_RUNTIME_COND_CREATE,
       XL6_IR_RUNTIME_COND_WAIT,
       XL6_IR_RUNTIME_COND_WAIT_TIMEOUT,
       XL6_IR_RUNTIME_COND_SIGNAL,
       XL6_IR_RUNTIME_COND_BROADCAST,
       XL6_IR_RUNTIME_RWLOCK_CREATE,
       XL6_IR_RUNTIME_RWLOCK_READ_LOCK,
       XL6_IR_RUNTIME_RWLOCK_TRY_READ_LOCK,
       XL6_IR_RUNTIME_RWLOCK_READ_UNLOCK,
       XL6_IR_RUNTIME_RWLOCK_WRITE_LOCK,
       XL6_IR_RUNTIME_RWLOCK_TRY_WRITE_LOCK,
       XL6_IR_RUNTIME_RWLOCK_WRITE_UNLOCK,
       XL6_IR_RUNTIME_RWLOCK_DOWNGRADE,
       XL6_IR_RUNTIME_RWLOCK_UPGRADE,
       XL6_IR_RUNTIME_SEM_CREATE_DEFAULT,
       /* Exact lowered operations for SPSC/MPSC/MPMC/WaitQueue.  Payload and
        * resource TypeIds remain instruction metadata, so the backend never
        * recovers generic arguments or queue flavor from source spelling. */
       XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED,
       XL6_IR_RUNTIME_STACK_CREATE,
       XL6_IR_RUNTIME_STACK_PUSH,
       XL6_IR_RUNTIME_STACK_RESERVE,
       XL6_IR_RUNTIME_STACK_CLONE,
       XL6_IR_RUNTIME_STACK_EQUALS,
       XL6_IR_RUNTIME_STACK_TRIM,
       XL6_IR_RUNTIME_STACK_CLEAR,
       XL6_IR_RUNTIME_STACK_COUNT,
       XL6_IR_RUNTIME_STACK_POP,
       XL6_IR_RUNTIME_STACK_PEEK,
       XL6_IR_RUNTIME_STACK_PEEK_AT,
       XL6_IR_RUNTIME_STACK_TRY_POP,
       XL6_IR_RUNTIME_STACK_TRY_PEEK,
       XL6_IR_RUNTIME_STACK_TRY_PEEK_AT,
       XL6_IR_RUNTIME_STACK_CAPACITY,
       XL6_IR_RUNTIME_AVL_TREE_CREATE,
       XL6_IR_RUNTIME_AVL_TREE_SET,
       XL6_IR_RUNTIME_AVL_TREE_GET_INTO,
       XL6_IR_RUNTIME_AVL_TREE_TAKE_INTO,
       XL6_IR_RUNTIME_AVL_TREE_CONTAINS,
       XL6_IR_RUNTIME_AVL_TREE_REMOVE,
       XL6_IR_RUNTIME_AVL_TREE_FIRST_INTO,
       XL6_IR_RUNTIME_AVL_TREE_LAST_INTO,
       XL6_IR_RUNTIME_AVL_TREE_LOWER_BOUND_INTO,
       XL6_IR_RUNTIME_AVL_TREE_UPPER_BOUND_INTO,
       XL6_IR_RUNTIME_AVL_TREE_TRIM_TO,
       XL6_IR_RUNTIME_AVL_TREE_EQUALS,
       XL6_IR_RUNTIME_AVL_TREE_MERGE,
       XL6_IR_RUNTIME_AVL_TREE_CLEAR,
       XL6_IR_RUNTIME_AVL_TREE_COUNT,
       XL6_IR_RUNTIME_AVL_TREE_KEYS,
       XL6_IR_RUNTIME_AVL_TREE_VALUES,
       XL6_IR_RUNTIME_AVL_TREE_TRIM,
       XL6_IR_RUNTIME_AVL_TREE_CLONE,
       /* Closed tagged-enum object operations.  Every instruction links one
        * verified EnumVariant/EnumPayload row; reserved runtime keys and
        * boxing details are backend mechanics, never source-name recovery.
        * NEW returns NULL on allocation failure and owns no partial object.
        * SET_NEW consumes its payload on both outcomes; false is failure,
        * never a language boolean. Both require immediate ERROR_TEST. The
        * partial enum remains an evaluation owner until construction commits. */
       XL6_IR_RUNTIME_TAGGED_ENUM_NEW,
       XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW,
       XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL,
       XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT,
       /* Append-only lowered batch operations.  Keeping these identities
        * separate from scalar push/pop prevents Cgen from synthesizing a
        * source-level loop or rediscovering an array element contract. */
       XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH,
       XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO,
       /* Open dynamic records use declared class storage for known fields and
        * the object.__fields -> DynamicFields.handle chain for unknown fields.
        * These append-only operations keep that class-aware lookup/update ABI
        * distinct from a plain XRT Object/dict access.  Literal keys are
        * canonical C-string immediates; string keys are borrowed managed
        * strings; SET consumes its owned xvalue item on every path. */
       XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL,
       XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING,
       XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW,
       XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW,
       /* Borrows a canonical xvalue handle whose stable runtime TypeId and
        * handle policy identify the language error carrier, then returns one
        * independent xerror reference.  This is an invariant projection for
        * typed storage reads, not a recoverable source-language cast. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF,
       /* Borrows a nullable declared-class carrier and returns either the
        * borrowed declared field xvalue or the borrowed null singleton.  The
        * literal key, exact class identity and declared field payload TypeId
        * are all frozen on the instruction; a following explicit nullable
        * projection reconstructs the language result representation. */
       XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL,
       /* Recoverable composite `var as T` projections.  Unlike the earlier
        * invariant-only UNION_UNBOX / PROJECT_CLOSURE_COPY operations, these
        * validate the exact stable TypeId and representation, set the language
        * error channel on mismatch, and are immediately observed by
        * ERROR_TEST.  payloadType fixes T; callable casts additionally repeat
        * the exact signature in calleeType. */
       XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED,
       XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED,
       /* Projects the borrowed runtime Future stored in one typed xvalue
        * collection element and acquires an independent Future owner.  The
        * result Future TypeId and payloadType together preserve Future<T>;
        * neither await nor Cgen may infer T from the source collection. */
       XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY,
       /* Initialize one erased slot of an unpublished class. Borrows the
        * pending class, consumes the boxed field on success AND failure.
        * False is an explicit exceptional outcome, never an invariant trap. */
       XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW,
       /* Explicit-length literal constructors.  The payload lives in the
        * instruction byte arena; neither operation receives quoted source text
        * and Cgen must emit exactly immediateSize bytes. */
       XL6_IR_RUNTIME_STRING_FROM_UTF8,
       XL6_IR_RUNTIME_BYTES_FROM_DATA,
       /* Closed scalar/dynamic bridges for language char. */
       XL6_IR_RUNTIME_CHAR_TO_STRING,
       XL6_IR_RUNTIME_XVALUE_CHAR,
       /* Statically typed byte values retain their own semantic ABI even though
        * the XRT carrier is reference-counted xvalue storage. */
       XL6_IR_RUNTIME_BYTES_RETAIN,
       XL6_IR_RUNTIME_BYTES_CONCAT,
       XL6_IR_RUNTIME_BYTES_EQUAL,
       XL6_IR_RUNTIME_BYTES_ORDER_LT,
       XL6_IR_RUNTIME_BYTES_ORDER_LE,
       XL6_IR_RUNTIME_BYTES_ORDER_GT,
       XL6_IR_RUNTIME_BYTES_ORDER_GE,
       XL6_IR_RUNTIME_BYTES_LENGTH,
       XL6_IR_RUNTIME_BYTES_AT,
       XL6_IR_RUNTIME_BYTES_SET_COPY,
       /* Unicode-scalar string operations.  Indexing returns char rather than
        * allocating a one-character string; replacement produces a fresh
        * owned value so ordinary assignment keeps language value semantics. */
       XL6_IR_RUNTIME_STRING_SCALAR_AT,
       XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY,
       XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT,
       XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT,
       /* Exact optional-value equality, not plain text/view equality. Both
        * borrowed operands have the same nullable<string> TypeId. */
       XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL
} XL6_IrRuntimeOp;

/* Canonical language identities for every stable XRT xvalue tag.  This table
 * is fixed at the Typed XLIR boundary; Cgen consumes the numeric identities
 * mechanically and never reconstructs language types from names. */
typedef struct XL6_IrRuntimeTypeMap {
    uint32 abiVersion;
    uint64 fallbackTypeValueId;
    uint64 nullTypeValueId;
    uint64 boolTypeValueId;
    uint64 i64TypeValueId;
    uint64 f64TypeValueId;
    uint64 stringTypeValueId;
    uint64 bytesTypeValueId;
    uint64 timeTypeValueId;
    uint64 pointerTypeValueId;
    uint64 handleTypeValueId;
    uint64 arrayTypeValueId;
    uint64 intMapTypeValueId;
    uint64 setTypeValueId;
    uint64 objectTypeValueId;
    uint64 u64TypeValueId;
} XL6_IrRuntimeTypeMap;

/* Checked integer conversion normalizes every accepted language/C source
 * representation at the Typed XLIR boundary.  F64 intentionally covers the
 * canonical f64 and foreign C float/double execution ABIs; the verifier still
 * proves that the operand TypeId belongs to that normalized family. */
typedef enum XL6_IrIntegerSourceAbi {
    XL6_IR_INTEGER_SOURCE_INVALID = 0,
    XL6_IR_INTEGER_SOURCE_I64,
    XL6_IR_INTEGER_SOURCE_U64,
    XL6_IR_INTEGER_SOURCE_F64,
    XL6_IR_INTEGER_SOURCE_MANAGED_STRING,
    XL6_IR_INTEGER_SOURCE_XVALUE
} XL6_IrIntegerSourceAbi;

/* Canonical semantic family for a trivial scalar conversion.  Exact width,
 * signedness and foreign C representation remain TypeId facts; this enum
 * prevents a generic C cast from erasing whether the language selected a
 * boolean, integer or floating-point conversion. */
typedef enum XL6_IrScalarConversionAbi {
    XL6_IR_SCALAR_CONVERSION_INVALID = 0,
    XL6_IR_SCALAR_CONVERSION_BOOL_TO_BOOL,
    XL6_IR_SCALAR_CONVERSION_BOOL_TO_INTEGER,
    XL6_IR_SCALAR_CONVERSION_BOOL_TO_FLOAT,
    XL6_IR_SCALAR_CONVERSION_INTEGER_TO_BOOL,
    XL6_IR_SCALAR_CONVERSION_INTEGER_TO_INTEGER,
    XL6_IR_SCALAR_CONVERSION_INTEGER_TO_FLOAT,
    XL6_IR_SCALAR_CONVERSION_FLOAT_TO_INTEGER,
    XL6_IR_SCALAR_CONVERSION_FLOAT_TO_FLOAT,
    XL6_IR_SCALAR_CONVERSION_TIME_TO_BOOL,
    XL6_IR_SCALAR_CONVERSION_TIME_TO_INTEGER,
    XL6_IR_SCALAR_CONVERSION_TIME_TO_FLOAT,
    XL6_IR_SCALAR_CONVERSION_INTEGER_TO_TIME,
    /* A binary operator whose Typed XLIR execution domain is explicitly u64
     * may reinterpret an i64 operand in that domain.  This is deliberately
     * not an ordinary implicit scalar conversion: assignments, arguments and
     * returns must continue to reject an unchecked signed-to-unsigned change. */
    XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64
} XL6_IrScalarConversionAbi;

/* Complete path by which one binary operand enters operatorExecutionType.
 * This is deliberately separate from ScalarConversionAbi: a flow-refined
 * NAME still loads its declared nullable/open-xvalue storage before producing
 * the proven logical value, while an integer literal can be materialized
 * directly in the selected execution domain. */
typedef enum XL6_IrOperatorOperandAbi {
    XL6_IR_OPERATOR_OPERAND_INVALID = 0,
    XL6_IR_OPERATOR_OPERAND_EXACT,
    XL6_IR_OPERATOR_OPERAND_INTEGER_LITERAL,
    XL6_IR_OPERATOR_OPERAND_SCALAR_CONVERT,
    XL6_IR_OPERATOR_OPERAND_XVALUE_PROJECT_EXACT,
    XL6_IR_OPERATOR_OPERAND_XVALUE_PROJECT_SCALAR_CONVERT,
    XL6_IR_OPERATOR_OPERAND_NULLABLE_UNWRAP_EXACT,
    XL6_IR_OPERATOR_OPERAND_NULLABLE_UNWRAP_SCALAR_CONVERT
} XL6_IrOperatorOperandAbi;

/* Source spelling is normalized before C emission.  The C backend consumes
 * this opcode, never an AST/operator text string. */
typedef enum XL6_IrOpcode {
    XL6_IR_OPCODE_INVALID = 0,
    XL6_IR_OPCODE_ADD,
    XL6_IR_OPCODE_SUB,
    XL6_IR_OPCODE_MUL,
    XL6_IR_OPCODE_DIV,
    XL6_IR_OPCODE_MOD,
    XL6_IR_OPCODE_EQ,
    XL6_IR_OPCODE_NE,
    XL6_IR_OPCODE_LT,
    XL6_IR_OPCODE_LE,
    XL6_IR_OPCODE_GT,
    XL6_IR_OPCODE_GE,
    XL6_IR_OPCODE_BIT_AND,
    XL6_IR_OPCODE_BIT_OR,
    XL6_IR_OPCODE_BIT_XOR,
    XL6_IR_OPCODE_SHL,
    XL6_IR_OPCODE_SHR,
    /* Source spellings such as &&/and and ||/or are normalized at the Typed
     * XLIR boundary.  Lowering consumes these identities to build CFG and
     * never parses operator text. */
    XL6_IR_OPCODE_LOGICAL_AND,
    XL6_IR_OPCODE_LOGICAL_OR,
    /* Language membership.  Lowering resolves the concrete container runtime
     * operation from canonical operand TypeIds before any backend runs. */
    XL6_IR_OPCODE_CONTAINS,
    XL6_IR_OPCODE_POS,
    XL6_IR_OPCODE_NEG,
    XL6_IR_OPCODE_NOT,
    XL6_IR_OPCODE_BIT_NOT,
    /* Null coalescing is a control/value-selection semantic, not a C binary
     * operator.  Typed XLIR preserves the identity here; lowering must turn
     * it into representation-aware CFG before Cgen can observe it. */
    XL6_IR_OPCODE_NULL_COALESCE,
    /* Power and approximate equality are semantic operations with
     * type-directed runtime/protocol lowering.  Preserve their identities in
     * Typed XLIR even while individual lowered ABI partitions remain gated. */
    XL6_IR_OPCODE_POW,
    XL6_IR_OPCODE_APPROX
} XL6_IrOpcode;

/* Function roles are semantic IR, not backend conventions.  In particular,
 * a module initializer is distinct from a user function even though it has
 * no source symbol or parameter ABI. */
typedef enum XL6_IrFunctionKind {
    XL6_IR_FUNCTION_INVALID = 0,
    XL6_IR_FUNCTION_MODULE_INIT,
    XL6_IR_FUNCTION_USER,
    XL6_IR_FUNCTION_ASYNC_WORKER,
    /* A lowering-synthesized closure body which adapts an already resolved
     * named callable to one exact first-class function signature.  Its CFG
     * owns forwarded parameters, default-argument evaluation and the direct
     * call; no backend reconstructs an adapter from a source name. */
    XL6_IR_FUNCTION_NAMED_ADAPTER,
    /* A synchronous generator body returns only through completion/error;
     * yielded values cross explicit YIELD suspension records instead of the
     * ordinary callable result ABI. */
    XL6_IR_FUNCTION_GENERATOR_WORKER,
    /* An effect-bearing module-owned cleanup program. Its explicit instructions
     * preserve pending error state and close globals in reverse registration
     * order. The boundary is idempotent, rejects reentry while finalizing and
     * permits initialization again only after cleanup completes. */
    XL6_IR_FUNCTION_MODULE_FINI,
    /* A lowered derived-to-base destruction program. Its single immutable
     * receiver is borrowed from the runtime finalizer, never retained or
     * released. CALL outcomes and first-failure continuation are ordinary CFG. */
    XL6_IR_FUNCTION_CLASS_FINI
} XL6_IrFunctionKind;

/* Error propagation is a function ABI effect.  It is fixed by lowering and
 * call-graph closure before any backend sees a function or a call target. */
typedef enum XL6_IrErrorAbi {
    XL6_IR_ERROR_ABI_INVALID = 0,
    XL6_IR_ERROR_ABI_NO_RAISE,
    XL6_IR_ERROR_ABI_MAY_RAISE
} XL6_IrErrorAbi;

/* Runtime `as` is a closed semantic conversion family, not a generic xvalue
 * projection.  These identities distinguish recoverable checked casts from
 * invariant-only container/member projections which deliberately trap on a
 * malformed carrier.  Append new target families without changing existing
 * numeric identities. */
typedef enum XL6_IrCastAbi {
    XL6_IR_CAST_INVALID = 0,
    XL6_IR_CAST_XVALUE_TO_BOOL_CHECKED,
    XL6_IR_CAST_XVALUE_TO_I64_CHECKED,
    XL6_IR_CAST_XVALUE_TO_U64_CHECKED,
    XL6_IR_CAST_XVALUE_TO_F64_CHECKED,
    XL6_IR_CAST_XVALUE_TO_TIME_CHECKED,
    XL6_IR_CAST_XVALUE_TO_STRING_CHECKED,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_BOOL_TRY,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_I64_TRY,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_U64_TRY,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_F64_TRY,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_TIME_TRY,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_STRING_TRY,
    XL6_IR_CAST_VALUE_TO_TAGGED_NULLABLE,
    XL6_IR_CAST_MANAGED_TO_POINTER_NULLABLE,
    XL6_IR_CAST_TAGGED_NULLABLE_TO_VALUE_CHECKED,
    XL6_IR_CAST_NULLABLE_POINTER_TO_MANAGED_CHECKED,
    XL6_IR_CAST_XVALUE_TO_NOMINAL_CHECKED,
    XL6_IR_CAST_XVALUE_TO_NULLABLE_NOMINAL_TRY,
    XL6_IR_CAST_NULLABLE_NOMINAL_TO_NOMINAL_CHECKED,
    /* Checked projection of the runtime pointer tag into the canonical
     * language raw-pointer ABI.  Kept distinct from the invariant-only
     * borrowed field projection so a source conversion always owns an
     * explicit TypeError edge. */
    XL6_IR_CAST_XVALUE_TO_RAW_POINTER_CHECKED,
    /* Checked projection of the compiler-defined error handle.  The xvalue
     * runtime TypeId must match exactly and success returns one owned xerror
     * reference; mismatch follows the ordinary recoverable TypeError edge. */
    XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED,
    /* Checked dynamic projections for composite value representations.  These
     * remain distinct from invariant-only storage/container projections so a
     * source `as` always owns an explicit recoverable TypeError edge. */
    XL6_IR_CAST_XVALUE_TO_UNION_CHECKED,
    XL6_IR_CAST_XVALUE_TO_CLOSURE_CHECKED,
    XL6_IR_CAST_XVALUE_TO_BYTES_CHECKED
} XL6_IrCastAbi;

/* Calls selected by language semantics but not represented by a source-level
 * callable declaration live in their own typed arena.  The spelling is used
 * once while Typed XLIR is built; every later phase consumes this identity. */
typedef enum XL6_IrIntrinsicKind {
    XL6_IR_INTRINSIC_INVALID = 0,
    XL6_IR_INTRINSIC_CHECKED_INTEGER_CONVERT,
    /* Exact builtin type conversion from one borrowed managed string to a
     * trivial F64 value.  Parsing failure is part of the intrinsic ABI and is
     * never reconstructed from a callee spelling by lowering or Cgen. */
    XL6_IR_INTRINSIC_CHECKED_FLOAT_CONVERT,
    /* Checked conversion to the canonical time TypeId from either one
     * borrowed managed string or one borrowed open xvalue. */
    XL6_IR_INTRINSIC_CHECKED_TIME_CONVERT,
    XL6_IR_INTRINSIC_STRING_CONVERT,
    /* Exact, non-raising scalar conversion.  The semantic family is carried
     * by scalarConversionAbi; this identity covers foreign C scalars and
     * distinct language primitives such as time without conflating either
     * category with checked/wrapping integer conversion. */
    XL6_IR_INTRINSIC_SCALAR_CONVERT,
    /* Explicit array/list/set/dict conversion from an erased xvalue.  Typed
     * XLIR records the requested collection ABI once, before lowering loses
     * the builtin callee spelling. */
    XL6_IR_INTRINSIC_ERASED_COLLECTION_CONVERT,
    XL6_IR_INTRINSIC_COPY,
    XL6_IR_INTRINSIC_CLONE,
    XL6_IR_INTRINSIC_ARRAY_APPEND,
    XL6_IR_INTRINSIC_SET_ADD,
    XL6_IR_INTRINSIC_COLLECTION_CONTAINS,
    XL6_IR_INTRINSIC_COLLECTION_REMOVE,
    XL6_IR_INTRINSIC_ARRAY_REMOVE_VALUE,
    XL6_IR_INTRINSIC_TYPED_COLLECTION_MEMBER,
    XL6_IR_INTRINSIC_DYNAMIC_COLLECTION_MEMBER,
    XL6_IR_INTRINSIC_RANGE,
    XL6_IR_INTRINSIC_XCALL,
    XL6_IR_INTRINSIC_FUTURE_CANCEL,
    /* Exact Future member identities.  The receiver is canonicalized as
     * operand zero so lowering never revisits a member name or argument list. */
    XL6_IR_INTRINSIC_FUTURE_WAIT,
    XL6_IR_INTRINSIC_FUTURE_WAIT_TIMEOUT,
    XL6_IR_INTRINSIC_FUTURE_WAIT_UNTIL,
    XL6_IR_INTRINSIC_FUTURE_IS_DONE,
    XL6_IR_INTRINSIC_FUTURE_IS_PENDING,
    XL6_IR_INTRINSIC_FUTURE_IS_CANCELLED,
    XL6_IR_INTRINSIC_FUTURE_IS_FAILED,
    XL6_IR_INTRINSIC_FUTURE_STATE,
    XL6_IR_INTRINSIC_FUTURE_STATUS,
    XL6_IR_INTRINSIC_FUTURE_ERROR,
    /* Typed continuation registration.  The receiver is operand zero and the
     * closure is operand one.  Source/output payload and closure compatibility
     * are fixed before lowering so these cannot degrade into ordinary calls. */
    XL6_IR_INTRINSIC_FUTURE_THEN,
    XL6_IR_INTRINSIC_FUTURE_CATCH,
    XL6_IR_INTRINSIC_FUTURE_FINALLY,
    /* A language-level, trivial and non-failing integer conversion.  This is
     * distinct from checked integer conversion and from foreign C ABI scalar
     * adaptation even though all three eventually lower to scalar values. */
    XL6_IR_INTRINSIC_TRIVIAL_INTEGER_CONVERT,
    /* An explicit conversion whose source and result have the exact same
     * trivial TypeId.  Keeping this identity in Typed XLIR distinguishes a
     * semantic type-conversion call from an unresolved ordinary call; it
     * lowers to an explicit VALUE instruction without a runtime operation. */
    XL6_IR_INTRINSIC_TRIVIAL_IDENTITY_CONVERT,
    /* An explicit builtin pointer conversion from the language null literal.
     * Typed XLIR proves the target is the canonical raw-pointer sentinel ABI;
     * lowering therefore materializes a target-typed NULL instruction without
     * asking Cgen to reinterpret either the call spelling or the null TypeId. */
    XL6_IR_INTRINSIC_RAW_POINTER_NULL_CONVERT,
    /* Evaluates one expression and returns its language type identity.  The
     * exact observed static TypeId is carried by the intrinsic; a dynamic
     * xvalue query is a distinct query ABI and may not be inferred later. */
    XL6_IR_INTRINSIC_TYPE_OF,
    /* Standard-library task aggregation is normalized at the Typed XLIR
     * boundary.  Literal collection syntax is flattened into borrowed Future
     * operands so lowering and Cgen never revisit the argument container. */
    XL6_IR_INTRINSIC_TASK_ALL,
    XL6_IR_INTRINSIC_TASK_RACE,
    /* A named/expression array<Future<T>> remains one borrowed typed-array
     * operand for all/race. Literal forms above stay flattened. */
    XL6_IR_INTRINSIC_TASK_ALL_ARRAY,
    XL6_IR_INTRINSIC_TASK_RACE_ARRAY,
    /* A non-empty literal is flattened into heterogeneous borrowed Future<T>
     * operands and the public result is fixed as owned Future<I64>. */
    XL6_IR_INTRINSIC_TASK_WHEN_ANY,
    /* A named/expression array<Future<T>> remains one borrowed typed-array
     * operand. This is distinct from the flattened literal form so lowering
     * and Cgen never guess whether one xvalue denotes an array or a Future. */
    XL6_IR_INTRINSIC_TASK_WHEN_ANY_ARRAY,
    /* task.spawn(fn, args...) carries one exact closure signature, canonical
     * physical arguments and Future payload ownership into lowering. */
    XL6_IR_INTRINSIC_TASK_SPAWN,
    /* Static I64/U64 conversion with modulo/bit-truncating language semantics.
     * Dynamic xvalue conversion remains CHECKED_INTEGER_CONVERT. */
    XL6_IR_INTRINSIC_WRAPPING_INTEGER_CONVERT,
    /* Explicit language truth conversion.  Static scalar sources lower to an
     * exact nonzero comparison, open dynamic values lower to XVALUE_TRUTHY,
     * and the null literal lowers to false.  The distinction prevents Cgen
     * from reconstructing language truth semantics from physical C types. */
    XL6_IR_INTRINSIC_BOOL_CONVERT,
    /* Standard-library JSON persistence over one exact nominal class.  The
     * class TypeId and field schema are owned by XLIR; lowering and Cgen do
     * not recover derive declarations, attributes, or the runtime callee
     * spelling. */
    XL6_IR_INTRINSIC_PERSISTENCE_JSON_STRINGIFY,
    /* Generic json.parse<T> fixes T as the result and schema TypeId.  Its sole
     * source operand is borrowed text; lowering materializes the default T
     * instance explicitly before selecting the parse RuntimeOp. */
    XL6_IR_INTRINSIC_PERSISTENCE_JSON_PARSE,
    /* The XLON identities share the persistence schema model while preserving
     * an exact source-level format choice through lowering. */
    XL6_IR_INTRINSIC_PERSISTENCE_XLON_STRINGIFY,
    XL6_IR_INTRINSIC_PERSISTENCE_XLON_PARSE,
    XL6_IR_INTRINSIC_PERSISTENCE_JSON_WRITE,
    XL6_IR_INTRINSIC_PERSISTENCE_JSON_READ,
    XL6_IR_INTRINSIC_PERSISTENCE_XLON_WRITE,
    XL6_IR_INTRINSIC_PERSISTENCE_XLON_READ,
    /* Exact Future metadata members.  The receiver is operand zero;
     * SET_DEBUG_NAME additionally borrows one managed string. */
    XL6_IR_INTRINSIC_FUTURE_SET_DEBUG_NAME,
    XL6_IR_INTRINSIC_FUTURE_DEBUG_NAME,
    XL6_IR_INTRINSIC_FUTURE_CREATE_TIME_MS,
    XL6_IR_INTRINSIC_FUTURE_COMPLETE_TIME_MS,
    XL6_IR_INTRINSIC_FUTURE_PENDING_CONTINUATIONS,
    /* Canonical weak<T>.lock(): operand zero is the borrowed receiver and the
     * owned result is exactly nullable<T>. */
    XL6_IR_INTRINSIC_WEAK_LOCK,
    /* One closed resource intrinsic family; runtimeResourceOp contains the
     * exact constructor/member identity and all resource types remain explicit
     * in the operand/result TypeIds. */
    XL6_IR_INTRINSIC_RUNTIME_RESOURCE,
    /* Exact builtin conversion from one borrowed open xvalue through a
     * closed checked Cast ABI.  This family starts with raw pointers; adding
     * another target requires verifier, lowering and runtime support rather
     * than a source-name fallback in Cgen. */
    XL6_IR_INTRINSIC_CHECKED_XVALUE_CONVERT
} XL6_IrIntrinsicKind;

/* The continuation mode survives intrinsic lowering because a single runtime
 * operation owns the common frame/capture ABI.  Keeping the mode explicit is
 * preferable to three nearly identical Cgen paths or a source-name lookup. */
typedef enum XL6_IrFutureContinuationKind {
    XL6_IR_FUTURE_CONTINUATION_INVALID = 0,
    XL6_IR_FUTURE_CONTINUATION_THEN,
    XL6_IR_FUTURE_CONTINUATION_CATCH,
    XL6_IR_FUTURE_CONTINUATION_FINALLY
} XL6_IrFutureContinuationKind;

typedef enum XL6_IrTypeQueryAbi {
    XL6_IR_TYPE_QUERY_INVALID = 0,
    XL6_IR_TYPE_QUERY_STATIC_ID,
    XL6_IR_TYPE_QUERY_DYNAMIC_XVALUE
} XL6_IrTypeQueryAbi;

/* `string(value)` owns a closed conversion family before lowering.  This is
 * deliberately more precise than a source C ABI: nominal classes split into
 * compiler-derived structural formatting and an exact protocol call, while a
 * language `type` value uses the module's stable type-name table. */
typedef enum XL6_IrStringConversionAbi {
    XL6_IR_STRING_CONVERSION_INVALID = 0,
    XL6_IR_STRING_CONVERSION_BOOL_VALUE,
    XL6_IR_STRING_CONVERSION_I64_VALUE,
    XL6_IR_STRING_CONVERSION_U64_VALUE,
    XL6_IR_STRING_CONVERSION_F64_VALUE,
    XL6_IR_STRING_CONVERSION_TIME_VALUE,
    XL6_IR_STRING_CONVERSION_BORROW_XVALUE,
    XL6_IR_STRING_CONVERSION_BORROW_STRING_COPY,
    XL6_IR_STRING_CONVERSION_TYPE_ID_NAME,
    XL6_IR_STRING_CONVERSION_CLASS_DERIVED,
    XL6_IR_STRING_CONVERSION_CLASS_PROTOCOL_CALL,
    /* Append-only: a language char is one already validated Unicode scalar,
     * not an integer that Cgen may format numerically. */
    XL6_IR_STRING_CONVERSION_CHAR_VALUE
} XL6_IrStringConversionAbi;

/* Operand ABI describes what the intrinsic is allowed to do with the source
 * value.  CONSUME means the operation receives a fresh owned representation;
 * it does not silently consume the source expression's storage. */
typedef enum XL6_IrIntrinsicOperandAbi {
    XL6_IR_INTRINSIC_OPERAND_INVALID = 0,
    XL6_IR_INTRINSIC_OPERAND_VALUE,
    XL6_IR_INTRINSIC_OPERAND_BORROW,
    XL6_IR_INTRINSIC_OPERAND_CONSUME,
    /* The operand TypeId is STORAGE_REF<T>; expression still names the source
     * lvalue of type T.  Lowering must materialize STORAGE_ADDRESS directly. */
    XL6_IR_INTRINSIC_OPERAND_STORAGE_REF
} XL6_IrIntrinsicOperandAbi;

typedef enum XL6_IrIntrinsicResultAbi {
    XL6_IR_INTRINSIC_RESULT_INVALID = 0,
    XL6_IR_INTRINSIC_RESULT_VOID,
    XL6_IR_INTRINSIC_RESULT_VALUE,
    XL6_IR_INTRINSIC_RESULT_OWNED,
    /* RANGE is a typed control-flow descriptor consumed by FOR lowering and
     * is never materialized as a runtime value. */
    XL6_IR_INTRINSIC_RESULT_DESCRIPTOR
} XL6_IrIntrinsicResultAbi;

/* Identity and admission are deliberately separate. UNSUPPORTED_ABI remains
 * a well-formed Typed XLIR fact so lowering can reject it without re-reading
 * source names. */
typedef enum XL6_IrIntrinsicAdmission {
    XL6_IR_INTRINSIC_ADMISSION_INVALID = 0,
    XL6_IR_INTRINSIC_ADMISSION_VERIFIED,
    XL6_IR_INTRINSIC_ADMISSION_UNSUPPORTED_ABI
} XL6_IrIntrinsicAdmission;

/* Emission reachability is a canonical Lowered XLIR fact.  ROOT entities are
 * named by a module/process boundary, TRANSITIVE entities are reached from a
 * root through an explicit call/closure/dispatch/async edge, and UNREACHABLE
 * entities remain typed/lowered facts but must not be emitted by a backend.
 * Typed XLIR leaves every entity UNREACHABLE until lowering has built the
 * complete callable graph. */
typedef enum XL6_IrReachability {
    XL6_IR_REACHABILITY_UNREACHABLE = 0,
    XL6_IR_REACHABILITY_ROOT,
    XL6_IR_REACHABILITY_TRANSITIVE
} XL6_IrReachability;

/* A Future payload is an ownership/representation contract, not merely its
 * source result TypeId.  The same enum is stored on INTERNAL_ASYNC targets
 * and AWAIT instructions so a backend never has to reinterpret future<T>,
 * and so an async void completion cannot be confused with a null value. */
/* Bidirectional language/Future boundary: every non-success state enters the
 * error CFG as an XRT error kind; an escaping cancellation/closure kind
 * completes the destination in that terminal state after catch/finally. */
typedef enum XL6_IrFutureOutcomeAbi {
    XL6_IR_FUTURE_OUTCOME_INVALID = 0,
    XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND
} XL6_IrFutureOutcomeAbi;

/* AWAIT has two physical outputs: its typed payload and an independent status.
 * ERROR_TEST consumes status, never diagnostic presence. Preparation failure
 * sets false before entering the same cleanup CFG; NULL is a valid diagnostic.
 * A null Future is a language StateError with the canonical diagnostic and the
 * AWAIT source span.  Encoding that rule here keeps every backend from guessing
 * whether a null input is a generic runtime failure or a language exception.
 * This applies equally to blocking, generator and resumed-machine execution. */
typedef enum XL6_IrAwaitOutcomeAbi {
    XL6_IR_AWAIT_OUTCOME_INVALID = 0,
    XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR
} XL6_IrAwaitOutcomeAbi;

typedef enum XL6_IrAsyncResultAbi {
    XL6_IR_ASYNC_RESULT_INVALID = 0,
    XL6_IR_ASYNC_RESULT_VOID,
    XL6_IR_ASYNC_RESULT_COPY_BOOL,
    XL6_IR_ASYNC_RESULT_COPY_I64,
    XL6_IR_ASYNC_RESULT_COPY_F64,
    XL6_IR_ASYNC_RESULT_OWNED_STRING,
    XL6_IR_ASYNC_RESULT_RETAINED_XVALUE,
    XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE,
    XL6_IR_ASYNC_RESULT_COPY_U64,
    /* Copies a language type token.  Its C representation is uint64_t, but
     * it is not an arithmetic U64 payload and therefore retains a distinct
     * async contract through Future storage and AWAIT. */
    XL6_IR_ASYNC_RESULT_COPY_TYPE_ID,
    /* Copies one compiler-owned trivial tagged value through Future storage.
     * The exact payload TypeId remains on the target/AWAIT instruction. */
    XL6_IR_ASYNC_RESULT_COPY_TAGGED,
    /* Future owns one strong xerror reference and every AWAIT materializes a
     * fresh strong reference.  This is deliberately distinct from failure:
     * a resolved Future<error> carries a language error value as data. */
    XL6_IR_ASYNC_RESULT_RETAINED_ERROR,
    /* Copies one non-owning raw C pointer value.  The Future owns only the
     * payload cell that stores the pointer; it never owns the pointee. */
    XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER,
    /* Future owns one strong reference to a nested Future.  Reading the
     * payload always creates an independent strong reference, so flattening
     * is never implicit in the storage or C ABI. */
    XL6_IR_ASYNC_RESULT_RETAINED_FUTURE,
    /* Future owns one real generator reference.  AWAIT acquires another V2
     * reference; closing one alias does not invalidate the other owner. */
    XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR,
    /* A Unicode scalar uses uint32_t storage and therefore cannot share the
     * uint64_t payload contract even though both are unsigned values. */
    XL6_IR_ASYNC_RESULT_COPY_CHAR
} XL6_IrAsyncResultAbi;

/* Open dynamic invocation uses the runtime's multi-value call protocol, but a
 * source `var` expression denotes exactly one owned value.  The projection
 * policy is therefore explicit IR data: retain result zero, then release the
 * complete runtime result vector (including every unprojected value). */
typedef enum XL6_IrDynamicCallResultAbi {
    XL6_IR_DYNAMIC_CALL_RESULT_INVALID = 0,
    XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE,
    /* Setter success is an owned xvalue-null; physical NULL means failure. */
    XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE,
    /* Statement invocation accepts zero or more results, releases the whole
     * result vector, and returns an owned unit sentinel for the error edge. */
    XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT
} XL6_IrDynamicCallResultAbi;

/* Yield ownership is a continuation boundary contract, not a memcpy chosen by
 * Cgen.  Trivial values are copied into the generator's current slot.  Managed
 * values transfer one already-owned result into that slot and are later taken
 * by GENERATOR_ITEM_TAKE or destroyed by close. */
typedef enum XL6_IrGeneratorYieldAbi {
    XL6_IR_GENERATOR_YIELD_INVALID = 0,
    XL6_IR_GENERATOR_YIELD_COPY_BOOL,
    XL6_IR_GENERATOR_YIELD_COPY_I64,
    XL6_IR_GENERATOR_YIELD_COPY_U64,
    XL6_IR_GENERATOR_YIELD_COPY_TYPE_ID,
    XL6_IR_GENERATOR_YIELD_COPY_F64,
    XL6_IR_GENERATOR_YIELD_COPY_TIME,
    XL6_IR_GENERATOR_YIELD_COPY_RAW_POINTER,
    XL6_IR_GENERATOR_YIELD_COPY_TAGGED,
    XL6_IR_GENERATOR_YIELD_MOVE_STRING,
    XL6_IR_GENERATOR_YIELD_MOVE_XVALUE,
    XL6_IR_GENERATOR_YIELD_MOVE_CLOSURE,
    XL6_IR_GENERATOR_YIELD_MOVE_FUTURE,
    XL6_IR_GENERATOR_YIELD_MOVE_ERROR,
    XL6_IR_GENERATOR_YIELD_MOVE_GENERATOR,
    XL6_IR_GENERATOR_YIELD_COPY_CHAR
} XL6_IrGeneratorYieldAbi;

typedef struct XL6_IrSpan {
    int line;
    int column;
} XL6_IrSpan;

/* Some source NAME expressions denote a value through the active instance
 * receiver even though they are not ordinary lexical storage bindings.
 * Canonicalize that distinction in Typed XLIR so lowering never guesses from
 * an unresolved symbol and Cgen sees only explicit LOAD/VALUE/runtime ops. */
typedef enum XL6_IrImplicitNameAbi {
    XL6_IR_IMPLICIT_NAME_NONE = 0,
    /* Source `base` observes the current class receiver through the exact
     * immediate-base TypeId stored on the NAME. */
    XL6_IR_IMPLICIT_NAME_BASE_RECEIVER_VIEW,
    /* An unqualified instance field is an explicit projection from the current
     * receiver. implicitFieldDeclaration identifies the verified ClassField. */
    XL6_IR_IMPLICIT_NAME_CLASS_FIELD
} XL6_IrImplicitNameAbi;

/* A source NAME can denote a top-level function value even when semantic
 * analysis leaves the expression untyped because its immediate destination is
 * the open `var` carrier.  Preserve that fact as an explicit Typed-XLIR
 * contract: targetSymbol remains diagnostic frontend identity, while the
 * canonical declaration symbol and exact closure signature below are the only
 * executable identities consumed by lowering. */
typedef enum XL6_IrNamedCallableAbi {
    XL6_IR_NAMED_CALLABLE_NONE = 0,
    XL6_IR_NAMED_CALLABLE_CAPTURE_FREE_ADAPTER
} XL6_IrNamedCallableAbi;

/* Canonical semantic identity for non-call member expressions.  The source
 * spelling remains diagnostic data only after this field is set; lowering
 * dispatches on the enum and Cgen never sees it. */
typedef enum XL6_IrMemberAbi {
    XL6_IR_MEMBER_ABI_NONE = 0,
    /* Source `string.length`: Unicode scalar count, never UTF-8 byte size. */
    XL6_IR_MEMBER_ABI_STRING_UTF8_COUNT,
    /* A source `value.length` that semantic analysis selected through the
     * zero-argument `__len__` class protocol.  targetSymbol is the exact
     * instance method declaration; lowering emits an ordinary verified CALL
     * and never repeats class/member lookup. */
    XL6_IR_MEMBER_ABI_LENGTH_PROTOCOL_CALL,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_XVALUE_BORROW,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_BOOL,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_I64,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_U64,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_F64,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_STRING_COPY,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_RAW_POINTER_BORROW,
    /* A language `point` physically stored in an owning xvalue handle.  The
     * result is still a non-owning raw pointer; only the field extraction ABI
     * differs from an ordinary XVALUE_POINTER field. */
    XL6_IR_MEMBER_ABI_CLASS_FIELD_OWNED_RAW_POINTER_BORROW,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_NATIVE_POINTER_BORROW,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_NATIVE_HANDLE_BORROW,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_TAGGED_NULLABLE,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_CLOSURE,
    /* Exact projection from a compiler-owned plain union. unionField names the
     * module UnionField record; lowering never searches by member spelling. */
    XL6_IR_MEMBER_ABI_UNION_FIELD,
    /* A nullable managed string is physically pointer-sentinel but its class
     * field is stored as an xvalue null-or-string.  Lowering must therefore
     * emit dynamic field lookup plus an explicit nullable-string projection;
     * it is not the non-null STRING_COPY member ABI. */
    XL6_IR_MEMBER_ABI_CLASS_FIELD_NULLABLE_STRING_COPY,
    XL6_IR_MEMBER_ABI_CLASS_FIELD_TIME,
    /* Safe collection count keeps its null-result representation explicit.
     * Typed collections produce a tagged nullable i64; open `var` keeps the
     * canonical xvalue null-or-int carrier. */
    XL6_IR_MEMBER_ABI_SAFE_XVALUE_COUNT_TAGGED_NULLABLE,
    XL6_IR_MEMBER_ABI_SAFE_XVALUE_COUNT_XVALUE,
    /* A trivial union stored in a class object is projected by exact TypeId:
     * borrow the xvalue field, then copy-unbox the selected union layout. */
    XL6_IR_MEMBER_ABI_CLASS_FIELD_UNION_COPY,
    /* Union mutation is an immutable UNION_SET followed by one explicit
     * write-back destination.  These identities prevent lowering from
     * recovering lvalue shape after Typed XLIR has been verified. */
    XL6_IR_MEMBER_ABI_UNION_FIELD_DIRECT_STORAGE_WRITE,
    XL6_IR_MEMBER_ABI_UNION_FIELD_CLASS_FIELD_WRITE,
    /* A field exposed by an anonymous union nested directly in a class.  The
     * declaration graph fixes the field name and exact result TypeId in Typed
     * XLIR; lowering selects a scalar object projection from that TypeId. */
    XL6_IR_MEMBER_ABI_CLASS_INLINE_UNION_FIELD,
    /* Error members are closed projections over an owned/borrowed xerror,
     * never dynamic object lookup.  Each identity fixes the result ownership
     * and the exact XRT accessor selected during lowering. */
    XL6_IR_MEMBER_ABI_ERROR_MESSAGE_COPY,
    XL6_IR_MEMBER_ABI_ERROR_KIND_COPY,
    XL6_IR_MEMBER_ABI_ERROR_FILE_COPY,
    XL6_IR_MEMBER_ABI_ERROR_LINE,
    XL6_IR_MEMBER_ABI_ERROR_COLUMN,
    XL6_IR_MEMBER_ABI_ERROR_CODE,
    XL6_IR_MEMBER_ABI_ERROR_SYSTEM_CODE,
    XL6_IR_MEMBER_ABI_ERROR_DOMAIN_COPY,
    XL6_IR_MEMBER_ABI_ERROR_OPERATION_COPY,
    XL6_IR_MEMBER_ABI_ERROR_DATA_COPY,
    XL6_IR_MEMBER_ABI_ERROR_CAUSE_REF,
    /* A declared class field physically stores an error as an owning xvalue
     * handle.  Reading it borrows that carrier and returns a retained xerror
     * pointer through an explicit projection operation. */
    XL6_IR_MEMBER_ABI_CLASS_FIELD_ERROR_REF,
    /* A source `nullableClass?.field` resolved to one declared class field.
     * Lowering keeps null propagation (borrowed xvalue lookup) separate from
     * physical nullable-result reconstruction. */
    XL6_IR_MEMBER_ABI_SAFE_CLASS_FIELD_NULLABLE,
    /* Source `bytes.length`: byte cardinality, kept distinct from Unicode
     * scalar count.  Both return canonical int, but their physical
     * observation is unrelated. */
    XL6_IR_MEMBER_ABI_BYTES_LENGTH
} XL6_IrMemberAbi;

/* Safe builtin indexing evaluates its key only on the present successor.
 * The receiver contract is closed before lowering, independently of the
 * lookup's missing/out-of-range result contract. */
typedef enum XL6_IrIndexReceiverGate {
    XL6_IR_INDEX_RECEIVER_UNCONDITIONAL = 0,
    XL6_IR_INDEX_RECEIVER_NULLABLE,
    XL6_IR_INDEX_RECEIVER_DYNAMIC
} XL6_IrIndexReceiverGate;

/* Canonical mutation identity for an INDEX node used as an lvalue.  Key
 * syntax, collection representation and consuming item ownership are
 * resolved before lowering; Cgen receives only the resulting RuntimeOp. */
typedef enum XL6_IrIndexWriteAbi {
    XL6_IR_INDEX_WRITE_NONE = 0,
    XL6_IR_INDEX_WRITE_XVALUE_ARRAY_I64_SET_NEW,
    XL6_IR_INDEX_WRITE_XVALUE_LIST_I64_SET_NEW,
    XL6_IR_INDEX_WRITE_XVALUE_OBJECT_LITERAL_SET_NEW,
    XL6_IR_INDEX_WRITE_XVALUE_OBJECT_STRING_SET_NEW,
    XL6_IR_INDEX_WRITE_XVALUE_KEY_DISPATCH_SET_NEW,
    /* Value-sequence mutation computes a fresh owner and commits it back to
     * addressable receiver storage; it never mutates an aliased carrier. */
    XL6_IR_INDEX_WRITE_STRING_SCALAR_SET_COPY,
    XL6_IR_INDEX_WRITE_BYTES_SET_COPY
} XL6_IrIndexWriteAbi;

/* Canonical target layout for a source loop.  A range descriptor does not
 * define how loop targets are bound; that semantic choice belongs to the FOR
 * node and is fixed here before CFG lowering. */
typedef enum XL6_IrLoopAbi {
    XL6_IR_LOOP_ABI_NONE = 0,
    XL6_IR_LOOP_ABI_RANGE_I64_VALUE,
    /* `for index, value in range(...)`: index is the zero-based ordinal and
     * value is the current arithmetic-progression element. */
    XL6_IR_LOOP_ABI_RANGE_I64_INDEX_VALUE,
    /* A stable, named array<i64> is shallow-cloned before iteration.  The
     * snapshot is an owned loop resource released from the loop exit block;
     * admission therefore requires a body with no exceptional/return escape
     * until iterator resources become first-class cleanup-region actions. */
    XL6_IR_LOOP_ABI_XVALUE_ARRAY_I64_VALUE,
    XL6_IR_LOOP_ABI_XVALUE_ARRAY_I64_INDEX_VALUE,
    /* Container mechanics, key layout and item projection are deliberately
     * orthogonal.  These values fix target arity only; loopKeyAbi and
     * loopItemAbi carry the exact projected representations. */
    XL6_IR_LOOP_ABI_XVALUE_ITERATOR_VALUE,
    XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY_VALUE,
    /* Dict's one-target source form binds the string key, not the item.  Keep
     * that layout distinct so lowering never guesses from target arity. */
    XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY,
    /* A managed string is traversed by Unicode scalar ordinal over one owned
     * snapshot.  The value target receives one trivial language char; scalar
     * decoding and its failure edge are explicit runtime operations. */
    XL6_IR_LOOP_ABI_STRING_UTF8_VALUE,
    XL6_IR_LOOP_ABI_STRING_UTF8_INDEX_VALUE,
    /* User-defined/native class iteration is normalized to two exact calls:
     * iterable.__iter__() -> iterator and
     * iterator.__next__() -> (bool, item).  The selected symbols and all
     * physical TypeIds live on the loop node; lowering emits ordinary CALL,
     * PRODUCT_GET/TAKE and CFG instructions, while Cgen never performs
     * protocol lookup. */
    XL6_IR_LOOP_ABI_PROTOCOL_ITERATOR_VALUE,
    XL6_IR_LOOP_ABI_PROTOCOL_ITERATOR_INDEX_VALUE,
    /* A unique generator handle is moved into loop-owned storage.  NEXT and
     * GET/TAKE are ordinary verified XLIR instructions; Cgen never discovers
     * generator semantics from a source generic name. */
    XL6_IR_LOOP_ABI_GENERATOR_VALUE,
    XL6_IR_LOOP_ABI_GENERATOR_INDEX_VALUE,
    /* Bytes traversal mirrors string traversal structurally but projects one
     * bounded uint8 value from an owned immutable snapshot. */
    XL6_IR_LOOP_ABI_BYTES_VALUE,
    XL6_IR_LOOP_ABI_BYTES_INDEX_VALUE
} XL6_IrLoopAbi;

typedef enum XL6_IrLoopKeyAbi {
    XL6_IR_LOOP_KEY_NONE = 0,
    XL6_IR_LOOP_KEY_ORDINAL_I64,
    XL6_IR_LOOP_KEY_INTEGER_I64,
    XL6_IR_LOOP_KEY_STRING_COPY,
    /* Runtime-dependent array/list/set ordinal or dict string key, boxed as
     * one owned canonical dynamic xvalue. */
    XL6_IR_LOOP_KEY_DYNAMIC_XVALUE,
    /* The source has an explicit first `_` target.  Iteration keeps its
     * two-target semantic shape but performs no key projection, storage write
     * or target cleanup. */
    XL6_IR_LOOP_KEY_DISCARD
} XL6_IrLoopKeyAbi;

typedef enum XL6_IrLoopItemAbi {
    XL6_IR_LOOP_ITEM_NONE = 0,
    XL6_IR_LOOP_ITEM_I64,
    XL6_IR_LOOP_ITEM_U64,
    XL6_IR_LOOP_ITEM_BOOL,
    XL6_IR_LOOP_ITEM_F64,
    XL6_IR_LOOP_ITEM_STRING_COPY,
    XL6_IR_LOOP_ITEM_XVALUE_RETAIN,
    /* The cursor lends one canonical xvalue pointer carrier.  Lowering
     * follows ITERATOR_ITEM_BORROW with the exact nominal native-handle
     * projection; the loop target remains a trivial non-owning handle. */
    XL6_IR_LOOP_ITEM_NATIVE_HANDLE_BORROW,
    /* One-target open dynamic iteration: dict projects its string key while
     * array/list/set project the current item.  The result is always owned. */
    XL6_IR_LOOP_ITEM_DYNAMIC_LOGICAL_XVALUE,
    /* Nullable projection is fixed before CFG lowering.  Keeping the three
     * physical carriers distinct prevents Cgen from inferring null handling,
     * copy policy or callable reconstruction from a source type spelling. */
    XL6_IR_LOOP_ITEM_TAGGED_NULLABLE_COPY,
    XL6_IR_LOOP_ITEM_NULLABLE_STRING_COPY,
    XL6_IR_LOOP_ITEM_NULLABLE_CLOSURE_COPY,
    /* The item is field one of the verified __next__ product.  VALUE is a
     * trivial projection; TAKE transfers one managed field owner into the
     * per-iteration target storage. */
    XL6_IR_LOOP_ITEM_PROTOCOL_PRODUCT_VALUE,
    XL6_IR_LOOP_ITEM_PROTOCOL_PRODUCT_TAKE,
    /* The source has an explicit value `_` target.  The loop keeps its
     * source-level target shape but does not project, store or clean up an
     * item value. */
    XL6_IR_LOOP_ITEM_DISCARD,
    XL6_IR_LOOP_ITEM_GENERATOR_VALUE,
    XL6_IR_LOOP_ITEM_GENERATOR_TAKE,
    /* A typed array/list/set cursor lends the canonical bytes xvalue used for
     * one verified trivial union element.  Lowering must follow that borrow
     * with XVALUE_UNION_UNBOX carrying the exact union payload TypeId; Cgen
     * never reconstructs layout or nominal identity from the collection. */
    XL6_IR_LOOP_ITEM_UNION_COPY,
    /* A cursor lends the callable xvalue stored for one exact func(...)
     * element.  Lowering reconstructs an owned signature-carrying closure;
     * the backend never infers an adapter from the collection parameter. */
    XL6_IR_LOOP_ITEM_CLOSURE_COPY,
    /* Primitive-sequence projections are trivial values.  Keeping their
     * semantic identities distinct prevents lowering or Cgen from treating a
     * Unicode scalar as an integer byte, or vice versa. */
    XL6_IR_LOOP_ITEM_CHAR,
    XL6_IR_LOOP_ITEM_U8
} XL6_IrLoopItemAbi;

/* Projection representation and target lifetime are distinct contracts.  A
 * scalar target is overwritten trivially; a copied string/xvalue target owns
 * one resource for exactly one successful iteration.  Lowering must replay
 * that release after finally before every edge which ends the iteration. */
typedef enum XL6_IrLoopTargetLifetimeAbi {
    XL6_IR_LOOP_TARGET_LIFETIME_NONE = 0,
    XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE,
    XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE
} XL6_IrLoopTargetLifetimeAbi;

/* Physical owners of one lowered loop activation. target=0 is its hidden
 * cursor/snapshot; 1..N index the canonical source target list (key then item).
 * A loop's key/item lifetime ends before its cursor lifetime. */
typedef struct XL6_IrLoopOwner {
    XL6_IrNodeId origin;
    XL6_IrNodeId owner;
    XL6_SymbolId symbol;
    /* Physical owner type: the cell type for a promoted element binding. */
    XL6_IrTypeId type;
    size_t target;
} XL6_IrLoopOwner;

/* A known failed continuation temporarily suspended while finally executes.
 * Its nullable diagnostic is an owned LOCAL, not ambient thread error state.
 * STORE activates it; MOVE restores it; RELEASE cancels an overridden exit.
 * The CFG retains failure even when the stored diagnostic is NULL. */
typedef struct XL6_IrPendingException {
    XL6_IrNodeId origin;
    XL6_IrNodeId body;
    XL6_IrNodeId owner;
    XL6_SymbolId symbol;
    XL6_IrTypeId type;
    size_t capture; /* One-based STORE instruction identity. */
    size_t outer;   /* One-based enclosing pending owner, or zero. */
} XL6_IrPendingException;

/* A loop may own hidden storage whose lifetime is wider than one body scope.
 * Typed XLIR fixes that resource contract independently from target binding:
 * lowering materializes the storage and every CFG edge that leaves the loop
 * must either pass through its canonical exit block or replay this cleanup. */
typedef enum XL6_IrLoopResourceAbi {
    XL6_IR_LOOP_RESOURCE_NONE = 0,
    XL6_IR_LOOP_RESOURCE_XVALUE_SHALLOW_SNAPSHOT_RELEASE,
    XL6_IR_LOOP_RESOURCE_XVALUE_OWNED_ITERATOR_RELEASE,
    XL6_IR_LOOP_RESOURCE_XVALUE_ITERATOR_DESTROY,
    XL6_IR_LOOP_RESOURCE_STRING_SNAPSHOT_RELEASE,
    /* Owned result of a verified __iter__ call.  Nominal class values always
     * use the managed xvalue object ABI, including native-backed classes, so
     * protocol cursors have one canonical RELEASE contract.  Opaque native
     * handles exist only at verified extern-C projection/call boundaries. */
    XL6_IR_LOOP_RESOURCE_PROTOCOL_ITERATOR_RELEASE,
    XL6_IR_LOOP_RESOURCE_GENERATOR_CLOSE,
    XL6_IR_LOOP_RESOURCE_BYTES_SNAPSHOT_RELEASE
} XL6_IrLoopResourceAbi;

typedef struct XL6_IrNode {
    XL6_IrNodeKind kind;
    XL6_AstKind sourceKind;
    /* Exact nominal owner of a class declaration or its direct callable member.
     * Templates retain the declared parameter vector, including unused type
     * parameters. Monomorphic clones substitute this identity with the body;
     * receiver ABI must never be inferred from an incidental self expression. */
    XL6_IrTypeId declarationOwnerType;
    unsigned int flags;
    char* text;
    /* Decoded literal payload.  STRING/BYTES retain their exact byte length;
     * CHAR stores one already validated Unicode scalar.  Source quoting and
     * escape syntax never cross this frontend-to-XLIR boundary. */
    unsigned char* literalData;
    size_t literalSize;
    uint32 literalScalar;
    int literalDecoded;
    /* Canonical semantic identity for unary/binary operators and compound
     * assignments.  Plain `=` remains INVALID; `?=` uses NULL_COALESCE.  The
     * separately modeled type-is/type-as forms also remain INVALID. */
    XL6_IrOpcode opcode;
    /* Semantic failure effect of this exact operator application.  Dynamic
     * integer division/remainder/shift can fail after runtime tag inspection;
     * lowering must materialize its ERROR_TEST CFG and Cgen may not infer the
     * effect from the opcode or physical carrier type. */
    XL6_IrErrorAbi operatorErrorAbi;
    /* Exact common operand TypeId selected for a builtin scalar operator.
     * Comparisons keep their bool storageType while this field preserves the
     * signed/unsigned/foreign execution domain.  INVALID means that the
     * operator is non-scalar or is lowered by another closed ABI. */
    XL6_IrTypeId operatorExecutionType;
    /* Exact physical-to-execution path for the two binary operands.  INVALID
     * is required when operatorExecutionType is INVALID.  Flow projections
     * are explicit here so lowering never needs to rediscover a surrounding
     * type/null test from source control flow. */
    XL6_IrOperatorOperandAbi operatorOperandAbis[2];
    /* Exact source-language `as` identity and its failure effect.  Both are
     * INVALID for every non-cast node and for a cast family not yet admitted
     * by canonical XLIR. */
    XL6_IrCastAbi castAbi;
    XL6_IrErrorAbi castErrorAbi;
    /* Exact runtime identity and semantic failure effect of a contextual
     * value conversion owned by this node.  A strong-to-weak conversion is a
     * checked allocation, not a static xvalue rebind.  Exact closures can
     * likewise become XRT callables at erased mutation boundaries even though
     * the source contains no CALL.  Lowering may only emit the recorded
     * RuntimeCall when this effect has an explicit THROW plan. */
    XL6_IrRuntimeOp implicitConversionRuntimeOp;
    XL6_IrErrorAbi implicitConversionErrorAbi;
    XL6_IrTypeId type;
    XL6_IrTypeId storageType;
    /* Exact type denoted by a source type-literal NAME.  INVALID for ordinary
     * values and for builtin callee syntax.  Lowering emits its precomputed
     * typeValueId and never resolves the diagnostic text. */
    XL6_IrTypeId typeValueTarget;
    XL6_SymbolId symbol;
    XL6_SymbolId targetSymbol;
    /* Exact callable selected for an overloaded compound operator.  This is
     * deliberately independent from targetSymbol because a property
     * assignment uses targetSymbol for its setter. */
    XL6_SymbolId compoundOperatorSymbol;
    XL6_SemCallKind callKind;
    XL6_IrImplicitNameAbi implicitNameAbi;
    /* Exact active instance parameter TypeId for an implicit NAME.  INVALID
     * for ordinary names.  A base view keeps its projected type in storageType;
     * a field keeps the exact field declaration below. */
    XL6_IrTypeId implicitReceiverType;
    XL6_IrNodeId implicitFieldDeclaration;
    XL6_IrNamedCallableAbi namedCallableAbi;
    /* Canonical FUNC_DECL symbol, never the declaration's child NAME symbol. */
    XL6_SymbolId namedCallableTargetSymbol;
    /* Exact XL6_IR_TYPE_FUNCTION / CLOSURE_PTR signature emitted by NAME
     * lowering before any contextual conversion to an open runtime callable. */
    XL6_IrTypeId namedCallableSignatureType;
    XL6_IrMemberAbi memberAbi;
    /* Failure effect of an owning physical projection. INVALID unless this
     * node owns a checked copy; independent of accessor call/error ABI. */
    XL6_IrErrorAbi projectionErrorAbi;
    /* Potential failure of acquiring or releasing evaluation-owned operands.
     * Separate from the operation's own result/error ABI, including no-raise
     * reads and implicit string copies before committing an abrupt exit. */
    XL6_IrErrorAbi evaluationErrorAbi;
    /* Fallible insertion/replacement during aggregate construction. This
     * operation effect is independent of operand cleanup and allocation
     * projection effects; INVALID when no such insertion is performed. */
    XL6_IrErrorAbi constructionErrorAbi;
    XL6_IrGeneratorClosePointAbi generatorClosePointAbi;
    /* SIZE_MAX except for XL6_IR_MEMBER_ABI_UNION_FIELD. */
    size_t unionField;
    XL6_IrIndexReceiverGate indexReceiverGate;
    XL6_IrIndexWriteAbi indexWriteAbi;
    /* Failure effect of an ASSIGN/MULTI_ASSIGN whose target uses an index
     * mutation ABI.  The write operation itself returns a boolean commit
     * result; lowering must turn false into an explicit RangeError CFG edge.
     * INVALID on nodes which do not own an index mutation. */
    XL6_IrErrorAbi indexWriteErrorAbi;
    XL6_IrLoopAbi loopAbi;
    XL6_IrLoopKeyAbi loopKeyAbi;
    XL6_IrLoopItemAbi loopItemAbi;
    XL6_IrLoopTargetLifetimeAbi loopKeyLifetimeAbi;
    XL6_IrLoopTargetLifetimeAbi loopItemLifetimeAbi;
    /* The loop's own failure effect.  This is distinct from the iterator
     * descriptor's error ABI: a dynamic three-argument range can reject a
     * zero step while constructing the loop CFG. */
    XL6_IrErrorAbi loopErrorAbi;
    XL6_IrLoopResourceAbi loopResourceAbi;
    XL6_IrTypeId loopResourceType;
    /* Complete class-iteration protocol contract.  Every field is zero for
     * non-protocol loops.  protocolNextResultType is the exact tuple<bool,T>
     * returned by protocolNextTarget; protocolItemType is T. */
    XL6_SymbolId loopProtocolIterTarget;
    XL6_SymbolId loopProtocolNextTarget;
    XL6_IrTypeId loopProtocolIterableReceiverType;
    XL6_IrTypeId loopProtocolIteratorReceiverType;
    XL6_IrTypeId loopProtocolIteratorType;
    XL6_IrTypeId loopProtocolNextResultType;
    XL6_IrTypeId loopProtocolItemType;
    /* Exact generator<T> payload for GENERATOR_* loops; INVALID otherwise. */
    XL6_IrTypeId loopGeneratorItemType;
    XL6_SemLvalueKind lvalueKind;
    XL6_SemExitKind exitKind;
    XL6_IrValueCategory valueCategory;
    XL6_IrOwnership ownership;
    int ownershipPlanned;
    uint32 scopeDepth;
    uint32 targetScopeDepth;
    uint32 region;
    uint32 targetRegion;
    /* Typed XLIR retains lexical containment as semantic structure.  Lowering
     * uses this to resolve the owning class and materialize an explicit
     * receiver ABI; Cgen never walks source ancestry. */
    XL6_IrNodeId parent;
    size_t firstChild;
    size_t childCount;
    XL6_IrSpan span;
} XL6_IrNode;

/* The lowered form uses CFG blocks and SSA-like result values.  These are
 * intentionally part of the canonical model before lowering is implemented. */
typedef struct XL6_IrInstruction {
    /* One-based field-initialization instance. The instance owns a verified
     * instruction interval; diagnostic origin is not an execution location. */
    size_t instantiation;
    /* Zero outside a cleanup region; otherwise the one-based BEGIN identity.
     * Every region instruction repeats its membership. This is a control
     * resource reference, not a spillable value or a source symbol. */
    size_t errorScope;
    /* One-based typed exit-step identity on an abrupt SCOPE CLEANUP_BEGIN;
     * zero for ordinary fallthrough and other cleanup programs. */
    size_t exitStep;
    /* Parameter cleanup transaction identity: self on its CLEANUP_BEGIN,
     * repeated by its failure-unwind entry and both terminal outcomes.
     * Zero for other completion paths, regardless of diagnostic origin. */
    size_t parameterScope;
    /* One-based CLEANUP_BEGIN identity for operation-operand cleanup and its
     * failure unwind. Result STORE/LOAD/MOVE/RELEASE repeat this identity so
     * the pending owner cannot be mistaken for an unrelated temporary. */
    size_t evaluationScope;
    /* Exact pending result storage, only on the evaluation CLEANUP_BEGIN.
     * Zero when the operation has no managed result to commit. */
    XL6_SymbolId evaluationResult;
    /* PendingException identity on its STORE/MOVE/RELEASE or a scope cleanup
     * which cancels it. A MOVE's nonzero pendingFailure names the CLEANUP_END
     * outcome whose secondary failure must preserve this original error. */
    size_t pendingException;
    XL6_IrValueId pendingFailure;
    /* One-based exceptional ExitPlan identity for an unwind, or for a loop/evaluation
     * cleanup transaction whose failure must consume that exact continuation. */
    size_t cleanupPlan;
    /* One-based LoopOwner identity on a physical RELEASE; zero otherwise. */
    size_t loopOwner;
    /* Pending owned result on CLEANUP_BEGIN. An abrupt finally exit cancels
     * it inside cleanup; normal finally/parameter cleanup preserves it on
     * success and cancels it only inside the failure unwind. */
    XL6_SymbolId pendingExit;
    XL6_IrInstructionKind kind;
    XL6_IrOpcode opcode;
    XL6_IrRuntimeOp runtimeOp;
    /* Valid for AWAIT and typed Future aggregate runtime operations.  VOID
     * defines a control token with INVALID type; every value form fixes the
     * exact source completion representation. */
    XL6_IrAsyncResultAbi asyncResultAbi;
    XL6_IrFutureOutcomeAbi futureOutcomeAbi;
    XL6_IrAwaitOutcomeAbi awaitOutcomeAbi;
    /* One-based callback activation identity; zero for non-callback ops. */
    size_t callbackBoundary;
    /* Valid only for FUTURE_CONTINUE_CLOSURE.  The source fields describe the
     * immutable input Future result independently from the output payload. */
    XL6_IrAsyncResultAbi sourceAsyncResultAbi;
    XL6_IrFutureContinuationKind futureContinuationKind;
    /* Valid exactly for YIELD and generator-machine item transfer. */
    XL6_IrGeneratorYieldAbi generatorYieldAbi;
    XL6_IrGeneratorClosePointAbi generatorClosePointAbi;
    /* One-based exact exit plan, exclusively for GENERATOR_CLOSE_TEST. */
    size_t generatorClosePlan;
    /* Valid exactly for SCALAR_CONVERT.  Operand/result TypeIds provide the
     * exact representations; this field fixes the semantic conversion family. */
    XL6_IrScalarConversionAbi scalarConversionAbi;
    /* Valid only for the closed cast carriers: xvalue checked/nullable runtime
     * calls and source TYPE_AS nullable wrap/unwrap.  Runtime calls repeat the
     * exact target or nullable-element TypeId in payloadType. */
    XL6_IrCastAbi castAbi;
    /* Contextual checked casts identify their source Typed-XLIR expression;
     * source `as` and intrinsic casts leave this INVALID. The verifier ties
     * this source and the result to the exact owning boundary's destination. */
    XL6_IrNodeId contextualCastSource;
    /* Valid for XVALUE_CALLABLE_FROM_CLOSURE,
     * XVALUE_NULLABLE_CLOSURE_BOX and TASK_SPAWN_CLOSURE.  The exact concrete
     * signature remains in calleeType; this field fixes its erased/canonical
     * frame binding rather than asking Cgen to recover variadic syntax. */
    XL6_IrCallableErasureAbi callableErasureAbi;
    /* Valid only for INTEGER_CONVERT_CHECKED/WRAPPING. */
    XL6_IrIntegerTarget integerTarget;
    XL6_IrIntegerSourceAbi integerSourceAbi;
    /* A checked integer conversion introduced by an expected-type boundary
     * records the exact source Typed-XLIR expression. Explicit constructor
     * intrinsics leave this INVALID. The verifier replays the boundary proof
     * instead of accepting an otherwise well-shaped injected runtime call. */
    XL6_IrNodeId contextualIntegerConversionSource;
    /* Valid only for checked erased-container cast/rebind runtime operations.
     * This is the canonical tag requested by language semantics; Cgen must not
     * recover it from a type or callee name. */
    XL6_IrCollectionAbi collectionAbi;
    /* Valid only for XVALUE_DYNAMIC_COLLECTION_METHOD.  Static collection
     * operations keep their exact collectionAbi and INVALID here. */
    XL6_IrDynamicCollectionOp dynamicCollectionOp;
    /* Valid only for XVALUE_TYPED_COLLECTION_METHOD. */
    XL6_IrTypedCollectionOp typedCollectionOp;
    /* CONST owns its canonical payload independently of its origin node.
     * Lowering fixes this spelling before Cgen runs, so emission never looks
     * up source/origin text. */
    char* immediateText;
    /* Owned binary immediate for explicit-length runtime operations.  This is
     * not a C string and may contain zero bytes. */
    unsigned char* immediateData;
    size_t immediateSize;
    XL6_IrValueId result;
    /* Reserved legacy encoding; verified Lowered XLIR requires INVALID.
     * SSA values are immutable. Branch joins use typed STORE/LOAD instead. */
    XL6_IrValueId targetValue;
    XL6_IrTypeId type;
    /* CLOSURE_CALL, XVALUE_CALLABLE_FROM_CLOSURE and
     * XVALUE_NULLABLE_CLOSURE_BOX carry their complete callable signature
     * here.  It is INVALID for every other instruction so a result type is
     * never overloaded as call metadata. */
    XL6_IrTypeId calleeType;
    /* Valid for typed Future aggregate operations, checked erased-container
     * projection, checked iterator acquisition, and class persistence
     * operations.  It is respectively the Future payload TypeId, exact
     * collection element TypeId, admitted logical iterator-source TypeId, or
     * exact class schema TypeId, kept separate from result and callable
     * metadata. */
    XL6_IrTypeId payloadType;
    /* Exact payload transfer contract for generic runtime resources.  INVALID
     * for every non-resource operation and for non-generic resources. */
    XL6_IrRuntimeResourcePayloadAbi runtimeResourcePayloadAbi;
    /* AvlTree only: independent key transfer and ordering contracts.  These
     * remain INVALID for every other runtime operation. */
    XL6_IrTypeId runtimeResourceKeyType;
    XL6_IrRuntimeResourcePayloadAbi runtimeResourceKeyPayloadAbi;
    XL6_IrRuntimeResourceKeyOrderAbi runtimeResourceKeyOrderAbi;
    XL6_IrTypeId sourcePayloadType;
    /* Names the exact semantic type whose stable TypeValueId is attached to
     * the owned runtime value.  In addition to explicit type bind/rebind
     * operations, callable erasure records its concrete signature here so an
     * xvalue never collapses to the generic `func` identity. */
    XL6_IrTypeId runtimeIdentityType;
    XL6_SymbolId targetSymbol;
    uint32 dispatchSlot;
    /* Valid for PRODUCT_GET/TAKE/DROP and UNION_GET/SET; SIZE_MAX otherwise. */
    size_t elementIndex;
    /* Zero-based DynamicCallSite index for the two dynamic-call runtime
     * operations; SIZE_MAX for every other instruction. */
    size_t dynamicCallSite;
    /* Exact projection/ownership contract copied from DynamicCallSite for a
     * dynamic-call runtime instruction; INVALID for every other instruction. */
    XL6_IrDynamicCallResultAbi dynamicCallResultAbi;
    /* Exact module arena rows for tagged-enum operations; SIZE_MAX for every
     * other instruction.  A payload row always belongs to enumVariant. */
    size_t enumVariant;
    size_t enumPayload;
    XL6_IrNodeId origin;
    /* Canonical source position copied during lowering.  Runtime diagnostics
     * consume this field directly; Cgen does not traverse Typed nodes. */
    XL6_IrSpan sourceSpan;
    XL6_IrBlockId trueTarget;
    XL6_IrBlockId falseTarget;
    size_t firstOperand;
    size_t operandCount;
} XL6_IrInstruction;

typedef struct XL6_IrBlock {
    size_t firstInstruction;
    size_t instructionCount;
} XL6_IrBlock;

/* A function owns a contiguous CFG/instruction range. Module-level runtime
 * statements use a synthetic function whose symbol is zero. */
typedef struct XL6_IrFunction {
    XL6_IrFunctionKind kind;
    XL6_IrReachability reachability;
    XL6_SymbolId symbol;
    XL6_IrNodeId declaration;
    /* Stable debugger-facing name selected while the typed declaration is
     * still available.  It borrows immutable text from the module node arena
     * (or a static synthetic name); backends must not recover a source name
     * by walking declaration nodes. */
    const char* debugName;
    XL6_IrTypeId returnType;
    XL6_IrErrorAbi errorAbi;
    XL6_IrBlockId entryBlock;
    size_t firstBlock;
    size_t blockCount;
    size_t firstInstruction;
    size_t instructionCount;
    XL6_IrValueId firstValue;
    XL6_IrValueId valueCount;
    size_t firstParameter;
    size_t parameterCount;
    /* Optional parameterCount entries. Each nonzero symbol is the exact
     * promoted cell owned by that parameter's activation, not an alias query. */
    XL6_SymbolId* parameterCells;
} XL6_IrFunction;

/* A typed call site owns argument binding before CFG lowering.  Source
 * wrappers such as named/default arguments are not executable semantics and
 * must never reach Cgen or be reinterpreted while emitting a CALL.  Implicit
 * syntax inputs (currently MEMBER -> __field__) are canonical IR arguments,
 * not backend reconstruction rules. */
typedef enum XL6_IrCallArgumentSource {
    XL6_IR_CALL_ARGUMENT_INVALID = 0,
    XL6_IR_CALL_ARGUMENT_EXPLICIT,
    XL6_IR_CALL_ARGUMENT_DEFAULT,
    XL6_IR_CALL_ARGUMENT_ARRAY_VARARG_ITEM,
    XL6_IR_CALL_ARGUMENT_DICT_VARARG_ITEM,
    XL6_IR_CALL_ARGUMENT_C_VARARG_ITEM,
    XL6_IR_CALL_ARGUMENT_IMPLICIT_STRING,
    /* Value computed by the paired getter/operator transaction, not a second
     * evaluation of the source RHS. expression names the update operation. */
    XL6_IR_CALL_ARGUMENT_PROPERTY_UPDATE
} XL6_IrCallArgumentSource;

typedef enum XL6_IrCallVariadicAbi {
    XL6_IR_CALL_VARIADIC_NONE = 0,
    XL6_IR_CALL_VARIADIC_ARRAY,
    XL6_IR_CALL_VARIADIC_DICT,
    XL6_IR_CALL_VARIADIC_C
} XL6_IrCallVariadicAbi;

/* The physical tail ABI is closed before C emission. Strings are borrowed
 * checked C text, never a language view passed through va_list. ISO C performs
 * the default scalar promotions; managed aggregates are not admissible. */
typedef enum XL6_IrCVarargPhysicalAbi {
    XL6_IR_C_VARARG_PHYSICAL_INVALID = 0,
    XL6_IR_C_VARARG_PHYSICAL_PROMOTED_SCALAR,
    XL6_IR_C_VARARG_PHYSICAL_POINTER,
    XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW
} XL6_IrCVarargPhysicalAbi;

typedef struct XL6_IrCallArgument {
    XL6_IrCallArgumentSource source;
    /* Expression evaluated at the caller.  A DEFAULT expression is owned by
     * the callee declaration tree but still evaluated in caller CFG.  INVALID
     * is valid only for IMPLICIT_STRING, whose canonical UTF-8 payload is in
     * name. */
    XL6_IrNodeId expression;
    size_t parameter;
    /* Dense order among argument expressions, independent of ABI position.
     * Explicit expressions follow source order; defaults follow explicit
     * arguments in declaration order. Receiver interleaving is on CallSite. */
    size_t evaluationIndex;
    /* Present for DICT_VARARG_ITEM keys and IMPLICIT_STRING payloads. */
    char* name;
} XL6_IrCallArgument;

typedef enum XL6_IrCallDispatchKind {
    XL6_IR_CALL_DISPATCH_DIRECT = 0,
    XL6_IR_CALL_DISPATCH_CLASS,
    XL6_IR_CALL_DISPATCH_PROTOCOL,
    XL6_IR_CALL_DISPATCH_CALLABLE_VALUE
} XL6_IrCallDispatchKind;

typedef enum XL6_IrCallReceiverGate {
    XL6_IR_CALL_RECEIVER_UNCONDITIONAL = 0,
    /* Evaluate/anchor the nullable receiver once. Invoke its exact getter
     * only on the present edge, lifting (not nesting) the declared result. */
    XL6_IR_CALL_RECEIVER_NULLABLE_GETTER
} XL6_IrCallReceiverGate;

typedef struct XL6_IrCallSite {
    XL6_IrNodeId origin;
    XL6_SemCallKind callKind;
    XL6_SymbolId targetSymbol;
    XL6_IrTypeId resultType;
    /* Explicit instance receiver; for FUNCTION_VALUE, the exact callable
     * subject expression.  INVALID only for a free/static declaration call. */
    XL6_IrNodeId receiver;
    XL6_IrCallReceiverGate receiverGate;
    /* For a compound property setter, origin of the canonical getter CallSite.
     * Receiver and key bindings are acquired once and shared across both calls.
     * The final argument is PROPERTY_UPDATE; getter, operator/RHS, then setter
     * form one transaction. INVALID on every ordinary call/read/simple write. */
    XL6_IrNodeId preparedGetter;
    /* Explicit dispatch domain; slot is one-based within its canonical
     * class/protocol table. Zero is unbound, never permission to fall back
     * from a protocol to a direct implementation. */
    XL6_IrCallDispatchKind dispatchKind;
    uint32 dispatchSlot;
    /* Number of argument expressions evaluated before the receiver.
     * SIZE_MAX iff there is no receiver; setters can place RHS before it. */
    size_t receiverEvaluationIndex;
    size_t firstArgument;
    size_t argumentCount;
    size_t fixedParameterCount;
    XL6_IrCallVariadicAbi variadicAbi;
} XL6_IrCallSite;

/* Dynamic calls keep source evaluation order but make positional/keyword
 * partitioning explicit before CFG lowering.  Cgen must never inspect
 * ARG/NAMED_ARG wrappers or recover a member name from an origin node. */
typedef enum XL6_IrDynamicCallKind {
    XL6_IR_DYNAMIC_CALL_INVALID = 0,
    XL6_IR_DYNAMIC_CALL_FUNCTION,
    XL6_IR_DYNAMIC_CALL_METHOD,
    /* Read a runtime member: invoke the selected getter, otherwise retain a
     * stored field (missing/null receiver yields owned xvalue-null). Unlike
     * a raw field lookup this always owns its result and may raise. */
    XL6_IR_DYNAMIC_CALL_MEMBER_GET,
    /* Invoke a setter or write a plain field; never bypass an existing property. */
    XL6_IR_DYNAMIC_CALL_MEMBER_SET
} XL6_IrDynamicCallKind;

typedef enum XL6_IrDynamicCallArgumentKind {
    XL6_IR_DYNAMIC_CALL_ARGUMENT_INVALID = 0,
    XL6_IR_DYNAMIC_CALL_ARGUMENT_POSITIONAL,
    XL6_IR_DYNAMIC_CALL_ARGUMENT_KEYWORD
} XL6_IrDynamicCallArgumentKind;

/* Typed dynamic-call construction records whether each source type already
 * has a canonical runtime-xvalue boxing ABI.  UNSUPPORTED remains valid Typed
 * IR so another backend can consume the same analyzed module; XLIR lowering
 * rejects it before emitting any partial call frame. */
typedef enum XL6_IrDynamicCallArgumentAdmission {
    XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_INVALID = 0,
    XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_VERIFIED,
    XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_UNSUPPORTED_BOX_ABI
} XL6_IrDynamicCallArgumentAdmission;

typedef struct XL6_IrDynamicCallArgument {
    XL6_IrDynamicCallArgumentKind kind;
    XL6_IrDynamicCallArgumentAdmission admission;
    XL6_IrNodeId expression;
    XL6_IrTypeId type;
    /* Present exactly for KEYWORD. */
    char* name;
} XL6_IrDynamicCallArgument;

typedef enum XL6_IrDynamicMemberBindingAbi {
    XL6_IR_DYNAMIC_MEMBER_BINDING_NONE = 0,
    /* A member read may produce an XRT callable. Its environment retains the
     * receiver once, owns it until callable destruction, and invokes the
     * matching METHOD registry through a borrowed-frame/owned-result bridge. */
    XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER
} XL6_IrDynamicMemberBindingAbi;

typedef enum XL6_IrDynamicCallEvaluationOrder {
    XL6_IR_DYNAMIC_CALL_SUBJECT_FIRST = 0,
    /* Ordinary assignment evaluates its RHS before its destination receiver. */
    XL6_IR_DYNAMIC_CALL_ARGUMENTS_FIRST
} XL6_IrDynamicCallEvaluationOrder;

typedef struct XL6_IrDynamicCallSite {
    XL6_IrNodeId origin;
    XL6_IrDynamicCallKind kind;
    XL6_IrDynamicMemberBindingAbi memberBindingAbi;
    XL6_IrDynamicCallEvaluationOrder evaluationOrder;
    /* FUNCTION: callable expression. METHOD/MEMBER_GET/MEMBER_SET: erased receiver. */
    XL6_IrNodeId subject;
    /* Direct value-storage provenance when FUNCTION invokes a named `var`.
     * This is not a statically resolved callee and is therefore kept out of
     * CallTarget; zero for compound callable expressions and METHOD. */
    XL6_SymbolId subjectSymbol;
    /* Present exactly for METHOD, MEMBER_GET and MEMBER_SET. */
    char* memberName;
    /* A METHOD on open `var` may collide with one closed standard collection
     * member.  Class dispatch has priority; this operation is the exact
     * receiver-tag fallback when no runtime class tag exists.  INVALID means
     * the call is class-only. */
    XL6_IrDynamicCollectionOp fallbackCollectionOp;
    XL6_IrTypeId resultType;
    XL6_IrDynamicCallResultAbi resultAbi;
    XL6_IrErrorAbi errorAbi;
    size_t firstArgument;
    size_t argumentCount;
    size_t positionalCount;
    size_t keywordCount;
} XL6_IrDynamicCallSite;

/* Runtime member lookup is a closed, typed registry.  A backend may emit an
 * adapter only for VERIFIED rows; UNSUPPORTED rows remain visible so a
 * missing bridge can never be mistaken for a source method that does not
 * exist.  Parameter names/defaults and the receiver tag are fixed before CFG
 * lowering, while lowering links the row to the exact CallTarget/error ABI. */
typedef enum XL6_IrDynamicMethodAdapterAbi {
    XL6_IR_DYNAMIC_METHOD_ADAPTER_INVALID = 0,
    XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED,
    XL6_IR_DYNAMIC_METHOD_ADAPTER_UNSUPPORTED
} XL6_IrDynamicMethodAdapterAbi;

/* A verified dynamic adapter never evaluates a source expression.  Defaults
 * are restricted to this closed literal set and canonicalized by lowering;
 * Cgen consumes only the tag and portable payload. */
typedef enum XL6_IrDynamicMethodDefaultAbi {
    XL6_IR_DYNAMIC_METHOD_DEFAULT_INVALID = 0,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_BOOL,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_I64,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_U64,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_F64,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_TIME,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL,
    XL6_IR_DYNAMIC_METHOD_DEFAULT_CHAR
} XL6_IrDynamicMethodDefaultAbi;

/* Non-mutating overload selection contract for an already-boxed argument. */
typedef enum XL6_IrDynamicMethodMatchAbi {
    XL6_IR_DYNAMIC_METHOD_MATCH_INVALID = 0,
    XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE,
    XL6_IR_DYNAMIC_METHOD_MATCH_BOOL,
    XL6_IR_DYNAMIC_METHOD_MATCH_I64,
    XL6_IR_DYNAMIC_METHOD_MATCH_U64,
    XL6_IR_DYNAMIC_METHOD_MATCH_F64,
    XL6_IR_DYNAMIC_METHOD_MATCH_TIME,
    XL6_IR_DYNAMIC_METHOD_MATCH_STRING,
    XL6_IR_DYNAMIC_METHOD_MATCH_CHAR
} XL6_IrDynamicMethodMatchAbi;

typedef struct XL6_IrDynamicMethodParameter {
    XL6_IrNodeId declaration;
    char* name;
    XL6_IrTypeId type;
    XL6_IrParameterAbi abi;
    XL6_IrDynamicMethodMatchAbi matchAbi;
    /* INVALID for a required/variadic parameter; otherwise the exact typed
     * expression evaluated when this candidate supplies its default. */
    XL6_IrNodeId defaultExpression;
    /* INVALID in Typed XLIR.  Lowered VERIFIED rows carry a closed source-free
     * descriptor; payload is NULL only for NONE and XVALUE_NULL. */
    XL6_IrDynamicMethodDefaultAbi defaultAbi;
    char* defaultLiteral;
    XL6_IrCallVariadicAbi variadicAbi;
} XL6_IrDynamicMethodParameter;

typedef struct XL6_IrDynamicMethod {
    /* METHOD and MEMBER_GET are distinct identities, even with equal names. */
    XL6_IrDynamicCallKind kind;
    XL6_IrNodeId ownerDeclaration;
    XL6_IrTypeId ownerType;
    uint32 runtimeTypeTag;
    XL6_IrNodeId declaration;
    XL6_SymbolId targetSymbol;
    char* name;
    size_t returnCount;
    XL6_IrTypeId returnType;
    /* INVALID in Typed XLIR.  Lowering copies the closed CallTarget effect. */
    XL6_IrErrorAbi targetErrorAbi;
    XL6_IrDynamicMethodAdapterAbi adapterAbi;
    size_t firstParameter;
    size_t parameterCount;
    size_t fixedParameterCount;
    XL6_IrCallVariadicAbi variadicAbi;
} XL6_IrDynamicMethod;

int xl6IrDynamicMethodMatchesCallSite(
    const XL6_IrDynamicCallSite* site, const XL6_IrDynamicMethod* method);

typedef struct XL6_IrIntrinsicOperand {
    XL6_IrNodeId expression;
    XL6_IrTypeId type;
    XL6_IrIntrinsicOperandAbi abi;
} XL6_IrIntrinsicOperand;

typedef struct XL6_IrIntrinsicCall {
    XL6_IrNodeId origin;
    XL6_IrIntrinsicKind kind;
    XL6_IrIntrinsicAdmission admission;
    XL6_IrTypeId resultType;
    XL6_IrIntrinsicResultAbi resultAbi;
    XL6_IrErrorAbi errorAbi;
    XL6_IrNodeId receiver;
    XL6_IrTypeId receiverType;
    /* Exact target/family for collection conversions and member operations;
     * NONE for every non-collection intrinsic. */
    XL6_IrCollectionAbi collectionAbi;
    /* Exact operation for RUNTIME_RESOURCE; INVALID for every other
     * intrinsic.  This survives lowering as one dedicated RuntimeOp. */
    XL6_IrRuntimeResourceOp runtimeResourceOp;
    /* Exact member operation for DYNAMIC_COLLECTION_MEMBER; INVALID for all
     * statically resolved and non-collection intrinsics. */
    XL6_IrDynamicCollectionOp dynamicCollectionOp;
    /* Exact operation for TYPED_COLLECTION_MEMBER; INVALID otherwise. */
    XL6_IrTypedCollectionOp typedCollectionOp;
    /* Typed task payload contract, or the exact class schema TypeId for a
     * persistence intrinsic.  payloadAbi remains INVALID for persistence. */
    XL6_IrTypeId payloadType;
    XL6_IrAsyncResultAbi payloadAbi;
    /* Exact generic runtime-resource transfer ABI.  This is independent from
     * Future payloadAbi, which remains dedicated to async completion values. */
    XL6_IrRuntimeResourcePayloadAbi runtimeResourcePayloadAbi;
    /* AvlTree only. payloadType above is the value type. */
    XL6_IrTypeId runtimeResourceKeyType;
    XL6_IrRuntimeResourcePayloadAbi runtimeResourceKeyPayloadAbi;
    XL6_IrRuntimeResourceKeyOrderAbi runtimeResourceKeyOrderAbi;
    /* Exact input Future payload for then/catch/finally; INVALID everywhere
     * else.  Output payload remains payloadType/payloadAbi. */
    XL6_IrTypeId sourcePayloadType;
    XL6_IrAsyncResultAbi sourcePayloadAbi;
    /* TYPE_OF query contract.  STATIC_ID names the exact observed TypeId;
     * DYNAMIC_XVALUE requires a runtime query of the evaluated xvalue. */
    XL6_IrTypeQueryAbi typeQueryAbi;
    XL6_IrTypeId typeQueryTarget;
    /* Valid only for STRING_CONVERT.  A class conversion additionally names
     * the exact ClassLayout through stringClassType; every other conversion
     * keeps stringClassType INVALID. */
    XL6_IrStringConversionAbi stringConversionAbi;
    XL6_IrTypeId stringClassType;
    /* Valid for SCALAR_CONVERT.  Exact source/result TypeIds are the
     * operand/result records; the family prevents later phases from treating
     * every pair as an undifferentiated C cast. */
    XL6_IrScalarConversionAbi scalarConversionAbi;
    /* CHECKED_FLOAT_CONVERT uses INVALID for the borrowed-string parse ABI or
     * XVALUE_TO_F64_CHECKED for an exact open-xvalue cast.  Every other
     * intrinsic keeps this INVALID, so lowering never rediscovers which
     * checked conversion the source spelling selected. */
    XL6_IrCastAbi castAbi;
    size_t firstOperand;
    size_t operandCount;
    XL6_IrIntegerTarget integerTarget;
    /* Valid only for CHECKED_INTEGER_CONVERT/WRAPPING_INTEGER_CONVERT. */
    XL6_IrIntegerSourceAbi integerSourceAbi;
} XL6_IrIntrinsicCall;

/* Declaration initialization is a typed language contract.  It is shared by
 * local/global value storage and class fields so lowering never derives a
 * default from C's zero-initialization rules or from a diagnostic type name. */
typedef enum XL6_IrInitializerAbi {
    XL6_IR_INITIALIZER_NONE = 0,
    XL6_IR_INITIALIZER_EXPLICIT,
    XL6_IR_INITIALIZER_SCALAR_ZERO,
    XL6_IR_INITIALIZER_STRING_EMPTY,
    XL6_IR_INITIALIZER_NULL_SENTINEL,
    XL6_IR_INITIALIZER_XVALUE_NULL,
    /* Construct an owned empty erased container using the exact runtimeOp
     * stored on XL6_IrValueInitializer.  Lowering never reclassifies a type
     * name into array/list/dict/set semantics. */
    XL6_IR_INITIALIZER_XVALUE_CONTAINER_EMPTY,
    /* Allocate the exact verified xvalue ClassLayout and initialize its tag,
     * base prefix, and ordered field-default slice. */
    XL6_IR_INITIALIZER_CLASS_DEFAULT,
    /* Materialize the language default for an exact concrete xvalue class
     * whose ClassLayout has an explicit constructor.  Such a declaration is
     * not an implicit constructor call: its initial value is the owned
     * xvalue-null sentinel.  Keep this distinct from dynamic xvalue-null so
     * Typed IR preserves the nominal class/default semantic decision. */
    XL6_IR_INITIALIZER_CLASS_NULL,
    /* Materialize the absent member of an exact tagged nullable TypeId.
     * Class lowering subsequently erases that value as the owned xvalue-null
     * field representation; local storage keeps the tagged carrier. */
    XL6_IR_INITIALIZER_TAGGED_NULL,
    /* Materialize an all-zero byte representation of one verified plain union.
     * This is explicit IR semantics, not reliance on ambient C initialization. */
    XL6_IR_INITIALIZER_UNION_ZERO,
    /* Construct the declaration's exact owning synchronization resource.
     * runtimeResourceOp fixes both the family and any language default values. */
    XL6_IR_INITIALIZER_RUNTIME_RESOURCE_DEFAULT
} XL6_IrInitializerAbi;

typedef struct XL6_IrValueInitializer {
    XL6_IrNodeId declaration;
    XL6_IrTypeId type;
    /* Present exactly for EXPLICIT; INVALID for every canonical default. */
    XL6_IrNodeId expression;
    XL6_IrInitializerAbi abi;
    /* Exact trivial scalar adaptation from the explicit expression TypeId to
     * `type`.  INVALID means that this initializer uses another boundary ABI
     * (including identity); lowering must never rediscover a scalar family. */
    XL6_IrScalarConversionAbi scalarConversionAbi;
    /* Concrete closure -> open `function` uses a verified XRT callable
     * construction operation; defaults use NONE except for erased containers. */
    XL6_IrRuntimeOp runtimeOp;
    /* Exact resource constructor for RUNTIME_RESOURCE_DEFAULT; INVALID for
     * explicit values and every non-resource default. */
    XL6_IrRuntimeResourceOp runtimeResourceOp;
    /* Generic resource defaults carry the same exact payload contract as
     * constructor intrinsics; lowering may not rediscover it from a type name. */
    XL6_IrTypeId payloadType;
    XL6_IrRuntimeResourcePayloadAbi runtimeResourcePayloadAbi;
    XL6_IrTypeId runtimeResourceKeyType;
    XL6_IrRuntimeResourcePayloadAbi runtimeResourceKeyPayloadAbi;
    XL6_IrRuntimeResourceKeyOrderAbi runtimeResourceKeyOrderAbi;
    /* Exact adapter frame shape for the callable construction operation. */
    XL6_IrCallableErasureAbi callableErasureAbi;
    /* Failure contract of the explicit runtime conversion.  This belongs to
     * the initializer rather than an unrelated operator/call descriptor. */
    XL6_IrErrorAbi errorAbi;
} XL6_IrValueInitializer;

/* Constructor spelling which binds a raw `point` into a native-backed class is
 * not a function call. Typed XLIR records both direct-handle and managed
 * class-field ABIs explicitly, so lowering never guesses a representation
 * from a class name. */
typedef enum XL6_IrNativeHandleBindAbi {
    XL6_IR_NATIVE_HANDLE_BIND_INVALID = 0,
    /* The target class has no destructor and observes the same foreign handle.
     * A source lvalue remains unchanged. */
    XL6_IR_NATIVE_HANDLE_BIND_BORROW,
    /* The target class owns destruction.  An lvalue source is explicitly
     * cleared after the handle value is captured; an rvalue transfers directly. */
    XL6_IR_NATIVE_HANDLE_BIND_TAKE,
    /* A field-only native-backed class is still an ordinary managed class
     * object. The raw pointer is borrowed into its exact native field; repeated
     * references therefore retain one stable object identity instead of
     * repeatedly synthesizing pointer-value boxes at erased boundaries. */
    XL6_IR_NATIVE_HANDLE_BIND_CLASS_FIELD_BORROW,
    /* The managed class owns the raw pointer through the exact field's
     * destructor. An lvalue source is cleared after transfer. */
    XL6_IR_NATIVE_HANDLE_BIND_CLASS_FIELD_TAKE
} XL6_IrNativeHandleBindAbi;

typedef enum XL6_IrNativeHandlePolicyAbi {
    XL6_IR_NATIVE_HANDLE_POLICY_NONE = 0,
    XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1
} XL6_IrNativeHandlePolicyAbi;

typedef struct XL6_IrNativeHandleBind {
    XL6_IrNodeId origin;
    XL6_IrTypeId targetType;
    XL6_IrNodeId source;
    XL6_IrTypeId sourceType;
    /* Direct raw-pointer storage cleared by TAKE, or zero for an rvalue. */
    XL6_SymbolId sourceSymbol;
    /* Exact extern destroy target for TAKE; zero for BORROW. */
    XL6_SymbolId destroySymbol;
    /* Exact native field initialized by CLASS_FIELD_BORROW; invalid for the
     * two direct-handle ABIs. */
    XL6_IrNodeId fieldDeclaration;
    XL6_IrNativeHandleBindAbi abi;
} XL6_IrNativeHandleBind;

/* A pure-native class does not have xvalue object storage, so its declared
 * native point field is a typed projection of the nominal handle itself.
 * This arena fixes the exact access node and declaration before lowering;
 * neither lowering nor Cgen searches for a conventional field name. */
typedef struct XL6_IrNativeHandleProject {
    XL6_IrNodeId origin;
    XL6_IrNodeId receiver;
    XL6_IrTypeId ownerType;
    XL6_IrTypeId resultType;
    XL6_IrNodeId fieldDeclaration;
} XL6_IrNativeHandleProject;

/* A call target is the complete ABI contract selected by semantic analysis.
 * It is deliberately separate from a lowered function body: an extern C
 * declaration is callable but has no XLIR CFG to emit. */
typedef enum XL6_IrCallTargetKind {
    XL6_IR_CALL_TARGET_INVALID = 0,
    XL6_IR_CALL_TARGET_INTERNAL,
    XL6_IR_CALL_TARGET_EXTERN_C,
    /* Constructor syntax for a pure native-handle class resolves to a C
     * factory returning its opaque handle.  This is distinct from an ordinary
     * extern declaration because the source special has no xlang return list. */
    XL6_IR_CALL_TARGET_NATIVE_FACTORY,
    XL6_IR_CALL_TARGET_INTERNAL_ASYNC,
    XL6_IR_CALL_TARGET_INTERNAL_GENERATOR
} XL6_IrCallTargetKind;

/* Execution boundary is independent of the callee's failure ABI. A foreign
 * call may block even when it reports failure as a bool or declares no raise.
 * LANGUAGE_CFG means its body is verified IR, NOT that its transitive callees
 * cannot block. Foreign targets are conservatively MAY_BLOCK unless an exact
 * extern declaration supplies the nonblocking contract. */
typedef enum XL6_IrCallExecutionBoundaryAbi {
    XL6_IR_CALL_EXECUTION_INVALID = 0,
    XL6_IR_CALL_EXECUTION_LANGUAGE_CFG,
    XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK,
    XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING
} XL6_IrCallExecutionBoundaryAbi;
XL6_IrCallExecutionBoundaryAbi xl6IrCallExecutionBoundaryForKind(XL6_IrCallTargetKind);
XL6_IrCallExecutionBoundaryAbi xl6IrCallExecutionBoundaryForDeclaration(
    XL6_IrCallTargetKind, XL6_AstKind, unsigned int);
const char* xl6IrCallExecutionBoundaryAbiName(XL6_IrCallExecutionBoundaryAbi);

/* Purity is a declared semantic fact, independent of failure and scheduling.
 * It is retained in XLIR for analysis and diagnostics; it is not, by itself,
 * permission to reorder a C call across ambient CRT state such as errno/fenv. */
typedef enum XL6_IrCallEffectAbi {
    XL6_IR_CALL_EFFECT_INVALID = 0,
    XL6_IR_CALL_EFFECT_CONSERVATIVE,
    XL6_IR_CALL_EFFECT_PURE
} XL6_IrCallEffectAbi;
XL6_IrCallEffectAbi xl6IrCallEffectForDeclaration(
    XL6_IrCallTargetKind, XL6_AstKind, unsigned int);
const char* xl6IrCallEffectAbiName(XL6_IrCallEffectAbi);

/* CALL transfers OWNED inputs to its invocation boundary. If preparation
 * prevents native entry, that boundary retires them in reverse parameter
 * order. It must not return detached owners to the caller or lose false/NULL
 * failure. The physical result is empty before EACH attempt: failure before
 * native entry cannot expose an uninitialized or previous invocation's owner
 * to result cleanup. NONE has no separate preparation step. */
typedef enum XL6_IrCallPreparationFailureAbi {
    XL6_IR_CALL_PREPARATION_INVALID = 0,
    XL6_IR_CALL_PREPARATION_NONE,
    XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE
} XL6_IrCallPreparationFailureAbi;
const char* xl6IrCallPreparationFailureAbiName(XL6_IrCallPreparationFailureAbi);

/* Source parameter ownership and foreign physical representation are
 * independent contracts.  Most parameters keep their canonical XLIR C
 * value, while an xlang function value crossing an `extern` boundary uses
 * the stable XL6_Function descriptor consumed by compiler-owned runtimes.
 * A raw C callback has a separate thunk ABI and is never inferred from this
 * descriptor form. */
typedef enum XL6_IrCallParameterPhysicalAbi {
    XL6_IR_CALL_PARAMETER_PHYSICAL_INVALID = 0,
    XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
    XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE,
    /* Capture-free, no-raise language closure projected to one exact ISO C
     * function-pointer signature.  The matching ClosureTarget owns the
     * adapter thunk; this is never interchangeable with XL6_Function. */
    XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK,
    /* A managed native-backed class remains an xvalue in the language ABI,
     * while one exact declared native field is borrowed as the void* expected
     * by its foreign C receiver. Lowering performs the projection; Cgen never
     * searches class fields or recognizes a conventional handle name. */
    XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW,
    /* Synchronous borrowed compiler closure with its checked invoke ABI.
     * Admitted only by an exact native contract, never inferred from a
     * source function type or confused with an erased XL6_Function. */
    XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW
} XL6_IrCallParameterPhysicalAbi;

/* A raw C callback may cross ordinary language wrapper functions before it
 * reaches the foreign sink.  Physical ABI remains DIRECT for those internal
 * parameters, but this source-independent constraint proves that every call
 * supplies a capture-free/no-raise ClosureTarget with a direct C thunk. */
typedef enum XL6_IrCallParameterConstraintAbi {
    XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE = 0,
    XL6_IR_CALL_PARAMETER_CONSTRAINT_RAW_CALLBACK_DIRECT_THUNK
} XL6_IrCallParameterConstraintAbi;

/* A call target's language result and foreign C result are independent.  A
 * native factory returns a raw pointer physically, then lowering constructs the
 * semantic managed class and initializes one exact field. */
typedef enum XL6_IrCallResultPhysicalAbi {
    XL6_IR_CALL_RESULT_PHYSICAL_INVALID = 0,
    XL6_IR_CALL_RESULT_PHYSICAL_DIRECT,
    XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW,
    XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE
} XL6_IrCallResultPhysicalAbi;

/* The C prototype's pointer type is independent of the language storage and
 * ownership conversion. A native char* may be carried as an opaque point;
 * erasing the C declaration to void* is not a compatible function type. */
typedef enum XL6_IrCallResultCAbi {
    XL6_IR_CALL_RESULT_C_ABI_STORAGE = 0,
    XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER
} XL6_IrCallResultCAbi;

/* CALL 在物理调用后立即完成拥有权转换，之后才允许释放参数和接收者。
 * SSA 结果已经是语言拥有值，C 返回的借用地址不得逃出这条调用边界。 */
typedef enum XL6_IrCallResultConversionAbi {
    XL6_IR_CALL_RESULT_CONVERSION_DIRECT = 0,
    XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY
} XL6_IrCallResultConversionAbi;

/* A callable's semantic failure effect does not determine how that failure is
 * represented at the physical C boundary. Internal functions and async/generator
 * launchers use compiler-owned result carriers. Launchers isolate allocation
 * and capture cleanup outcomes from the caller's scope. Public XRT-backed
 * bridges return raw C values and publish diagnostics through the XRT slot. Lowering
 * fixes this once on the CallTarget and every backend consumes it verbatim. */
typedef enum XL6_IrCallErrorPhysicalAbi {
    XL6_IR_CALL_ERROR_PHYSICAL_INVALID = 0,
    XL6_IR_CALL_ERROR_PHYSICAL_NONE,
    XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER,
    XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR,
    XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT,
    /* Unknown synchronous foreign code may reenter language callbacks even
     * without a callback parameter. CALL brackets a shared-runtime scope and
     * carries an owned xerror* separately from its ordinary C return value.
     * ERROR_TEST consumes that carrier after operand cleanup; it must never
     * reconstruct success from the native diagnostic TLS slot. */
    XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,
    /* The runtime operation consumes its declared inputs on both outcomes;
     * false is failure even if allocating the diagnostic itself failed. */
    XL6_IR_CALL_ERROR_PHYSICAL_FALSE_RESULT,
    /* Runtime reconstruction returns success separately from its nullable
     * language value. An immediate ERROR_TEST consumes this status, even if
     * allocating the optional diagnostic failed. */
    XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT,
    /* Consuming insertion succeeds only when both its native boolean and
     * isolated callback outcome succeed. Neither result can mask the other. */
    XL6_IR_CALL_ERROR_PHYSICAL_SCOPED_BOOL_RESULT
} XL6_IrCallErrorPhysicalAbi;

/* Compiler-private linkage is an explicit property of a resolved target.
 * Lowering selects it from the reserved declaration plus its complete ABI;
 * backends must not infer helper semantics by comparing C symbol names. */
typedef enum XL6_IrRuntimeBridgeKind {
    XL6_IR_RUNTIME_BRIDGE_NONE = 0,
    XL6_IR_RUNTIME_BRIDGE_PRINT,
    XL6_IR_RUNTIME_BRIDGE_STRING_CONTAINS,
    XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT_LINES,
    /* DynamicFields is exposed to the language through header-only stdlib
     * facades, not exported XRT symbols.  The bridge identity is fixed while
     * the complete semantic and physical call ABI is still available; Cgen
     * only emits the corresponding adapter family. */
    XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS,
    /* Header-only stdlib facade for dict.remove.  Its exact xvalue/string/
     * length ABI is fixed here so Cgen can emit the small XRT adapter without
     * importing a monolithic runtime implementation header. */
    XL6_IR_RUNTIME_BRIDGE_XVALUE_TABLE_REMOVE,
    XL6_IR_RUNTIME_BRIDGE_STRING_INDEX_OF,
    XL6_IR_RUNTIME_BRIDGE_JSON_PARSE,
    /* Converts a raw, owned C string buffer to a managed language string and
     * consumes the input allocation. */
    XL6_IR_RUNTIME_BRIDGE_TAKE_CSTR,
    /* Public-XRT-backed Template native-class component.  The exact symbol
     * selects factory, borrowed-handle operation, or destroy semantics during
     * lowering; Cgen only observes the verified component marker. */
    XL6_IR_RUNTIME_BRIDGE_TEMPLATE,
    /* Public-XRT-backed Buffer/BufferIter native-class component. */
    XL6_IR_RUNTIME_BRIDGE_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_STRING_CHAR_LEN,
    /* Owned/nullable environment values; CString validation is inside the adapter. */
    /* 字符串辅助函数逐项记录物理 ABI，后端只生成实际用到的适配器。 */
    XL6_IR_RUNTIME_BRIDGE_STRING_BYTE_LEN,
    XL6_IR_RUNTIME_BRIDGE_STRING_BYTE_AT,
    XL6_IR_RUNTIME_BRIDGE_STRING_SLICE,
    XL6_IR_RUNTIME_BRIDGE_STRING_REPLACE,
    XL6_IR_RUNTIME_BRIDGE_STRING_STARTS_WITH,
    XL6_IR_RUNTIME_BRIDGE_STRING_ENDS_WITH,
    XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT,
    XL6_IR_RUNTIME_BRIDGE_TEXT_CODEC,
    XL6_IR_RUNTIME_BRIDGE_CRYPTO_TEXT_HASH,
    /* Variadic path.join facade: two borrowed strings plus the boxed rest
     * array, returning an owned raw C string consumed by TAKE_CSTR. */
    XL6_IR_RUNTIME_BRIDGE_PATH_JOIN,
    /* Public-XRT-backed File and file/Buffer adapter component.  Lowering
     * validates the exact path, native-class-handle and result ABI before
     * assigning this marker; Cgen only selects the shared component. */
    XL6_IR_RUNTIME_BRIDGE_FILE,
    /* Public-XRT-backed TCP component split by semantic role.  These roles
     * also make asynchronous native payload adaptation explicit: dial/server
     * futures produce TcpStream, while stream futures may produce NetBytes. */
    XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL,
    XL6_IR_RUNTIME_BRIDGE_NET_TCP_LISTEN,
    XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM,
    XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER,
    /* Compiler-owned TLS wrappers are split by the lifetime and payload ABI
     * they implement.  Cgen selects the shared TLS component from these
     * verified roles and never reconstructs ownership from C symbol text. */
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_CONFIG,
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_SESSION,
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_RESUME,
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_DIAL,
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_STREAM,
    XL6_IR_RUNTIME_BRIDGE_NET_TLS_SERVER,
    /* Compiler-owned WebSocket wrappers are emitted as six dependency
     * components.  Lowering assigns the exact component identity from the
     * reserved declaration; Cgen consumes only these verified markers. */
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_FRAME,
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_HANDSHAKE,
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_UPGRADE,
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM_CONFIG,
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM,
    XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM_TLS,
    /* Public-XRT-backed HTTP/1 head/body/framing component.  WebSocket
     * upgrade depends on its owned parsed-head representation, so that
     * dependency is closed in the verified Cgen feature model. */
    XL6_IR_RUNTIME_BRIDGE_HTTP1,
    /* The raw runtime API exposes the current XRT error message as borrowed
     * text. The bridge materializes one owned managed string, making the
     * language result and destruction contract exact. */
    XL6_IR_RUNTIME_BRIDGE_LAST_ERROR_TEXT,
    /* Borrowed managed text + exact size + optional alphabet pointer -> owned
     * raw C text consumed by TAKE_CSTR; independent of Buffer admission. */
    XL6_IR_RUNTIME_BRIDGE_BASE64_TEXT_CODEC,
    XL6_IR_RUNTIME_BRIDGE_HEX_BYTES_CODEC,
    XL6_IR_RUNTIME_BRIDGE_BASE64_BYTES_CODEC,
    /* JSON value operations have exact borrowed-input/owned-result contracts.
     * Separate identities keep validation-only programs free of DOM/writer
     * dependencies; JSON_PARSE retains its existing identity. */
    XL6_IR_RUNTIME_BRIDGE_JSON_VALID,
    XL6_IR_RUNTIME_BRIDGE_JSON_PARSE_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSON_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_JSON_WRITE_FILE,
    /* Synchronous owned-event delivery via a borrowed var -> bool callable;
     * managed native Writer factories/methods/destroy use a separate role. */
    XL6_IR_RUNTIME_BRIDGE_JSON_VISIT,
    XL6_IR_RUNTIME_BRIDGE_JSON_WRITER,
    /* XLON options preserve language bool and exact visitor/native-handle ABI. */
    XL6_IR_RUNTIME_BRIDGE_XLON_PARSE,
    XL6_IR_RUNTIME_BRIDGE_XLON_VALID,
    XL6_IR_RUNTIME_BRIDGE_XLON_READ,
    XL6_IR_RUNTIME_BRIDGE_XLON_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_XLON_WRITE,
    XL6_IR_RUNTIME_BRIDGE_XLON_VISIT,
    XL6_IR_RUNTIME_BRIDGE_XLON_WRITER,
    /* Line-oriented formats share the XRT value model, but each adapter has
     * its own verified dependency so JSONL never drags in XLONL or vice versa. */
    XL6_IR_RUNTIME_BRIDGE_JSONL_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_XLONL_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_JSONL_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_PARSE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_VALID,
    XL6_IR_RUNTIME_BRIDGE_XLONL_PARSE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_VALID,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_PARSE,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_PARSE,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_PARSE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_PARSE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_STRINGIFY,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_WRITE_TO,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_WRITE_TO,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_WRITE_TO,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_WRITE_TO,
    XL6_IR_RUNTIME_BRIDGE_JSON_QUOTE,
    XL6_IR_RUNTIME_BRIDGE_JSON_QUOTE_TO,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_VISIT,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_VISIT,
    XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_VALID,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_VALID,
    XL6_IR_RUNTIME_BRIDGE_JSONL_CONFIG_VALID,
    XL6_IR_RUNTIME_BRIDGE_XLONL_CONFIG_VALID,
    XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_DECODE,
    /* Borrowed text and explicit success status for standard streams. */
    XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE,
    XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN,
    XL6_IR_RUNTIME_BRIDGE_TEXT_ERROR,
    XL6_IR_RUNTIME_BRIDGE_TEXT_ERRORLN,
    XL6_IR_RUNTIME_BRIDGE_TEXT_FLUSH,
    XL6_IR_RUNTIME_BRIDGE_DIR_LIST,
    XL6_IR_RUNTIME_BRIDGE_DIR_GLOB,
    XL6_IR_RUNTIME_BRIDGE_DIR_ENTRIES,
    XL6_IR_RUNTIME_BRIDGE_DIR_IS_EMPTY,
    XL6_IR_RUNTIME_BRIDGE_DIR_COUNT,
    XL6_IR_RUNTIME_BRIDGE_DIR_SIZE,
    /* Exact, non-allocating XRT hash adapters for language value views. */
    XL6_IR_RUNTIME_BRIDGE_HASH_STRING,
    XL6_IR_RUNTIME_BRIDGE_HASH_BYTES,
    XL6_IR_RUNTIME_BRIDGE_HASH_RAW,
    XL6_IR_RUNTIME_BRIDGE_SIPHASH_STRING,
    XL6_IR_RUNTIME_BRIDGE_SIPHASH_BYTES,
    XL6_IR_RUNTIME_BRIDGE_SIPHASH_RAW,
    /* Nominal 24-byte XID values use a single inline xvalue allocation.
     * Every operation has a distinct identity and an exact result/argument
     * contract; no C backend recognizes source spellings or guesses handles. */
    XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING,
    XL6_IR_RUNTIME_BRIDGE_XID_MAKE,
    XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY,
    XL6_IR_RUNTIME_BRIDGE_XID_PARSE,
    XL6_IR_RUNTIME_BRIDGE_XID_FROM_BYTES,
    XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING,
    XL6_IR_RUNTIME_BRIDGE_XID_TO_BYTES,
    XL6_IR_RUNTIME_BRIDGE_XID_TIMESTAMP,
    XL6_IR_RUNTIME_BRIDGE_XID_COMPARE,
    XL6_IR_RUNTIME_BRIDGE_XID_EQUAL,
    XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO,
    /* Exact binary file I/O; byte values stay length-aware end to end. */
    XL6_IR_RUNTIME_BRIDGE_IO_READ_FILE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITE_FILE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITE_TEXT_FILE,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_OPEN,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_BYTES,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_STDIN,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_CLOSE,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_HANDLE,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_READ,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_EXACT,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_ALL,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_INTO,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_SEEK,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_TELL,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_SIZE,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_EOF,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_CAN_SEEK,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_CAN_SIZE,
    XL6_IR_RUNTIME_BRIDGE_IO_READER_CLOSED,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_OPEN,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_STANDARD,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CLOSE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_HANDLE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_BYTES,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TEXT,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_WRITE_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_BYTES,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_TEXT,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_FLUSH,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_SEEK,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TELL,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_SIZE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CAN_SEEK,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CAN_SIZE,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CLOSED,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TO_BYTES,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_CREATE,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_CLOSE,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_ADVANCE,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_BYTES,
    XL6_IR_RUNTIME_BRIDGE_IO_LINES_ENDING,
    XL6_IR_RUNTIME_BRIDGE_IO_COPY,
    XL6_IR_RUNTIME_BRIDGE_IO_WRITE_BUFFER_FILE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_CHAR,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_IS_TERMINAL,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SIZE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_COLOR,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_STYLED,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_OPEN,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLOSE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLOSED,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_PASTE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_WRITE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_FLUSH,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_MOVE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLEAR,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CURSOR,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_STYLE,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_READ,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_WRAP,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_KIND,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_TEXT,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_KEY,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_MODIFIERS,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_REPEAT,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_X,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_Y,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_WHEEL,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_COLUMNS,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_ROWS,
    XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_DOWN,
    XL6_IR_RUNTIME_BRIDGE_VFS_CREATE,
    XL6_IR_RUNTIME_BRIDGE_VFS_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_VFS_READ,
    XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_CREATE,
    XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_BYTES,
    XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_STRING,
    XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_SEAL,
    XL6_IR_RUNTIME_BRIDGE_VFS_DISK_CREATE,
    XL6_IR_RUNTIME_BRIDGE_VFS_DISK_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_VFS_PACK_CREATE,
    XL6_IR_RUNTIME_BRIDGE_VFS_PACK_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_VFS_PACK_FORMAT,
    XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_MEMORY,
    XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DISK,
    XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_PACK,
    XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_VFS_UNMOUNT,
    XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_ID,
#define XL6_FS_OPERATION(Name, ...) XL6_IR_RUNTIME_BRIDGE_FS_##Name,
#include "src/ir/xl6_ir_fs_operations.inc"
#undef XL6_FS_OPERATION
    XL6_IR_RUNTIME_BRIDGE_FS_END,
    XL6_IR_RUNTIME_BRIDGE_STRING_REPEAT,
    XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST,
    XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC,
    XL6_IR_RUNTIME_BRIDGE_NET_UDP_SOCKET,
    XL6_IR_RUNTIME_BRIDGE_NET_UDP_PACKET,
    XL6_IR_RUNTIME_BRIDGE_NET_UDP_BATCH,
    XL6_IR_RUNTIME_BRIDGE_NET_UDP_RECV_ASYNC,
    XL6_IR_RUNTIME_BRIDGE_NET_ENGINE_STATS,
    XL6_IR_RUNTIME_BRIDGE_NET_RESOLVER_STATS,
    XL6_IR_RUNTIME_BRIDGE_NET_UDP_STATS,
    /* Compiler-embedding helpers emitted by the selected backend component.
     * Their exact class-handle/string/xvalue ABI is fixed during lowering. */
    XL6_IR_RUNTIME_BRIDGE_XLANG,
    XL6_IR_RUNTIME_BRIDGE_STRING_JOIN,
    XL6_IR_RUNTIME_BRIDGE_STRING_BYTES,
    XL6_IR_RUNTIME_BRIDGE_STRING_CHARS,
    XL6_IR_RUNTIME_BRIDGE_STRING_COMPARE,
    XL6_IR_RUNTIME_BRIDGE_STRING_LAST_INDEX_OF,
    XL6_IR_RUNTIME_BRIDGE_STRING_COUNT,
    XL6_IR_RUNTIME_BRIDGE_STRING_HAS_ANY,
    XL6_IR_RUNTIME_BRIDGE_STRING_LIKE,
    XL6_IR_RUNTIME_BRIDGE_STRING_APPROX,
    XL6_IR_RUNTIME_BRIDGE_STRING_SIMILARITY,
    XL6_IR_RUNTIME_BRIDGE_STRING_IS_UTF8,
    XL6_IR_RUNTIME_BRIDGE_STRING_LOWER,
    XL6_IR_RUNTIME_BRIDGE_STRING_UPPER,
    XL6_IR_RUNTIME_BRIDGE_STRING_TRIM,
    XL6_IR_RUNTIME_BRIDGE_STRING_LTRIM,
    XL6_IR_RUNTIME_BRIDGE_STRING_RTRIM,
    XL6_IR_RUNTIME_BRIDGE_STRING_FILTER,
    XL6_IR_RUNTIME_BRIDGE_STRING_LEFT,
    XL6_IR_RUNTIME_BRIDGE_STRING_RIGHT,
    XL6_IR_RUNTIME_BRIDGE_STRING_CHAR_AT,
    XL6_IR_RUNTIME_BRIDGE_STRING_IS_EMPTY,
    XL6_IR_RUNTIME_BRIDGE_STRING_IS_BLANK,
    XL6_IR_RUNTIME_BRIDGE_STRING_REMOVE,
    XL6_IR_RUNTIME_BRIDGE_STRING_REMOVE_AT,
    XL6_IR_RUNTIME_BRIDGE_STRING_INSERT,
    XL6_IR_RUNTIME_BRIDGE_STRING_PAD_LEFT,
    XL6_IR_RUNTIME_BRIDGE_STRING_PAD_RIGHT,
    XL6_IR_RUNTIME_BRIDGE_STRING_PAD_CENTER,
    XL6_IR_RUNTIME_BRIDGE_STRING_REVERSE,
    /* Resident, independently compiled logging adapters. */
    XL6_IR_RUNTIME_BRIDGE_LOG,
    /* Exact-length owned text from an independently compiled provider. */
    XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE
} XL6_IrRuntimeBridgeKind;

/* Some runtime components contain several operations with different emitted
 * dependencies or payload adaptation.  The broad bridge kind identifies the
 * component/lifetime role; this operation identifies the exact exceptional
 * sub-role which a backend must otherwise rediscover from the C symbol.  It
 * is assigned while the declaration name and complete ABI are available and
 * verified again before any backend runs. */
typedef enum XL6_IrRuntimeBridgeOperation {
    XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE = 0,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_FILE_TRANSFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_RECV_OWNED_BYTES_ASYNC,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TLS_RECV_OWNED_BYTES_ASYNC,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_CREATE,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_CREATE_DEFAULT,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_SEED,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_U32,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_U64,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_BELOW,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_RANGE,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_RANGE_CLOSED,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_REAL,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_BYTES,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_STRING,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_FILL_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_SHUFFLE_ARRAY,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_SHUFFLE_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_SEED,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_U32,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_U64,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_BELOW,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_RANGE,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_RANGE_CLOSED,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_REAL,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_BYTES,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_STRING,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_FILL_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_SHUFFLE_ARRAY,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_RAND_SHUFFLE_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SECURE_BYTES,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SECURE_FILL_BUFFER,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SECURE_HEX,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SECURE_STRING,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_CREATE,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_ADOPT,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_DESTROY,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_UPDATE_STRING,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_UPDATE_BYTES,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_UPDATE_RAW,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_DIGEST,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_RESET,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_CLONE,
    XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_INPUT_BYTES
} XL6_IrRuntimeBridgeOperation;

/* A callable's source-independent lifecycle role.  Ordinary BORROW
 * parameters become callee-owned lexical bindings at function entry, but a
 * class destroy receiver is the object currently being finalized by XRT: it
 * must remain a pure borrow and must never be retained, released, rebound or
 * captured by the body. */
typedef enum XL6_IrCallTargetLifecycleAbi {
    XL6_IR_CALL_TARGET_LIFECYCLE_NONE = 0,
    XL6_IR_CALL_TARGET_LIFECYCLE_CLASS_DESTROY_BORROW
} XL6_IrCallTargetLifecycleAbi;

/* The launcher is part of the resolved callable ABI.  It prevents a C
 * backend from deciding whether a source async call means a thread, pool, or
 * coroutine after lowering has completed. */
typedef enum XL6_IrAsyncDispatchKind {
    XL6_IR_ASYNC_DISPATCH_NONE = 0,
    XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL,
    /* A verified immediate worker with no suspension settles its result into
     * a fresh Future at launch time.  A no-raise worker resolves; a may-raise
     * worker consumes its explicit result carrier and rejects.  It has no
     * task frame and never occupies the module pool. */
    XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE
} XL6_IrAsyncDispatchKind;

/* Capturing a call argument into a task frame has a distinct lifetime from
 * passing it to the worker.  This first ABI owns a copied trivial value; a
 * managed capture requires its own retain/move and frame-destroy contract. */
typedef enum XL6_IrAsyncCaptureAbi {
    XL6_IR_ASYNC_CAPTURE_INVALID = 0,
    XL6_IR_ASYNC_CAPTURE_COPY_VALUE,
    /* Clones a borrowed managed string into the task frame. */
    XL6_IR_ASYNC_CAPTURE_COPY_STRING,
    /* Retains a borrowed xvalue into the task frame. */
    XL6_IR_ASYNC_CAPTURE_RETAIN_XVALUE,
    /* Retains a future handle into the task frame and releases it when the
     * accepted task completes. */
    XL6_IR_ASYNC_CAPTURE_RETAIN_FUTURE,
    /* Retains a first-class closure into the task frame. */
    XL6_IR_ASYNC_CAPTURE_RETAIN_CLOSURE,
    /* Retains a borrowed language error value into the task frame. */
    XL6_IR_ASYNC_CAPTURE_RETAIN_ERROR,
    /* Takes the caller's already-owned byval argument; never retain again. */
    XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER
} XL6_IrAsyncCaptureAbi;

/* Closure capture ownership is a separate ABI from a direct-call parameter.
 * A capture remains live after its source activation returns, so lowering
 * chooses the transfer here and emits its matching frame destruction edge. */
typedef enum XL6_IrClosureCaptureAbi {
    XL6_IR_CLOSURE_CAPTURE_INVALID = 0,
    XL6_IR_CLOSURE_CAPTURE_COPY_VALUE,
    XL6_IR_CLOSURE_CAPTURE_COPY_STRING,
    XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE,
    XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE,
    XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE,
    /* The capture operand is an owned/shared CELL value while `type` remains
     * the language-visible element type.  `cellType` fixes the concrete cell
     * ABI so a body LOAD/STORE never reaches into an environment implicitly.
     * Once any capture writes a binding, all captures of that sourceSymbol
     * share this identity, including read-only siblings and descendants.
     * Retain/release of the cell reference is atomic; payload access itself
     * is not synchronized by retaining a reference. */
    XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL,
    /* Preserve an absent string as NULL. A non-null input is copied and its
     * allocation must succeed before the environment can be committed. */
    XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING
} XL6_IrClosureCaptureAbi;

typedef struct XL6_IrClosureCapture {
    XL6_SymbolId sourceSymbol;
    /* Synthetic storage belonging to the closure body.  This breaks the
     * source activation dependency: the body never reads a creator C local. */
    XL6_SymbolId storageSymbol;
    XL6_IrTypeId type;
    XL6_IrTypeId cellType;
    XL6_IrClosureCaptureAbi abi;
} XL6_IrClosureCapture;

typedef enum XL6_IrClosureTargetKind {
    XL6_IR_CLOSURE_TARGET_INVALID = 0,
    XL6_IR_CLOSURE_TARGET_ANONYMOUS,
    XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER
} XL6_IrClosureTargetKind;

typedef enum XL6_IrClosureRawCallbackAbi {
    XL6_IR_CLOSURE_RAW_CALLBACK_INVALID = 0,
    XL6_IR_CLOSURE_RAW_CALLBACK_NONE,
    /* The target is capture-free, no-raise and has an exact raw C signature.
     * Cgen emits one static function-pointer thunk for this target. */
    XL6_IR_CLOSURE_RAW_CALLBACK_DIRECT_THUNK,
    /* Capture-free, exact raw C signature, may-raise target. Failure publishes
     * the original exception to the shared foreign-call scope and returns a
     * zero scalar/pointer (or void) to C. Native execution is not unwound;
     * the owning foreign CALL takes its explicit error edge on return. */
    XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK
} XL6_IrClosureRawCallbackAbi;

/* A closure target names the independently lowered body plus the complete
 * function signature and capture slice needed to construct its environment.
 * This is deliberately not a C function pointer: it is verified XLIR that a
 * backend renders through the uniform bool + optional out-result thunk ABI. */
typedef struct XL6_IrClosureTarget {
    XL6_IrClosureTargetKind kind;
    XL6_IrNodeId declaration;
    /* Lowered closure-body or synthetic adapter function symbol. */
    XL6_SymbolId targetSymbol;
    /* Resolved named call target consumed by a NAMED_ADAPTER body; zero for
     * an anonymous closure whose targetSymbol is its source body symbol. */
    XL6_SymbolId adaptedTargetSymbol;
    XL6_IrReachability reachability;
    XL6_IrTypeId signatureType;
    XL6_IrClosureRawCallbackAbi rawCallbackAbi;
    /* Runtime keyword binding belongs to the closure value, not to its
     * structural function TypeId.  Names are copied from the exact anonymous
     * declaration or named-adapter target during lowering.  Defaults have the
     * same value-specific identity: NONE denotes a required parameter,
     * INVALID denotes a source default which has no closed runtime binding
     * ABI, and every other row owns one canonical literal payload.  This lets
     * one structural function TypeId safely describe closures with different
     * required/default partitions without asking Cgen to inspect a source
     * declaration. */
    char** callParameterNames;
    XL6_IrDynamicMethodDefaultAbi* callParameterDefaultAbis;
    char** callParameterDefaultLiterals;
    size_t callParameterCount;
    size_t firstCapture;
    size_t captureCount;
} XL6_IrClosureTarget;


/* Frame acquisition and destruction are two independently verified facts.
 * Cgen must not infer a destructor from the slot C type or from a resume
 * operator.  COPY_VALUE owns no destructor; every managed acquisition has one
 * exact inverse used on allocation, registration, cancellation and normal
 * completion paths. */
typedef enum XL6_IrAsyncFrameDestroyAbi {
    XL6_IR_ASYNC_FRAME_DESTROY_INVALID = 0,
    XL6_IR_ASYNC_FRAME_DESTROY_NONE,
    XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING,
    XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE,
    XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE,
    XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE,
    XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL,
    XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR,
    /* Compiler-owned cursor used by the canonical xvalue iterator ABI.  A
     * cursor may be lexical frame storage when an await occurs inside a
     * collection loop; its exact drop glue must therefore survive suspension
     * as an explicit frame action. */
    XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR,
    /* Unique synchronous continuation activation. */
    XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_GENERATOR
} XL6_IrAsyncFrameDestroyAbi;

/* Physical strong slots, independently checked against the exact destruction
 * ABI after resident loans are bound. NONE includes unique byte/string memory
 * with no outgoing owner, never a managed loan's protecting owner. OPAQUE is
 * an explicit inspection refusal for not-yet-described unique runtime state;
 * it must not be lowered to an empty trace or prevent normal execution. */
typedef enum XL6_IrContinuationFrameTraceAbi {
    XL6_IR_FRAME_TRACE_INVALID = 0,
    XL6_IR_FRAME_TRACE_NONE,
    XL6_IR_FRAME_TRACE_XVALUE,
    XL6_IR_FRAME_TRACE_FUTURE,
    XL6_IR_FRAME_TRACE_CLOSURE,
    XL6_IR_FRAME_TRACE_CELL,
    XL6_IR_FRAME_TRACE_ERROR,
    XL6_IR_FRAME_TRACE_OPAQUE,
    XL6_IR_FRAME_TRACE_VALUE_ITERATOR,
    XL6_IR_FRAME_TRACE_GENERATOR
} XL6_IrContinuationFrameTraceAbi;

/* Async lowering is driven exclusively by the lowered CFG. */
typedef enum XL6_IrAsyncSpillSource {
    XL6_IR_ASYNC_SPILL_SOURCE_INVALID = 0,
    XL6_IR_ASYNC_SPILL_SOURCE_VALUE,
    XL6_IR_ASYNC_SPILL_SOURCE_STORAGE
} XL6_IrAsyncSpillSource;

/* Transfer into a machine frame is an ownership operation, not a property of
 * the stored type alone.  NONE is valid only for FRAME_STORAGE: the field is
 * the canonical lexical storage and is populated by ordinary STORE edges.
 * COPY/RETAIN acquire an independent owner from a borrowed source.  MOVE owns
 * an already-owned SSA result without cloning it.  Destruction remains a
 * separate representation-specific ABI below. */
typedef enum XL6_IrAsyncFrameTransferAbi {
    XL6_IR_ASYNC_FRAME_TRANSFER_INVALID = 0,
    XL6_IR_ASYNC_FRAME_TRANSFER_NONE,
    XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE,
    XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING,
    XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE,
    XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE,
    XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE,
    XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CELL,
    XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR,
    XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED,
    /* Launch-only ownership transfer from a consuming parameter, not an SSA
     * spill. All such inputs are bound before any fallible borrowed capture. */
    XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER,
    /* Non-owning SSA pointer protected by an exact activation storage slot.
     * No retain, allocation, init bit or independent destructor is permitted. */
    XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE
} XL6_IrAsyncFrameTransferAbi;

/* Runtime callbacks are activations too. Their captures are acquired in
 * operand order, dropped in reverse order before result publication, and
 * detached before calling user drop glue. Cleanup preserves a pending primary
 * error/terminal; otherwise its first error aborts the pending success.
 * Entry and fallback destruction isolate the caller's thread error context. */
typedef enum XL6_IrCallbackTransportAbi {
    XL6_IR_CALLBACK_TRANSPORT_INVALID = 0,
    XL6_IR_CALLBACK_TRANSPORT_TASK_POOL,
    /* The activation itself handles pre-invocation cancellation and publishes
     * only after cleanup; a runtime adapter must not bypass that barrier. */
    XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH
} XL6_IrCallbackTransportAbi;
typedef enum XL6_IrCallbackStorageAbi {
    XL6_IR_CALLBACK_STORAGE_INVALID = 0,
    XL6_IR_CALLBACK_STORAGE_RESIDENT_TASK_FRAME_V1,
    /* Same-thread accepted code capability may parent a child continuation;
     * ordinary registration still needs new owner admission. */
    XL6_IR_CALLBACK_STORAGE_RESIDENT_FUTURE_FRAME_V1,
    /* Closed physical capture layouts. V1 values remain reserved but are not
     * valid Lowered callback contracts: opaque Drop/Trace is not a layout. */
    XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2,
    XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V2
} XL6_IrCallbackStorageAbi;
typedef enum XL6_IrCallbackCleanupAbi {
    XL6_IR_CALLBACK_CLEANUP_INVALID = 0,
    XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL
} XL6_IrCallbackCleanupAbi;
typedef enum XL6_IrCallbackErrorAbi {
    XL6_IR_CALLBACK_ERROR_INVALID = 0,
    XL6_IR_CALLBACK_ERROR_PRESERVE,
    XL6_IR_CALLBACK_ERROR_ASYNC_CAUSE
} XL6_IrCallbackErrorAbi;
typedef enum XL6_IrCallbackForwardAbi {
    XL6_IR_CALLBACK_FORWARD_INVALID = 0,
    XL6_IR_CALLBACK_FORWARD_NONE,
    XL6_IR_CALLBACK_FORWARD_SOURCE_OWNER
} XL6_IrCallbackForwardAbi;
typedef enum XL6_IrCallbackCaptureRole {
    XL6_IR_CALLBACK_CAPTURE_INVALID = 0,
    XL6_IR_CALLBACK_CAPTURE_SOURCE,
    XL6_IR_CALLBACK_CAPTURE_CALLABLE,
    XL6_IR_CALLBACK_CAPTURE_ARGUMENT
} XL6_IrCallbackCaptureRole;
/* Capture acquisition keeps the caller intact. Invocation independently says
 * whether the frame keeps that owner or commits it into a consuming callee. */
typedef enum XL6_IrCallbackInvokeAbi {
    XL6_IR_CALLBACK_INVOKE_INVALID = 0,
    XL6_IR_CALLBACK_INVOKE_CONTEXT,
    XL6_IR_CALLBACK_INVOKE_VALUE,
    XL6_IR_CALLBACK_INVOKE_BORROW,
    XL6_IR_CALLBACK_INVOKE_TAKE
} XL6_IrCallbackInvokeAbi;
typedef struct XL6_IrCallbackCapture {
    size_t operand;
    XL6_IrValueId value;
    XL6_IrTypeId type;
    XL6_IrCallbackCaptureRole role;
    XL6_IrCallbackInvokeAbi invokeAbi;
    XL6_IrAsyncFrameTransferAbi transferAbi;
    XL6_IrAsyncFrameDestroyAbi destroyAbi;
    XL6_IrContinuationFrameTraceAbi traceAbi;
} XL6_IrCallbackCapture;
typedef struct XL6_IrCallbackBoundary {
    size_t instruction;
    size_t firstCapture;
    size_t captureCount;
    XL6_IrCallbackTransportAbi transportAbi;
    XL6_IrCallbackStorageAbi storageAbi;
    XL6_IrCallbackCleanupAbi cleanupAbi;
    XL6_IrCallbackErrorAbi errorAbi;
    XL6_IrCallbackForwardAbi forwardAbi;
} XL6_IrCallbackBoundary;

/* A bidirectional callable transport is a verified parameter program, not a
 * backend rule inferred from an OWNED/BORROW pointer spelling. Decode owns
 * exactly decodedDestroyAbi; invocation commits TAKE slots. Reverse entry
 * owns inputDestroyAbi until encode moves it or cleanup releases it. */
typedef enum XL6_IrCallableBridgeValueAbi {
    XL6_IR_CALLABLE_BRIDGE_INVALID = 0,
    XL6_IR_CALLABLE_BRIDGE_BOOL,
    XL6_IR_CALLABLE_BRIDGE_I64,
    XL6_IR_CALLABLE_BRIDGE_U64,
    XL6_IR_CALLABLE_BRIDGE_F64,
    XL6_IR_CALLABLE_BRIDGE_POINTER,
    XL6_IR_CALLABLE_BRIDGE_STRING_COPY,
    XL6_IR_CALLABLE_BRIDGE_ERROR_REF,
    XL6_IR_CALLABLE_BRIDGE_XVALUE_BORROW,
    XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN,
    XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE,
    XL6_IR_CALLABLE_BRIDGE_ARRAY_PACK,
    XL6_IR_CALLABLE_BRIDGE_DICT_PACK,
    XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY,
    XL6_IR_CALLABLE_BRIDGE_CLOSURE_REF,
    /* Preserve the language/XValue char identity across erased calls.  This
     * is deliberately not U64 even though both use an unsigned C carrier. */
    XL6_IR_CALLABLE_BRIDGE_CHAR,
    /* `bytes` is physically an xvalue*, but these opcodes preserve and check
     * its XVALUE_BYTES identity instead of treating it as open `var`. */
    XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW,
    XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN,
    XL6_IR_CALLABLE_BRIDGE_BYTES_MOVE
} XL6_IrCallableBridgeValueAbi;

typedef struct XL6_IrCallableBridgeParameter {
    size_t parameter;
    XL6_IrTypeId type;
    XL6_IrCallableBridgeValueAbi decodeAbi;
    XL6_IrCallableBridgeValueAbi encodeAbi;
    XL6_IrCallbackInvokeAbi invokeAbi;
    XL6_IrAsyncFrameDestroyAbi decodedDestroyAbi;
    XL6_IrAsyncFrameDestroyAbi inputDestroyAbi;
} XL6_IrCallableBridgeParameter;

/* Results remain private until conversion and every required cleanup succeeds.
 * Boxed frame cleanup replaces slots through XRT's API before destroying them,
 * observes each failure, and drains in reverse order. No public frame fields
 * may be mutated directly. */
typedef enum XL6_IrCallableBridgeResultCommitAbi {
    XL6_IR_CALLABLE_RESULT_COMMIT_INVALID = 0,
    XL6_IR_CALLABLE_RESULT_COMMIT_AFTER_CLEANUP
} XL6_IrCallableBridgeResultCommitAbi;

typedef enum XL6_IrCallableBridgeFrameCleanupAbi {
    XL6_IR_CALLABLE_FRAME_CLEANUP_INVALID = 0,
    XL6_IR_CALLABLE_FRAME_CLEANUP_REPLACE_REVERSE
} XL6_IrCallableBridgeFrameCleanupAbi;

typedef struct XL6_IrCallableBridgeResult {
    size_t result;
    XL6_IrTypeId type;
    XL6_IrCallableBridgeValueAbi decodeAbi;
    XL6_IrCallableBridgeValueAbi encodeAbi;
    XL6_IrAsyncFrameDestroyAbi destroyAbi;
} XL6_IrCallableBridgeResult;

typedef enum XL6_IrCallableBridgeErrorBoundaryAbi {
    XL6_IR_CALLABLE_ERROR_BOUNDARY_INVALID = 0,
    XL6_IR_CALLABLE_ERROR_BOUNDARY_NATIVE_CALLBACK,
    XL6_IR_CALLABLE_ERROR_BOUNDARY_ISOLATED_STATUS
} XL6_IrCallableBridgeErrorBoundaryAbi;

typedef struct XL6_IrCallableBridge {
    XL6_IrTypeId signatureType;
    size_t firstParameter;
    size_t parameterCount;
    size_t fixedParameterCount;
    size_t firstResult;
    size_t resultCount;
    XL6_IrTypeId resultType;
    XL6_IrCallableBridgeResultCommitAbi resultCommitAbi;
    XL6_IrCallableBridgeFrameCleanupAbi frameCleanupAbi;
    XL6_IrCallableBridgeErrorBoundaryAbi forwardErrorBoundaryAbi;
    XL6_IrCallableBridgeErrorBoundaryAbi reverseErrorBoundaryAbi;
    XL6_IrCallableErasureAbi erasureAbi;
    XL6_IrCallbackCleanupAbi cleanupAbi;
    XL6_IrCallbackErrorAbi errorAbi;
    int reverseSupported;
} XL6_IrCallableBridge;

/* Physical placement is independent from the transfer ABI.  Parameters
 * are initialized when the activation is launched, lexical storage is mapped
 * to the frame for the whole transformed function, and SSA values are spilled
 * only at a suspension boundary. */
typedef enum XL6_IrAsyncFrameResidence {
    XL6_IR_ASYNC_FRAME_RESIDENCE_INVALID = 0,
    XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER,
    XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE,
    XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL
} XL6_IrAsyncFrameResidence;

typedef enum XL6_IrAsyncMachineStatus {
    XL6_IR_ASYNC_MACHINE_INVALID = 0,
    /* Suspension boundaries and every live-slot lifetime ABI are complete. */
    XL6_IR_ASYNC_MACHINE_ANALYZED,
    /* CFG/liveness is complete, but at least one frame slot has no safe
     * representation yet.  This remains task-pool-only and cannot be emitted
     * as a continuation machine. */
    XL6_IR_ASYNC_MACHINE_BLOCKED_LIVE_ABI
} XL6_IrAsyncMachineStatus;

typedef struct XL6_IrAsyncMachineFrameSlot {
    XL6_IrAsyncSpillSource source;
    XL6_IrAsyncFrameResidence residence;
    XL6_IrValueId value;
    XL6_SymbolId storageSymbol;
    XL6_IrTypeId type;
    XL6_IrAsyncFrameTransferAbi transferAbi;
    XL6_IrAsyncFrameDestroyAbi destroyAbi;
    XL6_IrContinuationFrameTraceAbi traceAbi;
    /* Parameter index for a PARAMETER storage captured at launch; SIZE_MAX
     * for locals and SSA spills materialized by the machine itself. */
    size_t parameter;
    /* Lexical cleanup ownership copied from the source definition/declaration
     * before backend entry.  Synthetic temporaries use zero/zero. */
    uint32 scopeDepth;
    uint32 region;
    /* Inclusive state-id range in which this field participates. Gaps are
     * represented by suspension membership, never by widening this range.
     * UINT32_MAX/UINT32_MAX means a mutable launch parameter has activation
     * storage but is not live across any suspension. */
    uint32 firstLiveState;
    uint32 lastLiveState;
    /* Absolute frame-slot index in this machine family protecting a
     * BORROW_STORAGE spill; SIZE_MAX for all other transfers. The verifier
     * recomputes provenance and requires the owner at every live suspension. */
    size_t borrowFrameSlot;
    /* Dense per-machine initialization bit for managed fields.  Trivial fields
     * use SIZE_MAX.  Drop glue must test/set this bit and cannot assume a NULL
     * representation is a sufficient partial-initialization protocol. */
    size_t initBit;
} XL6_IrAsyncMachineFrameSlot;

/* Suspension membership separates SSA views from unique physical residents.
 * Several transparent views may name one owned slot, never several owners. */
typedef struct XL6_IrAsyncLiveSlot {
    size_t frameSlot;
    /* SSA view projected from the resident slot; zero for storage membership. */
    XL6_IrValueId value;
} XL6_IrAsyncLiveSlot;

/* A machine action is an already ordered lowering command, not a liveness
 * hint. Storage fields are installed once. Each reached suspension installs
 * residents and each resume restores explicit SSA views. Transfer commands
 * retire physical owners at their actual consuming CFG instructions. */
typedef enum XL6_IrAsyncFrameActionKind {
    XL6_IR_ASYNC_FRAME_ACTION_INVALID = 0,
    XL6_IR_ASYNC_FRAME_ACTION_INIT_PARAMETER,
    XL6_IR_ASYNC_FRAME_ACTION_BIND_STORAGE,
    XL6_IR_ASYNC_FRAME_ACTION_SPILL_VALUE,
    XL6_IR_ASYNC_FRAME_ACTION_PROJECT_VALUE,
    XL6_IR_ASYNC_FRAME_ACTION_DESTROY_IF_INITIALIZED,
    XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION,
    XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT,
    /* Move/copy one unique physical field to the next heap activation.
     * This is independent of how many SSA views project from that field.
     * Every initialized owning field belongs to the logical activation until
     * an explicit release/consumer or its completion program retires it;
     * absence from a read-liveness set is not an implicit destroy command.
     * Therefore managed fields always hand off (including their zero init
     * state), while trivial fields only need their current live membership. */
    XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT,
    /* Restore an unaccepted next activation to its caller, in reverse order. */
    XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT
} XL6_IrAsyncFrameActionKind;

typedef struct XL6_IrAsyncFrameAction {
    XL6_IrAsyncFrameActionKind kind;
    /* Zero-based machine index. */
    size_t machine;
    /* Zero-based suspension index for SPILL/PROJECT/HANDOFF; SIZE_MAX otherwise. */
    size_t suspension;
    /* Zero-based module asyncMachineFrameSlots index. */
    size_t frameSlot;
    /* Exact projected/spilled/consumed SSA view; zero for storage/drop actions. */
    XL6_IrValueId value;
    /* Exact consuming instruction for TRANSFER actions, SIZE_MAX otherwise. */
    size_t instruction;
} XL6_IrAsyncFrameAction;

/* A resumable machine enters its canonical CFG only through these records.
 * LAUNCH begins at the function entry.  A Future callback selects exactly one
 * success, failure, cancelled or closed edge for the suspension it completes.  This
 * keeps callback-to-CFG control transfer in XLIR instead of a Cgen convention. */
typedef enum XL6_IrAsyncMachineEntryKind {
    XL6_IR_ASYNC_MACHINE_ENTRY_INVALID = 0,
    XL6_IR_ASYNC_MACHINE_ENTRY_LAUNCH,
    XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_SUCCESS,
    XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_FAILURE,
    XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_CANCELLED,
    XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_CLOSED
} XL6_IrAsyncMachineEntryKind;

#define XL6_IR_ASYNC_RESUME_ENTRY_COUNT 4

typedef struct XL6_IrAsyncMachineEntry {
    XL6_IrAsyncMachineEntryKind kind;
    size_t machine;
    /* SIZE_MAX for LAUNCH; otherwise the exact module suspension index. */
    size_t suspension;
    XL6_IrBlockId block;
    /* Zero-based module instruction index at which this invocation starts.
     * All outcomes of a suspension resume immediately after AWAIT so any
     * lowered release/cleanup before ERROR_TEST remains executable CFG. */
    size_t firstInstruction;
    /* Zero-based module asyncMachineBlocks index for this exact instruction
     * slice. All four callback outcomes enter the canonical resume fragment. */
    size_t programBlock;
} XL6_IrAsyncMachineEntry;

typedef enum XL6_IrAsyncMachineBlockKind {
    XL6_IR_ASYNC_MACHINE_BLOCK_INVALID = 0,
    XL6_IR_ASYNC_MACHINE_BLOCK_EXECUTE,
    /* The final instruction is the exact AWAIT named by suspension. */
    XL6_IR_ASYNC_MACHINE_BLOCK_SUSPEND
} XL6_IrAsyncMachineBlockKind;

/* A machine block is a suspension-split fragment of one Lowered XLIR basic
 * block.  CFG successors are program-block indices, not source/function block
 * ids, so loops which re-enter a pre-await fragment remain unambiguous. */
typedef struct XL6_IrAsyncMachineBlock {
    XL6_IrAsyncMachineBlockKind kind;
    size_t machine;
    XL6_IrBlockId block;
    size_t firstInstruction;
    size_t instructionCount;
    /* Exact module suspension index for SUSPEND; SIZE_MAX for EXECUTE. */
    size_t suspension;
    /* SIZE_MAX when the fragment has no corresponding CFG successor. */
    size_t trueTarget;
    size_t falseTarget;
} XL6_IrAsyncMachineBlock;

typedef struct XL6_IrAsyncSuspension {
    /* Zero-based index into XL6_IrModule::asyncMachines. */
    size_t machine;
    uint32 stateId;
    /* Zero-based module instruction index. */
    size_t awaitInstruction;
    XL6_IrBlockId block;
    XL6_IrValueId inputValue;
    XL6_IrValueId resultValue;
    XL6_IrTypeId resultType;
    XL6_IrAsyncResultAbi resultAbi;
    XL6_IrBlockId successBlock;
    XL6_IrBlockId failureBlock;
    /* Exact lexical cleanup transition selected by lowering for a failed
     * await.  Cgen must not search exitPlans by origin. */
    uint32 scopeDepth;
    uint32 region;
    size_t failureExitPlan;
    uint32 cleanupTargetScopeDepth;
    uint32 cleanupTargetRegion;
    size_t firstCleanupAction;
    size_t cleanupActionCount;
    size_t firstLiveSlot;
    size_t liveSlotCount;
    size_t firstSpillAction;
    size_t spillActionCount;
    size_t firstResumeAction;
    size_t resumeActionCount;
    /* Unique physical residents carried by a nested async continuation.
     * Generator activations are persistent and do not have this handoff. */
    size_t firstHandoffAction;
    size_t handoffActionCount;
    size_t firstRollbackAction;
    size_t rollbackActionCount;
    /* Preparation failure resumes the exact lowered AWAIT failure entry in
     * the current activation. No failed registration has accepted the frame. */
    size_t setupFailureEntry;
} XL6_IrAsyncSuspension;

/* A RETURN publishes one owned payload atomically. An unaccepted result is
 * dropped under the original failure; an existing Promise receives that
 * failure, never an artificial CLOSED outcome. If no Promise can be created,
 * launch reports failure to its caller after dropping the unaccepted result. */
typedef enum XL6_IrAsyncResultCommitAbi {
    XL6_IR_ASYNC_RESULT_COMMIT_INVALID = 0,
    XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION
} XL6_IrAsyncResultCommitAbi;

/* RETURN and THROW execute the verified destroy-action program before any
 * Future publication. Retired init bits make later physical frame disposal
 * idempotent. Cleanup preserves the first (ok,error) outcome, including a
 * failed outcome without a diagnostic, and is isolated from the producer.
 * XRT requires a non-null rejection error: only at that transport boundary
 * a diagnostic-free failure is represented by the static XERR_INTERNAL.
 * The frame's outer Promise remains alive until publication has completed.
 * Every internal async target has this contract, including a task worker
 * without suspension: its capture vector is retired before handing an outcome
 * to the executor. Consuming inputs detach before invocation; borrowed captures
 * retire in reverse parameter order. Cancellation sampled after a successful
 * worker is a primary outcome, not a later capture-cleanup failure. */
typedef enum XL6_IrAsyncCompletionAbi {
    XL6_IR_ASYNC_COMPLETION_INVALID = 0,
    XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH
} XL6_IrAsyncCompletionAbi;

/* Cooperative output cancellation is delivered once as the current await's
 * CANCELLED outcome. Its ordinary catch/finally CFG determines the result;
 * later awaits in cleanup must not receive the same request again. This is
 * not source cancellation or forced termination of a pending producer. */
typedef enum XL6_IrAsyncCancellationAbi {
    XL6_IR_ASYNC_CANCELLATION_INVALID = 0,
    XL6_IR_ASYNC_CANCELLATION_AWAIT_ONCE,
    XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2
} XL6_IrAsyncCancellationAbi;

/* A transport lease protects only observation and acquisition of the await
 * result (copy/retain/duplicate according to resultAbi). Retire it before the
 * language resume CFG executes, not during physical frame disposal after
 * publication. Lexical/SSA owners keep their independently verified lifetime.
 * Capture failure wins over lease-retirement failure; an acquired result is
 * rolled back if lease retirement fails. The existing Future outcome ABI
 * represents a diagnostic-free transport failure at the await boundary. */
typedef enum XL6_IrAsyncSourceLeaseAbi {
    XL6_IR_ASYNC_SOURCE_LEASE_INVALID = 0,
    XL6_IR_ASYNC_SOURCE_LEASE_CAPTURE_THEN_RELEASE
} XL6_IrAsyncSourceLeaseAbi;

/* The launch prefix stays on the caller. A suspension reserves an owned root
 * execution credit before Watch registration; later suspensions inherit it.
 * Commit follows frame handoff. Run AND physical release execute in the
 * module's guaranteed-worker domain, never on a Future producer. */
typedef enum XL6_IrAsyncExecutionAbi {
    XL6_IR_ASYNC_EXECUTION_INVALID = 0,
    XL6_IR_ASYNC_EXECUTION_GUARANTEED_RESUME
} XL6_IrAsyncExecutionAbi;

/* One stable, real producer owns the current heap frame across suspensions.
 * The pending output owns that producer; the active frame owns a producer
 * reference. A source Watch owns an independently counted frame reference.
 * Commit switches the producer's current-frame edge; failure/Abort rolls back
 * the provisional binding. Neither protocol cancels accepted source duties. */
typedef enum XL6_IrAsyncOwnershipAbi {
    XL6_IR_ASYNC_OWNERSHIP_INVALID = 0,
    XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1
} XL6_IrAsyncOwnershipAbi;

typedef struct XL6_IrAsyncMachine {
    XL6_IrFutureOutcomeAbi futureOutcomeAbi;
    XL6_IrAsyncResultCommitAbi resultCommitAbi;
    XL6_IrAsyncCompletionAbi completionAbi;
    /* The execution queue owns a committed activation; its Future Watch owns
     * a separate storage-access lease until transport release. One outer
     * Promise and its cancellation-delivery bit move with the machine; no
     * externally cancellable adapter may skip its resume program. */
    XL6_IrCallbackTransportAbi transportAbi;
    XL6_IrAsyncCancellationAbi cancellationAbi;
    XL6_IrAsyncSourceLeaseAbi sourceLeaseAbi;
    XL6_IrAsyncExecutionAbi executionAbi;
    XL6_IrAsyncOwnershipAbi ownershipAbi;
    XL6_IrAsyncMachineStatus status;
    XL6_SymbolId targetSymbol;
    /* Zero-based index into XL6_IrModule::functions. */
    size_t function;
    XL6_IrBlockId entryBlock;
    size_t firstEntry;
    size_t entryCount;
    size_t firstBlock;
    size_t blockCount;
    size_t firstFrameSlot;
    size_t frameSlotCount;
    size_t initBitCount;
    size_t firstAction;
    size_t actionCount;
    size_t firstPrologueAction;
    size_t prologueActionCount;
    size_t firstDestroyAction;
    size_t destroyActionCount;
    size_t firstTransferAction;
    size_t transferActionCount;
    size_t firstSuspension;
    size_t suspensionCount;
} XL6_IrAsyncMachine;

/* Async and generator activations intentionally share one physical frame ABI.
 * The aliases make that common continuation layer explicit while preserving
 * the established async API/JSON vocabulary during migration.  Each machine
 * family owns a separate arena, so its indices remain dense and unambiguous. */
typedef XL6_IrAsyncFrameDestroyAbi XL6_IrContinuationFrameDestroyAbi;
typedef XL6_IrAsyncSpillSource XL6_IrContinuationSpillSource;
typedef XL6_IrAsyncFrameTransferAbi XL6_IrContinuationFrameTransferAbi;
typedef XL6_IrAsyncFrameResidence XL6_IrContinuationFrameResidence;
typedef XL6_IrAsyncMachineFrameSlot XL6_IrContinuationFrameSlot;
typedef XL6_IrAsyncLiveSlot XL6_IrContinuationLiveSlot;
typedef XL6_IrAsyncFrameActionKind XL6_IrContinuationFrameActionKind;
typedef XL6_IrAsyncFrameAction XL6_IrContinuationFrameAction;

typedef enum XL6_IrGeneratorMachineStatus {
    XL6_IR_GENERATOR_MACHINE_INVALID = 0,
    XL6_IR_GENERATOR_MACHINE_ANALYZED,
    XL6_IR_GENERATOR_MACHINE_BLOCKED_LIVE_ABI
} XL6_IrGeneratorMachineStatus;

typedef enum XL6_IrGeneratorMachineEntryKind {
    XL6_IR_GENERATOR_MACHINE_ENTRY_INVALID = 0,
    XL6_IR_GENERATOR_MACHINE_ENTRY_START,
    XL6_IR_GENERATOR_MACHINE_ENTRY_RESUME
} XL6_IrGeneratorMachineEntryKind;

typedef struct XL6_IrGeneratorMachineEntry {
    XL6_IrGeneratorMachineEntryKind kind;
    size_t machine;
    /* SIZE_MAX for START; otherwise the exact generatorSuspensions index. */
    size_t suspension;
    XL6_IrBlockId block;
    size_t firstInstruction;
    size_t programBlock;
} XL6_IrGeneratorMachineEntry;

typedef enum XL6_IrGeneratorMachineBlockKind {
    XL6_IR_GENERATOR_MACHINE_BLOCK_INVALID = 0,
    XL6_IR_GENERATOR_MACHINE_BLOCK_EXECUTE,
    /* The final instruction is the exact YIELD named by suspension. */
    XL6_IR_GENERATOR_MACHINE_BLOCK_YIELD
} XL6_IrGeneratorMachineBlockKind;

typedef struct XL6_IrGeneratorMachineBlock {
    XL6_IrGeneratorMachineBlockKind kind;
    size_t machine;
    XL6_IrBlockId block;
    size_t firstInstruction;
    size_t instructionCount;
    size_t suspension;
    size_t trueTarget;
    size_t falseTarget;
} XL6_IrGeneratorMachineBlock;

typedef struct XL6_IrGeneratorSuspension {
    size_t machine;
    uint32 stateId;
    size_t yieldInstruction;
    XL6_IrBlockId block;
    XL6_IrValueId itemValue;
    XL6_IrTypeId itemType;
    XL6_IrGeneratorYieldAbi yieldAbi;
    XL6_IrGeneratorClosePointAbi closePointAbi;
    size_t firstLiveSlot;
    size_t liveSlotCount;
    size_t firstSpillAction;
    size_t spillActionCount;
    size_t firstResumeAction;
    size_t resumeActionCount;
} XL6_IrGeneratorSuspension;

/* A synchronous generator has a single heap activation.  START captures every
 * parameter but executes no body code.  Each NEXT enters one START/RESUME
 * program slice; YIELD stores exactly one current item and returns to the
 * consumer.  RETURN/THROW close the activation through the same reverse-order,
 * init-bit guarded frame actions used by async continuation machines. */
typedef struct XL6_IrGeneratorMachine {
    XL6_IrGeneratorMachineStatus status;
    XL6_IrGeneratorCloseAbi closeAbi;
    XL6_SymbolId targetSymbol;
    size_t function;
    XL6_IrBlockId entryBlock;
    XL6_IrTypeId itemType;
    XL6_IrGeneratorYieldAbi yieldAbi;
    /* The current yielded slot owns exactly what its verified yield transfer
     * moves. It is independent of the frame's init-bit guarded residents. */
    XL6_IrContinuationFrameTraceAbi itemTraceAbi;
    size_t firstEntry;
    size_t entryCount;
    size_t firstBlock;
    size_t blockCount;
    size_t firstFrameSlot;
    size_t frameSlotCount;
    size_t initBitCount;
    size_t firstAction;
    size_t actionCount;
    size_t firstPrologueAction;
    size_t prologueActionCount;
    size_t firstDestroyAction;
    size_t destroyActionCount;
    size_t firstTransferAction;
    size_t transferActionCount;
    size_t firstSuspension;
    size_t suspensionCount;
} XL6_IrGeneratorMachine;

typedef struct XL6_IrParameterLifetime {
    /* Zero only for a compiler-supplied unused receiver or an external ABI
     * parameter with no local binding. Never a spelling-based alias. */
    XL6_SymbolId sourceSymbol;
    int rebindable;
    /* A synchronous, non-escaping managed string input may remain a borrowed
     * read-only binding instead of being duplicated at function entry. */
    int borrowReadOnly;
    XL6_IrReleaseEffect cleanupEffect;
} XL6_IrParameterLifetime;

typedef struct XL6_IrCallTarget {
    XL6_IrCallTargetKind kind;
    XL6_IrCallExecutionBoundaryAbi executionBoundaryAbi;
    XL6_IrCallEffectAbi effectAbi;
    XL6_IrRuntimeBridgeKind runtimeBridge;
    XL6_IrRuntimeBridgeOperation runtimeBridgeOperation;
    /* Closed resident resource declaration, resolved once from the private
     * ABI registry and independently verified. NONE preserves strict refusal
     * for unknown/native module-local callbacks. The name has registry lifetime. */
    XL6_IrNativeHandlePolicyAbi nativeHandlePolicyAbi;
    const char* nativeHandlePolicyCName;
    XL6_IrCallTargetLifecycleAbi lifecycleAbi;
    XL6_IrReachability reachability;
    XL6_SymbolId symbol;
    XL6_IrNodeId declaration;
    /* Public call result.  For INTERNAL_ASYNC this is future<T>. */
    XL6_IrTypeId returnType;
    XL6_IrCallResultPhysicalAbi resultPhysicalAbi;
    XL6_IrCallResultConversionAbi resultConversionAbi;
    XL6_IrCallResultCAbi resultCAbi;
    /* Exact managed class field initialized by a native factory result, or
     * invalid for DIRECT. */
    XL6_IrNodeId resultPhysicalFieldDeclaration;
    /* Exact destructor transferred into the field for FIELD_TAKE; zero for
     * DIRECT and FIELD_BORROW. */
    XL6_SymbolId resultPhysicalDestroySymbol;
    /* Worker body result.  It is INVALID except for INTERNAL_ASYNC. */
    XL6_IrTypeId workerReturnType;
    XL6_IrErrorAbi errorAbi;
    XL6_IrCallErrorPhysicalAbi errorPhysicalAbi;
    XL6_IrCallPreparationFailureAbi preparationFailureAbi;
    /* Async launch and worker failure effects are distinct ABI facts. */
    XL6_IrErrorAbi workerErrorAbi;
    /* Exact worker/Future payload ABI.  VOID has workerReturnType INVALID;
     * every value form has a concrete workerReturnType. */
    XL6_IrAsyncResultAbi asyncResultAbi;
    XL6_IrAsyncDispatchKind asyncDispatch;
    /* All internal async boundaries preserve the four-state language outcome.
     * Ready/machine completion uses Promise operations directly; task transport
     * uses a preallocated watch and a parent-linked cancellation token, with
     * owned payload forwarding after the task's frame-cleanup barrier. */
    XL6_IrFutureOutcomeAbi futureOutcomeAbi;
    /* SIZE_MAX in typed XLIR and for an async body with no suspension.
     * Lowered CFG analysis writes the one exact AsyncMachine index here, so a
     * backend never searches functions/machines by source symbol. */
    size_t asyncMachine;
    /* Completion semantics do not depend on whether a machine was needed.
     * INVALID on non-async targets; machines repeat this verified contract. */
    XL6_IrAsyncCompletionAbi asyncCompletionAbi;
    /* Exact generator payload and machine link.  Both are invalid/SIZE_MAX for
     * every non-generator target; a generator target always has a machine. */
    XL6_IrTypeId generatorItemType;
    XL6_IrGeneratorYieldAbi generatorYieldAbi;
    size_t generatorMachine;
    char* cName;
    XL6_IrTypeId* parameterTypes;
    XL6_IrParameterAbi* parameterAbis;
    XL6_IrParameterLifetime* parameterLifetimes;
    XL6_IrCallParameterPhysicalAbi* parameterPhysicalAbis;
    XL6_IrCallParameterConstraintAbi* parameterConstraintAbis;
    /* Exact native field declaration for NATIVE_CLASS_HANDLE_BORROW; invalid
     * for every other physical ABI. Indexed identically to parameterTypes. */
    XL6_IrNodeId* parameterPhysicalFieldDeclarations;
    XL6_IrAsyncCaptureAbi* asyncCaptureAbis;
    XL6_IrContinuationFrameTraceAbi* asyncCaptureTraceAbis;
    XL6_IrAsyncFrameDestroyAbi* asyncCaptureDestroyAbis;
    /* Parameter-to-generator-frame transfer, indexed exactly like the public
     * parameter ABI.  This uses the common continuation transfer vocabulary. */
    XL6_IrContinuationFrameTransferAbi* generatorCaptureAbis;
    size_t parameterCount;
    /* NONE/ARRAY/DICT targets have exactly parameterCount physical operands.
     * A C target has parameterCount fixed operands followed by typed native
     * operands retained in the lowered CALL instruction. */
    XL6_IrCallVariadicAbi variadicAbi;
} XL6_IrCallTarget;

/* Allocation-free view of the selected synchronous property ABI. It projects
 * the already canonical direct/class/protocol target, with receiver separated
 * from explicit parameters, for transaction lowering and independent checks.
 * Array storage belongs to the module; refresh the view after arena growth. */
typedef struct XL6_IrPropertyAccessorSignature {
    XL6_IrNodeId declaration;
    XL6_IrTypeId receiverType;
    XL6_IrTypeId resultType;
    XL6_IrErrorAbi errorAbi;
    const XL6_IrTypeId* parameterTypes;
    const XL6_IrParameterAbi* parameterAbis;
    size_t parameterCount;
} XL6_IrPropertyAccessorSignature;

typedef enum XL6_IrExportReturnAbi {
    XL6_IR_EXPORT_RETURN_INVALID = 0,
    XL6_IR_EXPORT_RETURN_VOID,
    XL6_IR_EXPORT_RETURN_VALUE,
    /* Ownership of the returned managed object crosses to the C caller. */
    XL6_IR_EXPORT_RETURN_OWNED
} XL6_IrExportReturnAbi;

typedef enum XL6_IrExportErrorAbi {
    XL6_IR_EXPORT_ERROR_INVALID = 0,
    /* The complete C entry is one shared-runtime error transaction, including
     * module entry, callback argument acquisition, the body and cleanup.
     * Capture a failed body BEFORE releasing arguments. Commit the pending
     * result only after cleanup succeeds; otherwise roll it back and publish
     * the first failure to the immediate enclosing scope. A NULL diagnostic
     * never means success. Caller diagnostics survive successful entries.
     * Plain C return values are zero/void on failure; callers needing a total
     * status must bracket the entry with ForeignCallBegin/End. */
    XL6_IR_EXPORT_ERROR_SHARED_SCOPE_TRANSACTION
} XL6_IrExportErrorAbi;

/* Exact public representation at an exported C boundary.  Most values use
 * their already-fixed Type CAbi; bounded language integers are the deliberate
 * exception because XLIR executes them in an i64 carrier while the public API
 * preserves their declared fixed width. */
typedef enum XL6_IrExportValueAbi {
    XL6_IR_EXPORT_VALUE_INVALID = 0,
    XL6_IR_EXPORT_VALUE_TYPE_REPRESENTATION,
    XL6_IR_EXPORT_VALUE_I8,
    XL6_IR_EXPORT_VALUE_U8,
    XL6_IR_EXPORT_VALUE_I16,
    XL6_IR_EXPORT_VALUE_U16,
    XL6_IR_EXPORT_VALUE_I32,
    XL6_IR_EXPORT_VALUE_U32,
    /* Public typed C function pointer -> nullable owned closure bridge. The
     * boundary acquires in parameter order, lends to the internal function,
     * and releases in reverse order on success/error. Allocation failure
     * publishes MemoryError without entering the target; retained copies may
     * escape the call. Invoking that C callback uses its own shared foreign
     * error scope and returns an explicit closure success flag, so a failed
     * reentrant export with no diagnostic is catchable inside the language.
     * No function pointer is cast to a data pointer. */
    XL6_IR_EXPORT_VALUE_RAW_C_CALLBACK_BORROW,
    /* Same boundary bridge, with an owned managed string/xvalue returned by
     * the callback. The adapter transfers it to the language result or closes
     * it on a native error/unused-result path. */
    XL6_IR_EXPORT_VALUE_RAW_C_CALLBACK_OWNED_RESULT_BORROW
} XL6_IrExportValueAbi;

/* A public C boundary is independent of both a source declaration spelling
 * and the private XLIR function label.  Lowering owns the fully resolved
 * linkage name, public parameter borrow/value modes, and return transfer;
 * Cgen only writes this already-verified adapter. */
typedef struct XL6_IrExport {
    XL6_SymbolId symbol;
    XL6_IrNodeId declaration;
    char* cName;
    XL6_IrTypeId returnType;
    XL6_IrExportReturnAbi returnAbi;
    XL6_IrExportValueAbi returnValueAbi;
    XL6_IrExportErrorAbi errorAbi;
    /* Exact private body carrier, independent of the always-fallible public
     * entry transaction. Verified against the resolved CallTarget. */
    XL6_IrErrorAbi bodyErrorAbi;
    XL6_IrTypeId* parameterTypes;
    XL6_IrParameterAbi* parameterAbis;
    XL6_IrExportValueAbi* parameterValueAbis;
    char** parameterNames;
    size_t parameterCount;
} XL6_IrExport;

typedef enum XL6_IrGlobalMutability {
    XL6_IR_GLOBAL_MUTABILITY_INVALID = 0,
    XL6_IR_GLOBAL_MUTABLE,
    XL6_IR_GLOBAL_CONST
} XL6_IrGlobalMutability;

/* A public C data symbol is not a function adapter and therefore owns a
 * separate linkage contract.  Its initializer is already a canonical,
 * portable C scalar payload; Cgen must neither run module-init code to create
 * the public object nor recover a declaration name/constness from typed AST. */
typedef struct XL6_IrGlobalExport {
    XL6_SymbolId storageSymbol;
    XL6_IrNodeId declaration;
    XL6_IrNodeId initializerNode;
    XL6_IrTypeId type;
    XL6_IrGlobalMutability mutability;
    char* cName;
    char* initializer;
} XL6_IrGlobalExport;

typedef enum XL6_IrExternGlobalAccessAbi {
    XL6_IR_EXTERN_GLOBAL_ACCESS_INVALID = 0,
    XL6_IR_EXTERN_GLOBAL_READ_WRITE
} XL6_IrExternGlobalAccessAbi;

/* An imported C data symbol is storage owned by another translation unit.  It
 * has no XLIR initializer and is deliberately not re-exported by the generated
 * module header. */
typedef struct XL6_IrExternGlobal {
    XL6_SymbolId storageSymbol;
    XL6_IrNodeId declaration;
    XL6_IrTypeId type;
    XL6_IrExternGlobalAccessAbi accessAbi;
    char* cName;
} XL6_IrExternGlobal;

/* A generic source declaration never becomes a callable C ABI directly.
 * Lowering records every concrete instance so verifier/Cgen can reason about
 * the ordinary, monomorphic declaration and function it produced. */
typedef struct XL6_IrGenericSpecialization {
    XL6_IrNodeId sourceDeclaration;
    XL6_IrNodeId declaration;
    XL6_SymbolId symbol;
    XL6_IrTypeId* typeArgs;
    size_t typeArgCount;
} XL6_IrGenericSpecialization;

typedef enum XL6_IrModuleEntryKind {
    XL6_IR_MODULE_ENTRY_NONE = 0,
    XL6_IR_MODULE_ENTRY_XLANG_MAIN
} XL6_IrModuleEntryKind;

/* Module resource contract, independent of language global owner count.
 * A guarded lazy pool admits submissions until quiescence starts. Finalization
 * must join workers AND their callbacks before touching global owners; failure
 * retains resources, globals and executable code for a later host retry.
 * Module activation/finalization are serialized by the embedding boundary. */
typedef enum XL6_IrModuleTaskPoolAbi {
    XL6_IR_MODULE_TASK_POOL_NONE = 0,
    XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE
} XL6_IrModuleTaskPoolAbi;

/* Optional host code capability, copied once before the first initialization
 * attempt and immutable for the artifact's entire executable lifetime. An
 * unbound standalone artifact requires externally resident code. This binding
 * contract does not assert that every escaped callable already acquires pins. */
typedef enum XL6_IrModuleCodeOwnerAbi {
    XL6_IR_MODULE_CODE_OWNER_NONE = 0,
    XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
} XL6_IrModuleCodeOwnerAbi;
const char* xl6IrModuleCodeOwnerAbiName(XL6_IrModuleCodeOwnerAbi);
/* Explicit opt-in to checked, phased retirement. This describes the artifact
 * boundary; runtime admission still rejects unmodelled transitive ownership. */
typedef enum XL6_IrModuleRetirementAbi {
    XL6_IR_MODULE_RETIREMENT_NONE = 0,
    XL6_IR_MODULE_RETIREMENT_CHECKED_GRAPH_V1,
    XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2,
    /* Modelled services prepare through their own graph adapters. The module
     * preflight must not drain them before their dependency graph is known. */
    XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3,
    /* Explicit post-finalizer physical progress, not a complete native graph. */
    XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4
} XL6_IrModuleRetirementAbi;

typedef enum XL6_IrModuleNetworkServiceAbi {
    XL6_IR_MODULE_NETWORK_SERVICE_NONE = 0,
    XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1
} XL6_IrModuleNetworkServiceAbi;

/* One physical closure ABI across ordinary functions, continuations, boxes
 * and native callback wrappers. Construction acquires/inherits code ownership
 * and can fail; invocation roots and last destruction run in resident code.
 * An unbound module still explicitly requires externally resident code. */
typedef enum XL6_IrClosureStorageAbi {
    XL6_IR_CLOSURE_STORAGE_NONE = 0,
    XL6_IR_CLOSURE_STORAGE_RESIDENT_SCOPED_LEASE_V1,
    /* Immutable closed capture slots; resident trace/drop, no producer-owned
     * environment callbacks. V1 remains a rejected legacy lowered value. */
    XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2
} XL6_IrClosureStorageAbi;

/* Cell lifetime and payload transitions are resident operations. In
 * particular CELL_EXCHANGE is one transfer, never a borrowed load plus store.
 * The closed element C ABI determines size/carrier, not cleanup inference. */
typedef enum XL6_IrCellStorageAbi {
    XL6_IR_CELL_STORAGE_NONE = 0,
    XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1
} XL6_IrCellStorageAbi;

typedef enum XL6_IrValueIteratorStorageAbi {
    XL6_IR_VALUE_ITERATOR_STORAGE_NONE = 0,
    XL6_IR_VALUE_ITERATOR_STORAGE_RESIDENT_SNAPSHOT_V1
} XL6_IrValueIteratorStorageAbi;

/* Storage/frame commands already carry exact types, owners and commit phases.
 * This closes the physical payload/activation/detach publication mechanism,
 * not a promise that every graph node or arbitrary native write participates. */
typedef enum XL6_IrSlotTransferAbi {
    XL6_IR_SLOT_TRANSFER_NONE = 0,
    XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1
} XL6_IrSlotTransferAbi;

/* Resident generator control/frame storage, item transfer and callback tails.
 * The IR retains exact continuation states, cleanup CFGs and payload ABIs. */
typedef enum XL6_IrGeneratorStorageAbi {
    XL6_IR_GENERATOR_STORAGE_NONE = 0,
    XL6_IR_GENERATOR_STORAGE_RESIDENT_SCOPED_FRAME_V1, /* Reserved opaque frame; not canonical Lowered IR. */
    XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V2
} XL6_IrGeneratorStorageAbi;

/* Physical async machine/task frame allocation and final disposal cooperate
 * with inspection. Payload slots, execution activity, code ownership and
 * transport admission remain separate verified obligations. */
typedef enum XL6_IrAsyncStorageAbi {
    XL6_IR_ASYNC_STORAGE_NONE = 0,
    XL6_IR_ASYNC_STORAGE_RESIDENT_PHYSICAL_FRAME_V1, /* Historical unpinnable storage, not canonical Lowered IR. */
    XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V2
} XL6_IrAsyncStorageAbi;

/* One resident physical task frame, scoped callbacks and module pool admission.
 * Exact capture and result semantics remain in the verified activation IR. */
typedef enum XL6_IrTaskStorageAbi {
    XL6_IR_TASK_STORAGE_NONE = 0,
    XL6_IR_TASK_STORAGE_RESIDENT_FRAME_POOL_V1, /* Reserved opaque layout; not canonical Lowered IR. */
    XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V2
} XL6_IrTaskStorageAbi;

/* Allocation prepares code ownership before any initialization can escape;
 * only the explicit successful construction commit arms user destruction. */
typedef enum XL6_IrClassStorageAbi {
    XL6_IR_CLASS_STORAGE_NONE = 0,
    XL6_IR_CLASS_STORAGE_RESIDENT_PREPARED_LEASE_V1,
    /* Prepared lifetime plus certified identity/finalizer callbacks. */
    XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2
} XL6_IrClassStorageAbi;

typedef enum XL6_IrModuleContinuationAbi {
    XL6_IR_MODULE_CONTINUATION_NONE = 0,
    XL6_IR_MODULE_CONTINUATION_GUARANTEED_WORKERS
} XL6_IrModuleContinuationAbi;

/* Distinct from execution strategy: the module owns one resident executor,
 * whose real parked wait slots and joined resource lifetime are traceable. */
typedef enum XL6_IrModuleExecutorOwnershipAbi {
    XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE = 0,
    XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1
} XL6_IrModuleExecutorOwnershipAbi;

/* Separate from scheduling/quiescence: one module-owned wrapper traces its
 * actual native pool owner. Accepted frames own the wrapper; native queued
 * Jobs and all transitive Data policies must be independently certified. */
typedef enum XL6_IrModuleTaskPoolOwnershipAbi {
    XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE = 0,
    XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1
} XL6_IrModuleTaskPoolOwnershipAbi;

/* Physical aggregate transport for the lowered ALL/RACE/WHEN_ANY operations.
 * A resident mapper owns its generated payload-boxing code through the
 * module capability, and array materialization is a shared runtime operation.
 * This certifies neither a wait-cycle proof nor unknown source producers. */
typedef enum XL6_IrFutureAggregateAbi {
    XL6_IR_FUTURE_AGGREGATE_NONE = 0,
    XL6_IR_FUTURE_AGGREGATE_RESIDENT_OWNED_MAPPER_V1
} XL6_IrFutureAggregateAbi;

/* Native network result adaptation owns an exclusive source production chain.
 * Its generated mapper runs in a resident leased callback frame, cancellation
 * propagates to that source, and result boxes use resident payload policies.
 * Native owning context is explicitly opaque until independently described;
 * this does not certify engines, native handles or arbitrary mixed cycles. */
typedef enum XL6_IrNativeFutureMapAbi {
    XL6_IR_NATIVE_FUTURE_MAP_NONE = 0,
    XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1
} XL6_IrNativeFutureMapAbi;

/* Process/module boundary contract.  This intentionally describes the
 * runtime-facing wrapper separately from source functions and CFG bodies. */
typedef struct XL6_IrModuleAbi {
    XL6_IrModuleEntryKind entryKind;
    XL6_SymbolId entrySymbol;
    XL6_IrCAbi entryArgumentAbi;
    XL6_IrTypeId entryReturnType;
    /* One-based lowered finalizer index, including owner-free modules.
     * Zero is reserved for Typed IR or fragments without an initializer. */
    uint32 finalizerFunction;
    size_t finalizerCleanupCount;
    XL6_IrModuleTaskPoolAbi taskPoolAbi;
    XL6_IrModuleCodeOwnerAbi codeOwnerAbi;
    XL6_IrClosureStorageAbi closureStorageAbi;
    XL6_IrModuleRetirementAbi retirementAbi;
    XL6_IrCellStorageAbi cellStorageAbi;
    XL6_IrValueIteratorStorageAbi valueIteratorStorageAbi;
    XL6_IrSlotTransferAbi slotTransferAbi;
    XL6_IrGeneratorStorageAbi generatorStorageAbi;
    XL6_IrAsyncStorageAbi asyncStorageAbi;
    XL6_IrTaskStorageAbi taskStorageAbi;
    XL6_IrClassStorageAbi classStorageAbi;
    XL6_IrModuleContinuationAbi continuationAbi;
    XL6_IrModuleExecutorOwnershipAbi executorOwnershipAbi;
    XL6_IrModuleTaskPoolOwnershipAbi taskPoolOwnershipAbi;
    XL6_IrFutureAggregateAbi futureAggregateAbi;
    XL6_IrNativeFutureMapAbi nativeFutureMapAbi;
    XL6_IrModuleNetworkServiceAbi networkServiceAbi;
} XL6_IrModuleAbi;

typedef enum XL6_IrStorageKind {
    XL6_IR_STORAGE_INVALID = 0,
    XL6_IR_STORAGE_GLOBAL,
    XL6_IR_STORAGE_EXTERN_GLOBAL,
    XL6_IR_STORAGE_LOCAL,
    XL6_IR_STORAGE_PARAMETER,
    /* A closure-body binding read from its verified environment slot. */
    XL6_IR_STORAGE_CAPTURE,
    /* Compiler-materialized ownership/lowering slot.  This is deliberately
     * distinct from a source local so verifier and Cgen never infer lifetime
     * semantics from adjacent STORE/LOAD instructions. */
    XL6_IR_STORAGE_TEMPORARY
} XL6_IrStorageKind;

/* Storage is an explicit backend contract.  A symbol alone never tells Cgen
 * whether a name is global, local, an ABI parameter/capture, or a lowering
 * temporary with its own verified lifetime. */
typedef struct XL6_IrStorage {
    XL6_IrStorageKind kind;
    XL6_SymbolId symbol;
    XL6_IrTypeId type;
    /* INVALID for ordinary storage.  A non-invalid cell type means this
     * symbol is accessed only through explicit CELL_* XLIR instructions. */
    XL6_IrTypeId cellType;
    XL6_IrNodeId declaration;
    XL6_IrNodeId owner;
    /* Lowered GLOBAL slots use the same exact physical owning-view vocabulary
     * as continuation frames. Other storage and Typed IR keep INVALID.
     * OPAQUE is an explicit inspection refusal, never a guessed leaf. */
    XL6_IrContinuationFrameTraceAbi moduleTraceAbi;
} XL6_IrStorage;

/* A source union is an untagged, nominal, trivially-copyable value.  Typed
 * XLIR owns its ordered layout and field identity so lowering and Cgen never
 * recover either from AST children or names. */
typedef struct XL6_IrUnionField {
    XL6_IrTypeId ownerType;
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    char* name;
    /* Exact public C member spelling.  Cgen consumes this verified ABI fact;
     * it never substitutes pool position or reconstructs source syntax. */
    char cName[XL6_C_SYMBOL_COMPONENT_MAX + 1];
} XL6_IrUnionField;

typedef struct XL6_IrUnionLayout {
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    size_t firstField;
    size_t fieldCount;
} XL6_IrUnionLayout;

/* Enum layout is nominal and closed.  Numeric enums preserve one exact
 * underlying scalar ABI; tagged enums use one owned runtime xvalue object.
 * These records are the only source of variant identity, payload order and
 * payload transfer policy after the Typed XLIR boundary; lowering and Cgen
 * never walk enum AST children. */
typedef enum XL6_IrEnumPayloadAbi {
    XL6_IR_ENUM_PAYLOAD_INVALID = 0,
    XL6_IR_ENUM_PAYLOAD_TRIVIAL_BOX,
    XL6_IR_ENUM_PAYLOAD_STRING_COPY,
    XL6_IR_ENUM_PAYLOAD_XVALUE_RETAIN,
    XL6_IR_ENUM_PAYLOAD_CLOSURE_COPY,
    XL6_IR_ENUM_PAYLOAD_RAW_POINTER_BORROW,
    XL6_IR_ENUM_PAYLOAD_NATIVE_HANDLE_BORROW,
    XL6_IR_ENUM_PAYLOAD_UNION_COPY
} XL6_IrEnumPayloadAbi;

typedef struct XL6_IrEnumPayload {
    XL6_IrTypeId ownerType;
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    XL6_IrEnumPayloadAbi abi;
    char* name;
} XL6_IrEnumPayload;

typedef struct XL6_IrEnumVariant {
    XL6_IrTypeId ownerType;
    XL6_IrNodeId declaration;
    XL6_SymbolId symbol;
    /* Stable, nonzero ordinal local to the concrete EnumLayout. */
    uint32 tag;
    /* Numeric enum initializer expression, INVALID for tagged variants. */
    XL6_IrNodeId valueExpression;
    size_t firstPayload;
    size_t payloadCount;
    char* name;
} XL6_IrEnumVariant;

typedef struct XL6_IrEnumLayout {
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    size_t firstVariant;
    size_t variantCount;
} XL6_IrEnumLayout;

/* Class layout is frontend-resolved, owned XLIR data.  It separates class
 * construction from source-tree recovery: lowering receives an exact object
 * type, optional base layout and an ordered field-initializer slice; Cgen
 * only ever sees the resulting object runtime instructions. */
/* Class construction never guesses a language default from a C zeroed local.
 * Explicit source initializers, scalar/string defaults, and reference-slot
 * null sentinels are distinct typed-IR contracts.  A class field is stored in
 * an xvalue object slot, so its reference default deliberately differs from a
 * local concrete-class declaration: the former is null until the constructor
 * replaces it, while the latter may invoke a verified default constructor. */
/* A persistence decoder sometimes has to materialize a class value which did
 * not already exist in the outer parse target (for example `Stats?` or an
 * `array<Item>` element).  This is an owned, source-free field construction
 * contract.  The optional literal is canonical C payload data and is never
 * reparsed by Cgen. */
typedef enum XL6_IrPersistenceDefaultAbi {
    XL6_IR_PERSISTENCE_DEFAULT_NONE = 0,
    XL6_IR_PERSISTENCE_DEFAULT_XVALUE_NULL,
    XL6_IR_PERSISTENCE_DEFAULT_BOOL,
    XL6_IR_PERSISTENCE_DEFAULT_I64,
    XL6_IR_PERSISTENCE_DEFAULT_U64,
    XL6_IR_PERSISTENCE_DEFAULT_F64,
    XL6_IR_PERSISTENCE_DEFAULT_STRING,
    XL6_IR_PERSISTENCE_DEFAULT_EMPTY_ARRAY,
    XL6_IR_PERSISTENCE_DEFAULT_EMPTY_LIST,
    XL6_IR_PERSISTENCE_DEFAULT_EMPTY_SET,
    XL6_IR_PERSISTENCE_DEFAULT_EMPTY_DICT,
    XL6_IR_PERSISTENCE_DEFAULT_CLASS
} XL6_IrPersistenceDefaultAbi;

typedef struct XL6_IrClassField {
    XL6_IrNativeHandlePolicyAbi nativeHandlePolicyAbi;
    const char* nativeHandlePolicyCName; /* Immutable declaration-registry lifetime. */
    XL6_IrTypeId ownerType;
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    XL6_IrNodeId initializer;
    XL6_IrInitializerAbi initializerAbi;
    /* A default resource field is a constructor operation, not an erased-null
     * slot.  Preserve the same exact generic payload and effect contract used
     * by ordinary ValueInitializer records so lowering never reclassifies the
     * field TypeId (and Cgen only observes the resulting RuntimeCall). */
    XL6_IrRuntimeResourceOp initializerRuntimeResourceOp;
    XL6_IrTypeId initializerPayloadType;
    XL6_IrRuntimeResourcePayloadAbi initializerRuntimeResourcePayloadAbi;
    XL6_IrTypeId initializerRuntimeResourceKeyType;
    XL6_IrRuntimeResourcePayloadAbi initializerRuntimeResourceKeyPayloadAbi;
    XL6_IrRuntimeResourceKeyOrderAbi initializerRuntimeResourceKeyOrderAbi;
    XL6_IrErrorAbi initializerErrorAbi;
    XL6_IrPersistenceDefaultAbi persistenceDefaultAbi;
    /* Canonical C scalar/string literal for the matching literal ABI. */
    char* persistenceDefaultLiteral;
    /* Exact pure-native destructor transferred with a field assignment, or
     * zero when the field is not an owning native resource. */
    XL6_SymbolId nativeDestroySymbol;
    char* name;
    /* Format-specific persistence is resolved while the typed declaration is
     * still present. INCLUDED is explicit: private/skip filtering never leaks
     * into lowering or Cgen. keyLiteral is NULL for the field name or an
     * XLIR-owned canonical C string literal for a renamed key. */
    uint32 jsonPersistenceFlags;
    char* jsonPersistenceKeyLiteral;
    uint32 xlonPersistenceFlags;
    char* xlonPersistenceKeyLiteral;
    /* Exact structural `derive string` field partition.  This flag is copied
     * from the declaration's @string(skip) contract while Typed XLIR still
     * owns attributes; Cgen never scans declaration children. */
    int stringDeriveIncluded;
    /* Exact structural `derive equals` / `derive hash` field partition.  Both
     * protocols share @compare(skip), so equality and hashing cannot drift. */
    int compareDeriveIncluded;
} XL6_IrClassField;

/* One materialization of a field declaration at a construction invocation.
 * Nested initializers retain their declaration provenance and parent instance;
 * resolving that chain yields execution context without losing diagnostics.
 * All indices except first/commitInstruction are one-based; zero parent means
 * a directly executed invocation. Ranges include generated failure cleanup. */
typedef struct XL6_IrInstantiation {
    size_t parent;
    size_t function;
    size_t field;
    XL6_IrNodeId site;
    XL6_IrTypeId layoutType;
    XL6_IrValueId receiver;
    size_t firstInstruction;
    size_t instructionCount;
    size_t commitInstruction;
} XL6_IrInstantiation;

#define XL6_IR_FIELD_PERSIST_INCLUDED   0x00000001u
#define XL6_IR_FIELD_PERSIST_REQUIRED   0x00000002u
#define XL6_IR_FIELD_PERSIST_OMIT_EMPTY 0x00000004u
#define XL6_IR_FIELD_PERSIST_OMIT_NULL  0x00000008u
#define XL6_IR_FIELD_PERSIST_ALL        0x0000000Fu

#define XL6_IR_PERSIST_FORMAT_NONE  0x00000000u
#define XL6_IR_PERSIST_FORMAT_VALUE 0x00000001u
#define XL6_IR_PERSIST_FORMAT_JSON  0x00000002u
#define XL6_IR_PERSIST_FORMAT_XLON  0x00000004u
#define XL6_IR_PERSIST_FORMAT_ALL   0x00000007u

/* Persistence must distinguish a closed class schema from the language's
 * open `object` storage.  OPEN_DYNAMIC_SCHEMA means the serialized value owns
 * an additional, schema-independent key set which must survive projection and
 * reconstruction.  This is a frontend-resolved layout fact; lowering and
 * Cgen must not rediscover the `object` base by spelling. */
typedef enum XL6_IrPersistenceObjectAbi {
    XL6_IR_PERSISTENCE_OBJECT_NONE = 0,
    XL6_IR_PERSISTENCE_OBJECT_CLOSED_SCHEMA,
    XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_ROOT,
    XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA
} XL6_IrPersistenceObjectAbi;

/* Whether a decoder may construct this class without executing a user create
 * body.  SCHEMA_DEFAULT means every base/field has a verified persistence
 * default and no open dynamic-storage allocation is required. */
typedef enum XL6_IrPersistenceConstructionAbi {
    XL6_IR_PERSISTENCE_CONSTRUCTION_NONE = 0,
    XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT
} XL6_IrPersistenceConstructionAbi;

#define XL6_IR_CLASS_FIELD_INDEX_INVALID ((size_t)-1)

/* Static class state has a different lifetime and addressability contract
 * from an xvalue object's instance layout.  The semantic tree does not give
 * static field declarations an ordinary value symbol, so typed XLIR assigns a
 * stable synthetic storage symbol here before lowering.  Cgen subsequently
 * sees only normal GLOBAL storage loads and stores. */
typedef struct XL6_IrClassStaticField {
    XL6_IrTypeId ownerType;
    XL6_IrNodeId ownerDeclaration;
    XL6_IrTypeId type;
    XL6_IrNodeId declaration;
    XL6_IrNodeId initializer;
    XL6_SymbolId storageSymbol;
    char* name;
} XL6_IrClassStaticField;

/* Copy/clone protocol selection belongs to typed class layout, not to Cgen.
 * A runtime ABI means a compiler-derived structural operation; INTERNAL_CALL
 * names a concrete user declaration whose complete callable ABI is validated
 * after lowering registers call targets. */
typedef enum XL6_IrClassCopyAbi {
    XL6_IR_CLASS_COPY_ABI_NONE = 0,
    /* The class owns a unique native resource and has no copy protocol.
     * Language copy/clone evaluates the source and yields null rather than
     * aliasing that resource.  Keeping this distinct from NONE makes the
     * rejection behavior a verified semantic ABI instead of a lowering
     * fallback. */
    XL6_IR_CLASS_COPY_ABI_NONCOPYABLE_NULL,
    XL6_IR_CLASS_COPY_ABI_RUNTIME_SHALLOW,
    XL6_IR_CLASS_COPY_ABI_RUNTIME_DEEP,
    XL6_IR_CLASS_COPY_ABI_INTERNAL_CALL
} XL6_IrClassCopyAbi;

/* Class string protocol selection is owned by ClassLayout.  RUNTIME_DERIVED
 * means structural formatting over the layout's explicitly marked fields;
 * INTERNAL_CALL names one concrete user __toString__ declaration. */
typedef enum XL6_IrClassStringAbi {
    XL6_IR_CLASS_STRING_ABI_NONE = 0,
    XL6_IR_CLASS_STRING_ABI_RUNTIME_DERIVED,
    XL6_IR_CLASS_STRING_ABI_INTERNAL_CALL
} XL6_IrClassStringAbi;

/* Hash and equality are separate source protocols, but one ClassLayout owns
 * both contracts so keyed collections can require a complete identity pair.
 * RUNTIME_REFERENCE is the default object-identity policy, RUNTIME_DERIVED
 * uses the structural @compare(skip) field partition, and INTERNAL_CALL names
 * the exact user protocol declaration. */
typedef enum XL6_IrClassHashAbi {
    XL6_IR_CLASS_HASH_ABI_NONE = 0,
    XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE,
    XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED,
    XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL
} XL6_IrClassHashAbi;

typedef enum XL6_IrClassEqualityAbi {
    XL6_IR_CLASS_EQUALITY_ABI_NONE = 0,
    XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE,
    XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED,
    XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL
} XL6_IrClassEqualityAbi;

/* One user-class lifecycle step.  Layouts own a flattened derived-to-base
 * slice so neither lowering nor Cgen walks source ancestry or guesses whether
 * an XL6_AST_CREATE declaration spells create or destroy. */
typedef struct XL6_IrClassDestroyStep {
    XL6_IrTypeId receiverType;
    XL6_IrNodeId declaration;
    XL6_SymbolId targetSymbol;
} XL6_IrClassDestroyStep;

typedef struct XL6_IrClassLayout {
    XL6_IrTypeId type;
    XL6_IrTypeId baseType;
    XL6_IrNodeId declaration;
    /* Always records the syntactic/semantic class declaration edge when an
     * extends clause exists, even if the base has no independently interned
     * runtime type.  This keeps inheritance a typed-IR fact rather than an
     * accidental consequence of value-type reachability. */
    XL6_IrNodeId baseDeclaration;
    /* Nonzero module-local identity for the concrete runtime object shape.
     * It is assigned by typed XLIR, never inferred from C addresses or a
     * source class spelling.  Lowering will materialize it into the object
     * metadata key before any virtual/is/as operation is admitted. */
    uint32 runtimeTypeTag;
    size_t firstField;
    size_t fieldCount;
    /* Prefix of this layout's field slice materialized from a declared base
     * which has no standalone runtime type.  It remains explicit so create
     * lowering can initialize inherited defaults before its own body. */
    size_t inheritedFieldCount;
    int hasExplicitCreate;
    size_t firstDestroyStep;
    size_t destroyStepCount;
    /* One-based lowered function, zero in typed IR or for an empty plan.
     * The native callback invokes this body; it never expands the step list. */
    uint32 destroyFunction;
    /* Exact source derive capabilities. VALUE includes structural value
     * conversion and enables both JSON and XLON projections. */
    uint32 persistenceFormatMask;
    XL6_IrPersistenceObjectAbi persistenceObjectAbi;
    XL6_IrPersistenceConstructionAbi persistenceConstructionAbi;
    /* Global ClassField arena indices for the open-object storage chain:
     * object.__fields and DynamicFields.handle.  CLOSED layouts keep both at
     * INVALID.  OPEN schemas inherit the exact same pair from the builtin
     * dynamic root, so Cgen never spells either implementation field. */
    size_t persistenceDynamicField;
    size_t persistenceDynamicHandleField;
    XL6_IrClassCopyAbi copyAbi;
    XL6_SymbolId copyTarget;
    XL6_IrClassCopyAbi cloneAbi;
    XL6_SymbolId cloneTarget;
    XL6_IrClassStringAbi stringAbi;
    XL6_SymbolId stringTarget;
    XL6_IrClassHashAbi hashAbi;
    XL6_SymbolId hashTarget;
    XL6_IrClassEqualityAbi equalityAbi;
    XL6_SymbolId equalityTarget;
} XL6_IrClassLayout;

/* A virtual method slot is a frontend-resolved callable contract.  It is
 * deliberately separate from an ordinary call target: its receiver may hold
 * any derived runtime object, and the selected implementation is determined
 * only by the verified class-dispatch table below.  Cgen must never recover
 * this relation from method names or source ancestry. */
typedef struct XL6_IrClassDispatchSlot {
    XL6_IrNodeId ownerDeclaration;
    XL6_IrTypeId ownerType;
    XL6_IrNodeId declaration;
    XL6_SymbolId declarationSymbol;
    XL6_IrTypeId receiverType;
    XL6_IrTypeId returnType;
    XL6_IrTypeId* parameterTypes;
    XL6_IrParameterAbi* parameterAbis;
    /* Joined implementation effect, fixed before body lowering. The receiver
     * is always borrowed; every other argument has the same ABI in all rows. */
    XL6_IrErrorAbi errorAbi;
    XL6_IrCallErrorPhysicalAbi errorPhysicalAbi;
    size_t parameterCount;
    char* name;
} XL6_IrClassDispatchSlot;

/* Each concrete class contributes exactly one implementation for every slot
 * it inherits.  `implementationDeclaration` is the actual body selected for
 * this class (possibly the inherited declaration); no backend performs a
 * fallback lookup when a row is absent. */
typedef struct XL6_IrClassDispatchImpl {
    size_t slot;
    XL6_IrNodeId ownerDeclaration;
    XL6_IrTypeId ownerType;
    XL6_IrNodeId implementationDeclaration;
    XL6_SymbolId implementationSymbol;
} XL6_IrClassDispatchImpl;

/* User protocols have a separate dispatch domain from class virtuals.  A
 * protocol handle carries a concrete xvalue at runtime, while each call is
 * governed by the requirement signature declared here.  The lowerer/Cgen
 * must use only this slot and its witness rows; neither is allowed to redo
 * semantic member lookup by name. */
typedef struct XL6_IrProtocolDispatchSlot {
    XL6_IrNodeId protocolDeclaration;
    XL6_IrTypeId protocolType;
    XL6_IrNodeId declaration;
    XL6_SymbolId declarationSymbol;
    XL6_IrTypeId returnType;
    /* Joined across all witnesses before operand lowering. No-raise bodies
     * remain no-raise under direct calls; the slot owns their result lift. */
    XL6_IrErrorAbi errorAbi;
    XL6_IrCallErrorPhysicalAbi errorPhysicalAbi;
    XL6_IrTypeId* parameterTypes;
    /* The protocol requirement owns its call boundary.  In particular Cgen
     * must never infer whether a managed argument is borrowed from the
     * selected concrete witness. */
    XL6_IrParameterAbi* parameterAbis;
    size_t parameterCount;
    char* name;
} XL6_IrProtocolDispatchSlot;

/* A row is present only when the concrete runtime class satisfies every
 * callable requirement of the protocol.  `implementationSymbol` is the
 * preselected source body, so generated C never falls back to discovery. */
typedef struct XL6_IrProtocolDispatchImpl {
    size_t slot;
    XL6_IrNodeId ownerDeclaration;
    XL6_IrTypeId ownerType;
    XL6_IrNodeId implementationDeclaration;
    XL6_SymbolId implementationSymbol;
} XL6_IrProtocolDispatchImpl;

/* Closed runtime identities accepted by one class or protocol target.  Class
 * hierarchies and protocol witnesses are resolved while Typed XLIR is built;
 * lowering/Cgen consume only this source-free set.  Runtime-tag sets preserve
 * derived-class identity; exact-TypeId sets cover nominal XRT classes without
 * a compiler-owned class layout (for example imported native classes). */
typedef enum XL6_IrNominalIdentityKind {
    XL6_IR_NOMINAL_IDENTITY_INVALID = 0,
    XL6_IR_NOMINAL_IDENTITY_CLASS_RUNTIME_TAG,
    XL6_IR_NOMINAL_IDENTITY_EXACT_TYPE_ID,
    XL6_IR_NOMINAL_IDENTITY_PROTOCOL_RUNTIME_TAG
} XL6_IrNominalIdentityKind;

typedef struct XL6_IrNominalIdentitySet {
    XL6_IrTypeId targetType;
    XL6_IrNominalIdentityKind kind;
    /* Class values not backed by an object use this exact bound identity;
     * object-backed values continue to use the derived-class runtime tags. */
    uint64 nonObjectTypeId;
    size_t firstIdentity;
    size_t identityCount;
} XL6_IrNominalIdentitySet;

/* An exit plan is typed frontend data, not a Cgen cleanup-stack snapshot.
 * Lowering must materialize every listed action before its terminator. */
typedef enum XL6_IrCleanupKind {
    XL6_IR_CLEANUP_INVALID = 0,
    XL6_IR_CLEANUP_RELEASE,
    XL6_IR_CLEANUP_DROP,
    /* Calls the native class lifecycle destructor identified by
     * XL6_IrCleanupAction::calleeSymbol with the owned handle storage as its
     * explicit borrowed receiver. */
    XL6_IR_CLEANUP_NATIVE_DESTROY,
    /* Releases the cell reference held by a cell-backed lexical storage;
     * this is distinct from replacing the element via CELL_STORE. */
    XL6_IR_CLEANUP_CELL_RELEASE,
    /* A trivial lexical binding still has an end-of-lifetime position. Cell
     * promotion replaces this anchor with CELL_RELEASE before emission; an
     * unpromoted binding needs no destructor. Scope and abrupt-exit plans
     * preserve the same lexical lifetime position. */
    XL6_IR_CLEANUP_END_STORAGE
} XL6_IrCleanupKind;

typedef struct XL6_IrCleanupAction {
    XL6_IrCleanupKind kind;
    XL6_IrTypeId type;
    XL6_SymbolId targetSymbol;
    /* Callable identity whose exit owns this action: a FUNC_DECL, or the
     * module root for MODULE_INIT.  Lowered modules may retain a dormant
     * generic-template plan, but only an owner present in the function arena
     * is executable and therefore must materialize cleanup. */
    XL6_IrNodeId ownerDeclaration;
    /* Nonzero only for NATIVE_DESTROY.  This is a semantic callable symbol,
     * not a C function name reconstructed by a backend. */
    XL6_SymbolId calleeSymbol;
    XL6_IrNodeId origin;
} XL6_IrCleanupAction;

typedef enum XL6_IrExitPoint {
    /* Abrupt source exit or failure while evaluating the origin node. */
    XL6_IR_EXIT_POINT_BEFORE_NODE = 0,
    /* Normal fallthrough AFTER all direct statements of the origin BLOCK.
     * Unlike a before-node exit, this owns declarations following its NodeId.
     * The list is explicit IR, never reconstructed by a backend cleanup stack. */
    XL6_IR_EXIT_POINT_AFTER_SCOPE,
    /* Failure AFTER the direct owners have been cleaned. This continuation
     * owns only live enclosing bindings; the finished scope is never replayed. */
    XL6_IR_EXIT_POINT_AFTER_SCOPE_FAILURE,
    /* Current item/key are closed; the loop-wide resource is still owned. */
    XL6_IR_EXIT_POINT_AFTER_LOOP_ITERATION_FAILURE,
    /* The loop-wide resource and current item/key have all been closed. */
    XL6_IR_EXIT_POINT_AFTER_LOOP_RESOURCE_FAILURE,
    /* Catch dispatch did not match, or exceptional finally completed. The
     * TRY's own children are closed; only enclosing owners remain live. */
    XL6_IR_EXIT_POINT_AFTER_TRY_EXCEPTION,
    /* Failure of an independent generated function, not of the lexical
     * expression which created its callable. Owns parameters and terminal
     * propagation only; evaluation owners remain explicit CFG transactions. */
    XL6_IR_EXIT_POINT_FUNCTION_FAILURE,
    /* Non-exceptional completion requested after a committed yield or a
     * completed try/finally boundary. Owns only still-active enclosing scopes. */
    XL6_IR_EXIT_POINT_GENERATOR_CLOSE
} XL6_IrExitPoint;

/* A normal abrupt or exceptional continuation, in lexical execution order.
 * A cleanup failure cancels a normal continuation and enters the
 * step's explicit exception boundary. In particular, a FINALLY precedes all
 * cleanup of bindings visible to that finally. This is typed IR topology;
 * the CFG still has to materialize and prove its ownership/outcome edges. */
typedef enum XL6_IrExitStepKind {
    XL6_IR_EXIT_STEP_INVALID = 0,
    XL6_IR_EXIT_STEP_SCOPE,
    XL6_IR_EXIT_STEP_CATCH_PAYLOAD,
    XL6_IR_EXIT_STEP_FINALLY,
    XL6_IR_EXIT_STEP_LOOP_ITERATION,
    XL6_IR_EXIT_STEP_LOOP_RESOURCE,
    XL6_IR_EXIT_STEP_PARAMETERS,
    XL6_IR_EXIT_STEP_TERMINAL
} XL6_IrExitStepKind;

typedef struct XL6_IrExitStep {
    XL6_IrExitStepKind kind;
    XL6_IrNodeId origin;
    /* Only SCOPE has a cleanup slice, drawn from its owning exit plan. */
    size_t firstCleanup;
    size_t cleanupCount;
    /* LOOP_ITERATION closes only this source-ordered initialized prefix.
     * A failure constructing target N leaves targets [0, N) installed. */
    size_t loopTargetCount;
    /* TRY with catches, a finally BLOCK, or the enclosing callable. No
     * runtime handler-stack search is needed to choose this destination. */
    XL6_IrNodeId failureTarget;
    uint32 failureScopeDepth;
    uint32 failureRegion;
} XL6_IrExitStep;

typedef struct XL6_IrExitPlan {
    XL6_IrNodeId origin;
    XL6_IrExitPoint point;
    XL6_SemExitKind kind;
    uint32 targetScopeDepth;
    uint32 targetRegion;
    size_t firstCleanup;
    size_t cleanupCount;
    /* Ordered normal abrupt or exceptional continuation. AFTER_SCOPE alone
     * is a direct owner list rather than an exit route. */
    size_t firstStep;
    size_t stepCount;
} XL6_IrExitPlan;

typedef struct XL6_IrStats {
    size_t nodeCount;
    size_t typeCount;
    size_t typedValueNodeCount;
    size_t resolvedCallCount;
    size_t ownershipPlanCount;
    size_t exitPlanCount;
    size_t blockCount;
    size_t instructionCount;
    size_t functionCount;
} XL6_IrStats;

typedef struct XL6_IrModule {
    XL6_IrStage stage;
    XL6_IrTypePool types;
    XL6_IrRuntimeTypeMap runtimeTypeMap;
    XL6_IrNode* nodes;
    size_t nodeCount;
    size_t nodeCapacity;
    XL6_IrNodeId* children;
    size_t childCount;
    size_t childCapacity;
    XL6_IrNodeId* symbolNodes;
    size_t symbolNodeCapacity;
    XL6_IrBlock* blocks;
    size_t blockCount;
    size_t blockCapacity;
    XL6_IrFunction* functions;
    XL6_IrLoopOwner* loopOwners;
    size_t loopOwnerCount;
    size_t loopOwnerCapacity;
    XL6_IrPendingException* pendingExceptions;
    size_t pendingExceptionCount;
    size_t pendingExceptionCapacity;
    size_t functionCount;
    size_t functionCapacity;
    XL6_IrCallTarget* callTargets;
    size_t callTargetCount;
    size_t callTargetCapacity;
    XL6_IrCallSite* callSites;
    size_t callSiteCount;
    size_t callSiteCapacity;
    XL6_IrCallArgument* callArguments;
    size_t callArgumentCount;
    size_t callArgumentCapacity;
    XL6_IrDynamicCallSite* dynamicCallSites;
    size_t dynamicCallSiteCount;
    size_t dynamicCallSiteCapacity;
    XL6_IrDynamicCallArgument* dynamicCallArguments;
    size_t dynamicCallArgumentCount;
    size_t dynamicCallArgumentCapacity;
    XL6_IrDynamicMethod* dynamicMethods;
    size_t dynamicMethodCount;
    size_t dynamicMethodCapacity;
    XL6_IrDynamicMethodParameter* dynamicMethodParameters;
    size_t dynamicMethodParameterCount;
    size_t dynamicMethodParameterCapacity;
    XL6_IrIntrinsicCall* intrinsicCalls;
    size_t intrinsicCallCount;
    size_t intrinsicCallCapacity;
    XL6_IrIntrinsicOperand* intrinsicOperands;
    size_t intrinsicOperandCount;
    size_t intrinsicOperandCapacity;
    XL6_IrValueInitializer* valueInitializers;
    size_t valueInitializerCount;
    size_t valueInitializerCapacity;
    XL6_IrNativeHandleBind* nativeHandleBinds;
    size_t nativeHandleBindCount;
    size_t nativeHandleBindCapacity;
    XL6_IrNativeHandleProject* nativeHandleProjects;
    size_t nativeHandleProjectCount;
    size_t nativeHandleProjectCapacity;
    XL6_IrClosureTarget* closureTargets;
    size_t closureTargetCount;
    size_t closureTargetCapacity;
    XL6_IrClosureCapture* closureCaptures;
    size_t closureCaptureCount;
    size_t closureCaptureCapacity;
    XL6_IrCallableBridge* callableBridges;
    size_t callableBridgeCount;
    size_t callableBridgeCapacity;
    XL6_IrCallableBridgeParameter* callableBridgeParameters;
    size_t callableBridgeParameterCount;
    size_t callableBridgeParameterCapacity;
    XL6_IrCallableBridgeResult* callableBridgeResults;
    size_t callableBridgeResultCount;
    size_t callableBridgeResultCapacity;
    XL6_IrCallbackBoundary* callbackBoundaries;
    size_t callbackBoundaryCount;
    size_t callbackBoundaryCapacity;
    XL6_IrCallbackCapture* callbackCaptures;
    size_t callbackCaptureCount;
    size_t callbackCaptureCapacity;
    XL6_IrAsyncMachine* asyncMachines;
    size_t asyncMachineCount;
    size_t asyncMachineCapacity;
    XL6_IrAsyncSuspension* asyncSuspensions;
    size_t asyncSuspensionCount;
    size_t asyncSuspensionCapacity;
    XL6_IrAsyncMachineFrameSlot* asyncMachineFrameSlots;
    size_t asyncMachineFrameSlotCount;
    size_t asyncMachineFrameSlotCapacity;
    XL6_IrAsyncLiveSlot* asyncLiveSlots;
    size_t asyncLiveSlotCount;
    size_t asyncLiveSlotCapacity;
    XL6_IrAsyncFrameAction* asyncFrameActions;
    size_t asyncFrameActionCount;
    size_t asyncFrameActionCapacity;
    XL6_IrAsyncMachineEntry* asyncMachineEntries;
    size_t asyncMachineEntryCount;
    size_t asyncMachineEntryCapacity;
    XL6_IrAsyncMachineBlock* asyncMachineBlocks;
    size_t asyncMachineBlockCount;
    size_t asyncMachineBlockCapacity;
    XL6_IrGeneratorMachine* generatorMachines;
    size_t generatorMachineCount;
    size_t generatorMachineCapacity;
    XL6_IrGeneratorSuspension* generatorSuspensions;
    size_t generatorSuspensionCount;
    size_t generatorSuspensionCapacity;
    XL6_IrContinuationFrameSlot* generatorFrameSlots;
    size_t generatorFrameSlotCount;
    size_t generatorFrameSlotCapacity;
    XL6_IrContinuationLiveSlot* generatorLiveSlots;
    size_t generatorLiveSlotCount;
    size_t generatorLiveSlotCapacity;
    XL6_IrContinuationFrameAction* generatorFrameActions;
    size_t generatorFrameActionCount;
    size_t generatorFrameActionCapacity;
    XL6_IrGeneratorMachineEntry* generatorMachineEntries;
    size_t generatorMachineEntryCount;
    size_t generatorMachineEntryCapacity;
    XL6_IrGeneratorMachineBlock* generatorMachineBlocks;
    size_t generatorMachineBlockCount;
    size_t generatorMachineBlockCapacity;
    XL6_IrExport* exports;
    size_t exportCount;
    size_t exportCapacity;
    XL6_IrGlobalExport* globalExports;
    size_t globalExportCount;
    size_t globalExportCapacity;
    XL6_IrExternGlobal* externGlobals;
    size_t externGlobalCount;
    size_t externGlobalCapacity;
    XL6_IrGenericSpecialization* genericSpecializations;
    size_t genericSpecializationCount;
    size_t genericSpecializationCapacity;
    XL6_IrModuleAbi abi;
    XL6_SymbolId* functionParameters;
    size_t functionParameterCount;
    size_t functionParameterCapacity;
    XL6_IrStorage* storages;
    size_t storageCount;
    size_t storageCapacity;
    XL6_IrUnionLayout* unionLayouts;
    size_t unionLayoutCount;
    size_t unionLayoutCapacity;
    XL6_IrUnionField* unionFields;
    size_t unionFieldCount;
    size_t unionFieldCapacity;
    XL6_IrEnumLayout* enumLayouts;
    size_t enumLayoutCount;
    size_t enumLayoutCapacity;
    XL6_IrEnumVariant* enumVariants;
    size_t enumVariantCount;
    size_t enumVariantCapacity;
    XL6_IrEnumPayload* enumPayloads;
    size_t enumPayloadCount;
    size_t enumPayloadCapacity;
    XL6_IrClassLayout* classLayouts;
    size_t classLayoutCount;
    size_t classLayoutCapacity;
    XL6_IrClassDestroyStep* classDestroySteps;
    size_t classDestroyStepCount;
    size_t classDestroyStepCapacity;
    XL6_IrClassField* classFields;
    size_t classFieldCount;
    size_t classFieldCapacity;
    XL6_IrInstantiation* instantiations;
    size_t instantiationCount;
    size_t instantiationCapacity;
    XL6_IrClassStaticField* classStaticFields;
    size_t classStaticFieldCount;
    size_t classStaticFieldCapacity;
    XL6_IrClassDispatchSlot* classDispatchSlots;
    size_t classDispatchSlotCount;
    size_t classDispatchSlotCapacity;
    XL6_IrClassDispatchImpl* classDispatchImpls;
    size_t classDispatchImplCount;
    size_t classDispatchImplCapacity;
    XL6_IrProtocolDispatchSlot* protocolDispatchSlots;
    size_t protocolDispatchSlotCount;
    size_t protocolDispatchSlotCapacity;
    XL6_IrProtocolDispatchImpl* protocolDispatchImpls;
    size_t protocolDispatchImplCount;
    size_t protocolDispatchImplCapacity;
    XL6_IrNominalIdentitySet* nominalIdentitySets;
    size_t nominalIdentitySetCount;
    size_t nominalIdentitySetCapacity;
    uint64* nominalIdentities;
    size_t nominalIdentityCount;
    size_t nominalIdentityCapacity;
    XL6_IrCleanupAction* cleanupActions;
    size_t cleanupActionCount;
    size_t cleanupActionCapacity;
    XL6_IrExitPlan* exitPlans;
    size_t exitPlanCount;
    size_t exitPlanCapacity;
    XL6_IrExitStep* exitSteps;
    size_t exitStepCount;
    size_t exitStepCapacity;
    XL6_IrInstruction* instructions;
    size_t instructionCount;
    size_t instructionCapacity;
    XL6_IrValueId* operands;
    size_t operandCount;
    size_t operandCapacity;
    XL6_IrBlockId entryBlock;
    XL6_IrValueId valueCount;
    XL6_IrNodeId root;
    XL6_IrStats stats;
    int ready;
} XL6_IrModule;

void xl6IrModuleInit(XL6_IrModule* module);
void xl6IrModuleFree(XL6_IrModule* module);
/* Primary frontend entry: copy the semantic AST into owned typed XLIR. */
int xl6IrBuildModuleFromAst(
    XL6_IrModule* module,
    const XL6_AstNode* root,
    XL6_DiagList* diags
);
int xl6IrVerifyModule(const XL6_IrModule* module, XL6_DiagList* diags);
XL6_IrAsyncFrameTransferAbi xl6IrParameterFrameTransfer(
    const XL6_IrModule* module, XL6_IrTypeId type, XL6_IrParameterAbi parameterAbi);
XL6_IrAsyncCaptureAbi xl6IrAsyncParameterCapture(
    const XL6_IrModule* module, XL6_IrTypeId type, XL6_IrParameterAbi parameterAbi);
XL6_IrContinuationFrameTraceAbi xl6IrOwnedValueTraceAbi(
    const XL6_IrModule* module, XL6_IrTypeId type);
XL6_IrAsyncFrameDestroyAbi xl6IrOwnedValueDestroyAbi(
    const XL6_IrModule* module, XL6_IrTypeId type);
/* Managed read expressions own their result independently of source lvalue syntax. */
int xl6IrVerifyOwnedReadResults(const XL6_IrModule* module, XL6_DiagList* diags);
/* File bridge recognition and its complete physical ABI are shared by
 * lowering and final verification, so the two phases cannot drift into
 * accepting different compiler-private helper signatures. */
int xl6IrRuntimeFileBridgeName(const char* name);
XL6_IrErrorAbi xl6IrRuntimeFileBridgeErrorAbi(const char* name);
int xl6IrRuntimeFileBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* lowering 与验证器共用字符串合同，不分别猜测参数或返回值类型。 */
XL6_IrRuntimeBridgeKind xl6IrRuntimeStringBridgeKind(const char* name);
XL6_IrErrorAbi xl6IrRuntimeStringBridgeErrorAbi(const char* name);
int xl6IrRuntimeStringBridgeKindIs(XL6_IrRuntimeBridgeKind kind);
int xl6IrRuntimeStringBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* One declarative JSON/XLON ABI registry is shared by lowering and verification. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeSerializationBridgeKind(const char* name);
/* Value/resource bridges share one signature/ownership/effect registry. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeDirectBridgeKind(const char* name);
const char* xl6IrRuntimeNativeHandlePolicyForName(const char* name);
int xl6IrRuntimeDirectBridgeFactory(const char* name);
XL6_IrErrorAbi xl6IrRuntimeDirectBridgeErrorAbi(const char* name);
const char* xl6IrRuntimeDirectBridgeParameterOwner(const char* name, size_t parameter);
/* Borrow the exact checked compiler closure instead of erasing it to the
 * general XL6_Function foreign ABI. Signature still needs full verification. */
int xl6IrRuntimeDirectBridgeCheckedClosureParameter(const char* name, size_t parameter);
XL6_IrRuntimeBridgeKind xl6IrRuntimeXidBridgeKind(const char* name);
int xl6IrRuntimeXidBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
XL6_IrRuntimeBridgeKind xl6IrRuntimeIoBridgeKind(const char* name);
int xl6IrRuntimeLogBridgeName(const char* name);
int xl6IrRuntimeLogBridgeFactory(const char* name);
int xl6IrRuntimeLogBridgeForeignScope(const char* name);
XL6_IrErrorAbi xl6IrRuntimeLogBridgeErrorAbi(const char* name);
const char* xl6IrRuntimeLogBridgeParameterOwner(const char* name, size_t parameter);
int xl6IrRuntimeLogBridgeTargetMatches(const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrRuntimeLogPrintfSiteMatches(const XL6_IrModule* module, const XL6_IrCallSite* site);
XL6_IrRuntimeBridgeKind xl6IrRuntimeVfsBridgeKind(const char* name);
int xl6IrRuntimeVfsBridgeIsFactory(const char* name);
XL6_IrErrorAbi xl6IrRuntimeVfsBridgeErrorAbi(const char* name);
const char* xl6IrRuntimeFsBridgeModule(XL6_IrRuntimeBridgeKind kind);
int xl6IrRuntimeFsBridgeUsesOwner(XL6_IrRuntimeBridgeKind kind, const char* owner);
const char* xl6IrRuntimeVfsBridgeParameterOwner(const char* name, size_t parameter);
int xl6IrRuntimeVfsBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrRuntimeIoBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrRuntimeXlangBridgeName(const char* name);
XL6_IrErrorAbi xl6IrRuntimeXlangBridgeErrorAbi(const char* name);
int xl6IrRuntimeXlangBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Visitor status codes and callback exceptions are independent outcomes. */
int xl6IrRuntimeSerializationBridgeHasErrorCarrierResult(XL6_IrRuntimeBridgeKind kind);
int xl6IrRuntimeSerializationBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Resolve generated-program TCP declarations into exact component roles. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeNetTcpBridgeKind(const char* name);
/* Closed resolver/datagram registry. Complete C and ownership signatures are
 * checked before assigning a component; names are not consumed by Cgen. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeNetDatagramBridgeKind(const char* name);
const XL6_IrClassField* xl6IrRuntimeNetNativeField(const XL6_IrModule* module,
    XL6_IrTypeId type, XL6_IrRuntimeBridgeKind destroyRole);
const XL6_IrClassField* xl6IrRuntimeNetBufferField(const XL6_IrModule* module);
int xl6IrRuntimeNetDatagramBridgeKindIs(XL6_IrRuntimeBridgeKind kind);
int xl6IrRuntimeNetDatagramBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrRuntimeNetTcpBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Resolve the exact operation inside a broad network bridge.  Lowering owns
 * name recognition; verification proves the stored operation and physical
 * payload ABI agree before Cgen consumes it. */
XL6_IrRuntimeBridgeOperation xl6IrRuntimeBridgeOperationForName(
    const char* name);
int xl6IrRuntimeBridgeOperationTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Resolve generated-program TLS declarations into exact component roles. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeNetTlsBridgeKind(const char* name);
int xl6IrRuntimeNetTlsBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Resolve and validate compiler-owned WebSocket component declarations once
 * while their complete semantic and physical call ABI is available. */
XL6_IrRuntimeBridgeKind xl6IrRuntimeWebSocketBridgeKind(const char* name);
int xl6IrRuntimeWebSocketBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Resolve HTTP/1 compiler-owned declarations into one verified component. */
int xl6IrRuntimeHttp1BridgeName(const char* name);
int xl6IrRuntimeHttp1BridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
/* Generic class specialization appends concrete layouts and substitutes
 * member result TypeIds before body lowering.  Refresh their canonical
 * MemberAbi identities at that boundary so Cgen never receives a template
 * representation choice. */
int xl6IrRefreshMemberAbis(XL6_IrModule* module);
/* Generic specialization can replace an index receiver/element TypeId.
 * Recompute the lvalue mutation identity before lowering those cloned bodies. */
int xl6IrRefreshIndexWriteAbis(XL6_IrModule* module);
/* Generic specialization substitutes operand/result TypeIds in cloned
 * operator nodes.  Recompute their exact execution domain, conversion path
 * and failure effect before any cloned body is lowered. */
int xl6IrRefreshOperatorAbis(XL6_IrModule* module);
/* Generic protocol specialization can intern an iterator/result TypeId and
 * replace template method identities with monomorphic call targets.  Rebuild
 * the complete loop execution/resource contract at that lowering boundary. */
int xl6IrRefreshLoopAbis(XL6_IrModule* module);
/* Rebuild declaration initializers and contextual conversion effects from
 * concrete TypeIds after specialization, before rebuilding exit plans. */
int xl6IrRefreshConversionAbis(XL6_IrModule* module);
/* Capture analysis can introduce allocations at declaration/parameter entry.
 * Close those construction effects before building exits and callable effects. */
int xl6IrRefreshConstructionAbis(XL6_IrModule* module);
/* Recompute semantic-exit cleanup vectors after a lowering-time ABI refresh
 * changes whether an operation may raise or owns a lexical resource. */
int xl6IrRefreshExitPlans(XL6_IrModule* module);
int xl6IrRefreshExitRoutes(XL6_IrModule* module);
const char* xl6IrExitStepKindName(XL6_IrExitStepKind kind);
XL6_IrErrorAbi xl6IrCopyIntrinsicErrorAbi(const XL6_IrModule* module,
    XL6_IrTypeId type, XL6_IrIntrinsicKind kind);
/* Generic specialization may intern concrete nominal class types after Typed
 * XLIR construction.  Append exactly the missing runtime layouts and rebuild
 * their derived destruction/persistence contracts before body lowering. */
int xl6IrRefreshClassLayouts(XL6_IrModule* module);
/* Validated execution provenance, distinct from instruction->sourceSpan. */
XL6_IrNodeId xl6IrInstructionExecutionOrigin(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
/* Runtime tags are derived from the complete concrete ClassLayout set. */
int xl6IrRefreshNominalIdentitySets(XL6_IrModule* module);
/* Replace one Typed dynamic-method template row with the isolated
 * monomorphic declaration produced by generic lowering.  The parameter arena
 * is rebuilt from that declaration as one atomic ABI update; backends never
 * choose between a generic source row and a concrete runtime candidate. */
int xl6IrRebindDynamicMethodSpecialization(
    XL6_IrModule* module, size_t methodIndex, XL6_IrNodeId declaration);
/* Rebuild the exact root/transitive callable closure after all lowered
 * functions, dispatch tables and async continuation states are final. */
int xl6IrRefreshReachability(XL6_IrModule* module);
/* Derive suspension boundaries and cross-suspension liveness exclusively from
 * verified Lowered XLIR blocks/instructions. */
int xl6IrTaskSpawnParameterAbiSupported(const XL6_IrType* type, XL6_IrParameterMode mode, XL6_IrParameterAbi abi);
int xl6IrTaskSpawnArgumentCanBind(const XL6_IrModule* module, XL6_IrTypeId actualType, XL6_IrTypeId parameterType);
int xl6IrBuildCallableBridges(XL6_IrModule* module, XL6_DiagList* diags);
const XL6_IrCallableBridge* xl6IrCallableBridgeGet(
    const XL6_IrModule* module, XL6_IrTypeId signatureType);
const XL6_IrCallableBridgeParameter* xl6IrCallableBridgeParameterGet(
    const XL6_IrModule* module, XL6_IrTypeId signatureType, size_t parameter);
int xl6IrBuildCallbackBoundaries(XL6_IrModule* module, XL6_DiagList* diags);
int xl6IrBuildAsyncMachines(XL6_IrModule* module, XL6_DiagList* diags);
/* Derive synchronous generator yield/resume slices and frame liveness solely
 * from Lowered XLIR. */
int xl6IrBuildGeneratorMachines(XL6_IrModule* module, XL6_DiagList* diags);
void xl6IrBindModuleStorageTrace(XL6_IrModule* module);
/* Lowers the verified Typed XLIR tree into explicit blocks and instructions.
 * It never reads AST or HIR objects; unsupported constructs fail rather than
 * being guessed by a backend. */
int xl6IrLowerModule(XL6_IrModule* module, XL6_DiagList* diags);
const XL6_IrNode* xl6IrNodeGet(const XL6_IrModule* module, XL6_IrNodeId nodeId);
const XL6_IrNode* xl6IrSymbolNodeGet(const XL6_IrModule* module, XL6_SymbolId symbol);
const XL6_IrType* xl6IrTypeGet(const XL6_IrTypePool* pool, XL6_IrTypeId typeId);
/* One closed payload transfer contract shared by Typed IR, lowering and Cgen.
 * Bytes use the language's existing read-sharing/write-copy discipline;
 * retaining their physical carrier does not grant a mutable language alias. */
XL6_IrRuntimeResourcePayloadAbi xl6IrTypeResourcePayloadAbi(const XL6_IrType* type);
int xl6IrStringCompareRuntimeSignatureValid(const XL6_IrModule*, const XL6_IrInstruction*);
/* Exact (bool, optional payload) product for non-exceptional empty retrieval. */
int xl6IrStackTryResultCompatible(const XL6_IrModule*, XL6_IrTypeId, XL6_IrTypeId);
/* Return the precomputed closed nominal acceptance set for one concrete class
 * or protocol target.  Lowering and Cgen consume this set directly; neither is
 * permitted to rediscover inheritance or protocol witnesses. */
const XL6_IrNominalIdentitySet* xl6IrNominalIdentitySetGet(
    const XL6_IrModule* module, XL6_IrTypeId targetType,
    XL6_IrNominalIdentitySetId* idOut);
/* One canonical predicate owns all synthetic THROW-plan admission.  It keeps
 * Typed construction, lowering, verification and statistics from developing
 * separate lists of operations which can fail without source throw syntax. */
int xl6IrNodeIsDynamicMemberRead(const XL6_IrModule* module, XL6_IrNodeId nodeId);
int xl6IrNodeIsDynamicMemberWrite(const XL6_IrModule* module, XL6_IrNodeId nodeId);
XL6_IrDynamicCallResultAbi xl6IrDynamicCallExpectedResultAbi(
    const XL6_IrModule* module, const XL6_IrDynamicCallSite* site);
int xl6IrNodeHasFallibleMemberWrite(const XL6_IrModule* module, XL6_IrNodeId nodeId);
int xl6IrInstructionIsFallibleProjection(const XL6_IrModule* module, const XL6_IrInstruction* instruction);
int xl6IrNodeRequiresThrowExitPlan(
    const XL6_IrModule* module, XL6_IrNodeId nodeId);
int xl6IrNodeRequiresAnyExitPlan(
    const XL6_IrModule* module, XL6_IrNodeId nodeId);
const XL6_IrExitPlan* xl6IrScopeExitPlanGet(
    const XL6_IrModule* module, XL6_IrNodeId block);
int xl6IrScopeCleanupMayRaise(const XL6_IrModule* module, XL6_IrNodeId block);
const XL6_IrExitPlan* xl6IrScopeFailurePlanGet(
    const XL6_IrModule* module, XL6_IrNodeId block);
const XL6_IrExitPlan* xl6IrLoopFailurePlanGet(
    const XL6_IrModule* module, XL6_IrNodeId loop, int afterResource);
const XL6_IrExitPlan* xl6IrTryExceptionPlanGet(
    const XL6_IrModule* module, XL6_IrNodeId tryNode);
int xl6IrLoopCleanupMayRaise(const XL6_IrModule* module,
    XL6_IrNodeId loop, int resource);
/* Closed set of source TypeIds which have an explicit typed operation for
 * producing one owned runtime xvalue.  Typed admission, lowering and verifier
 * use this same predicate; adding a representation is an IR contract change,
 * never a backend-side boxing guess. */
int xl6IrTypeCanBoxRuntimeXValue(
    const XL6_IrModule* module, XL6_IrTypeId typeId);
/* True when an ExternGlobal can use the TypeId's exact, trivially-copyable C
 * representation.  Nominal unions are admitted only with a verified layout;
 * lowering, verification and Cgen share this predicate. */
int xl6IrExternGlobalTypeSupported(
    const XL6_IrModule* module, XL6_IrTypeId typeId);
/* Return the verified physical layout for one concrete union TypeId.  Lowering
 * and backends share this lookup so neither reconstructs a layout from source
 * names or declaration order. */
const XL6_IrUnionLayout* xl6IrUnionLayoutGet(
    const XL6_IrModule* module, XL6_IrTypeId typeId);
/* Resolve one concrete tagged-enum layout or one of its semantic variants.
 * Duplicate metadata is rejected instead of being selected implicitly. */
const XL6_IrEnumLayout* xl6IrEnumLayoutGet(
    const XL6_IrModule* module, XL6_IrTypeId typeId);
/* Canonical payload identity of a named statement/expression pattern binding. */
const XL6_IrEnumPayload* xl6IrPatternBindingPayload(
    const XL6_IrModule* module, XL6_IrNodeId bindingId);
const XL6_IrEnumVariant* xl6IrEnumVariantGet(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    XL6_SymbolId variantSymbol, size_t* variantIndexOut);
/* Recompute the representation facts which depend on type parameters.  This
 * is used both by primary AST type interning and generic specialization. */
int xl6IrTypeFinalizeRepresentation(XL6_IrTypePool* pool, XL6_IrType* type);
/* Finalize the stable source-language `type` token for a concrete type.  The
 * value is structural and intentionally independent of the backend C ABI. */
int xl6IrTypeFinalizeTypeValueId(XL6_IrTypePool* pool, XL6_IrType* type);
/* Canonical direct-call ABI for a source parameter.  Call targets and
 * protocol requirements use the same ownership rule so a witness cannot
 * silently change the caller's lifetime contract. */
XL6_IrParameterAbi xl6IrParameterAbiForNode(
    const XL6_IrModule* module, const XL6_IrNode* parameter);
/* Derive the physical callable boundary from an explicit source mode and the
 * fully substituted parameter type. */
XL6_IrParameterAbi xl6IrParameterAbiForMode(
    const XL6_IrType* type, XL6_IrParameterMode mode);
/* The single scalar-conversion classifier shared by Typed construction,
 * lowering and verification.  INVALID means that the pair needs a checked,
 * wrapping, truth or other nontrivial operation instead. */
XL6_IrScalarConversionAbi xl6IrClassifyScalarConversionAbi(
    const XL6_IrType* source, const XL6_IrType* result);
/* Classify one conversion implied by a binary node's already-fixed
 * operatorExecutionType.  INVALID means either no conversion is required or
 * the operand cannot enter that execution domain. */
XL6_IrScalarConversionAbi xl6IrClassifyOperatorOperandConversionAbi(
    const XL6_IrModule* module, const XL6_IrNode* node, size_t operandIndex);
/* TypeId produced by ordinary expression lowering before an enclosing typed
 * boundary performs any conversion.  A flow-narrowed NAME materializes its
 * proven payload type; equivalent duplicate scalar builtins keep the exact
 * registered storage TypeId because they need no physical projection. */
XL6_IrTypeId xl6IrNodeMaterializedValueType(
    const XL6_IrModule* module, const XL6_IrNode* node);
/* Exact collection family fixed by an intrinsic kind.  Operations such as
 * COLLECTION_CONTAINS whose family is an explicit record field return NONE;
 * no later phase may recover either case from a member spelling. */
XL6_IrCollectionAbi xl6IrIntrinsicFixedCollectionAbiForKind(
    XL6_IrIntrinsicKind kind);
/* Canonical arity contract shared by Typed verification, lowering and Cgen. */
int xl6IrDynamicCollectionOpOperandCountValid(
    XL6_IrDynamicCollectionOp operation, size_t operandCount);
int xl6IrTypedCollectionOpOperandCountValid(
    XL6_IrTypedCollectionOp operation, size_t operandCount);
int xl6IrTypedCollectionOpSupportsCollection(
    XL6_IrTypedCollectionOp operation, XL6_IrCollectionAbi collectionAbi);
/* A statically shaped collection retains its own TypeId even when its item
 * is erased.  These queries never reinterpret an open var as a collection. */
int xl6IrTypeIsStaticallyShapedErasedCollection(
    const XL6_IrModule* module, XL6_IrTypeId type);
XL6_IrTypeId xl6IrCollectionElementType(
    const XL6_IrModule* module, XL6_IrTypeId receiverType);
XL6_IrTypeId xl6IrCollectionBoxedElementType(
    const XL6_IrModule* module, XL6_IrTypeId receiverType);
int xl6IrCollectionElementCompatible(
    const XL6_IrModule* module, XL6_IrTypeId receiverType,
    XL6_IrTypeId itemType);
int xl6IrCollectionContainsOperandCompatible(
    const XL6_IrModule* module, XL6_IrTypeId receiverType,
    XL6_IrTypeId itemType);
int xl6IrTypedCollectionSignatureCompatible(
    const XL6_IrModule* module, XL6_IrTypedCollectionOp operation,
    XL6_IrCollectionAbi collectionAbi, XL6_IrTypeId receiverType,
    XL6_IrTypeId resultType, const XL6_IrTypeId* operandTypes,
    size_t operandCount);
int xl6IrTypedCollectionRuntimeSignatureCompatible(
    const XL6_IrModule* module, XL6_IrTypedCollectionOp operation,
    XL6_IrCollectionAbi collectionAbi, XL6_IrTypeId receiverType,
    XL6_IrTypeId resultType, const XL6_IrTypeId* operandTypes,
    size_t operandCount);
int xl6IrCollectionMemberRuntimeOpIs(XL6_IrRuntimeOp operation);
/* Ordered physical bindings of one typed declaration, including destructuring.
 * A zero symbol denotes a discarded product field, never a local storage. */
typedef struct XL6_IrDeclarationBinding {
    XL6_IrNodeId origin;
    XL6_SymbolId symbol;
    XL6_IrTypeId type;
} XL6_IrDeclarationBinding;

int xl6IrDynamicFieldsRuntimeBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
int xl6IrNodeIsPropertyUpdate(const XL6_IrNode* node);
int xl6IrGetPropertyAccessorSignature(const XL6_IrModule* module,
    const XL6_IrCallSite* site, XL6_IrPropertyAccessorSignature* signature);
size_t xl6IrDeclarationBindingCount(const XL6_IrModule* module, XL6_IrNodeId declaration);
int xl6IrDeclarationBindingAt(const XL6_IrModule* module, XL6_IrNodeId declaration,
    size_t index, XL6_IrDeclarationBinding* binding);
/* Closed dynamic comparison: two borrowed boxed values produce a trivial bool.
 * Shared by module verification and every execution-mode C emitter. */
int xl6IrDynamicCompareRuntimeOpIs(XL6_IrRuntimeOp operation);
int xl6IrDynamicCompareRuntimeInstructionCompatible(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
int xl6IrCollectionReadRuntimeOpIs(XL6_IrRuntimeOp operation);
int xl6IrCollectionReadRuntimeInstructionCompatible(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
int xl6IrCollectionMemberRuntimeInstructionCompatible(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
/* Exact item transfer selected for generator<T> and every YIELD of its worker. */
XL6_IrGeneratorYieldAbi xl6IrGeneratorYieldAbiForType(
    const XL6_IrTypePool* types, XL6_IrTypeId itemType);
/* Resolve one closed collection-algebra operation from canonical Typed XLIR
 * facts.  NONE means the opcode/type combination is not in this semantic
 * family.  Lowering and verification share this function; Cgen receives only
 * the returned explicit runtime identity. */
XL6_IrRuntimeOp xl6IrCollectionAlgebraRuntimeOp(
    const XL6_IrTypePool* types, XL6_IrTypeId resultType,
    XL6_IrTypeId leftType, XL6_IrTypeId rightType, XL6_IrOpcode opcode);
int xl6IrCollectionAlgebraRuntimeOpIs(XL6_IrRuntimeOp operation);
int xl6IrCollectionAlgebraRuntimeSignatureValid(
    const XL6_IrTypePool* types, XL6_IrRuntimeOp operation,
    XL6_IrTypeId resultType, XL6_IrTypeId leftType, XL6_IrTypeId rightType);
XL6_IrCallVariadicAbi xl6IrCallVariadicAbiForParameter(
    const XL6_IrModule* module, const XL6_IrNode* parameter);
/* True when a lowered value may cross an ISO C ellipsis boundary without an
 * erased runtime wrapper or backend-selected representation conversion. */
int xl6IrTypeSupportsCVararg(const XL6_IrModule* module, XL6_IrTypeId type);
int xl6IrTaggedNullableScalarSupported(XL6_IrCAbi abi);
XL6_IrCAbi xl6IrNullableScalarElementAbi(const XL6_IrModule* module, XL6_IrTypeId type);
/* Fitting literals may choose a narrower expected integer TypeId;
 * nonliteral inputs still require a lossless scalar conversion. */
int xl6IrContextualIntegerLiteralValue(const XL6_IrModule* module, XL6_IrNodeId expression,
    XL6_IrTypeId targetType, uint64_t* magnitude, int* negative);
int xl6IrCollectionErasureAllowed(const XL6_IrModule* module,
    XL6_IrTypeId target, XL6_IrTypeId source);
XL6_IrCVarargPhysicalAbi xl6IrCVarargPhysicalAbi(
    const XL6_IrModule* module, XL6_IrTypeId type);
/* Exact source function signature admitted at a raw ISO C callback boundary.
 * Parameter/result representations and ownership are fully resolved here;
 * a backend may only render a signature for which this predicate is true. */
int xl6IrRawCCallbackSignatureSupported(
    const XL6_IrModule* module, XL6_IrTypeId type);
int xl6IrClosureRawCallbackAbiHasEntry(XL6_IrClosureRawCallbackAbi abi);
/* Exact fixed signatures admitted by the stable XL6_Function descriptor.
 * This is the one source of truth shared by lowering, verification and Cgen;
 * the backend only renders an adapter for an already admitted TypeId. */
int xl6IrLanguageFunctionValueSignatureSupported(
    const XL6_IrModule* module, XL6_IrTypeId type);
/* Runtime-callable bridges preserve the verified function TypeId.  The
 * closure-to-xvalue direction also admits dictionary variadics because the
 * call frame can collect unmatched keywords into the final dict carrier.
 * Class-field storage remains bidirectional and therefore deliberately uses
 * the narrower predicate until the reverse dict expansion ABI is admitted.
 * A successful closure invocation transfers every managed result slot to the
 * adapter. Result transport is transactional: move each xvalue owner, retain
 * the other carriers until boxing commits, and on failure release the current
 * box, committed prefix and all remaining carrier owners exactly once. Publish
 * failure before cleanup so a reentrant destructor cannot replace its cause.
 * Backends must not reclassify either contract from C spellings. */
int xl6IrRuntimeCallableClosureToXValueSignatureSupported(
    const XL6_IrModule* module, XL6_IrTypeId type);
int xl6IrRuntimeCallableFieldSignatureSupported(
    const XL6_IrModule* module, XL6_IrTypeId type);
XL6_IrTypeId xl6IrRuntimeCallableFieldSourceSignature(
    const XL6_IrModule* module, XL6_IrTypeId type);
int xl6IrTypeHasStorageReleaseAbi(
    const XL6_IrModule* module, XL6_IrTypeId type);
/* Ownership of a lowered SSA producer, independent of source value category:
 * COPY is trivial, BORROW observes storage, MOVE owns a transferable result,
 * NONE is unresolved. Representation-only projections preserve ownership. */
XL6_IrOwnership xl6IrValueOwnershipGet(
    const XL6_IrModule* module, XL6_IrValueId value);
XL6_IrOperandOwnership xl6IrRuntimeOperandOwnership(
    XL6_IrRuntimeOp operation, size_t operand);
XL6_IrOperandOwnership xl6IrInstructionOperandOwnership(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction, size_t operand);
/* Affine SSA transfer analysis, after lowered instruction/type/layout checks.
 * This does not replace storage/borrow liveness or partial-product validation. */
int xl6IrVerifyOwnerConsumption(const XL6_IrModule* module, XL6_DiagList* diags);
/* Lowered SSA availability and managed borrow protection across CFG edges. */
int xl6IrVerifyValueLifetimes(const XL6_IrModule* module, XL6_DiagList* diags);
int xl6IrFunctionParameterBindingOwnsStorage(const XL6_IrModule* module,
    const XL6_IrFunction* function, const XL6_IrCallTarget* target, size_t parameter);
int xl6IrCallTargetParameterBindingOwnsStorage(const XL6_IrModule* module,
    const XL6_IrCallTarget* target, size_t parameter);
int xl6IrCallTargetParameterCanBorrowReadOnly(const XL6_IrModule* module,
    const XL6_IrCallTarget* target, size_t parameter);
int xl6IrParameterBindingMayRebind(const XL6_IrModule* module, XL6_SymbolId symbol);
XL6_IrReleaseEffect xl6IrParameterCleanupEffect(const XL6_IrModule* module,
    const XL6_IrCallTarget* target, size_t parameter, int rebindable);
XL6_IrReleaseEffect xl6IrTypeReleaseEffect(
    const XL6_IrModule* module, XL6_IrTypeId type);
/* Returns 1 for one exact module-owned cleanup, 0 for non-owning storage,
 * and -1 for an unresolved owned representation. Shared by lowering and
 * verification; Cgen never classifies global lifetime itself. */
int xl6IrGlobalStorageCleanup(
    const XL6_IrModule* module, const XL6_IrStorage* storage,
    XL6_IrCleanupAction* action);
/* True only when every lowered callable requirement of `protocolType` has a
 * concrete witness row for `classType`.  This makes a class-to-protocol
 * rebind a checked XLIR conversion rather than an erased C pointer cast. */
int xl6IrProtocolClassImplements(
    const XL6_IrModule* module, XL6_IrTypeId protocolType, XL6_IrTypeId classType);
/* Intern the compiler-only physical TypeId carrier used by runtime identity
 * operations.  It is appended on demand so source-visible TypeIds remain
 * stable when a module does not otherwise spell the language `type` value. */
XL6_IrTypeId xl6IrTypeInternTypeIdCarrier(XL6_IrModule* module);
/* Intern the compiler-only cell<T> type used by mutable lexical captures.
 * `elementType` is the language-visible value ABI; the returned type is a
 * managed opaque cell pointer and never appears in source type inference. */
XL6_IrTypeId xl6IrTypeInternCell(XL6_IrModule* module, XL6_IrTypeId elementType);
XL6_IrTypeId xl6IrTypeInternValueIterator(
    XL6_IrModule* module, XL6_IrTypeId collectionType);
/* Intern the compiler-only ref<T> carrier consumed by INOUT/OUT calls. */
XL6_IrTypeId xl6IrTypeInternStorageRef(
    XL6_IrModule* module, XL6_IrTypeId elementType);
/* Intern the canonical tuple type for a language-level multi-result list.
 * This must be driven by the declaration ABI as well as by call expressions:
 * an unused callable declaration is still part of the complete XLIR type
 * graph.  Representation finalization records whether the tuple is a
 * currently supported trivial C product; callers must inspect that contract. */
XL6_IrTypeId xl6IrTypeInternTuple(
    XL6_IrModule* module, const XL6_IrTypeId* elementTypes, size_t elementCount);
const XL6_IrStorage* xl6IrStorageGet(const XL6_IrModule* module, XL6_SymbolId symbol);
/* Canonical requirement from materialized lowered operations, including dead
 * functions. The resource stays lazy; no AST/reachability guess is needed. */
XL6_IrModuleTaskPoolAbi xl6IrModuleTaskPoolRequirement(const XL6_IrModule* module);
XL6_IrModuleContinuationAbi xl6IrModuleContinuationRequirement(const XL6_IrModule* module);
XL6_IrModuleExecutorOwnershipAbi xl6IrModuleExecutorOwnershipRequirement(const XL6_IrModule* module);
XL6_IrModuleTaskPoolOwnershipAbi xl6IrModuleTaskPoolOwnershipRequirement(const XL6_IrModule* module);
XL6_IrFutureAggregateAbi xl6IrModuleFutureAggregateRequirement(const XL6_IrModule* module);
XL6_IrNativeFutureMapAbi xl6IrModuleNativeFutureMapRequirement(const XL6_IrModule* module);
XL6_IrModuleNetworkServiceAbi xl6IrModuleNetworkServiceRequirement(const XL6_IrModule* module);
int xl6IrModuleHasFinalizerWork(const XL6_IrModule* module);
const XL6_IrCallTarget* xl6IrCallTargetGet(const XL6_IrModule* module, XL6_SymbolId symbol);
XL6_IrCallResultConversionAbi xl6IrCallResultConversionForTarget(
    const XL6_IrModule* module, const XL6_IrCallTarget* target);
XL6_IrCallResultCAbi xl6IrCallResultCAbiForTarget(const XL6_IrCallTarget* target);
int xl6IrCallResultCAbiMatches(const XL6_IrModule* module, const XL6_IrCallTarget* target);
const XL6_IrCallSite* xl6IrCallSiteGet(const XL6_IrModule* module, XL6_IrNodeId origin);
XL6_IrNodeId xl6IrCallParameterListGet(const XL6_IrModule* module, XL6_IrNodeId declaration);
const XL6_IrCallArgument* xl6IrCallSiteEvaluationArgument(
    const XL6_IrModule* module, const XL6_IrCallSite* site, size_t evaluationIndex);
const XL6_IrIntrinsicCall* xl6IrIntrinsicCallGet(
    const XL6_IrModule* module, XL6_IrNodeId origin);
const XL6_IrValueInitializer* xl6IrValueInitializerGet(
    const XL6_IrModule* module, XL6_IrNodeId declaration);
int xl6IrContextualCallableCastRequired(const XL6_IrModule* module,
    XL6_IrTypeId source, XL6_IrTypeId target);
int xl6IrContextualCallableCastAllowed(const XL6_IrModule* module,
    XL6_IrNodeId origin, XL6_IrNodeId source, XL6_IrTypeId target);
int xl6IrContextualCheckedIntegerConversionAllowed(
    const XL6_IrModule* module, XL6_IrNodeId origin,
    XL6_IrNodeId source, XL6_IrTypeId target);
const XL6_IrNativeHandleBind* xl6IrNativeHandleBindGet(
    const XL6_IrModule* module, XL6_IrNodeId origin);
const XL6_IrNativeHandleProject* xl6IrNativeHandleProjectGet(
    const XL6_IrModule* module, XL6_IrNodeId origin);
XL6_IrExportValueAbi xl6IrExportValueAbiForType(const XL6_IrType* type);
XL6_IrExportValueAbi xl6IrExportParameterValueAbiForType(
    const XL6_IrModule* module, XL6_IrTypeId type, XL6_IrParameterAbi abi);
XL6_IrTypeId xl6IrExportCallbackSignatureType(const XL6_IrModule* module, XL6_IrTypeId type);
int xl6IrExportValueAbiIsCallback(XL6_IrExportValueAbi abi);
const XL6_IrExport* xl6IrExportGet(const XL6_IrModule* module, XL6_SymbolId symbol);
const XL6_IrGlobalExport* xl6IrGlobalExportGet(const XL6_IrModule* module, XL6_SymbolId storageSymbol);
const XL6_IrExternGlobal* xl6IrExternGlobalGet(const XL6_IrModule* module, XL6_SymbolId storageSymbol);
/* Canonical outcome of a call or async/generator launch. This describes
 * the invoked operation, not an arbitrary implementation of a dispatch slot. */
XL6_IrErrorAbi xl6IrCallInstructionErrorAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
XL6_IrCallErrorPhysicalAbi xl6IrCallInstructionErrorPhysicalAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
/* Fixed result protocol for runtime string allocation. INVALID means this
 * operation is outside the family. Shared by lowering and module verification;
 * a nullable string copy uses status, never NULL as a failure discriminator. */
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeStringAllocationErrorPhysicalAbi(XL6_IrRuntimeOp operation);
/* Allocating scalar/string/union erasure produces an owned xvalue. Language
 * null is the non-NULL XRT singleton; NULL always denotes boxing failure. */
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeBoxingErrorPhysicalAbi(XL6_IrRuntimeOp operation);
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeContainerAllocationErrorPhysicalAbi(XL6_IrRuntimeOp operation);
/* Publication commits an already constructed object's callback contract.
 * Failure consumes the unpublished input; it is not an infallible type cast. */
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeObjectPublicationErrorPhysicalAbi(XL6_IrRuntimeOp operation);
int xl6IrCallErrorPhysicalUsesForeignScope(XL6_IrCallErrorPhysicalAbi abi);
XL6_IrRuntimeOp xl6IrAllocatingBoxOpForType(const XL6_IrModule* module, XL6_IrTypeId type);
void xl6IrDumpModuleText(XL6_Output* out, const XL6_IrModule* module);
void xl6IrDumpModuleJson(XL6_Output* out, const XL6_IrModule* module);

#endif
