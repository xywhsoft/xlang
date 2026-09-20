#include "src/cgen/xl6_cgen.h"
#include "src/cgen/xl6_cgen_buffer.h"
#include "src/cgen/xl6_cgen_dir.h"
#include "src/cgen/xl6_cgen_process_run.h"
#include "src/cgen/xl6_cgen_entry.h"
#include "src/cgen/xl6_cgen_file.h"
#include "src/cgen/xl6_cgen_http1.h"
#include "src/cgen/xl6_cgen_json_value.h"
#include "src/cgen/xl6_cgen_json_stream.h"
#include "src/cgen/xl6_cgen_serialization_callback.h"
#include "src/cgen/xl6_cgen_xson.h"
#include "src/cgen/xl6_cgen_lifecycle.h"
#include "src/cgen/xl6_cgen_net.h"
#include "src/cgen/xl6_cgen_needs_basic.h"
#include "src/cgen/xl6_cgen_runtime.h"
#include "src/cgen/xl6_cgen_text_codec.h"
#include "src/cgen/xl6_cgen_tls.h"
#include "src/cgen/xl6_cgen_websocket.h"
#include "src/cgen/xl6_cgen_xlang.h"
#include "src/ir/xl6_hir.h"
#include "src/type/xl6_type_model.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/*
    Private implementation shards.
    Keep them in one translation unit so existing static boundaries stay stable.
*/
#include "src/cgen/xl6_cgen_core_context.c"
#include "src/cgen/xl6_cgen_core_names.c"
#include "src/cgen/xl6_cgen_hir.c"
#include "src/cgen/xl6_cgen_core_lookup.c"
#include "src/cgen/xl6_cgen_core_state.c"
#include "src/cgen/xl6_cgen_type_nullable.c"
static const XL6_AstNode* xl6CgenTypedContainerConcreteType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static const char* xl6CgenTypedContainerCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static unsigned long long xl6CgenTypeIdForTypeNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode);
#include "src/cgen/xl6_cgen_core_types.c"
#include "src/cgen/xl6_cgen_core_generics.c"
static XL6_AstNode* xl6CgenCloneImplicitSelfFieldType(XL6_CgenContext* ctx, const char* fieldName);
static XL6_AstNode* xl6CgenCloneExprStorageType(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static int xl6CgenConditionalIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenBinaryArithmeticIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node);
#include "src/cgen/xl6_cgen_core_runtime_state.c"
#include "src/cgen/xl6_cgen_builtin_object.c"
#include "src/cgen/xl6_cgen_type_infer_helpers.c"
#include "src/cgen/xl6_cgen_type_cache.c"
#include "src/cgen/xl6_cgen_type_infer_literals.c"
#include "src/cgen/xl6_cgen_type_infer.c"
#include "src/cgen/xl6_cgen_type_hir.c"
static const char* xl6CgenValueStorageCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static int xl6CgenEmitCAbiCallbackArgument(
    XL6_CgenContext* ctx,
    const XL6_AstNode* functionType,
    const XL6_AstNode* expr);
static void xl6CgenEmitCAbiCallbackMakeExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valueName);
static void xl6CgenCAbiCallbackTypeName(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode);
#include "src/cgen/xl6_cgen_type_generic.c"
static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenNamedValueReference(XL6_CgenContext* ctx, const char* name);
static void xl6CgenAddWeakCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode);
static int xl6CgenExprIsFunctionBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static int xl6CgenDeclTryStaticNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenUserProtocolCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* member);
static int xl6CgenUserProtocolPropertyCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member,
    const XL6_AstNode* indexNode,
    const XL6_AstNode* valueExpr,
    XL6_AstKind kind,
    const XL6_AstNode* semanticSource);
static void xl6CgenProtocolTypeDescSymbol(
    XL6_Output* out,
    const XL6_AstNode* protocolNode);
static void xl6CgenGenericProtocolTypeDescSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolType);
static void xl6CgenUserProtocolTypeProtos(XL6_CgenContext* ctx);
static void xl6CgenBlock(XL6_CgenContext* ctx, const XL6_AstNode* block);
static void xl6CgenAddManagedCleanupForType(
    XL6_CgenContext* ctx,
    const char* name,
    const XL6_AstNode* typeNode
);
static void xl6CgenTaggedEnumValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr
);
static void xl6CgenEnumOpsSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode,
    const char* suffix
);
static void xl6CgenEmitDynamicMethodXCallProtos(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode);
static void xl6CgenEmitDynamicMethodTableProto(
    XL6_CgenContext* ctx,
    const char* cTypeName);
static void xl6CgenEmitDynamicMethodTable(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const char* cTypeName);
static void xl6CgenDynamicMethodTableSymbol(
    XL6_CgenContext* ctx,
    const char* cTypeName);
static void xl6CgenEmitDynamicMethodDefinitions(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode);
static void xl6CgenEmitGenericDynamicMethodDefinitions(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode);
#include "src/cgen/xl6_cgen_nullable.c"
#include "src/cgen/xl6_cgen_expr_support_types.c"
#include "src/cgen/xl6_cgen_expr_support_base.c"
#include "src/cgen/xl6_cgen_extern_call.c"
#include "src/cgen/xl6_cgen_special_collection.c"
#include "src/cgen/xl6_cgen_special_collection_runtime.c"
#include "src/cgen/xl6_cgen_expr_cleanup.c"
#include "src/cgen/xl6_cgen_loop_target.c"
#include "src/cgen/xl6_cgen_finally_target.c"
static void xl6CgenDiscardPendingFinallyReturn(XL6_CgenContext* ctx, int targetId);
#include "src/cgen/xl6_cgen_function_value.c"
#include "src/cgen/xl6_cgen_function_member.c"
static int xl6CgenExprIsTypedContainerReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static void xl6CgenTypedContainerStorageToXValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* storageName);
static void xl6CgenTypedContainerExprToXValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* valueExpr,
    const char* format);
static int xl6CgenPersistenceStringifyName(const char* name, const char** outFormat);
static int xl6CgenPersistenceWriteName(const char* name, const char** outFormat);
static const char* xl6CgenCallableName(const XL6_AstNode* node);
static void xl6CgenFixedParamArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* param, const XL6_AstNode* expr);
static const XL6_AstNode* xl6CgenResolveClassCreateMode(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call, int reportNoMatch);
#include "src/cgen/xl6_cgen_args_box_typed.c"
#include "src/cgen/xl6_cgen_args.c"
#include "src/cgen/xl6_cgen_args_null.c"
#include "src/cgen/xl6_cgen_debug.c"
#include "src/cgen/xl6_cgen_fstring.c"
#include "src/cgen/xl6_cgen_args_containers.c"
#include "src/cgen/xl6_cgen_runtime_type.c"
#include "src/cgen/xl6_cgen_symbol.c"
#include "src/cgen/xl6_cgen_resolve_match.c"
#include "src/cgen/xl6_cgen_resolve_call.c"
#include "src/cgen/xl6_cgen_class_protocol_resolve.c"
#include "src/cgen/xl6_cgen_class_static.c"
#include "src/cgen/xl6_cgen_value_object_typed.c"
#include "src/cgen/xl6_cgen_value_object_containers.c"
#include "src/cgen/xl6_cgen_value_object.c"
#include "src/cgen/xl6_cgen_value_object_member.c"
#include "src/cgen/xl6_cgen_class_value_protocol.c"
#include "src/cgen/xl6_cgen_record_field.c"
#include "src/cgen/xl6_cgen_class_copy.c"
#include "src/cgen/xl6_cgen_class_persistence.c"
#include "src/cgen/xl6_cgen_class_derive_protocol.c"
#include "src/cgen/xl6_cgen_class_protocol.c"
#include "src/cgen/xl6_cgen_class_property.c"
static int xl6CgenClassPropertyCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* operation,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right);
#include "src/cgen/xl6_cgen_assign_target.c"
#include "src/cgen/xl6_cgen_assign_numeric.c"
#include "src/cgen/xl6_cgen_value_assign_index.c"
#include "src/cgen/xl6_cgen_value_assign_containers.c"
static void xl6CgenXValueWriteReceiverExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver);
#include "src/cgen/xl6_cgen_value_assign.c"
#include "src/cgen/xl6_cgen_value_index.c"
#include "src/cgen/xl6_cgen_enum.c"
#include "src/cgen/xl6_cgen_expr.c"
#include "src/cgen/xl6_cgen_stmt_decl_class_temp.c"
#include "src/cgen/xl6_cgen_stmt_decl.c"
#include "src/cgen/xl6_cgen_stmt_decl_array.c"
#include "src/cgen/xl6_cgen_stmt_decl_xvalue.c"
#include "src/cgen/xl6_cgen_stmt_decl_call_args.c"
#include "src/cgen/xl6_cgen_stmt_decl_call_stmt.c"
#include "src/cgen/xl6_cgen_stmt_decl_call_arg_scope.c"
#include "src/cgen/xl6_cgen_stmt_decl_main.c"
#include "src/cgen/xl6_cgen_stmt_multi.c"
#include "src/cgen/xl6_cgen_class_property_compound.c"
#include "src/cgen/xl6_cgen_stmt_branch.c"
#include "src/cgen/xl6_cgen_stmt_loop_while.c"
#include "src/cgen/xl6_cgen_stmt_loop_range.c"
static int xl6CgenIsClassProtocolIterableType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* iterableType);
#include "src/cgen/xl6_cgen_stmt_loop.c"
#include "src/cgen/xl6_cgen_stmt_loop_string.c"
#include "src/cgen/xl6_cgen_stmt_loop_protocol.c"
#include "src/cgen/xl6_cgen_stmt_loop_typed.c"
static void xl6CgenAsyncPendingResultCompletion(XL6_CgenContext* ctx, int targetId);
#include "src/cgen/xl6_cgen_stmt_error.c"
#include "src/cgen/xl6_cgen_stmt_effect.c"
#include "src/cgen/xl6_cgen_stmt_dispatch.c"
static void xl6CgenGenericClassFuncProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* funcNode);
#include "src/cgen/xl6_cgen_decl_proto.c"
#include "src/cgen/xl6_cgen_export_wrapper.c"
#include "src/cgen/xl6_cgen_class_virtual.c"
#include "src/cgen/xl6_cgen_decl_union.c"
#include "src/cgen/xl6_cgen_decl_class.c"
#include "src/cgen/xl6_cgen_decl_methods.c"
#include "src/cgen/xl6_cgen_async.c"
#include "src/cgen/xl6_cgen_async_api.c"
#include "src/cgen/xl6_cgen_async_func.c"
#include "src/cgen/xl6_cgen_generator.c"
#include "src/cgen/xl6_cgen_async_runtime_when_any.c"
#include "src/cgen/xl6_cgen_async_runtime.c"
#include "src/cgen/xl6_cgen_async_file.c"
#include "src/cgen/xl6_cgen_closure.c"
#include "src/cgen/xl6_cgen_abi.c"
#include "src/cgen/xl6_cgen_user_protocol.c"
#include "src/cgen/xl6_cgen_callable_runtime.c"
#include "src/cgen/xl6_cgen_callable_frame.c"
#include "src/cgen/xl6_cgen_c_callback.c"
#include "src/cgen/xl6_cgen_class_dynamic_method.c"
#include "src/cgen/xl6_cgen_callable_wrappers.c"
#include "src/cgen/xl6_cgen_func.c"
#include "src/cgen/xl6_cgen_needs.c"
#include "src/cgen/xl6_cgen_xvalue_runtime.c"
#include "src/cgen/xl6_cgen_crypto.c"
#include "src/cgen/xl6_cgen_xid.c"
#include "src/cgen/xl6_cgen_json.c"
#include "src/cgen/xl6_cgen_process_extended.c"
#include "src/cgen/xl6_cgen_process.c"
#include "src/cgen/xl6_cgen_string_edge.c"
#include "src/cgen/xl6_cgen_std_string.c"
#include "src/cgen/xl6_cgen_path.c"
#include "src/cgen/xl6_cgen_os.c"
#include "src/cgen/xl6_cgen_net_extended.c"
#include "src/cgen/xl6_cgen_encoding.c"
#include "src/cgen/xl6_cgen_log.c"
#include "src/cgen/xl6_cgen_signal.c"
#include "src/cgen/xl6_cgen_template.c"
#include "src/cgen/xl6_cgen_tcc.c"
#include "src/cgen/xl6_cgen_xlang.c"
#include "src/cgen/xl6_cgen_main_wrapper.c"
#include "src/cgen/xl6_cgen_main.c"
#include "src/cgen/xl6_cgen_module.c"
