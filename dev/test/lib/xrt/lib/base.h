


// 申请内存
XXAPI ptr xrtMalloc(size_t iSize)
{
	ptr mem = xCore.malloc(iSize);
	if ( mem == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}
	return mem;
}



// 申请类内存
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	ptr mem = xCore.calloc(iNum, iSize);
	if ( mem == NULL ) {
		xrtSetError("class memory allocate failed.", FALSE);
	}
	return mem;
}



// 重新申请内存
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	ptr mem = xCore.realloc(pMem, iSize);
	if ( mem == NULL ) {
		xrtSetError("memory reallocate failed.", FALSE);
	}
	return mem;
}



// 释放内存（ 会先判断是否为 null ）
XXAPI void xrtFree(ptr pmem)
{
	if ( pmem && (pmem != xCore.sNull) ) { xCore.free(pmem); }
}



// 申请无需主动释放的临时内存 (线程安全)
XXAPI ptr xrtTempMemory(size_t iSize)
{
	xrtThreadLocal* tls = xrtGetTLS();
	if ( tls == NULL ) {
		// TLS 未初始化，回退到全局模式
		ptr pMem = xrtMalloc(iSize);
		if ( pMem == NULL ) {
			return NULL;
		}
		if ( xCore.TempMem[xCore.TempMemIdx] ) {
			xrtFree(xCore.TempMem[xCore.TempMemIdx]);
			xCore.TempMem[xCore.TempMemIdx] = NULL;
		}
		xCore.TempMem[xCore.TempMemIdx] = pMem;
		xCore.TempMemIdx++;
		if ( xCore.TempMemIdx > 31 ) {
			xCore.TempMemIdx = 0;
		}
		return pMem;
	}
	
	// 申请内存
	ptr pMem = xrtMalloc(iSize);
	if ( pMem == NULL ) {
		return NULL;
	}
	// 释放过期内存
	if ( tls->TempMem[tls->TempMemIdx] ) {
		xrtFree(tls->TempMem[tls->TempMemIdx]);
		tls->TempMem[tls->TempMemIdx] = NULL;
	}
	// 处理环形临时内存数据
	tls->TempMem[tls->TempMemIdx] = pMem;
	tls->TempMemIdx++;
	if ( tls->TempMemIdx > 31 ) {
		tls->TempMemIdx = 0;
	}
	// 返回内存指针
	return pMem;
}



// 释放所有临时内存 (线程安全)
XXAPI void xrtFreeTempMemory()
{
	xrtThreadLocal* tls = xrtGetTLS();
	if ( tls == NULL ) {
		// TLS 未初始化，回退到全局模式
		for ( int i = 0; i < 32; i++ ) {
			if ( xCore.TempMem[i] ) {
				xrtFree(xCore.TempMem[i]);
				xCore.TempMem[i] = NULL;
			}
		}
		xCore.TempMemIdx = 0;
		return;
	}
	
	for ( int i = 0; i < 32; i++ ) {
		if ( tls->TempMem[i] ) {
			xrtFree(tls->TempMem[i]);
			tls->TempMem[i] = NULL;
		}
	}
	tls->TempMemIdx = 0;
}



// 设置错误 (线程安全)
XXAPI void xrtSetError(str sError, bool bFree)
{
	// 回调通知 (全局)
	if ( xCore.OnError ) {
		xCore.OnError(sError);
	}
	
	xrtThreadLocal* tls = xrtGetTLS();
	if ( tls == NULL ) {
		// TLS 未初始化，回退到全局模式
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
		}
		xCore.LastError = sError;
		xCore.__pri_FreeError = bFree;
		return;
	}
	
	if ( tls->__pri_FreeError && tls->LastError && tls->LastError != xCore.sNull ) {
		xrtFree(tls->LastError);
	}
	tls->LastError = sError;
	tls->__pri_FreeError = bFree;
	
	// 同时更新全局版本 (兼容性)
	xCore.LastError = sError;
	xCore.__pri_FreeError = FALSE; // 全局版本不负责释放
}
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF16to8(sError, iSize);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF32to8(sError, iSize);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}



// 清除错误 (线程安全)
XXAPI void xrtClearError()
{
	xrtThreadLocal* tls = xrtGetTLS();
	if ( tls == NULL ) {
		// TLS 未初始化，回退到全局模式
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
		}
		xCore.LastError = xCore.sNull;
		xCore.__pri_FreeError = FALSE;
		return;
	}
	
	if ( tls->__pri_FreeError && tls->LastError && tls->LastError != xCore.sNull ) {
		xrtFree(tls->LastError);
	}
	tls->LastError = xCore.sNull;
	tls->__pri_FreeError = FALSE;
	
	// 同时更新全局版本 (兼容性)
	xCore.LastError = xCore.sNull;
}


