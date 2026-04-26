


// 获取本机 IP ( 需使用 xrtFree 释放 )
str xrtGetLocalIP()
{
	str sRet = xCore.sNull;
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		#ifdef __TINYC__
			// TCC 不支持 getaddrinfo，使用 gethostbyname
			struct hostent* host = gethostbyname(sLocalName);
			if ( host ) {
				sRet = xrtFormat("%d.%d.%d.%d", (uint8)(host->h_addr_list[0][0]), (uint8)(host->h_addr_list[0][1]), (uint8)(host->h_addr_list[0][2]), (uint8)(host->h_addr_list[0][3]));
			}
		#else
			struct addrinfo hints, *res, *p;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;  // IPv4
			hints.ai_socktype = SOCK_STREAM;
			if ( getaddrinfo(sLocalName, NULL, &hints, &res) == 0 ) {
				for ( p = res; p != NULL; p = p->ai_next ) {
					if ( p->ai_family == AF_INET ) {
						struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
						uint8* addr = (uint8*)&ipv4->sin_addr;
						sRet = xrtFormat("%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
						break;
					}
				}
				freeaddrinfo(res);
			}
		#endif
	}
	return sRet;
}



// 获取本机 IP ( 返回 uint32 )
uint32 xrtGetLocalRawIP()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		#ifdef __TINYC__
			// TCC 不支持 getaddrinfo，使用 gethostbyname
			struct hostent* host = gethostbyname(sLocalName);
			if ( host ) {
				return ((uint8)(host->h_addr_list[0][0]) << 24) | ((uint8)(host->h_addr_list[0][1]) << 16) | ((uint8)(host->h_addr_list[0][2]) << 8) | (uint8)(host->h_addr_list[0][3]);
			}
		#else
			struct addrinfo hints, *res, *p;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;  // IPv4
			hints.ai_socktype = SOCK_STREAM;
			if ( getaddrinfo(sLocalName, NULL, &hints, &res) == 0 ) {
				for ( p = res; p != NULL; p = p->ai_next ) {
					if ( p->ai_family == AF_INET ) {
						struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
						uint8* addr = (uint8*)&ipv4->sin_addr;
						uint32 result = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];
						freeaddrinfo(res);
						return result;
					}
				}
				freeaddrinfo(res);
			}
		#endif
	}
	return 0;
}



// 获取本机 MAC 地址 ( 需使用 xrtFree 释放 )
str xrtGetLocalMAC()
{
	#if defined(_WIN32) || defined(_WIN64)
		// 分类 IP_ADAPTER_INFO 结构内存
		ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
		PIP_ADAPTER_INFO pAdapterInfo = xrtMalloc(sizeof(IP_ADAPTER_INFO));
		if ( pAdapterInfo == NULL ) {
			return xCore.sNull;
		}
		//空间不够，重新分配
		if ( GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW ) {
			xrtFree(pAdapterInfo);
			pAdapterInfo = xrtMalloc(ulOutBufLen);
			if ( pAdapterInfo == NULL ) {
				return xCore.sNull;
			}
		}
		// 获取第一个 MAC 地址
		str sRet = xCore.sNull;
		DWORD errValue = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
		if ( errValue == NO_ERROR ) {
			sRet = xrtHexEncode(pAdapterInfo->Address, 6);
		}
		if ( pAdapterInfo ) {
			xrtFree(pAdapterInfo);
		}
		return sRet;
	#else
		struct ifreq buf[16];
		struct ifconf ifc;
		int fd = socket(AF_INET, SOCK_DGRAM, 0);
		if ( fd < 0 ) {
			xrtSetError("socket error !", FALSE);
			return xCore.sNull;
		}
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = (caddr_t)buf;
		if ( ioctl(fd, SIOCGIFCONF, (char*)&ifc) ) {
			xrtSetError(xrtFormat("ioctl (SIOCGIFCONF) : %s [%s:%d]", strerror(errno), __FILE__, __LINE__), TRUE);
			close(fd);
			return xCore.sNull;
		}
		int interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
		if ( interfaceNum <= 0 ) {
			xrtSetError("Network device not found !", FALSE);
			close(fd);
			return xCore.sNull;
		}
		interfaceNum--;
		struct ifreq ifrcopy = buf[interfaceNum];
		if ( ioctl(fd, SIOCGIFFLAGS, &ifrcopy) ) {
			xrtSetError(xrtFormat("ioctl (SIOCGIFFLAGS) : %s [%s:%d]", strerror(errno), __FILE__, __LINE__), TRUE);
			close(fd);
			return xCore.sNull;
		}
		if ( ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum])) ) {
			xrtSetError(xrtFormat("ioctl (SIOCGIFHWADDR) : %s [%s:%d]", strerror(errno), __FILE__, __LINE__), TRUE);
			close(fd);
			return xCore.sNull;
		}
		str sRet = xrtHexEncode(buf[interfaceNum].ifr_hwaddr.sa_data, 6);
		close(fd);
		return sRet;
	#endif
}



// windows 获取网卡信息 - 代码备份 : https://www.cnblogs.com/qing123/p/13223027.html
/*
void test()
{
	// 分类 IP_ADAPTER_INFO 结构内存
	ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	PIP_ADAPTER_INFO pAdapterInfo = xrtMalloc(sizeof(IP_ADAPTER_INFO));
	if ( pAdapterInfo == NULL ) {
		return xCore.sNull;
	}
	//空间不够，重新分配
	if ( GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW ) {
		xrtFree(pAdapterInfo);
		pAdapterInfo = xrtMalloc(ulOutBufLen);
		if ( pAdapterInfo == NULL ) {
			return xCore.sNull;
		}
	}
	DWORD errValue = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if ( errValue == NO_ERROR ) {
		PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
		while ( pAdapter ) {
			printf("Adapter Name : %s \n", pAdapter->AdapterName);//名字
			printf("Adapter Desc : %s \n", pAdapter->Description);//描述
			printf("Adapter Mac : %s \n", xrtHexEncode(pAdapter->Address, 6));//MAC地址
			if ( pAdapter->Type == MIB_IF_TYPE_OTHER ) {
				printf("网卡类型：其他\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_ETHERNET ) {
				printf("网卡类型：以太网接口\n");
			} else if ( pAdapter->Type == IF_TYPE_ISO88025_TOKENRING ) {
				printf("网卡类型：ISO88025令牌环\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_PPP ) {
				printf("网卡类型：PPP接口\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_LOOPBACK ) {
				printf("网卡类型：软件回路接口\n");
			} else if ( pAdapter->Type == MIB_IF_TYPE_SLIP ) {
				printf("网卡类型：ATM网络接口\n");
			} else if ( pAdapter->Type == IF_TYPE_IEEE80211 ) {
				printf("网卡类型：无线网络接口\n");
			} else {
				printf("网卡类型：未知接口\n");
			}
			printf("IP地址：%s \n", pAdapter->IpAddressList.IpAddress.String);
			printf("子网掩码：%s \n", pAdapter->IpAddressList.IpMask.String);
			printf("默认网关：%s \n", pAdapter->GatewayList.IpAddress.String);
			printf("是否DHCP：%d \n", pAdapter->DhcpEnabled);
			printf("DHCP地址：%s \n", pAdapter->DhcpServer.IpAddress.String);
			pAdapter = pAdapter->Next;
		}
	} else {
		printf("GetAdaptersInfo failed with error: %d\n", errValue);
	}
	if ( pAdapterInfo ) {
		xrtFree(pAdapterInfo);
	}
}
*/



// 获取本机名称 ( 需使用 xrtFree 释放 )
str xrtGetLocalName()
{
	char sLocalName[260];
	if ( gethostname(sLocalName, 260) == 0 ) {
		return xrtCopyStr(sLocalName, 0);
	}
	return xCore.sNull;
}


