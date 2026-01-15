


// XID 转 字符串 ( 需要使用 xrtFree 释放内存 )
XXAPI str xrtEncodeXID(xid pXID)
{
	return xrtBase64Encode(pXID, 24, RandStringDefaultTemplate);
}



// 字符串 转 XID ( 需要使用 xrtFree 释放内存 )
XXAPI xid xrtDecodeXID(str sXID)
{
	return xrtBase64Decode(sXID, 32, RandStringDefaultTemplate);
}



// 获取 XID ( 需要使用 xrtFree 释放内存 )
XXAPI xid xrtMakeXID()
{
	xid pXID = xrtMalloc(24);
	if ( pXID == NULL ) {
		return NULL;
	}
	// 获取高精度时钟
	uint32 iTick;
	#if defined(_WIN32) || defined(_WIN64)
		if ( xCore.Frequency == 0.0 ) {
			iTick = GetTickCount();
		} else {
			LARGE_INTEGER QPC;
			QueryPerformanceCounter(&QPC);
			iTick = QPC.LowPart;
		}
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		iTick = ((timer.tv_sec & 3) << 30) | timer.tv_nsec;
	#endif
	// 生成 XID 数据
	pXID->Tick = iTick;
	pXID->Time = xrtNow();
	pXID->Addr = xCore.LocalAddr;
	pXID->Rand = xrtRand64();
	return pXID;
}



// 获取 XID 字符串 ( 需要使用 xrtFree 释放内存 )
XXAPI str xrtMakeXIDS()
{
	xid pXID = xrtMakeXID();
	if ( pXID == NULL ) { return xCore.sNull; }
	str sRet = xrtEncodeXID(pXID);
	xrtFree(pXID);
	return sRet;
}



// 比较两个 XID 是否相同
XXAPI bool xrtCompXID(xid pXID1, xid pXID2)
{
	if ( (pXID1->Time == pXID2->Time) && (pXID1->Tick == pXID2->Tick) && (pXID1->Addr == pXID2->Addr) && (pXID1->Rand == pXID2->Rand) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}


