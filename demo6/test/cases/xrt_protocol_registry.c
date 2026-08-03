#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"

#include <stdio.h>


static void testEntry(void)
{
}


int main(void)
{
	static const xrt_type_desc protocolType = {
		0x700001u, XRT_TYPE_KIND_RECORD,
		"Writer", 6u, "test.Writer", 11u,
		sizeof(xvalue), sizeof(void*), NULL, NULL, NULL, NULL
	};
	static const xrt_type_desc concreteType = {
		0x700002u, XRT_TYPE_KIND_CLASS,
		"ConsoleWriter", 13u, "test.ConsoleWriter", 18u,
		sizeof(xvalue), sizeof(void*), NULL, NULL, NULL, NULL
	};
	static const xrt_func_sig signature = {
		0u, "write", 5u, 0u, NULL, 0u, NULL, 0u
	};
	static const xrtprotocolrequirement requirements[] = {
		{ "write", 5u, &signature }
	};
	static const xrtprotocol protocol = {
		&protocolType, 1u, requirements
	};
	static const xrtprotocolentry entries[] = {
		{ "write", 5u, &signature, (ptr)testEntry }
	};
	static const xrtprotocolwitness witness = {
		&protocol, &concreteType, 1u, entries
	};
	xrtprotocolregistry* registry;
	const xrtprotocolwitness* found;

	if ( !xrtInit() ) {
		return 1;
	}
	registry = xrtProtocolRegistryCreate();
	if (
		registry == NULL ||
		!xrtProtocolRegistryAdd(registry, &witness) ||
		xrtProtocolRegistryCount(registry) != 1u
	) {
		xrtProtocolRegistryDestroy(registry);
		xrtUnit();
		return 2;
	}
	found = xrtProtocolRegistryFind(
		registry,
		protocolType.TypeId,
		concreteType.TypeId
	);
	if (
		found != &witness ||
		xrtProtocolWitnessFind(found, "write", 5u, 0u) != &entries[0]
	) {
		xrtProtocolRegistryDestroy(registry);
		xrtUnit();
		return 3;
	}
	if (
		!xrtProtocolRegistryRemove(registry, &witness) ||
		xrtProtocolRegistryCount(registry) != 0u
	) {
		xrtProtocolRegistryDestroy(registry);
		xrtUnit();
		return 4;
	}
	xrtProtocolRegistryDestroy(registry);
	if (
		xrtProtocolDefaultRegistry() == NULL ||
		!xrtProtocolRegister(&witness) ||
		xrtProtocolLookup(protocolType.TypeId, concreteType.TypeId) != &witness ||
		!xrtProtocolUnregister(&witness) ||
		xrtProtocolLookup(protocolType.TypeId, concreteType.TypeId) != NULL
	) {
		xrtUnit();
		return 5;
	}
	xrtUnit();
	puts("xrt protocol registry ok");
	return 0;
}
