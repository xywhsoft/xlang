


// 创建结构体静态栈
XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength)
{
	xstack objSTK = xrtMalloc(sizeof(xstack_struct) + (iMaxCount * iItemLength));
	if ( objSTK ) {
		objSTK->Memory = (void*)&objSTK[1];
		objSTK->ItemLength = iItemLength;
		objSTK->MaxCount = iMaxCount;
		objSTK->Count = 0;
	}
	return objSTK;
}

// 压栈
XXAPI ptr xrtStackPush(xstack objSTK)
{
	if ( objSTK->Count < objSTK->MaxCount ) {
		objSTK->Count++;
		return &objSTK->Memory[(objSTK->Count - 1) * objSTK->ItemLength];
	}
	return NULL;
}
XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData)
{
	if ( objSTK->Count < objSTK->MaxCount ) {
		memcpy(&objSTK->Memory[objSTK->Count * objSTK->ItemLength], pData, objSTK->ItemLength);
		objSTK->Count++;
		return objSTK->Count;
	}
	return 0;
}
XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal)
{
	if ( objSTK->Count < objSTK->MaxCount ) {
		if ( objSTK->ItemLength == sizeof(ptr) ) {
			objSTK->PtrMem[objSTK->Count] = pVal;
		} else {
			ptr* pValPtr = (ptr*)&objSTK->Memory[objSTK->Count * objSTK->ItemLength];
			pValPtr[0] = pVal;
		}
		objSTK->Count++;
		return objSTK->Count;
	}
	return 0;
}

// 出栈
XXAPI ptr xrtStackPop(xstack objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	objSTK->Count--;
	return &objSTK->Memory[objSTK->Count * objSTK->ItemLength];
}
XXAPI ptr xrtStackPopPtr(xstack objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	objSTK->Count--;
	if ( objSTK->ItemLength == sizeof(ptr) ) {
		return objSTK->PtrMem[objSTK->Count];
	} else {
		ptr* pVal = (ptr*)&objSTK->Memory[objSTK->Count * objSTK->ItemLength];
		return pVal[0];
	}
}

// 获取栈顶对象
XXAPI ptr xrtStackTop(xstack objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	return &objSTK->Memory[(objSTK->Count - 1) * objSTK->ItemLength];
}
XXAPI ptr xrtStackTopPtr(xstack objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	if ( objSTK->ItemLength == sizeof(ptr) ) {
		return objSTK->PtrMem[objSTK->Count - 1];
	} else {
		ptr* pVal = (ptr*)&objSTK->Memory[(objSTK->Count - 1) * objSTK->ItemLength];
		return pVal[0];
	}
}

// 获取任意位置对象
XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos)
{
	if ( iPos > 0 ) {
		iPos--;
		if ( iPos < objSTK->Count ) {
			return &objSTK->Memory[iPos * objSTK->ItemLength];
		}
	}
	return NULL;
}
XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos)
{
	return &objSTK->Memory[(iPos - 1) * objSTK->ItemLength];
}
XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos)
{
	if ( iPos > 0 ) {
		iPos--;
		if ( iPos < objSTK->Count ) {
			if ( objSTK->ItemLength == sizeof(ptr) ) {
				return objSTK->PtrMem[iPos];
			} else {
				ptr* pVal = (ptr*)&objSTK->Memory[iPos * objSTK->ItemLength];
				return pVal[0];
			}
		}
	}
	return NULL;
}
XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint32 iPos)
{
	if ( objSTK->ItemLength == sizeof(ptr) ) {
		return objSTK->PtrMem[iPos - 1];
	} else {
		ptr* pVal = (ptr*)&objSTK->Memory[(iPos - 1) * objSTK->ItemLength];
		return pVal[0];
	}
}


