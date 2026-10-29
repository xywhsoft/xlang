#ifndef XL6_NATIVE_CLASS_RESULT_H
#define XL6_NATIVE_CLASS_RESULT_H
#include "xl6_native_class_abi.h"
#include "xl6_code_lease.h"
/* Synchronous construction only: never retain this unfinished object across
 * suspension or publish it to an ownership graph. An async frame retains its
 * real CodeLease and immutable recipe instead, and prepares ONLY on success.
 * The unpublished object has its real class/type identity and CodeLease but
 * NO native field. Only a successful native result may complete construction.
 * The caller owns the returned shell and must never expose it to user code.
 * A copied context needs its name/schema bytes kept alive independently. */
XL6_CODE_LEASE_API struct xvalue* xl6NativeClassResultPrepareV1(XL6_NativeClassResultContextV1);
XL6_CODE_LEASE_API bool xl6NativeClassResultContextValidV1(XL6_NativeClassResultContextV1);
XL6_CODE_LEASE_API struct xvalue* xl6NativeClassResultPrepareFromLeaseV1(
    XL6_NativeClassResultContextV1,XL6_CodeLease*);
/* For a valid sealed recipe, consumes the independent prepared-shell and raw
 * native owners on BOTH outcomes. Invalid recipe/slot arguments leave them
 * unchanged (unknown policy callbacks cannot be trusted for rollback).
 * No enclosing mutation may span arbitrary native cleanup. */
XL6_CODE_LEASE_API struct xvalue* xl6NativeClassResultCommitTakeV1(
    XL6_NativeClassResultContextV1,struct xvalue**,void**);
#endif
