#ifndef XL6_XRT_TRIM_H
#define XL6_XRT_TRIM_H

#include "src/compile/xl6_imports.h"

/* 追加 C 源没有模块合同，不解析 C 猜测依赖；与未声明模块的包同样使用完整运行库。 */
int xl6XrtTrimRequiresFull(const XL6_ImportDeps* deps, size_t additionalCSourceCount);

/* 从产物依赖、包合同和追加 C 源数量生成实现，声明可见性也复用上面的判定。 */
int xl6XrtTrimBuildImplementationSourceFromArtifact(
    const XL6_ImportDeps* deps,
    size_t additionalCSourceCount,
    const char* const* modules,
    size_t moduleCount,
    char** outText);

#endif
