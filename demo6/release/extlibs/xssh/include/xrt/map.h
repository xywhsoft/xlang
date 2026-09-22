#ifndef XRT_MAP_H
#define XRT_MAP_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_MAP) && !defined(XRT_FEATURE_HASH64)
	#error "XRT_FEATURE_MAP requires XRT_FEATURE_HASH64"
#endif

#if defined(XRT_FEATURE_INT_MAP) && !defined(XRT_FEATURE_AVL_TREE)
	#error "XRT_FEATURE_INT_MAP requires XRT_FEATURE_AVL_TREE"
#endif



#define XRT_MAP_ALIGNMENT_DEFAULT 16u
#define XRT_MAP_BUCKETS_MIN 16u
#define XRT_INT_MAP_ALIGNMENT_DEFAULT 16u



#if defined(XRT_FEATURE_MAP)

typedef struct xmapentry xmapentry;



/* 键哈希器必须保证相等键产生相同哈希值，且不得重入当前映射。 */
typedef uint64 (*xmaphash)(xbytesview Key, ptr pUserData);



/* 键相等器只比较键内容，不取得所有权，也不得重入当前映射。 */
typedef bool (*xmapequal)(xbytesview Left, xbytesview Right, ptr pUserData);



/* 映射释放器处理值内部的拥有资源，不释放值槽、键或重入当前映射。 */
typedef void (*xmapdrop)(xbytesview Key, ptr pValue, ptr pUserData);



/* 新值初始化器失败时自行清理部分状态并设置错误。 */
typedef bool (*xmapinit)(xbytesview Key, ptr pValue, ptr pUserData);



/* 访问器返回 false 时停止遍历；回调内允许查询，不允许结构修改。 */
typedef bool (*xmapvisitor)(xbytesview Key, ptr pValue, ptr pUserData);



/* 字节键映射使用哈希桶查找，并以独立条目保持键和值地址稳定。 */
typedef struct xmap {
	xmapentry** Buckets;
	xmapentry* First;
	xmapentry* Last;
	size_t ValueSize;
	size_t ValueOffset;
	size_t KeyOffset;
	size_t Alignment;
	size_t Count;
	size_t BucketCount;
	size_t Threshold;
	uint64 Version;
	xmaphash Hash;
	xmapequal Equal;
	xmapdrop Drop;
	ptr KeyUserData;
	ptr DropUserData;
	uint32 Flags;
} xmap;



/* 外置迭代器允许同一映射存在多个独立遍历状态。 */
typedef struct xmapiter {
	xmap* Map;
	xmapentry* Next;
	uint64 Version;
	int Direction;
} xmapiter;



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节值对齐初始化空字节键映射。 */
XRT_API bool xrtMapInit(xmap* pMap, size_t iValueSize);



/* 使用显式值对齐初始化空字节键映射。 */
XRT_API bool xrtMapInitAligned(
	xmap* pMap,
	size_t iValueSize,
	size_t iAlignment
);



/* 创建使用默认 16 字节值对齐的空字节键映射。 */
XRT_API xmap* xrtMapCreate(size_t iValueSize);



/* 创建使用显式值对齐的空字节键映射。 */
XRT_API xmap* xrtMapCreateAligned(size_t iValueSize, size_t iAlignment);



/* 为仍为空的映射设置成对的自定义哈希器和相等器，两个空指针恢复默认策略。 */
XRT_API bool xrtMapSetKeyPolicy(
	xmap* pMap,
	xmaphash pHash,
	xmapequal pEqual,
	ptr pUserData
);



/* 为仍为空的映射设置值资源释放器和独立用户数据。 */
XRT_API bool xrtMapSetDrop(xmap* pMap, xmapdrop pDrop, ptr pUserData);



/* 释放全部键值和桶数组，但不释放映射结构。 */
XRT_API void xrtMapUnit(xmap* pMap);



/* 释放全部键值、桶数组和映射结构。 */
XRT_API void xrtMapDestroy(xmap* pMap);



/* 清空全部键值并保留桶数组供后续复用。 */
XRT_API void xrtMapClear(xmap* pMap);



/* 确保映射无需扩容即可容纳指定数量的键。 */
XRT_API bool xrtMapReserve(xmap* pMap, size_t iCapacity);



/* 把桶数组收缩到当前键数所需的最小容量。 */
XRT_API bool xrtMapTrim(xmap* pMap);



/* 返回当前键值数量，非法映射返回零。 */
XRT_API size_t xrtMapCount(const xmap* pMap);



/* 返回当前桶数组在再次扩容前可容纳的键数。 */
XRT_API size_t xrtMapCapacity(const xmap* pMap);



/* 返回已有值槽，或复制键并原地创建、清零一个新值槽。 */
XRT_API ptr xrtMapGetOrAdd(xmap* pMap, xbytesview Key, bool* pNew);



/* 返回已有值槽，或失败原子地复制键并原位初始化新值。 */
XRT_API ptr xrtMapGetOrInit(
	xmap* pMap,
	xbytesview Key,
	xmapinit pInit,
	ptr pUserData,
	bool* pNew
);



/* 复制插入或替换值；来源不得触及映射元数据或目标条目。 */
XRT_API bool xrtMapSet(xmap* pMap, xbytesview Key, const void* pValue);



/* 返回指定键的可写值槽，未找到是正常结果。 */
XRT_API ptr xrtMapGet(xmap* pMap, xbytesview Key);



/* 返回指定键的只读值槽，未找到是正常结果。 */
XRT_API const void* xrtMapConstGet(const xmap* pMap, xbytesview Key);



/* 判断指定字节键是否存在。 */
XRT_API bool xrtMapHas(const xmap* pMap, xbytesview Key);



/* 返回与查询键等价的内部键副本，缺失时清空输出。 */
XRT_API bool xrtMapStoredKey(
	const xmap* pMap,
	xbytesview Key,
	xbytesview* pStoredKey
);



/* 删除指定键并调用值释放器。 */
XRT_API bool xrtMapRemove(xmap* pMap, xbytesview Key);



/* 将值移交给映射外缓冲后删除；输出不得触及映射拥有的内存。 */
XRT_API bool xrtMapTake(xmap* pMap, xbytesview Key, ptr pValue);



/* 对 sizeof(ptr) 值映射执行指针类型友好的插入或替换。 */
XRT_API bool xrtMapSetPtr(xmap* pMap, xbytesview Key, ptr pValue);



/* 返回 sizeof(ptr) 值映射中保存的指针，空值与缺失键用 Has 区分。 */
XRT_API ptr xrtMapGetPtr(xmap* pMap, xbytesview Key);



/* 从 sizeof(ptr) 值映射中移交指针，不调用值释放器。 */
XRT_API bool xrtMapTakePtr(xmap* pMap, xbytesview Key, ptr* pValue);



/* 按插入顺序访问键值，并返回实际访问数量。 */
XRT_API size_t xrtMapVisit(xmap* pMap, xmapvisitor pVisitor, ptr pUserData);



/* 启动按插入顺序的外置迭代器。 */
XRT_API bool xrtMapIterBegin(xmap* pMap, xmapiter* pIterator);



/* 启动按插入顺序逆序遍历的外置迭代器。 */
XRT_API bool xrtMapIterRBegin(xmap* pMap, xmapiter* pIterator);



/* 返回下一值槽并可选返回内部键视图，结构修改后报告状态错误。 */
XRT_API ptr xrtMapIterNext(xmapiter* pIterator, xbytesview* pKey);



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtMapIterEnd(xmapiter* pIterator);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_INT_MAP)

#include <xrt/avl.h>



/* 整数映射释放器处理值内部资源，期间不得调用同一映射的 API。 */
typedef void (*xintmapdrop)(int64 iKey, ptr pValue, ptr pUserData);



/* 新值初始化器失败时自行清理部分状态并设置错误。 */
typedef bool (*xintmapinit)(int64 iKey, ptr pValue, ptr pUserData);



/* 访问器可查询映射和修改值，不得修改结构或生命周期。 */
typedef bool (*xintmapvisitor)(int64 iKey, ptr pValue, ptr pUserData);



/* 整数映射把 int64 键和固定大小值槽内联保存在拥有式 AVL 树中。 */
typedef struct xintmap {
	xavltree Tree;
	size_t ValueSize;
	size_t ValueOffset;
	size_t Alignment;
	xintmapdrop Drop;
	ptr UserData;
	uint32 Flags;
} xintmap;



/* 外置迭代器可以同时遍历同一映射，结构修改后失效。 */
typedef struct xintmapiter {
	xintmap* Map;
	xavltreeiter Base;
} xintmapiter;



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节值对齐初始化空整数映射。 */
XRT_API bool xrtIntMapInit(xintmap* pMap, size_t iValueSize);



/* 使用显式值对齐初始化空整数映射。 */
XRT_API bool xrtIntMapInitAligned(
	xintmap* pMap,
	size_t iValueSize,
	size_t iAlignment
);



/* 创建使用默认 16 字节值对齐的空整数映射。 */
XRT_API xintmap* xrtIntMapCreate(size_t iValueSize);



/* 创建使用显式值对齐的空整数映射。 */
XRT_API xintmap* xrtIntMapCreateAligned(size_t iValueSize, size_t iAlignment);



/* 为仍为空的映射设置值资源释放器和用户数据。 */
XRT_API bool xrtIntMapSetDrop(
	xintmap* pMap,
	xintmapdrop pDrop,
	ptr pUserData
);



/* 释放全部值和池页，但不释放映射结构。 */
XRT_API void xrtIntMapUnit(xintmap* pMap);



/* 释放全部值、池页和映射结构。 */
XRT_API void xrtIntMapDestroy(xintmap* pMap);



/* 清空全部值并保留固定池的复用能力。 */
XRT_API void xrtIntMapClear(xintmap* pMap);



/* 释放空闲池页，并返回实际释放的页数。 */
XRT_API size_t xrtIntMapTrim(xintmap* pMap, size_t iRetainEmpty);



/* 返回当前键值数量，非法映射返回零。 */
XRT_API size_t xrtIntMapCount(const xintmap* pMap);



/* 返回已有值槽，或原地创建并清零一个新值槽。 */
XRT_API ptr xrtIntMapGetOrAdd(xintmap* pMap, int64 iKey, bool* pNew);



/* 返回已有值槽，或失败原子地原位初始化一个新值。 */
XRT_API ptr xrtIntMapGetOrInit(
	xintmap* pMap,
	int64 iKey,
	xintmapinit pInit,
	ptr pUserData,
	bool* pNew
);



/* 复制插入或替换值；替换时先调用旧值释放器。 */
XRT_API bool xrtIntMapSet(xintmap* pMap, int64 iKey, const void* pValue);



/* 返回指定键的可写值槽，未找到是正常结果。 */
XRT_API ptr xrtIntMapGet(xintmap* pMap, int64 iKey);



/* 返回指定键的只读值槽，未找到是正常结果。 */
XRT_API const void* xrtIntMapConstGet(const xintmap* pMap, int64 iKey);



/* 判断指定整数键是否存在。 */
XRT_API bool xrtIntMapHas(const xintmap* pMap, int64 iKey);



/* 删除指定键并调用值释放器。 */
XRT_API bool xrtIntMapRemove(xintmap* pMap, int64 iKey);



/* 将指定键的值字节移交给调用方后删除，不调用值释放器。 */
XRT_API bool xrtIntMapTake(xintmap* pMap, int64 iKey, ptr pValue);



/* 对 sizeof(ptr) 值映射执行指针类型友好的插入或替换。 */
XRT_API bool xrtIntMapSetPtr(xintmap* pMap, int64 iKey, ptr pValue);



/* 返回 sizeof(ptr) 值映射中保存的指针，空指针值与缺失键用 Has 区分。 */
XRT_API ptr xrtIntMapGetPtr(xintmap* pMap, int64 iKey);



/* 从 sizeof(ptr) 值映射中移交指针，不调用值释放器。 */
XRT_API bool xrtIntMapTakePtr(xintmap* pMap, int64 iKey, ptr* pValue);



/* 返回顺序第一项的值槽，并可选返回键。 */
XRT_API ptr xrtIntMapFirst(xintmap* pMap, int64* pKey);



/* 返回顺序最后一项的值槽，并可选返回键。 */
XRT_API ptr xrtIntMapLast(xintmap* pMap, int64* pKey);



/* 返回第一个不小于指定键的值槽和实际键。 */
XRT_API ptr xrtIntMapLowerBound(xintmap* pMap, int64 iKey, int64* pActualKey);



/* 返回第一个严格大于指定键的值槽和实际键。 */
XRT_API ptr xrtIntMapUpperBound(xintmap* pMap, int64 iKey, int64* pActualKey);



/* 按键升序访问值；回调期间查询可用，结构和生命周期修改被拒绝。 */
XRT_API size_t xrtIntMapVisit(
	xintmap* pMap,
	xintmapvisitor pVisitor,
	ptr pUserData
);



/* 启动按键升序的外置迭代器。 */
XRT_API bool xrtIntMapIterBegin(xintmap* pMap, xintmapiter* pIterator);



/* 启动按键降序的外置迭代器。 */
XRT_API bool xrtIntMapIterRBegin(xintmap* pMap, xintmapiter* pIterator);



/* 从第一个不小于指定键的条目开始升序迭代。 */
XRT_API bool xrtIntMapIterFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
);



/* 从第一个不大于指定键的条目开始降序迭代。 */
XRT_API bool xrtIntMapIterRFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
);



/* 返回下一值槽和键；正常结束或结构已修改时返回空指针。 */
XRT_API ptr xrtIntMapIterNext(xintmapiter* pIterator, int64* pKey);



/* 提前结束迭代并清除它持有的借用状态。 */
XRT_API void xrtIntMapIterEnd(xintmapiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif
