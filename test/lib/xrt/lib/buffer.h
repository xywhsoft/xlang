


// 创建内存缓冲区管理器
XXAPI xbuffer xrtBufferCreate(uint32 iStep)
{
	xbuffer pBuf = xrtMalloc(sizeof(xbuffer_struct));
	if ( pBuf ) {
		xrtBufferInit(pBuf, iStep);
	}
	return pBuf;
}

// 销毁内存缓冲区管理器
XXAPI void xrtBufferDestroy(xbuffer pBuf)
{
	if ( pBuf ) {
		xrtBufferUnit(pBuf);
		xrtFree(pBuf);
	}
}

// 初始化缓冲区管理器（对自维护结构体指针使用）
XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep)
{
	pBuf->Buffer = NULL;
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
	pBuf->AllocStep = iStep ? iStep : XBUFFER_ALLOC_STEP;
}

// 释放缓冲区管理器（对自维护结构体指针使用）
XXAPI void xrtBufferUnit(xbuffer pBuf)
{
	if ( pBuf->Buffer ) { xrtFree(pBuf->Buffer); pBuf->Buffer = NULL; }
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
}

// 分配内存
XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount)
{
	if ( iCount > pBuf->AllocLength ) {
		// 增量
		ptr pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			return TRUE;
		}
	} else if ( iCount < pBuf->AllocLength ) {
		// 裁剪
		ptr pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			if ( iCount <= pBuf->Length ) {
				// 需要裁剪数据
				pBuf->Length = iCount;
			}
			return TRUE;
		}
	} else if ( iCount = 0 ) {
		// 清空
		xrtBufferUnit(pBuf);
		return TRUE;
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode)
{
	// 长度为 0 时自动计算数据长度
	if ( iSize == 0 ) {
		if ( bStrMode == XBUF_ANSI ) {
			iSize = strlen(pData);
		} else if ( bStrMode == XBUF_UTF16 ) {
			iSize = u16len(pData) * XBUF_UTF16;
		} else if ( bStrMode == XBUF_UTF32 ) {
			iSize = u32len(pData) * XBUF_UTF32;
		} else {
			return FALSE;
		}
	}
	// 分配内存
	if ( (iPos + iSize + bStrMode) > pBuf->AllocLength ) {
		if ( xrtBufferMalloc(pBuf, iPos + iSize + bStrMode + pBuf->AllocStep) == 0 ) {
			return FALSE;
		}
	}
	// 复制数据
	if ( iSize ) {
		memcpy(&pBuf->Buffer[iPos], pData, iSize);
		pBuf->Length = iPos + iSize;
	}
	// 字符串模式自动添加 \0
	if ( bStrMode ) {
		for ( int i = 0; i < bStrMode; i++ ) {
			pBuf->Buffer[pBuf->Length + i] = 0;
		}
	}
	return TRUE;
}

// 末尾添加数据
XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode)
{
	return xrtBufferInsert(pBuf, pBuf->Length, pData, iSize, bStrMode);
}


