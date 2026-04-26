


// 平衡二叉树（内部函数）
static inline void xrtAVLTreeRebalance(xavltnode** ancestors, int count)
{
	while ( count > 0 ) {
		xavltnode* ppNode = ancestors[--count];							// 指向当前子树根节点的指针地址
		xavltnode pNode = *ppNode;										// 指向当前子树的根节点
		xavltnode leftp = pNode->left;									// 指向左子树的根节点
		xavltnode rightp = pNode->right;								// 指向右子树的根节点
		int lefth = (leftp != NULL) ? leftp->height : 0;						// 左子树的高度
		int righth = (rightp != NULL) ? rightp->height : 0;						// 右子树的高度
		// 找到当前根节点及其两个子树。通过构造，我们知道它们都符合AVL平衡规则
		if ( righth - lefth < -1 ) {
			/*
			 *         *
			 *       /   \
			 *    n+2      n
			 *
			 * 当前子树左侧过高，违反了平衡规则。根据左子树的配置，我们必须使用两种不同的再平衡方法之一。
			 * 请注意，left p不能为NULL，否则我们不会传递给它
			 */
			xavltnode leftleftp = leftp->left;							// 指向左-左子树的根
			xavltnode leftrightp = leftp->right;						// 指向左-右子树的根
			int leftrighth = (leftrightp != NULL) ? leftrightp->height : 0;		// 左右子树高度
			if ( (leftleftp != NULL) && (leftleftp->height >= leftrighth) ) {
				/*
				 *            <D>                     <B>
				 *             *                    n+2|n+3
				 *           /   \                   /   \
				 *        <B>     <E>    ---->    <A>     <D>
				 *        n+2      n              n+1   n+1|n+2
				 *       /   \                           /   \
				 *    <A>     <C>                     <C>     <E>
				 *    n+1    n|n+1                   n|n+1     n
				 */
				pNode->left = leftrightp;										// D.left = C
				pNode->height = leftrighth + 1;
				leftp->right = pNode;											// B.right = D
				leftp->height = leftrighth + 2;
				*ppNode = leftp;												// B 成为 root
			} else {
				/*
				 *           <F>
				 *            *
				 *          /   \                        <D>
				 *       <B>     <G>                     n+2
				 *       n+2      n                     /   \
				 *      /   \           ---->        <B>     <F>
				 *   <A>     <D>                     n+1     n+1
				 *    n      n+1                    /  \     /  \
				 *          /   \                <A>   <C> <E>   <G>
				 *       <C>     <E>              n  n|n-1 n|n-1  n
				 *      n|n-1   n|n-1
				 *
				 * 我们可以假设left-rightp不是NULL，因为我们希望leftp和rightp符合AVL平衡规则。
				 * 请注意，如果这个假设是错误的，算法将在这里崩溃。
				 */
				leftp->right = leftrightp->left;								// B.right = C
				leftp->height = leftrighth;
				pNode->left = leftrightp->right;								// F.left = E
				pNode->height = leftrighth;
				leftrightp->left = leftp;										// D.left = B
				leftrightp->right = pNode;										// D.right = F
				leftrightp->height = leftrighth + 1;
				*ppNode = leftrightp;											// D 成为 root
			}
		} else if ( righth - lefth > 1 ) {
			/*
			 *        *
			 *      /   \
			 *    n      n+2
			 * 
			 * 当前子树在右侧过高，违反了平衡规则。这与前一种情况完全对称。我们必须根据右子树的配置使用两种不同的再平衡方法之一。
			 * 请注意，rightp不能为NULL，否则我们不会传递给它
			 */
			 xavltnode rightleftp = rightp->left;						// 指向右-左子树的根
			 xavltnode rightrightp = rightp->right;						// 指向右-右子树的根
			 int rightlefth = (rightleftp != NULL) ? rightleftp->height : 0;	// 右左子树高度
			 if ( (rightrightp != NULL) && (rightrightp->height >= rightlefth) ) {
				/*        <B>                             <D>
				 *         *                            n+2|n+3
				 *       /   \                           /   \
				 *    <A>     <D>        ---->        <B>     <E>
				 *     n      n+2                   n+1|n+2   n+1
				 *           /   \                   /   \
				 *        <C>     <E>             <A>     <C>
				 *       n|n+1    n+1              n     n|n+1
				 */
				pNode->right = rightleftp;										// B.right = C
				pNode->height = rightlefth + 1;
				rightp->left = pNode;											// D.left = B
				rightp->height = rightlefth + 2;
				*ppNode = rightp;												// D 成为 root
			} else {
				/*        <B>
				 *         *
				 *       /   \                            <D>
				 *    <A>     <F>                         n+2
				 *     n      n+2                        /   \
				 *           /   \       ---->        <B>     <F>
				 *        <D>     <G>                 n+1     n+1
				 *        n+1      n                 /  \     /  \
				 *       /   \                    <A>   <C> <E>   <G>
				 *    <C>     <E>                  n  n|n-1 n|n-1  n
				 *   n|n-1   n|n-1
				 *
				 * 我们可以假设right-leftp不为NULL，因为我们期望left p和right p符合AVL平衡规则
				 * 请注意，如果这个假设是错误的，算法将在这里崩溃
				 */
				pNode->right = rightleftp->left;								// B.right = C
				pNode->height = rightlefth;
				rightp->left = rightleftp->right;								// F.left = E
				rightp->height = rightlefth;
				rightleftp->left = pNode;										// D.left = B
				rightleftp->right = rightp;										// D.right = F
				rightleftp->height = rightlefth + 1;
				*ppNode = rightleftp;											// D 成为 root
			}
		} else {
			/*
			 * 无需重新平衡，只需设置树的高度
			 * 如果当前子树的高度没有改变，我们可以在这里停下来
			 * 因为我们知道我们没有违反祖先的AVL平衡规则。
			 */
			int height = ((righth > lefth) ? righth : lefth) + 1;
			if ( pNode->height == height ) {
				break;
			}
			pNode->height = height;
		}
	}
}
	
// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey, xavltnode pNewNode)
{
	// 初始化数据
	xavltnode* ppNode = &objAVLT->RootNode;
	xavltnode* ancestor[AVLTree_MAX_HEIGHT];	// 上层节点列表
	int ancestorCount = 0;								// 上层节点数量
	// 找到要添加新节点的叶子节点
	while ( ancestorCount < AVLTree_MAX_HEIGHT ) {
		xavltnode pNode = *ppNode;
		if ( pNode == NULL ) { break; }
		ancestor[ancestorCount++] = ppNode;
		int delta = procComp(&pNode[1], pKey);
		if ( delta < 0 ) {
			ppNode = &(pNode->left);
		} else if ( delta > 0 ) {
			ppNode = &(pNode->right);
		} else {
			return pNode;
		}
	}
	if ( ancestorCount == AVLTree_MAX_HEIGHT ) { return NULL; }
	// 初始化 pNewNode
	pNewNode->left = NULL;
	pNewNode->right = NULL;
	pNewNode->height = 1;
	*ppNode = pNewNode;				// 替换掉空节点
	// 平衡二叉树
	xrtAVLTreeRebalance(ancestor, ancestorCount);
	// 返回节点指针
	objAVLT->Count++;
	return pNewNode;
}

// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey)
{
	xavltnode* ppNode = &objAVLT->RootNode;
	xavltnode* ancestor[AVLTree_MAX_HEIGHT];	// 上层节点列表
	int ancestorCount = 0;								// 上层节点数量
	xavltnode pNode = NULL;
	// 查找需要删除的节点
	while ( ancestorCount < AVLTree_MAX_HEIGHT ) {
		pNode = *ppNode;
		if ( pNode == NULL ) { return NULL; }
		ancestor[ancestorCount++] = ppNode;
		int delta = procComp(&pNode[1], pKey);
		if ( delta < 0 ) {
			ppNode = &(pNode->left);
		} else if ( delta > 0 ) {
			ppNode = &(pNode->right);
		} else {
			break;										// 找到要删除的节点了
		}
	}
	// 没找到要删除的节点
	if ( ancestorCount == AVLTree_MAX_HEIGHT ) {
		return NULL;
	}
	xavltnode pDelete = pNode;
	if ( pNode->left == NULL ) {
		// 删除节点的的左子树上没有节点。
		// 要么在它的右子树上有子节点（根据平衡规则，只能有一个），它取代了要删除的节点，要么它没有子节点(被删除)
		*ppNode = pNode->right;
		// 我们知道pNode->right已经平衡，所以我们不必再次检查
		ancestorCount--;
	} else {
		// 我们将在树的顺序中找到刚好在delNode之前的节点，并将其提升到树中delNode的位置
		xavltnode* ppDelete = ppNode;			// 指向我们必须删除的节点
		int deleteAncestorCount = ancestorCount;		// 替换节点必须插入到祖先列表中的位置
		// 在树排序中搜索要删除节点之前的节点
		ppNode = &(pNode->left);
		while ( ancestorCount < AVLTree_MAX_HEIGHT ) {
			pNode = *ppNode;
			if ( pNode->right == NULL ) { break; }
			ancestor[ancestorCount++] = ppNode;
			ppNode = &(pNode->right);
		}
		if ( ancestorCount == AVLTree_MAX_HEIGHT ) { return NULL; }
		// 此节点将被其（由于平衡规则，它是唯一的）ld替换，或者如果它根本没有子节点，则将被删除
		*ppNode = pNode->left;
		// 现在，此节点将替换树中要删除的节点
		pNode->left = pDelete->left;
		pNode->right = pDelete->right;
		pNode->height = pDelete->height;
		*ppDelete = pNode;
		// 我们用pNode替换了delNode。因此，指向delNode左子树的指针存储在delNode->left中
		// 现在存储在pNode->left，我们必须调整祖先列表以反映这一点。
		ancestor[deleteAncestorCount] = &(pNode->left);
	}
	// 平衡二叉树
	xrtAVLTreeRebalance(ancestor, ancestorCount);
	// 返回结果
	if ( pDelete ) {
		objAVLT->Count--;
		return pDelete;
	} else {
		return NULL;
	}
}

// 从 AVLTree 中查找节点
XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey)
{
	xavltnode pNode = objAVLT->RootNode;
	while ( pNode != NULL ) {
		int delta = procComp(&pNode[1], pKey);
		if ( delta < 0 ) {
			pNode = pNode->left;
		} else if ( delta > 0 ) {
			pNode = pNode->right;
		} else {
			return pNode;
		}
	}
	return NULL;
}

// 遍历 AVLTree 所有节点
XXAPI bool xrtAVLTB_WalkRecuProc(xavltnode root, AVLTree_EachProc procEach, ptr pArg)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			if ( xrtAVLTB_WalkRecuProc(root->left, procEach, pArg) ) {
				return TRUE;
			}
		}
		// 调用回调函数
		if ( procEach ) {
			if ( procEach(xrtAVLTreeGetNodeData(root), pArg) ) {
				return TRUE;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( xrtAVLTB_WalkRecuProc(root->right, procEach, pArg) ) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
XXAPI bool xrtAVLTB_WalkExRecuProc(xavltnode root, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg)
{
	if ( root ) {
		// 调用回调函数(前置)
		if ( procPre ) {
			if ( procPre(xrtAVLTreeGetNodeData(root), pArg) ) {
				return TRUE;
			}
		}
		// 递归左子树
		if ( root->left != NULL ) {
			if ( xrtAVLTB_WalkExRecuProc(root->left, procPre, procIn, procPost, pArg) ) {
				return TRUE;
			}
		}
		// 调用回调函数
		if ( procIn ) {
			if ( procIn(xrtAVLTreeGetNodeData(root), pArg) ) {
				return TRUE;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( xrtAVLTB_WalkExRecuProc(root->right, procPre, procIn, procPost, pArg) ) {
				return TRUE;
			}
		}
		// 调用回调函数(后置)
		if ( procPost ) {
			if ( procPost(xrtAVLTreeGetNodeData(root), pArg) ) {
				return TRUE;
			}
		}
	}
	return FALSE;
}


