#include "include/xl.h"

#define XL_RUNTIME_DECLARATIONS_ONLY
#include "include/xl_runtime.h"



typedef struct xl6_protocol_registration {
	uint32 ProtocolTypeId;
	uint32 ConcreteTypeId;
	const xl6_protocol_witness* Witness;
} xl6_protocol_registration;



static xonce xl6__protocol_once;
static xmutex* xl6__protocol_lock;
static xl6_protocol_registration* xl6__protocol_items;
static size_t xl6__protocol_count;
static size_t xl6__protocol_capacity;



/* 初始化进程级协议注册表；生成模块只借用见证表，不转移其所有权。 */
static bool xl6ProtocolRegistryInit(ptr context)
{
	(void)context;
	xl6__protocol_lock = xrtMutexCreate();
	return (xl6__protocol_lock != NULL);
}



/* 确保协议注册表完成一次性初始化。 */
static bool xl6ProtocolRegistryEnsure(void)
{
	return xrtOnce(&xl6__protocol_once, xl6ProtocolRegistryInit, NULL);
}



/* 比较注册项与协议、具体类型组成的稳定键。 */
static int xl6ProtocolKeyCompare(
	const xl6_protocol_registration* item,
	uint32 protocolTypeId,
	uint32 concreteTypeId)
{
	if ( item->ProtocolTypeId != protocolTypeId ) {
		return (item->ProtocolTypeId < protocolTypeId) ? -1 : 1;
	}
	if ( item->ConcreteTypeId != concreteTypeId ) {
		return (item->ConcreteTypeId < concreteTypeId) ? -1 : 1;
	}
	return 0;
}



/* 在已加锁的有序注册表中查找插入位置。 */
static size_t xl6ProtocolLowerBound(uint32 protocolTypeId, uint32 concreteTypeId)
{
	size_t first = 0;
	size_t count = xl6__protocol_count;

	while ( count != 0 ) {
		size_t step = (count / 2);
		size_t index = (first + step);

		if ( xl6ProtocolKeyCompare(
				&xl6__protocol_items[index],
				protocolTypeId,
				concreteTypeId) < 0 ) {
			first = (index + 1);
			count -= (step + 1);
		} else {
			count = step;
		}
	}
	return first;
}



/* 判断两个协议成员名称是否完全相同。 */
static bool xl6ProtocolNameEqual(
	const char* left,
	uint32 leftSize,
	const char* right,
	uint32 rightSize)
{
	return (leftSize == rightSize) &&
		(left != NULL) &&
		(right != NULL) &&
		(memcmp(left, right, leftSize) == 0);
}



/* 验证 xlang 协议见证表的描述、成员和签名是一一对应的。 */
XL_API bool xl6ProtocolWitnessValidate(const xl6_protocol_witness* witness)
{
	uint32 i;
	uint32 j;

	if ( (witness == NULL) ||
		(witness->Protocol == NULL) ||
		(witness->Protocol->Type == NULL) ||
		(witness->ConcreteType == NULL) ||
		(witness->Protocol->Type->TypeId == 0) ||
		(witness->ConcreteType->TypeId == 0) ||
		(witness->EntryCount != witness->Protocol->RequirementCount) ) {
		return false;
	}
	if ( witness->EntryCount == 0 ) {
		return true;
	}
	if ( (witness->Entries == NULL) || (witness->Protocol->Requirements == NULL) ) {
		return false;
	}

	for ( i = 0; i < witness->Protocol->RequirementCount; ++i ) {
		const xl6_protocol_requirement* requirement =
			&witness->Protocol->Requirements[i];
		uint32 matches = 0;

		if ( (requirement->Name == NULL) ||
			(requirement->NameSize == 0) ||
			(requirement->Signature == NULL) ||
			(requirement->Signature->TypeId == 0) ) {
			return false;
		}
		for ( j = 0; j < witness->EntryCount; ++j ) {
			const xl6_protocol_entry* entry = &witness->Entries[j];

			if ( (entry->Signature != NULL) &&
				(entry->Signature->TypeId == requirement->Signature->TypeId) &&
				xl6ProtocolNameEqual(
					entry->Name,
					entry->NameSize,
					requirement->Name,
					requirement->NameSize) ) {
				if ( entry->Entry == NULL ) {
					return false;
				}
				++matches;
			}
		}
		if ( matches != 1 ) {
			return false;
		}
	}
	for ( i = 0; i < witness->EntryCount; ++i ) {
		const xl6_protocol_entry* entry = &witness->Entries[i];
		uint32 matches = 0;

		if ( (entry->Name == NULL) ||
			(entry->NameSize == 0) ||
			(entry->Signature == NULL) ||
			(entry->Signature->TypeId == 0) ||
			(entry->Entry == NULL) ) {
			return false;
		}
		for ( j = 0; j < witness->Protocol->RequirementCount; ++j ) {
			const xl6_protocol_requirement* requirement =
				&witness->Protocol->Requirements[j];

			if ( (requirement->Signature != NULL) &&
				(requirement->Signature->TypeId == entry->Signature->TypeId) &&
				xl6ProtocolNameEqual(
					requirement->Name,
					requirement->NameSize,
					entry->Name,
					entry->NameSize) ) {
				++matches;
			}
		}
		if ( matches != 1 ) {
			return false;
		}
	}
	return true;
}



/* 按名称和签名 ID 查询协议见证入口。 */
XL_API const xl6_protocol_entry* xl6ProtocolWitnessFind(
	const xl6_protocol_witness* witness,
	const char* name,
	uint32 nameSize,
	uint64 signatureId)
{
	uint32 i;

	if ( (witness == NULL) ||
		(name == NULL) ||
		(nameSize == 0) ||
		(signatureId == 0) ) {
		return NULL;
	}
	for ( i = 0; i < witness->EntryCount; ++i ) {
		const xl6_protocol_entry* entry = &witness->Entries[i];

		if ( (entry->Signature != NULL) &&
			(entry->Signature->TypeId == signatureId) &&
			xl6ProtocolNameEqual(
				entry->Name,
				entry->NameSize,
				name,
				nameSize) ) {
			return entry;
		}
	}
	return NULL;
}



/* 注册一个协议与具体类型的唯一见证表。 */
XL_API bool xl6ProtocolRegister(const xl6_protocol_witness* witness)
{
	xl6_protocol_registration* items;
	size_t index;
	size_t capacity;
	uint32 protocolTypeId;
	uint32 concreteTypeId;
	bool result = false;

	if ( !xl6ProtocolWitnessValidate(witness) || !xl6ProtocolRegistryEnsure() ) {
		return false;
	}
	protocolTypeId = witness->Protocol->Type->TypeId;
	concreteTypeId = witness->ConcreteType->TypeId;
	if ( !xrtMutexLock(xl6__protocol_lock) ) {
		return false;
	}

	index = xl6ProtocolLowerBound(protocolTypeId, concreteTypeId);
	if ( (index < xl6__protocol_count) &&
		(xl6ProtocolKeyCompare(
			&xl6__protocol_items[index],
			protocolTypeId,
			concreteTypeId) == 0) ) {
		result = (xl6__protocol_items[index].Witness == witness);
		(void)xrtMutexUnlock(xl6__protocol_lock);
		return result;
	}

	if ( xl6__protocol_count == xl6__protocol_capacity ) {
		capacity = (xl6__protocol_capacity == 0) ?
			16 : (xl6__protocol_capacity + (xl6__protocol_capacity / 2));
		if ( (capacity <= xl6__protocol_capacity) ||
			(capacity > (SIZE_MAX / sizeof(*items))) ) {
			(void)xrtMutexUnlock(xl6__protocol_lock);
			return false;
		}
		items = (xl6_protocol_registration*)xrtRealloc(
			xl6__protocol_items,
			(capacity * sizeof(*items)));
		if ( items == NULL ) {
			(void)xrtMutexUnlock(xl6__protocol_lock);
			return false;
		}
		xl6__protocol_items = items;
		xl6__protocol_capacity = capacity;
	}

	if ( index < xl6__protocol_count ) {
		memmove(
			&xl6__protocol_items[index + 1],
			&xl6__protocol_items[index],
			((xl6__protocol_count - index) * sizeof(*xl6__protocol_items)));
	}
	xl6__protocol_items[index].ProtocolTypeId = protocolTypeId;
	xl6__protocol_items[index].ConcreteTypeId = concreteTypeId;
	xl6__protocol_items[index].Witness = witness;
	++xl6__protocol_count;
	result = true;
	(void)xrtMutexUnlock(xl6__protocol_lock);
	return result;
}



/* 注销指定见证表；键相同但地址不同的见证不会被误删。 */
XL_API bool xl6ProtocolUnregister(const xl6_protocol_witness* witness)
{
	size_t index;
	uint32 protocolTypeId;
	uint32 concreteTypeId;

	if ( (witness == NULL) ||
		(witness->Protocol == NULL) ||
		(witness->Protocol->Type == NULL) ||
		(witness->ConcreteType == NULL) ||
		!xl6ProtocolRegistryEnsure() ) {
		return false;
	}
	protocolTypeId = witness->Protocol->Type->TypeId;
	concreteTypeId = witness->ConcreteType->TypeId;
	if ( !xrtMutexLock(xl6__protocol_lock) ) {
		return false;
	}

	index = xl6ProtocolLowerBound(protocolTypeId, concreteTypeId);
	if ( (index >= xl6__protocol_count) ||
		(xl6ProtocolKeyCompare(
			&xl6__protocol_items[index],
			protocolTypeId,
			concreteTypeId) != 0) ||
		(xl6__protocol_items[index].Witness != witness) ) {
		(void)xrtMutexUnlock(xl6__protocol_lock);
		return false;
	}

	--xl6__protocol_count;
	memmove(
		&xl6__protocol_items[index],
		&xl6__protocol_items[index + 1],
		((xl6__protocol_count - index) * sizeof(*xl6__protocol_items)));
	(void)xrtMutexUnlock(xl6__protocol_lock);
	return true;
}



/* 查询协议与具体类型对应的借用见证表。 */
XL_API const xl6_protocol_witness* xl6ProtocolLookup(
	uint32 protocolTypeId,
	uint32 concreteTypeId)
{
	const xl6_protocol_witness* witness = NULL;
	size_t index;

	if ( (protocolTypeId == 0) ||
		(concreteTypeId == 0) ||
		!xl6ProtocolRegistryEnsure() ||
		!xrtMutexLock(xl6__protocol_lock) ) {
		return NULL;
	}
	index = xl6ProtocolLowerBound(protocolTypeId, concreteTypeId);
	if ( (index < xl6__protocol_count) &&
		(xl6ProtocolKeyCompare(
			&xl6__protocol_items[index],
			protocolTypeId,
			concreteTypeId) == 0) ) {
		witness = xl6__protocol_items[index].Witness;
	}
	(void)xrtMutexUnlock(xl6__protocol_lock);
	return witness;
}
