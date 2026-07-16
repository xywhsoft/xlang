#include "xrt.h"

/*
 * 保持上游源码不变，仅在这个编译单元把 tomlc17 的分配器接到 XRT。
 * 这样既能继续直接更新上游，也不会在每次解析时修改它的全局配置。
 */
#define realloc xrtRealloc
#define free xrtFree
#include "src/tomlc17.c"
#undef free
#undef realloc
