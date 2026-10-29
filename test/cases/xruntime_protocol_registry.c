#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime.h"

#include <stdint.h>
#include <stdio.h>



static void testEntry(void)
{
}



int main(void)
{
	xrttype protocolType = {
		.Kind = XRT_TYPE_PROTOCOL,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Writer"),
		.AbiName = XRT_STR_INIT("test.Writer"),
		.Size = sizeof(ptr),
		.Align = _Alignof(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = _Alignof(ptr)
	};
	xrttype concreteType = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ConsoleWriter"),
		.AbiName = XRT_STR_INIT("test.ConsoleWriter"),
		.Size = sizeof(ptr),
		.Align = _Alignof(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = _Alignof(ptr)
	};
	xrtfunctionsig signature = {
		.Name = XRT_STR_INIT("write")
	};
	xrtprotocolrequirement requirements[] = {
		{ XRT_STR_INIT("write"), &signature }
	};
	xrtprotocol protocol = {
		&protocolType,
		1u,
		requirements
	};
	xrtprotocolentry entries[] = {
		{
			XRT_STR_INIT("write"),
			&signature,
			(ptr)(uintptr_t)&testEntry
		}
	};
	xrtprotocolwitness witness = {
		&protocol,
		&concreteType,
		1u,
		entries
	};
	xrtprotocolregistry* registry;
	const xrtprotocolwitness* found;

	protocolType.Id = xrtTypeId(protocolType.AbiName);
	concreteType.Id = xrtTypeId(concreteType.AbiName);
	signature.Id = xrtFunctionSigId(&signature);

	if ( !xrtProtocolWitnessValidate(&witness) ) {
		return 1;
	}
	registry = xrtProtocolRegistryCreate();
	if (
		(registry == NULL) ||
		!xrtProtocolRegistryAdd(registry, &witness) ||
		(xrtProtocolRegistryCount(registry) != 1u)
	) {
		xrtProtocolRegistryDestroy(registry);
		return 2;
	}
	found = xrtProtocolRegistryFind(
		registry,
		protocolType.Id,
		concreteType.Id
	);
	if (
		(found != &witness) ||
		(xrtProtocolWitnessFind(
			found,
			XRT_STR_LITERAL("write"),
			signature.Id
		) != &entries[0])
	) {
		xrtProtocolRegistryDestroy(registry);
		return 3;
	}
	if (
		!xrtProtocolRegistryRemove(registry, &witness) ||
		(xrtProtocolRegistryCount(registry) != 0u)
	) {
		xrtProtocolRegistryDestroy(registry);
		return 4;
	}

	xrtProtocolRegistryDestroy(registry);
	puts("xruntime protocol registry ok");
	return 0;
}
