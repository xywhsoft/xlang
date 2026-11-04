#ifndef FOREIGN_HEADER_CPP_H
#define FOREIGN_HEADER_CPP_H
typedef enum native_enum { native_minus=-1, native_zero=0 } native_enum;
typedef struct EnumProof* EnumHandle;
typedef const native_enum* NativeEnumPointer;
#ifdef __cplusplus
extern "C" {
#endif
native_enum enum_proof(EnumHandle, const native_enum*);
#ifdef __cplusplus
}
#endif
#endif
