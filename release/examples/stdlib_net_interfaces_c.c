#define XRT_MODULE_NET_INTERFACE
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>



/* 输出与 xlang 范例一致的结果行，并在契约失败时终止。 */
static void report(const char* sName, bool bCondition)
{
	if ( !bCondition ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}
	printf("%s=ok\n", sName);
}



/* 验证至少一个接口的名称和地址族索引可以双向转换。 */
static bool interfaceRoundtrip(const xnetinterfacelist* pList)
{
	size_t i;

	for ( i = 0u; i < pList->Count; i++ ) {
		const xnetinterface* pInterface = &pList->Items[i];
		xnetfamily Family;
		uint32 iIndex;
		size_t iRequired;
		char* sName;
		bool bEqual;

		if ( pInterface->IPv6Index != 0u ) {
			Family = XNET_FAMILY_IPV6;
			iIndex = pInterface->IPv6Index;
		} else if ( pInterface->IPv4Index != 0u ) {
			Family = XNET_FAMILY_IPV4;
			iIndex = pInterface->IPv4Index;
		} else {
			continue;
		}

		iRequired = xrtNetInterfaceName(iIndex, Family, NULL, 0u);
		if ( iRequired == XRT_NPOS ) {
			continue;
		}
		sName = (char*)xrtMalloc(iRequired + 1u);
		if ( sName == NULL ) {
			return false;
		}
		bEqual = (xrtNetInterfaceName(
			iIndex, Family, sName, iRequired + 1u
		) == iRequired) &&
			(xrtNetInterfaceIndex(sName, Family) == iIndex);
		xrtFree(sName);
		if ( bEqual ) {
			return true;
		}
	}
	return false;
}



/* 展示接口快照、标志和名称索引转换的 C/XRT 对照路径。 */
int main(void)
{
	xnetinterfacelist List = {0};
	bool bFlagsValid;

	bFlagsValid = (XNET_INTERFACE_UP == 1) &&
		(XNET_INTERFACE_RUNNING == 2) &&
		(XNET_INTERFACE_LOOPBACK == 4) &&
		(XNET_INTERFACE_BROADCAST == 8) &&
		(XNET_INTERFACE_POINT_TO_POINT == 16) &&
		(XNET_INTERFACE_MULTICAST == 32);
	report("interface_flags", bFlagsValid);

	report("interface_list", xrtNetInterfaces(&List) && (List.Count != 0u));
	report("interface_roundtrip", interfaceRoundtrip(&List));
	xrtNetInterfacesFree(&List);
	return 0;
}
