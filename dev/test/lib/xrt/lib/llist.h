


// 创建链表
XXAPI xllist xrtLListCreate(uint32 iItemLength)
{
	xllist objLL = xrtMalloc(sizeof(xllist_struct));
	if ( objLL ) {
		xrtLListInit(objLL, iItemLength);
	}
	return objLL;
}

// 销毁链表
XXAPI void xrtLListDestroy(xllist objLL)
{
	if ( objLL ) {
		xrtLListUnit(objLL);
		xrtFree(objLL);
	}
}

// 初始化链表（对自维护结构体指针使用）
XXAPI void xrtLListInit(xllist objLL, uint32 iItemLength)
{
	xrtLLB_Init(objLL);
	xrtFSMemPoolInit(&objLL->objMM, iItemLength);
}

// 释放链表（对自维护结构体指针使用）
XXAPI void xrtLListUnit(xllist objLL)
{
	xrtLLB_Unit(objLL);
	xrtFSMemPoolUnit(&objLL->objMM);
}

// 节点前插入 (objNode为空则插入到FirstNode之前)
XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode)
{
	xllistnode objNewNode = xrtFSMemPoolAlloc(&objLL->objMM);
	if ( objNewNode ) {
		xrtLLB_InsertPrev((xllistbase)objLL, objNode, objNewNode);
	}
	return objNewNode;
}

// 节点后插入 (objNode为空则插入到LastNode之后)
XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode)
{
	xllistnode objNewNode = xrtFSMemPoolAlloc(&objLL->objMM);
	if ( objNewNode ) {
		xrtLLB_InsertNext((xllistbase)objLL, objNode, objNewNode);
	}
	return objNewNode;
}

// 删除节点
XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode)
{
	if ( objNode ) {
		xrtLLB_Remove((xllistbase)objLL, objNode);
		xrtFSMemPoolFree(&objLL->objMM, objNode);
	}
}


