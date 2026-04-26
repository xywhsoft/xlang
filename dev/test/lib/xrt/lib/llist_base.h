


// 节点前插入 (objNode为空则插入到FirstNode之前)
XXAPI void xrtLLB_InsertPrev(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode)
{
	if ( objNode ) {
		// 有参考节点（插入到参考节点前面）
		if ( objNode->Prev ) {
			objNode->Prev->Next = objNewNode;
		}
		objNewNode->Prev = objNode->Prev;
		objNewNode->Next = objNode;
		objNode->Prev = objNewNode;
	} else {
		// 无参考节点（插入到链表最前）
		objNewNode->Prev = NULL;
		objNewNode->Next = objLLB->FirstNode;
		if ( objLLB->FirstNode ) {
			// 有首节点
			objLLB->FirstNode->Prev = objNewNode;
		} else {
			// 没有首节点，代表这是一个空链表，所以尾节点也要修改
			objLLB->LastNode = objNewNode;
		}
		objLLB->FirstNode = objNewNode;
	}
	objLLB->Count++;
}

// 节点后插入 (objNode为空则插入到LastNode之后)
XXAPI void xrtLLB_InsertNext(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode)
{
	if ( objNode ) {
		// 有参考节点（插入到参考节点后面）
		if ( objNode->Next ) {
			objNode->Next->Prev = objNewNode;
		}
		objNewNode->Prev = objNode;
		objNewNode->Next = objNode->Next;
		objNode->Next = objNewNode;
	} else {
		// 无参考节点（插入到链表末尾）
		objNewNode->Prev = objLLB->LastNode;
		objNewNode->Next = NULL;
		if ( objLLB->LastNode ) {
			// 有尾节点
			objLLB->LastNode->Next = objNewNode;
		} else {
			// 没有尾节点，代表这是一个空链表，所以首节点也要修改
			objLLB->FirstNode = objNewNode;
		}
		objLLB->LastNode = objNewNode;
	}
	objLLB->Count++;
}

// 删除节点
XXAPI void xrtLLB_Remove(xllistbase objLLB, xllistnode objNode)
{
	xllistnode pNext = objNode->Next;
	xllistnode pPrev = objNode->Prev;
	if ( pPrev ) {
		pPrev->Next = pNext;
	} else {
		objLLB->FirstNode = pNext;
	}
	if ( pNext ) {
		pNext->Prev = pPrev;
	} else {
		objLLB->LastNode = pPrev;
	}
	objLLB->Count--;
}


