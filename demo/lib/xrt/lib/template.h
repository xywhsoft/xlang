


/*
	符号列表：
		{						模板起始符号				需要转义				{{
		! $ % & ? * = @ #		模板操作符号				不需要转义
		:						参数分隔符				需要转义				\:
		}						模板结束符号				需要转义				\}
		\						语句内转义符号			语句内需要转义		\\
*/



/*
	高级语法：
		
		1. define : 注册子模板，子模板可使用 {=子模板名称}、{*变量值:子模板名称} 等方式代入
			{#define:子模板名称}
				子模板内容
			{#end}
			注：子模板不可嵌套，内部可使用 {$__self} 变量访问代入值
		
		2. if : 条件判断
			{#if:表达式}
				语句
			{#elseif:表达式}
				语句
			{#else}
				语句
			{#end}
			注：xTemplateEngine 不对表达式做处理，不要宿主程序自行解析
		
		3. for
			{#for:1:10:1}
				语句
			{#end}
			注：内部可使用 {#__index} 访问当前循环索引
		
		4. foreach
			{#foreach:变量名}
				语句
			{#end}
			注：内部可使用 {#__index} 访问当前循环索引，{$__value} 访问当前值（如果当前值是字符串的话）
		
		5. script
			{#script:脚本语言}
				语句
			{#end}
			注：脚本语句块不可嵌套，另外脚本语句块不解析任何转义符，只在遇到 {#end} 时结束语句
		
		6. include
			{#include:文件名}
		
*/



// 字符串对比函数（strncmp）
#define XTE_STRNCMP		strncasecmp

// 循环最大迭代次数（防止无限循环攻击）
#ifndef XTE_LOOP_MAX_ITERATIONS
#define XTE_LOOP_MAX_ITERATIONS 100000
#endif



// 错误列表
/*
	0		成功							success
	1		内存申请失败					malloc failed
	2		Token 列表添加失败			token list append failed
	3		无法识别的符号				unrecognized symbols
	4		不允许使用空符号				empty symbols are not allowed
	5		参数数量过多					too many parameters
	6		语句未结束					statement not ended
	7		未定义标识符					Undefined identifier
*/
const char* XTE_ERROR_DESC[] = {
	"success",
	"malloc failed",
	"token list append failed",
	"unrecognized symbols",
	"empty symbols are not allowed",
	"too many parameters",
	"statement not ended",
	"Undefined identifier",
	"Missing parameters",
	"Nesting of define statements is not allowed.",
	"syntax error"
};



// 内存申请失败错误返回对象（避免返回 NULL）
XTE_TokenList_Struct XTE_LEXER_ERROR_MALLOC = {
	0,
	1,
	"malloc failed",
	0,
	0,
	0,
	0,
	0,
	0,
	{
		NULL,
		0,
		0,
		0,
		0
	}
};



// 创建关键字列表（失败返回 NULL）
xarray xteCreateIdentList()
{
	return xrtArrayCreate(sizeof(XTE_IdentInfo_Struct));
}

// 销毁关键字列表
void xteDestroyIdentList(xarray objList)
{
	if ( objList ) {
		for ( int i = 1; i <= objList->Count; i++ ) {
			XTE_IdentInfo objID = xrtArrayGet_Inline(objList, i);
			if ( objID->Ident ) {
				xrtFree(objID->Ident);
			}
		}
		xrtArrayDestroy(objList);
	}
}

// 添加一个关键字到列表
int xteAddIdentToList(xarray objList, char* sID, unsigned int iSize, unsigned int iIndex, unsigned int iType, unsigned int iMinParamCount, unsigned int iMaxParamCount)
{
	// 自动计算关键字长度
	if ( iSize == 0 ) {
		iSize = strlen(sID);
		if ( iSize == 0 ) {
			return 0;
		}
	}
	if ( objList ) {
		// 创建关键字字符串副本
		char* sKey = xrtMalloc(iSize + 1);
		if ( sKey == NULL ) {
			return 0;
		}
		memcpy(sKey, sID, iSize);
		sKey[iSize] = 0;
		// 添加关键字到列表
		unsigned int idx = xrtArrayAppend(objList, 1);
		if ( idx == 0 ) {
			xrtFree(sKey);
			return 0;
		}
		XTE_IdentInfo objID = xrtArrayGet_Inline(objList, idx);
		objID->Ident = sKey;
		objID->TokenIndex = iIndex;
		objID->Type = iType;
		objID->Size = iSize;
		objID->MinParamCount = iMinParamCount;
		objID->MaxParamCount = iMaxParamCount;
		objID->Hash = xrtHash32(sKey, iSize);
		return idx;
	}
	return 0;
}



// Trim buffer 前后空白字符（原地修改）
static void xte_private_trim_buffer(xbuffer objBuf)
{
	if ( objBuf == NULL || objBuf->Length == 0 ) return;
	
	char* buf = objBuf->Buffer;
	size_t len = objBuf->Length;
	size_t start = 0;
	size_t end = len;
	
	// 跳过前导空白
	while ( start < len && (buf[start] == ' ' || buf[start] == '\t') ) {
		start++;
	}
	
	// 跳过尾部空白
	while ( end > start && (buf[end - 1] == ' ' || buf[end - 1] == '\t') ) {
		end--;
	}
	
	// 如果有变化，移动数据
	if ( start > 0 || end < len ) {
		size_t newLen = end - start;
		if ( start > 0 && newLen > 0 ) {
			memmove(buf, buf + start, newLen);
		}
		objBuf->Length = newLen;
		buf[newLen] = '\0';
	}
}

// 构建分词器解析错误
#define XTE_OnLexerError(id) { objRet->Success=0; objRet->ErrorCode=id; objRet->ErrorDesc=XTE_ERROR_DESC[id]; objRet->ErrorLine=iLine; objRet->ErrorLinePos=iLinePos; objRet->ErrorPos=i; objRet->ErrorRefLine=iRefLine; objRet->ErrorRefLinePos=iRefLinePos; objRet->ErrorRefPos=iRefPos; if(objBuf){xrtBufferDestroy(objBuf);}; xte_private_free_tokenlist(&objRet->Tokens); return objRet; }



// 释放 XTE_TokenList
void xte_private_free_tokenlist(xarray arrToken)
{
	if ( arrToken->Memory ) {
		for ( int i = 1; i <= arrToken->Count; i++ ) {
			XTE_TokenItem objTok = xrtArrayGet_Inline(arrToken, i);
			if ( objTok->Text ) {
				xrtFree(objTok->Text);
			}
			for ( int iParam = 0; iParam < objTok->ParamCount; iParam++ ) {
				if ( objTok->ParamText[iParam] ) {
					xrtFree(objTok->ParamText[iParam]);
				}
			}
		}
		xrtArrayUnit(arrToken);
	}
}
void xteLexerFree(XTE_TokenList arrToken)
{
	if ( arrToken != &XTE_LEXER_ERROR_MALLOC ) {
		xte_private_free_tokenlist(&arrToken->Tokens);
		xrtFree(arrToken);
	}
}

// 解析模板文件为 Token 列表
XTE_TokenList xteLexer(char* sText, size_t iSize, xarray objIdentList, char* sBracket)
{
	// 创建返回值结构体
	XTE_TokenList objRet = xrtMalloc(sizeof(XTE_TokenList_Struct));
	if ( objRet == NULL ) {
		return &XTE_LEXER_ERROR_MALLOC;
	}
	objRet->ErrorLine = 0;
	objRet->ErrorLinePos = 0;
	objRet->ErrorPos = 0;
	objRet->ErrorRefLine = 0;
	objRet->ErrorRefLinePos = 0;
	objRet->ErrorRefPos = 0;
	xrtArrayInit(&objRet->Tokens, sizeof(XTE_TokenItem_Struct));
	// 自动计算文本长度
	if ( iSize == 0 ) {
		iSize = strlen(sText);
		if ( iSize == 0 ) {
			objRet->Success = -1;
			objRet->ErrorCode = 0;
			objRet->ErrorDesc = XTE_ERROR_DESC[0];
			return objRet;
		}
	}
	// 默认使用花括号作为模板符号
	if ( sBracket == NULL ) {
		sBracket = "{}";
	}
	// 行号信息
	size_t i = 0;
	size_t iPos = 0;
	size_t iRefLine = 1;
	size_t iRefLinePos = 1;
	size_t iRefPos = 0;
	size_t iLine = 1;
	size_t iLinePos = 0;
	// 创建临时缓冲区对象
	xbuffer objBuf = xrtBufferCreate(0);
	if ( objBuf == NULL ) {
		XTE_OnLexerError(1);
	}
	// 遍历获取 Token
	int iMode = XTE_TK_TEXT;
	XTE_TokenItem objCurTok;
	size_t iSkip = 0;
	for ( i = 0; i < iSize; i++ ) {
		// 处理行号和行位置
		if ( sText[i] == '\n' ) {
			iLine++;
			iLinePos = 0;
		} else if ( sText[i] != '\r' ) {
			iLinePos++;
		}
		// 处理跳过字符
		if ( iSkip ) {
			iSkip--;
			continue;
		}
		// Token 扫描逻辑
		if ( iMode == XTE_TK_TEXT ) {
			// XTE_TK_TEXT 采集模式
			if ( sText[i] == sBracket[0] && (i + 1 < iSize) ) {
				if ( sText[i + 1] == sBracket[0] ) {
					// 处理 {{ 转义符
					xrtBufferAppend(objBuf, &sText[iPos], (i - iPos) + 1, XBUF_UTF8);
					iSkip++;
					iPos = i + 2;
				} else {
					// 将符号前的内容添加到临时缓冲区
					if ( i > iPos ) {
						xrtBufferAppend(objBuf, &sText[iPos], i - iPos, XBUF_UTF8);
					}
					// 如果临时缓冲区存在数据，则创建一个 XTE_TK_TEXT 节点
					if ( objBuf->Length > 0 ) {
						unsigned int idx = xrtArrayAppend(&objRet->Tokens, 1);
						if ( idx == 0 ) {
							XTE_OnLexerError(2);
						}
						XTE_TokenItem objTok = xrtArrayGet_Inline(&objRet->Tokens, idx);
						objTok->Type = XTE_TK_TEXT;
						objTok->Text = xrtMalloc(objBuf->Length + 1);
						if ( objTok->Text == NULL ) {
							XTE_OnLexerError(1);
						}
						objTok->Size = objBuf->Length;
						memcpy(objTok->Text, objBuf->Buffer, objBuf->Length);
						objTok->Text[objBuf->Length] = 0;
						objTok->ParamCount = 0;
						objTok->IdentInfo = NULL;
						objTok->RefLine = iRefLine;
						objTok->RefLinePos = iRefLinePos;
						objTok->RefPos = iRefPos + 1;
						objTok->RefSize = i - iRefPos;
						xrtBufferClear(objBuf);
					}
					// 更新引用位置信息
					iRefLine = iLine;
					iRefLinePos = iLinePos;
					iRefPos = i;
					// 创建一个新的 TokenItem
					unsigned int idx = xrtArrayAppend(&objRet->Tokens, 1);
					if ( idx == 0 ) {
						XTE_OnLexerError(2);
					}
					objCurTok = xrtArrayGet_Inline(&objRet->Tokens, idx);
					// 根据下一个符号判断 Token 类型
					if ( sText[i + 1] == '!' ) {
						objCurTok->Type = XTE_TK_COMMEN;
					} else if ( sText[i + 1] == '$' ) {
						objCurTok->Type = XTE_TK_VAR;
					} else if ( sText[i + 1] == '%' ) {
						objCurTok->Type = XTE_TK_NUM;
					} else if ( sText[i + 1] == '&' ) {
						objCurTok->Type = XTE_TK_TIME;
					} else if ( sText[i + 1] == '?' ) {
						objCurTok->Type = XTE_TK_BOOL;
					} else if ( sText[i + 1] == '*' ) {
						objCurTok->Type = XTE_TK_ARR;
					} else if ( sText[i + 1] == '@' ) {
						objCurTok->Type = XTE_TK_PROC;
					} else if ( sText[i + 1] == '=' ) {
						objCurTok->Type = XTE_TK_SUBTEMPLATE;
					} else if ( sText[i + 1] == '#' ) {
						if ( objIdentList ) {
							objCurTok->Type = XTE_TK_SYMBOL;
						} else {
							// objIdentList 传递为空时，不支持 {# xxx } 的写法
							XTE_OnLexerError(3);
						}
					} else {
						// 无法识别的 Token 类型 - 将 { 回归到文本 buffer 中
						objRet->Tokens.Count--;  // 回退刚刚创建的 token
						xrtBufferAppend(objBuf, &sBracket[0], 1, XBUF_UTF8);  // 将 { 追加到 buffer
						// 回到文本采集模式，继续从下一个字符解析
						iMode = XTE_TK_TEXT;
						iPos = i + 1;
						continue;
					}
					// 切换为对应的 Token 信息采集模式
					iMode = objCurTok->Type;
					objCurTok->Size = 0;
					objCurTok->Text = NULL;
					objCurTok->ParamCount = 0;
					for ( int iParam = 0; iParam < XTE_PARAM_MAXCOUNT; iParam++ ) {
						objCurTok->ParamText[iParam] = NULL;
						objCurTok->ParamSize[iParam] = 0;
					}
					objCurTok->IdentInfo = NULL;
					objCurTok->RefLine = iRefLine;
					objCurTok->RefLinePos = iRefLinePos;
					objCurTok->RefPos = iRefPos + 1;
					iSkip++;
					iPos = i + 2;
				}
			}
		} else if ( iMode == XTE_MODE_BLOCK ) {
			// XTE_MODE_BLOCK 采集模式（只在遇到 {#end} 时退出）
			if ( (i + 5 < iSize) && (sText[i] == sBracket[0]) && (sText[i+1] == '#') && (sText[i+2] == 'e') && (sText[i+3] == 'n') && (sText[i+4] == 'd') && (sText[i+5] == sBracket[1]) ) {
				objCurTok->Size = i - iPos;
				objCurTok->Text = xrtMalloc(objCurTok->Size + 1);
				if ( objCurTok->Text == NULL ) {
					XTE_OnLexerError(1);
				}
				memcpy(objCurTok->Text, &sText[iPos], objCurTok->Size);
				objCurTok->Text[objCurTok->Size] = 0;
				// 更新引用位置信息
				iRefLine = iLine;
				iRefLinePos = iLinePos + 6;
				objCurTok->RefSize = i - iRefPos + 6;
				iRefPos = i + 6;
				// 语句结束，切换为文本采集模式
				iSkip += 5;
				iPos = i + 6;
				iMode = XTE_TK_TEXT;
			}
		} else if ( iMode == XTE_TK_COMMEN ) {
			// XTE_TK_COMMEN 采集模式（跳过转义符，不处理参数列表，这样可以加快效率）
			if ( sText[i] == '\\' ) {
				// 转义符处理
				iSkip++;
			} else if ( sText[i] == sBracket[1] ) {
				// 语句结尾处理
				objCurTok->Size = i - iPos;
				if ( objCurTok->Size > 0 ) {
					objCurTok->Text = xrtMalloc(objCurTok->Size + 1);
					if ( objCurTok->Text == NULL ) {
						XTE_OnLexerError(1);
					}
					memcpy(objCurTok->Text, &sText[iPos], objCurTok->Size);
					objCurTok->Text[objCurTok->Size] = 0;
				}
				objCurTok->ParamCount = 0;
				// 更新引用位置信息
				iRefLine = iLine;
				iRefLinePos = iLinePos + 1;
				objCurTok->RefSize = i - iRefPos + 1;
				iRefPos = i + 1;
				// 语句结束，切换为文本采集模式
				iMode = XTE_TK_TEXT;
				iPos = i + 1;
			}
		} else {
			// 通用 XTE_TK_* 的 Token 信息采集模式
			if ( sText[i] == '\\' ) {
				// 转义符处理
				if ( i > iPos ) {
					xrtBufferAppend(objBuf, &sText[iPos], i - iPos, XBUF_UTF8);
				}
				iSkip++;
				xrtBufferAppend(objBuf, &sText[i+1], 1, XBUF_UTF8);
				iPos = i + 2;
			} else if ( (sText[i] == ':') || (sText[i] == sBracket[1]) ) {
				// 参数处理
				if ( i > iPos ) {
					xrtBufferAppend(objBuf, &sText[iPos], i - iPos, XBUF_UTF8);
				}
				// 如果 objCurTok->Text 为 NULL 则将 objBuf 传递给 objCurTok->Text，否则传递到 ParamText
				if ( objCurTok->Text ) {
					if ( objCurTok->ParamCount < XTE_PARAM_MAXCOUNT ) {
						// 如果是 {# xxx } 语句，检查参数数量是否超出允许最大数量
						if ( objCurTok->IdentInfo ) {
							if ( objCurTok->ParamCount >= objCurTok->IdentInfo->MaxParamCount ) {
								// 参数数量超出允许的最大数量
								XTE_OnLexerError(5);
							}
						}
						// 更新参数数据（先 trim 空格）
						xte_private_trim_buffer(objBuf);
						objCurTok->ParamSize[objCurTok->ParamCount] = objBuf->Length;
						if ( objBuf->Length > 0 ) {
							objCurTok->ParamText[objCurTok->ParamCount] = xrtMalloc(objBuf->Length + 1);
							if ( objCurTok->ParamText[objCurTok->ParamCount] == NULL ) {
								XTE_OnLexerError(1);
							}
							memcpy(objCurTok->ParamText[objCurTok->ParamCount], objBuf->Buffer, objBuf->Length);
							objCurTok->ParamText[objCurTok->ParamCount][objBuf->Length] = 0;
						}
						objCurTok->ParamCount++;
					} else {
						// 参数数量超出允许的最大数量
						XTE_OnLexerError(5);
					}
				} else {
					// 先 trim 空格
					xte_private_trim_buffer(objBuf);
					if ( objBuf->Length > 0 ) {
						// 如果是 {# xxx } 语句，先判断是否支持对应关键字
						if ( objCurTok->Type == XTE_TK_SYMBOL ) {
							unsigned int iHash = 0;
							for ( int iIdent = 1; iIdent <= objIdentList->Count; iIdent++ ) {
								XTE_IdentInfo objID = xrtArrayGet_Inline(objIdentList, iIdent);
								if ( objID->Size == objBuf->Length ) {
									if ( iHash == 0 ) {
										iHash = xrtHash32(objBuf->Buffer, objBuf->Length);
									}
									if ( objID->Hash == iHash ) {
										if ( XTE_STRNCMP(objBuf->Buffer, objID->Ident, objBuf->Length) == 0 ) {
											objCurTok->IdentInfo = objID;
											break;
										}
									}
								}
							}
							// 找不到标识符时，报错
							if ( objCurTok->IdentInfo == NULL ) {
								XTE_OnLexerError(7);
							}
							// 使用标识符对应的分类覆盖 XTE_TK_SYMBOL
							objCurTok->Type = objCurTok->IdentInfo->TokenIndex;
						}
						// 更新 Text 及关联字段
						objCurTok->Size = objBuf->Length;
						objCurTok->Text = xrtMalloc(objCurTok->Size + 1);
						if ( objCurTok->Text == NULL ) {
							XTE_OnLexerError(1);
						}
						memcpy(objCurTok->Text, objBuf->Buffer, objBuf->Length);
						objCurTok->Text[objCurTok->Size] = 0;
					} else {
						// objCurTok->Text 不允许 0 长度
						XTE_OnLexerError(4);
					}
				}
				xrtBufferClear(objBuf);
				iPos = i + 1;
				// 语句结尾处理
				if ( sText[i] == sBracket[1] ) {
					if ( objCurTok->IdentInfo ) {
						// 如果是 {# xxx } 语句，检查参数数量是否低于允许最小数量
						if ( objCurTok->ParamCount < objCurTok->IdentInfo->MinParamCount ) {
							XTE_OnLexerError(8);
						}
						// 语句块采集类，进入专用的处理流程
						if ( objCurTok->IdentInfo->Type == XTE_IDTPE_BLOCK ) {
							xrtFree(objCurTok->Text);
							objCurTok->Text = NULL;
							iMode = XTE_MODE_BLOCK;
						}
					}
					// 直接结束的语句，更新引用位置信息，切换为文本采集模式
					if ( iMode != XTE_MODE_BLOCK ) {
						iRefLine = iLine;
						iRefLinePos = iLinePos + 1;
						objCurTok->RefSize = i - iRefPos + 1;
						iRefPos = i + 1;
						iMode = XTE_TK_TEXT;
					}
				}
			}
		}
	}
	// 收尾操作
	if ( iMode == XTE_TK_TEXT ) {
		// 如果临时缓冲区存在数据，将尾部字符串添加为 XTE_TK_TEXT 节点
		if ( i > iPos ) {
			xrtBufferAppend(objBuf, &sText[iPos], i - iPos, XBUF_UTF8);
		}
		if ( objBuf->Length > 0 ) {
			unsigned int idx = xrtArrayAppend(&objRet->Tokens, 1);
			if ( idx == 0 ) {
				XTE_OnLexerError(2);
			}
			XTE_TokenItem objTok = xrtArrayGet_Inline(&objRet->Tokens, idx);
			objTok->Type = XTE_TK_TEXT;
			objTok->Text = xrtMalloc(objBuf->Length + 1);
			if ( objTok->Text == NULL ) {
				XTE_OnLexerError(1);
			}
			objTok->Size = objBuf->Length;
			memcpy(objTok->Text, objBuf->Buffer, objBuf->Length);
			objTok->Text[objBuf->Length] = 0;
			objTok->ParamCount = 0;
			objTok->RefLine = iRefLine;
			objTok->RefLinePos = iRefLinePos;
			objTok->RefPos = iRefPos + 1;
			objTok->RefSize = i - iRefPos;
			xrtBufferClear(objBuf);
		}
	} else {
		// 如果仍进行 XTE_TK_* 信息采集，代表词法有错误，语句没有正确结束
		XTE_OnLexerError(6);
	}
	// 返回 Token 列表
	objRet->Success = -1;
	objRet->ErrorCode = 0;
	objRet->ErrorDesc = XTE_ERROR_DESC[0];
	return objRet;
}



// ==================== 路径解析器 ====================

/*
	支持的路径语法：
		- a.b.c          通过点号访问嵌套属性
		- arr[0]         通过数字索引访问数组
		- obj["key"]     通过字符串键访问表
		- arr[0].name    组合访问
*/

// 路径解析器：支持 a.b.c 和 arr[0] 语法
// 返回解析到的 xvalue，失败返回 &XVO_VALUE_NULL
xvalue xteResolvePath(const char* path, size_t pathLen, xvalue tblVal, xvalue tblRoot, xvalue tblENV)
{
	// 空路径检查
	if ( path == NULL || pathLen == 0 ) {
		if ( path != NULL ) {
			pathLen = strlen(path);
		}
		if ( pathLen == 0 ) {
			return &XVO_VALUE_NULL;
		}
	}
	
	// 快速路径：如果没有 . 和 [ 则直接查找
	int hasAccessor = 0;
	for ( size_t i = 0; i < pathLen; i++ ) {
		if ( path[i] == '.' || path[i] == '[' ) {
			hasAccessor = 1;
			break;
		}
	}
	
	if ( !hasAccessor ) {
		// 简单路径，直接查找
		xvalue result = &XVO_VALUE_NULL;
		if ( tblVal && tblVal->Type == XVO_DT_TABLE ) {
			result = xvoTableGetValue(tblVal, (str)path, pathLen);
		}
		if ( result == &XVO_VALUE_NULL && tblRoot && tblRoot->Type == XVO_DT_TABLE ) {
			result = xvoTableGetValue(tblRoot, (str)path, pathLen);
		}
		if ( result == &XVO_VALUE_NULL && tblENV && tblENV->Type == XVO_DT_TABLE ) {
			result = xvoTableGetValue(tblENV, (str)path, pathLen);
		}
		return result;
	}
	
	// 复杂路径解析
	xvalue current = NULL;
	size_t pos = 0;
	size_t segStart = 0;
	int isFirst = 1;
	
	while ( pos <= pathLen ) {
		char ch = (pos < pathLen) ? path[pos] : '\0';
		
		// 到达分隔符或结尾
		if ( ch == '.' || ch == '[' || ch == '\0' ) {
			// 提取当前段
			size_t segLen = pos - segStart;
			
			if ( segLen > 0 ) {
				const char* seg = path + segStart;
				
				if ( isFirst ) {
					// 第一段：从三个表中查找
					isFirst = 0;
					if ( tblVal && tblVal->Type == XVO_DT_TABLE ) {
						current = xvoTableGetValue(tblVal, (str)seg, segLen);
					}
					if ( (current == NULL || current == &XVO_VALUE_NULL) && tblRoot && tblRoot->Type == XVO_DT_TABLE ) {
						current = xvoTableGetValue(tblRoot, (str)seg, segLen);
					}
					if ( (current == NULL || current == &XVO_VALUE_NULL) && tblENV && tblENV->Type == XVO_DT_TABLE ) {
						current = xvoTableGetValue(tblENV, (str)seg, segLen);
					}
				} else {
					// 后续段：从当前值中查找
					if ( current == NULL || current == &XVO_VALUE_NULL ) {
						return &XVO_VALUE_NULL;
					}
					if ( current->Type == XVO_DT_TABLE ) {
						current = xvoTableGetValue(current, (str)seg, segLen);
					} else {
						return &XVO_VALUE_NULL;
					}
				}
			}
			
			// 处理 [ ] 索引访问
			if ( ch == '[' ) {
				pos++;
				size_t idxStart = pos;
				
				// 查找匹配的 ]
				while ( pos < pathLen && path[pos] != ']' ) {
					pos++;
				}
				
				if ( pos >= pathLen ) {
					// 没有找到 ]
					return &XVO_VALUE_NULL;
				}
				
				size_t idxLen = pos - idxStart;
				const char* idxStr = path + idxStart;
				
				// 确保 current 有效
				if ( current == NULL || current == &XVO_VALUE_NULL ) {
					// 第一个 [] 之前没有标识符，错误
					return &XVO_VALUE_NULL;
				}
				
				if ( idxLen > 0 ) {
					// 判断是数字索引还是字符串键
					if ( (idxStr[0] == '"' || idxStr[0] == '\'') && idxLen >= 2 ) {
						// 字符串键: ["key"] 或 ['key']
						const char* key = idxStr + 1;
						size_t keyLen = idxLen - 2;
						if ( current->Type == XVO_DT_TABLE ) {
							current = xvoTableGetValue(current, (str)key, keyLen);
						} else {
							return &XVO_VALUE_NULL;
						}
					} else {
						// 数字索引: [0]
						int index = 0;
						for ( size_t k = 0; k < idxLen; k++ ) {
							if ( idxStr[k] >= '0' && idxStr[k] <= '9' ) {
								index = index * 10 + (idxStr[k] - '0');
							} else {
								// 非数字，尝试作为键名处理
								if ( current->Type == XVO_DT_TABLE ) {
									current = xvoTableGetValue(current, (str)idxStr, idxLen);
								} else {
									return &XVO_VALUE_NULL;
								}
								goto next_segment;
							}
						}
						// 数字索引访问
						if ( current->Type == XVO_DT_ARRAY ) {
							current = xvoArrayGetValue(current, index);
						} else if ( current->Type == XVO_DT_LIST ) {
							current = xvoListGetValue(current, index);
						} else if ( current->Type == XVO_DT_TABLE ) {
							// 表也可以用数字作为键
							current = xvoTableGetValue(current, (str)idxStr, idxLen);
						} else {
							return &XVO_VALUE_NULL;
						}
					}
				}
				
				next_segment:
				pos++; // 跳过 ]
				segStart = pos;
				
				// 跳过 ] 后的 .
				if ( pos < pathLen && path[pos] == '.' ) {
					pos++;
					segStart = pos;
				}
				continue;
			}
			
			// 跳过 .
			if ( ch == '.' ) {
				pos++;
				segStart = pos;
				continue;
			}
			
			// 结尾
			break;
		}
		
		pos++;
	}
	
	return (current != NULL) ? current : &XVO_VALUE_NULL;
}






// 简单版模板引擎语法转换套件（编译后得到一个列表，不支持语法结构）
/*
	支持的写法：
		{!xxx}				注释
		{$xxx}				代入变量
		{%xxx}				代入数组变量
		{&xxx}				代入时间变量
		{?xxx}				根据变量逻辑值代入不用内容
		{*xxx}				代入数组变量（套子模板）
		{=xxx}				代入子变量（可指定代入值，代入值可以是任何数据类型，__self 表示代入值）
		{@xxx}				代入函数运行结果
		{#include}			引用文件（需要自行处理文件I/O）
		{#define}			定义子模板
		{#script}			运行脚本块（可指定脚本引擎）
*/



// 内存申请失败错误返回对象（避免返回 NULL）
XTE_LiteStruct XTE_LITE_ERROR_MALLOC = {
	0,
	1,
	"malloc failed",
	0,
	0,
	0,
	0,
	0,
	0,
	{ NULL, 0, 0, 0 },							// Tokens (xarray_struct)
	{ NULL, 0, 0, 0 },							// Actions (xparray_struct)
	{
		{
			NULL,
			0,
			NULL,
			NULL,
			NULL,
			{
				0,
				{
					0,
					0,
					{
						NULL,
						0,
						0,
						0
					},
					NULL
				},
				NULL,
				NULL,
				NULL,
				NULL
			},
			NULL
		},
		NULL
	}
};



// xTemplate Engine Lite 模板管理器数据结构
typedef struct {
	xdict_struct Templates;					// 已加载模板列表（哈希表）
} XTE_LiteManageStruct, *XTE_LiteManage;



// 构建语法器解析错误
#define XTE_OnParseError(id) { objRet->Success=0; objRet->ErrorCode=id; objRet->ErrorDesc=XTE_ERROR_DESC[id]; objRet->ErrorLine=objTok->RefLine; objRet->ErrorLinePos=objTok->RefLinePos; objRet->ErrorPos=objTok->RefPos; if(objRefTok){ objRet->ErrorRefLine=objRefTok->RefLine; objRet->ErrorRefLinePos=objRefTok->RefLinePos; objRet->ErrorRefPos=objRefTok->RefPos; }else{ objRet->ErrorRefLine=objTok->RefLine; objRet->ErrorRefLinePos=objTok->RefLinePos; objRet->ErrorRefPos=objTok->RefPos; } return objRet; }



// 将 XTE_TokenList 转换为 XTE_LiteObject（XTE_TokenList将被释放）
XTE_LiteObject xteParseFromTokenList(XTE_TokenList objToks)
{
	// 创建返回值结构体
	XTE_LiteObject objRet = xrtMalloc(sizeof(XTE_LiteStruct));
	if ( objRet == NULL ) {
		xteLexerFree(objToks);							// 如果一开始就失败了，objToks 会被无条件释放
		return &XTE_LITE_ERROR_MALLOC;
	}
	// Token 必须解析成功，否则返回 Lexer 报告的错误
	if ( objToks->Success == 0 ) {
		objRet->Success = 0;
		objRet->ErrorCode = objToks->ErrorCode;
		objRet->ErrorDesc = objToks->ErrorDesc;
		objRet->ErrorLine = objToks->ErrorLine;
		objRet->ErrorLinePos = objToks->ErrorLinePos;
		objRet->ErrorPos = objToks->ErrorPos;
		objRet->ErrorRefLine = objToks->ErrorRefLine;
		objRet->ErrorRefLinePos = objToks->ErrorRefLinePos;
		objRet->ErrorRefPos = objToks->ErrorRefPos;
		xteLexerFree(objToks);							// 如果一开始就失败了，objToks 会被无条件释放
		return objRet;
	}
	// 转移 Token 数组所有权（结构体复制）
	objRet->Tokens = objToks->Tokens;
	objToks->Tokens.Memory = NULL;		// 防止 xteLexerFree 释放数组
	xteLexerFree(objToks);				// 安全释放 TokenList 外壳
	// 初始化数据结构
	xrtPtrArrayInit(&objRet->Actions);
	xrtDictInit(&objRet->SubTemplates, sizeof(xparray_struct));
	// 遍历 Token 列表，添加到 Actions 列表
	XTE_TokenItem objRefTok = NULL;
	xparray objCurTemplate = NULL;
	for ( int i = 1; i <= objRet->Tokens.Count; i++ ) {
		XTE_TokenItem objTok = xrtArrayGet_Inline(&objRet->Tokens, i);
		if ( objTok->Type == XTE_TK_DEFINE ) {
			// 定义子模板语句开始
			if ( objCurTemplate ) {
				XTE_OnParseError(9);
			} else {
				objCurTemplate = xrtDictSet(&objRet->SubTemplates, objTok->ParamText[0], objTok->ParamSize[0], NULL);
				if ( objCurTemplate == NULL ) {
					XTE_OnParseError(2);
				}
				objRefTok = objTok;
				xrtPtrArrayInit(objCurTemplate);
			}
		} else if ( objTok->Type == XTE_TK_END ) {
			// 语句结束
			if ( objCurTemplate ) {
				// 在 define 块内，结束 define 块
				objCurTemplate = NULL;
			} else {
				// 不在 define 块内，这可能是 if/for/foreach 的 end，添加到 Actions
				unsigned int idx = xrtPtrArrayAppend(&objRet->Actions, objTok);
				if ( idx == 0 ) {
					XTE_OnParseError(2);
				}
			}
		} else if ( objTok->Type == XTE_TK_ELSEIF || objTok->Type == XTE_TK_ELSE ) {
			// elseif/else 也需要添加到 Actions
			if ( objCurTemplate ) {
				unsigned int idx = xrtPtrArrayAppend(objCurTemplate, objTok);
				if ( idx == 0 ) {
					XTE_OnParseError(2);
				}
			} else {
				unsigned int idx = xrtPtrArrayAppend(&objRet->Actions, objTok);
				if ( idx == 0 ) {
					XTE_OnParseError(2);
				}
			}
		} else if ( objTok->Type == XTE_TK_COMMEN ) {
			// 丢弃注释语句
		} else {
			// 其他语句、指令或文本块（根据当前子模板决定存入位置）
			if ( objCurTemplate ) {
				unsigned int idx = xrtPtrArrayAppend(objCurTemplate, objTok);
				if ( idx == 0 ) {
					XTE_OnParseError(2);
				}
			} else {
				unsigned int idx = xrtPtrArrayAppend(&objRet->Actions, objTok);
				if ( idx == 0 ) {
					XTE_OnParseError(2);
				}
			}
		}
	}
	// 检查 define 块是否未闭合
	if ( objCurTemplate != NULL ) {
		// define 块未闭合，报错
		objRet->Success = 0;
		objRet->ErrorCode = 6;  // statement not ended
		objRet->ErrorDesc = XTE_ERROR_DESC[6];
		if ( objRefTok ) {
			objRet->ErrorLine = objRefTok->RefLine;
			objRet->ErrorLinePos = objRefTok->RefLinePos;
			objRet->ErrorPos = objRefTok->RefPos;
			objRet->ErrorRefLine = objRefTok->RefLine;
			objRet->ErrorRefLinePos = objRefTok->RefLinePos;
			objRet->ErrorRefPos = objRefTok->RefPos;
		}
		return objRet;
	}
	
	// 返回 Token 列表
	objRet->Success = -1;
	objRet->ErrorCode = 0;
	objRet->ErrorDesc = XTE_ERROR_DESC[0];
	return objRet;
}



// 模板引擎全局状态（静态初始化方案）
// 注: XTE_IDENT_LIST 在 xrtInit 时初始化一次，后续只读，无需线程同步
static xarray XTE_IDENT_LIST = NULL;
static xdict XTE_EXPR_CACHE = NULL;  // 表达式 AST 缓存
static int XTE_INITIALIZED = 0;

// 初始化模板引擎（由 xrtInit 调用，用户无感）
static int xte_private_init(void)
{
	if ( XTE_INITIALIZED ) {
		return 1;  // 已初始化
	}
	
	// 创建标识符列表
	XTE_IDENT_LIST = xteCreateIdentList();
	if ( XTE_IDENT_LIST == NULL ) {
		return 0;
	}
	
	// 注册内置标识符
	xteAddIdentToList(XTE_IDENT_LIST, "end"     , 3, XTE_TK_END     , XTE_IDTPE_DEFAULT, 0, 0);
	xteAddIdentToList(XTE_IDENT_LIST, "include" , 7, XTE_TK_INCLUDE , XTE_IDTPE_DEFAULT, 1, 1);
	xteAddIdentToList(XTE_IDENT_LIST, "define"  , 6, XTE_TK_DEFINE  , XTE_IDTPE_DEFAULT, 1, 1);
	xteAddIdentToList(XTE_IDENT_LIST, "script"  , 6, XTE_TK_SCRIPT  , XTE_IDTPE_BLOCK  , 1, 1);
	// 控制语句
	xteAddIdentToList(XTE_IDENT_LIST, "if"      , 2, XTE_TK_IF      , XTE_IDTPE_DEFAULT, 1, 1);
	xteAddIdentToList(XTE_IDENT_LIST, "elseif"  , 6, XTE_TK_ELSEIF  , XTE_IDTPE_DEFAULT, 1, 1);
	xteAddIdentToList(XTE_IDENT_LIST, "else"    , 4, XTE_TK_ELSE    , XTE_IDTPE_DEFAULT, 0, 0);
	xteAddIdentToList(XTE_IDENT_LIST, "for"     , 3, XTE_TK_FOR     , XTE_IDTPE_DEFAULT, 3, 4);
	xteAddIdentToList(XTE_IDENT_LIST, "foreach" , 7, XTE_TK_FOREACH , XTE_IDTPE_DEFAULT, 1, 2);
	xteAddIdentToList(XTE_IDENT_LIST, "break"   , 5, XTE_TK_BREAK   , XTE_IDTPE_DEFAULT, 0, 0);
	xteAddIdentToList(XTE_IDENT_LIST, "continue", 8, XTE_TK_CONTINUE, XTE_IDTPE_DEFAULT, 0, 0);
	
	// 创建表达式 AST 缓存
	XTE_EXPR_CACHE = xrtDictCreate(sizeof(ptr));  // 存储指针需要 sizeof(ptr) 空间
	if ( XTE_EXPR_CACHE == NULL ) {
		xteDestroyIdentList(XTE_IDENT_LIST);
		XTE_IDENT_LIST = NULL;
		return 0;
	}
	
	XTE_INITIALIZED = 1;
	return 1;
}

// 缓存的表达式 AST 清理回调
static int xte_private_free_expr_cache(Dict_Key* pKey, XTE_ExprResult result, void* pArg)
{
	if ( result ) {
		xteExprFree(result);
	}
	return 0;
}

// 清理模板引擎（由 xrtUnit 调用，用户无感）
static void xte_private_unit(void)
{
	if ( !XTE_INITIALIZED ) {
		return;
	}
	
	if ( XTE_IDENT_LIST ) {
		xteDestroyIdentList(XTE_IDENT_LIST);  // 修复: 使用正确的销毁函数避免内存泄漏
		XTE_IDENT_LIST = NULL;
	}
	
	// 清理表达式 AST 缓存
	if ( XTE_EXPR_CACHE ) {
		xrtDictWalk(XTE_EXPR_CACHE, (void*)xte_private_free_expr_cache, NULL);
		xrtDictDestroy(XTE_EXPR_CACHE);
		XTE_EXPR_CACHE = NULL;
	}
	
	XTE_INITIALIZED = 0;
}

// 解析返回语法列表
XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket)
{
	// 检查是否已初始化
	if ( !XTE_INITIALIZED ) {
		// 自动初始化（兼容未调用 xrtInit 的情况）
		if ( !xte_private_init() ) {
			return &XTE_LITE_ERROR_MALLOC;
		}
	}
	// 词法解析
	XTE_TokenList objToks = xteLexer(sText, iSize, XTE_IDENT_LIST, sBracket);
	// 语法解析
	return xteParseFromTokenList(objToks);
}



// 释放 XTE_LiteObject 对象
int xte_private_free_subtemplate(Dict_Key* pKey, xparray objAction, void* pArg)
{
	xrtPtrArrayUnit(objAction);
	return 0;
}
void xteParseFree(XTE_LiteObject objLite)
{
	if ( objLite != &XTE_LITE_ERROR_MALLOC ) {
		xrtDictWalk(&objLite->SubTemplates, (void*)xte_private_free_subtemplate, NULL);
		xrtDictUnit(&objLite->SubTemplates);
		xrtPtrArrayUnit(&objLite->Actions);
		xrtArrayUnit(&objLite->Tokens);
		xrtFree(objLite);
	}
}







// 完整版模板引擎语法转换套件
/*
	支持的写法（包含简单版支持的所有写法，这里只列出额外支持的）：
		{#if:xxx} ... {#elseif:xxx} ... {#else} ... {#end}			条件判断语句
		{#for:1:10:1} ... {#end}									计次循环语句（可代入 __index）
		{#foreach:xxx} ... {#end}									迭代循环语句（可代入 __index、__value）
	支持自定义语句：
		{#xxx:xxx...}												自定义单标签语句
		{#xxx:xxx...} ... {#end}									自定义语句块
	高级语法支持嵌套使用，自定义语句仅提供有限的支持
*/
#ifdef XTE_USE_FULL
	
	
	
	// 
	
	
	
#endif











/* -------------------- 控制语句辅助函数 (Phase 3) -------------------- */

// 找到配对的 end （支持嵌套）
// 返回 end 的索引（基于1），找不到返回 -1
static int xte_find_matching_end(xparray arrAction, int startIdx)
{
	int depth = 1;
	for ( int i = startIdx; i <= arrAction->Count; i++ ) {
		XTE_TokenItem tok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, i);
		if ( tok->Type == XTE_TK_IF || tok->Type == XTE_TK_FOR || tok->Type == XTE_TK_FOREACH ) {
			depth++;
		} else if ( tok->Type == XTE_TK_END ) {
			depth--;
			if ( depth == 0 ) {
				return i;
			}
		}
	}
	return -1;
}

// if 分支结构
typedef struct {
	int startIdx;       // 分支起始索引（不含 if/elseif/else 本身）
	int endIdx;         // 分支结束索引（不含 elseif/else/end）
	char* condition;    // 条件表达式（else 为 NULL）
	size_t condLen;     // 条件长度
} XTE_IfBranch;

// 解析 if 语句的所有分支
// branches: 输出数组，最多 XTE_PARAM_MAXCOUNT 个分支
// 返回: 分支数量，同时设置 endIdx 为整个 if 结构的 end 索引
static int xte_parse_if_branches(xparray arrAction, int ifIdx, XTE_IfBranch* branches, int* pEndIdx)
{
	int branchCount = 0;
	int depth = 1;
	
	// 第一个分支从if后开始
	XTE_TokenItem ifTok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, ifIdx);
	branches[branchCount].startIdx = ifIdx + 1;
	branches[branchCount].condition = ifTok->ParamText[0];
	branches[branchCount].condLen = ifTok->ParamSize[0];
	
	for ( int i = ifIdx + 1; i <= arrAction->Count; i++ ) {
		XTE_TokenItem tok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, i);
		
		if ( tok->Type == XTE_TK_IF || tok->Type == XTE_TK_FOR || tok->Type == XTE_TK_FOREACH ) {
			depth++;
		} else if ( depth == 1 && tok->Type == XTE_TK_ELSEIF ) {
			// 结束当前分支
			branches[branchCount].endIdx = i - 1;
			branchCount++;
			if ( branchCount >= XTE_PARAM_MAXCOUNT ) break;
			// 开始新分支
			branches[branchCount].startIdx = i + 1;
			branches[branchCount].condition = tok->ParamText[0];
			branches[branchCount].condLen = tok->ParamSize[0];
		} else if ( depth == 1 && tok->Type == XTE_TK_ELSE ) {
			// 结束当前分支
			branches[branchCount].endIdx = i - 1;
			branchCount++;
			if ( branchCount >= XTE_PARAM_MAXCOUNT ) break;
			// else 分支（无条件）
			branches[branchCount].startIdx = i + 1;
			branches[branchCount].condition = NULL;
			branches[branchCount].condLen = 0;
		} else if ( tok->Type == XTE_TK_END ) {
			depth--;
			if ( depth == 0 ) {
				// 结束最后一个分支
				branches[branchCount].endIdx = i - 1;
				branchCount++;
				*pEndIdx = i;
				return branchCount;
			}
		}
	}
	// 找不到 end
	*pEndIdx = -1;
	return 0;
}

// 执行 Action 列表的一段范围 [startIdx, endIdx]
static void xte_exec_range(xparray arrAction, int startIdx, int endIdx, 
                           XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, 
                           xdict tblInclude, xbuffer objBuf);

// 前向声明 xteMakeActions (内部版本，支持 break/continue)
static char* xteMakeActions_ex(xparray arrAction, XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, xdict tblInclude, size_t* pRetSize, int* pBreakFlag, int* pContinueFlag);



// foreach 表迭代回调结构
typedef struct {
	xbuffer Buf;
	xparray Action;
	XTE_LiteObject Template;
	xvalue RootVal;
	xdict Include;
	int Index;
	int* BreakFlag;      // 指向外层循环的 break 标志
	int* ContinueFlag;   // 指向外层循环的 continue 标志
	int MaxIterations;   // 最大迭代次数限制
} XTE_ForeachTableCtx;

// foreach 表迭代回调函数
// 注意: Dict_EachProc 传递的是存储区域地址，字典存储的是 xvalue 指针，所以 pVal 是 xvalue* 类型
static int xte_foreach_table_proc(Dict_Key* pKey, ptr pVal, void* pArg)
{
	xvalue* ppVal = (xvalue*)pVal;  // pVal 是指向 xvalue 指针的地址
	xvalue itemVal = *ppVal;        // 解引用获取真正的 xvalue
	if ( pKey == NULL || itemVal == NULL || pArg == NULL ) {
		return FALSE;
	}
	XTE_ForeachTableCtx* ctx = (XTE_ForeachTableCtx*)pArg;
	
	// 检查是否已经要求 break
	if ( ctx->BreakFlag && *ctx->BreakFlag ) {
		return TRUE;  // 停止遍历
	}
	
	// 检查迭代次数限制
	if ( ctx->Index >= ctx->MaxIterations ) {
		return TRUE;  // 超出最大迭代次数，停止遍历
	}
	
	xvalue loopEnv = xvoCreateTable();
	xvoTableSetInt(loopEnv, "__index__", 0, ctx->Index);
	xvoTableSetValue(loopEnv, "__value__", 0, itemVal, FALSE);
	xvoTableSetText(loopEnv, "__key__", 0, pKey->Key, pKey->KeyLen, FALSE);
	
	// 重置 continue 标志（每次迭代开始时清除）
	if ( ctx->ContinueFlag ) {
		*ctx->ContinueFlag = 0;
	}
	
	size_t loopSize = 0;
	char* loopResult = xteMakeActions_ex(ctx->Action, ctx->Template, itemVal, ctx->RootVal, loopEnv, ctx->Include, &loopSize, ctx->BreakFlag, ctx->ContinueFlag);
	if ( loopResult ) {
		xrtBufferAppend(ctx->Buf, loopResult, loopSize, XBUF_UTF8);
		xrtFree(loopResult);
	}
	xvoUnref(loopEnv);
	ctx->Index++;
	
	// 检查 break 标志，如果设置了则停止遍历
	if ( ctx->BreakFlag && *ctx->BreakFlag ) {
		return TRUE;
	}
	return FALSE;
}

// 根据 XTE_LiteObject 模板对象生成文档
// 注意: Dict_EachProc 传递的是存储区域地址，字典存储的是 xvalue 指针，所以 pVal 是 xvalue* 类型
int xte_private_Make_EachTableProc(Dict_Key* pKey, ptr pVal, void* pArg)
{
	xvalue* ppVal = (xvalue*)pVal;  // pVal 是指向 xvalue 指针的地址
	xvalue itemVal = *ppVal;        // 解引用获取真正的 xvalue
	struct {
		xbuffer Buf;
		XTE_TokenItem Tok;
		xvalue RootEnv;
		xvalue ENV;
		xparray Action;
		XTE_LiteObject Template;
		xdict Include;
	} *tblProcParam = pArg;
	size_t iSizeRet = 0;
	char* sEachPage = xteMakeActions(tblProcParam->Action, tblProcParam->Template, itemVal, tblProcParam->RootEnv, tblProcParam->ENV, tblProcParam->Include, &iSizeRet);
	if ( sEachPage ) {
		xrtBufferAppend(tblProcParam->Buf, sEachPage, iSizeRet, XBUF_UTF8);
		xrtFree(sEachPage);
	} else {
		xrtBufferAppend(tblProcParam->Buf, "(foreach table generation failed : ", 0, XBUF_UTF8);
		xrtBufferAppend(tblProcParam->Buf, tblProcParam->Tok->Text, tblProcParam->Tok->Size, XBUF_UTF8);
		xrtBufferAppend(tblProcParam->Buf, " [", 2, XBUF_UTF8);
		xrtBufferAppend(tblProcParam->Buf, pKey->Key, pKey->KeyLen, XBUF_UTF8);
		xrtBufferAppend(tblProcParam->Buf, "])", 2, XBUF_UTF8);
	}
	return FALSE;
}
char* xteMakeActions_ex(xparray arrAction, XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, xdict tblInclude, size_t* pRetSize, int* pBreakFlag, int* pContinueFlag)
{
	// 检查环境表
	if ( tblVal == NULL ) {
		return NULL;
	}
	// 申请自增长缓冲区
	xbuffer objBuf = xrtBufferCreate(0);
	if ( objBuf == NULL ) {
		return NULL;
	}
	if ( (tblVal->Type == XVO_DT_TABLE) || (tblVal->Type == XVO_DT_ARRAY) ) {
		// 表或数组（支持全功能）
		
		// 遍历模板 Action 生成内容
		for ( int i = 1; i <= arrAction->Count; i++ ) {
			// 检查 break/continue 标志，如果已设置则立即跳出
			if ( (pBreakFlag && *pBreakFlag) || (pContinueFlag && *pContinueFlag) ) {
				break;
			}
			XTE_TokenItem objTok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, i);
			if ( objTok->Type == XTE_TK_TEXT ) {
				// 文本节点
				xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
			} else if ( objTok->Type == XTE_TK_VAR ) {
				// 代入变量 - 转为字符串
				char* sTemp = xvoTableGetText(tblVal, objTok->Text, objTok->Size);
				if ( tblRoot && (sTemp == NULL) ) {
					sTemp = xvoTableGetText(tblRoot, objTok->Text, objTok->Size);
				}
				if ( sTemp == NULL ) {
					sTemp = xvoTableGetText(tblENV, objTok->Text, objTok->Size);
				}
				if ( sTemp ) {
					xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
				}
			} else if ( objTok->Type == XTE_TK_NUM ) {
				// 代入数字 - 支持格式化
				xvalue varNum = xvoTableGetValue(tblVal, objTok->Text, objTok->Size);
				if ( tblRoot && (varNum == &XVO_VALUE_NULL) ) {
					varNum = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varNum == &XVO_VALUE_NULL ) {
					varNum = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				str sFormat = (objTok->ParamCount > 0 && objTok->ParamText[0]) ? objTok->ParamText[0] : NULL;
				str sResult = NULL;
				if ( varNum == &XVO_VALUE_NULL || varNum->Type == XVO_DT_NULL ) {
					// NULL - 作为 0 处理
					sResult = xrtIntFormat(0, sFormat);
				} else if ( varNum->Type == XVO_DT_BOOL ) {
					// BOOL - TRUE=1, FALSE=0
					sResult = xrtIntFormat(varNum->vBool ? 1 : 0, sFormat);
				} else if ( varNum->Type == XVO_DT_INT ) {
					sResult = xrtIntFormat(varNum->vInt, sFormat);
				} else if ( varNum->Type == XVO_DT_FLOAT ) {
					sResult = xrtNumFormat(varNum->vFloat, sFormat);
				} else if ( varNum->Type == XVO_DT_TEXT && varNum->vText ) {
					// 字符串类型 - 使用 xrtParseNumSkipSpace 解析
					jnum_type_t numType = JNUM_NULL;
					jnum_value_t numValue;
					int parsed = xrtParseNumSkipSpace(varNum->vText, &numType, &numValue);
					if ( parsed > 0 ) {
						switch ( numType ) {
							case JNUM_INT: sResult = xrtIntFormat((int64)numValue.vint, sFormat); break;
							case JNUM_HEX: sResult = xrtIntFormat((int64)numValue.vhex, sFormat); break;
							case JNUM_LINT: sResult = xrtIntFormat(numValue.vlint, sFormat); break;
							case JNUM_LHEX: sResult = xrtIntFormat((int64)numValue.vlhex, sFormat); break;
							case JNUM_DOUBLE: sResult = xrtNumFormat(numValue.vdbl, sFormat); break;
							case JNUM_BOOL: sResult = xrtIntFormat(numValue.vbool ? 1 : 0, sFormat); break;
							default: sResult = xrtIntFormat(0, sFormat); break;
						}
					} else {
						sResult = xrtIntFormat(0, sFormat);
					}
				} else {
					sResult = xrtIntFormat(0, sFormat);
				}
				if ( sResult ) {
					xrtBufferAppend(objBuf, sResult, 0, XBUF_UTF8);
					xrtFree(sResult);
				}
			} else if ( objTok->Type == XTE_TK_TIME ) {
				// 代入时间 - 支持自定义格式
				xvalue varTime = xvoTableGetValue(tblVal, objTok->Text, objTok->Size);
				if ( tblRoot && (varTime == &XVO_VALUE_NULL) ) {
					varTime = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varTime == &XVO_VALUE_NULL ) {
					varTime = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				if ( varTime != &XVO_VALUE_NULL ) {
					xtime iTime = 0;
					if ( varTime->Type == XVO_DT_INT ) {
						iTime = varTime->vInt;
					} else if ( varTime->Type == XVO_DT_TEXT && varTime->vText ) {
						iTime = xrtStrToI64(varTime->vText);
					}
					str sResult = NULL;
					if ( objTok->ParamCount > 0 && objTok->ParamText[0] && objTok->ParamSize[0] > 0 ) {
						// 使用自定义格式
						sResult = xrtTimeFormat(iTime, objTok->ParamText[0]);
					} else {
						// 默认格式
						sResult = xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME);
					}
					if ( sResult ) {
						xrtBufferAppend(objBuf, sResult, 0, XBUF_UTF8);
						xrtFree(sResult);
					}
				}
			} else if ( objTok->Type == XTE_TK_BOOL ) {
				// 根据逻辑结果决定代入什么内容
				xvalue varBool = xvoTableGetValue(tblVal, objTok->Text, objTok->Size);
				if ( tblRoot && (varBool == &XVO_VALUE_NULL) ) {
					varBool = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varBool == &XVO_VALUE_NULL ) {
					varBool = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				int bRet = xvoGetBool(varBool);
				int idx = 1;
				if ( bRet ) {
					idx = 0;
				}
				if ( (objTok->ParamCount > idx) && objTok->ParamText[idx] ) {
					if ( objTok->ParamText[idx][0] == '=' ) {
						// 参数首字符为 = 则作为模板生成
						do {
							xparray arrSubAction = xrtDictGet(&objTemplate->SubTemplates, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							if ( arrSubAction == NULL ) {
								xrtBufferAppend(objBuf, "(cannot find sub template : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							// 根据参数决定是否代入子表
							xvalue tblParam = tblVal;
							for ( int j = 2; j < objTok->ParamCount; j++ ) {
								if ( tblParam->Type == XVO_DT_TABLE ) {
									tblParam = xvoTableGetValue(tblParam, objTok->ParamText[j], objTok->ParamSize[j]);
								} else if ( tblParam->Type == XVO_DT_ARRAY ) {
									tblParam = xvoArrayGetValue(tblParam, xrtStrToU32(objTok->ParamText[j]));
								} else {
									xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
									xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
									xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
									xrtBufferAppend(objBuf, objTok->ParamText[j], objTok->ParamSize[j], XBUF_UTF8);
									xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								}
							}
							// 根据模板生成页面
							xvalue tblRootEnv = tblParam == tblVal ? NULL : tblVal;
							size_t iSizeRet = 0;
							char* sSubPage = xteMakeActions(arrSubAction, objTemplate, tblParam, tblRootEnv, tblENV, tblInclude, &iSizeRet);
							if ( sSubPage == NULL ) {
								xrtBufferAppend(objBuf, "(sub template generation failed : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							xrtBufferAppend(objBuf, sSubPage, iSizeRet, XBUF_UTF8);
							xrtFree(sSubPage);
						} while (0);
					} else if ( objTok->ParamText[idx][0] == '@' ) {
						// 参数首字符为 @ 则作为函数调用参数
						/*
						while ( 1 ) {
							XTE_FUNC pFunc = xvoTableGetFunc(tblVal, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							if ( pFunc == NULL ) {
								pFunc = xvoTableGetFunc(tblENV, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							}
							if ( pFunc == NULL ) {
								xrtBufferAppend(objBuf, "(cannot find function : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							// 传递参数，调用函数，代入返回值，参数首字符为 @ 则引用变量
							xvalue varParam = xvoValueCreateText(objTok->ParamText[2], FALSE);
							xvalue varRet = pFunc(tblVal, varParam);
							char* sTemp = xvoValueGetText(varRet);
							if ( sTemp ) {
								xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
							}
							break;
						}
						*/
					} else if ( objTok->ParamText[idx][0] == '$' ) {
						// 参数首字符为 $ 则作为字符串获取内容
						char* sRet = xvoTableGetText(tblVal, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
						if ( sRet ) {
							xrtBufferAppend(objBuf, sRet, 0, XBUF_UTF8);
						} else {
							sRet = xvoTableGetText(tblENV, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							if ( sRet ) {
								xrtBufferAppend(objBuf, sRet, 0, XBUF_UTF8);
							}
						}
					} else if ( objTok->ParamText[idx][0] == ' ' ) {
						// 参数首字符为空格则跳过这个空格输出参数文本（忽略首空格）
						xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
					} else if ( objTok->ParamText[idx][0] == 0 ) {
						// 跳过参数
					} else {
						xrtBufferAppend(objBuf, objTok->ParamText[idx], objTok->ParamSize[idx], XBUF_UTF8);
					}
				}
			} else if ( objTok->Type == XTE_TK_ARR ) {
				// 遍历元素 表 or 数组，代入子模板
				do {
					xparray arrSubAction = xrtDictGet(&objTemplate->SubTemplates, objTok->Text, objTok->Size);
					if ( arrSubAction == NULL ) {
						xrtBufferAppend(objBuf, "(cannot find sub template : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					// 根据参数决定是否代入子表（没有参数就代入自己）
					xvalue tblParam = tblVal;
					for ( int j = 0; j < objTok->ParamCount; j++ ) {
						if ( tblParam->Type == XVO_DT_TABLE ) {
							tblParam = xvoTableGetValue(tblParam, objTok->ParamText[j], objTok->ParamSize[j]);
						} else if ( tblParam->Type == XVO_DT_ARRAY ) {
							tblParam = xvoArrayGetValue(tblParam, xrtStrToU32(objTok->ParamText[j]));
						} else {
							xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
							xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->ParamText[j], objTok->ParamSize[j], XBUF_UTF8);
							xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
						}
					}
					xvalue tblRootEnv = tblParam == tblVal ? NULL : tblVal;
					// 根据模板生成页面
					if ( tblParam->Type == XVO_DT_TABLE ) {
						// 遍历表
						struct {
							xbuffer Buf;
							XTE_TokenItem Tok;
							xvalue RootEnd;
							xvalue ENV;
							xparray Action;
							XTE_LiteObject Template;
							xdict Include;
						} tblProcParam = { objBuf, objTok, tblRootEnv, tblENV, arrSubAction, objTemplate, tblInclude };
						xrtDictWalk(tblParam->vTable, (void*)xte_private_Make_EachTableProc, &tblProcParam);
					} else if ( tblParam->Type == XVO_DT_ARRAY ) {
						// 遍历数组
						for ( int k = 0; k < tblParam->vArray->Count; k++ ) {
							xvalue tblEachItem = xvoArrayGetValue(tblParam, k);
							size_t iSizeRet = 0;
							char* sEachPage = xteMakeActions(arrSubAction, objTemplate, tblEachItem, tblRootEnv, tblENV, tblInclude, &iSizeRet);
							if ( sEachPage ) {
								xrtBufferAppend(objBuf, sEachPage, iSizeRet, XBUF_UTF8);
								xrtFree(sEachPage);
							} else {
								// 数组元素生成失败，记录错误但继续处理其他元素
								xrtBufferAppend(objBuf, "(foreach array generation failed : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								char sk[8];
								xrtI32ToStr(k, sk);
								xrtBufferAppend(objBuf, sk, 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								// 不再 break，继续处理其他元素
							}
						}
					} else {
						xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
					}
				} while (0);
			} else if ( objTok->Type == XTE_TK_PROC ) {
				// 调用函数（目前只支持传递一个参数）
				/*
				while ( 1 ) {
					XTE_FUNC pFunc = xvoTableGetFunc(tblVal, objTok->Text, objTok->Size);
					if ( tblRoot && (pFunc == NULL) ) {
						pFunc = xvoTableGetFunc(tblRoot, objTok->Text, objTok->Size);
					}
					if ( pFunc == NULL ) {
						pFunc = xvoTableGetFunc(tblENV, objTok->Text, objTok->Size);
					}
					if ( pFunc == NULL ) {
						xrtBufferAppend(objBuf, "(cannot find function : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					// 传递参数，调用函数，代入返回值
					xvalue varParam = xvoValueCreateText(objTok->ParamText[0], FALSE);
					xvalue varRet = pFunc(tblVal, varParam);
					char* sTemp = xvoValueGetText(varRet);
					if ( sTemp ) {
						xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
					}
					break;
				}
				*/
			} else if ( objTok->Type == XTE_TK_SUBTEMPLATE ) {
				// 代入子模板
				do {
					xparray arrSubAction = xrtDictGet(&objTemplate->SubTemplates, objTok->Text, objTok->Size);
					if ( arrSubAction == NULL ) {
						xrtBufferAppend(objBuf, "(cannot find sub template : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					// 根据参数决定是否代入子表
					xvalue tblParam = tblVal;
					for ( int j = 0; j < objTok->ParamCount; j++ ) {
						if ( tblParam->Type == XVO_DT_TABLE ) {
							tblParam = xvoTableGetValue(tblParam, objTok->ParamText[j], objTok->ParamSize[j]);
						} else if ( tblParam->Type == XVO_DT_ARRAY ) {
							tblParam = xvoArrayGetValue(tblParam, xrtStrToU32(objTok->ParamText[j]));
						} else {
							xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
							xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->ParamText[j], objTok->ParamSize[j], XBUF_UTF8);
							xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
						}
					}
					// 根据模板生成页面
					xvalue tblRootEnv = tblParam == tblVal ? NULL : tblVal;
					size_t iSizeRet = 0;
					char* sSubPage = xteMakeActions(arrSubAction, objTemplate, tblParam, tblRootEnv, tblENV, tblInclude, &iSizeRet);
					if ( sSubPage == NULL ) {
						xrtBufferAppend(objBuf, "(sub template generation failed : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					xrtBufferAppend(objBuf, sSubPage, iSizeRet, XBUF_UTF8);
					xrtFree(sSubPage);
				} while (0);
			} else if ( objTok->Type == XTE_TK_INCLUDE ) {
				// 引用外部模板
				do {
					XTE_LiteObject objIncTemplate = xrtDictGetPtr(tblInclude, objTok->ParamText[0], objTok->ParamSize[0]);
					if ( objIncTemplate == NULL ) {
						xrtBufferAppend(objBuf, "(cannot find file : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->ParamText[0], objTok->ParamSize[0], XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					// 根据参数决定是否代入子表
					xvalue tblParam = tblVal;
					for ( int j = 1; j < objTok->ParamCount; j++ ) {
						if ( tblParam->Type == XVO_DT_TABLE ) {
							tblParam = xvoTableGetValue(tblParam, objTok->ParamText[j], objTok->ParamSize[j]);
						} else if ( tblParam->Type == XVO_DT_ARRAY ) {
							tblParam = xvoArrayGetValue(tblParam, xrtStrToU32(objTok->ParamText[j]));
						} else {
							xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->ParamText[0], objTok->ParamSize[0], XBUF_UTF8);
							xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
							xrtBufferAppend(objBuf, objTok->ParamText[j], objTok->ParamSize[j], XBUF_UTF8);
							xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
						}
					}
					// 根据模板生成页面
					xvalue tblRootEnv = tblParam == tblVal ? NULL : tblVal;
					size_t iSizeRet = 0;
					char* sIncPage = xteMakeActions(&objIncTemplate->Actions, objIncTemplate, tblParam, tblRootEnv, tblENV, tblInclude, &iSizeRet);
					if ( sIncPage == NULL ) {
						xrtBufferAppend(objBuf, "(template generation failed : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->ParamText[0], objTok->ParamSize[0], XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
						break;
					}
					xrtBufferAppend(objBuf, sIncPage, iSizeRet, XBUF_UTF8);
					xrtFree(sIncPage);
				} while (0);
			} else if ( objTok->Type == XTE_TK_IF ) {
				// if/elseif/else 条件语句
				XTE_IfBranch branches[XTE_PARAM_MAXCOUNT];
				int endIdx = -1;
				int branchCount = xte_parse_if_branches(arrAction, i, branches, &endIdx);
				
				if ( branchCount > 0 && endIdx > 0 ) {
					// 遍历所有分支，找到第一个条件为真的分支执行
					for ( int b = 0; b < branchCount; b++ ) {
						int shouldExec = 0;
						if ( branches[b].condition == NULL ) {
							// else 分支，直接执行
							shouldExec = 1;
						} else {
							// 评估条件表达式
							shouldExec = xteExprEvalBool(branches[b].condition, branches[b].condLen, tblVal, tblRoot, tblENV);
						}
						if ( shouldExec ) {
							// 执行该分支的内容
							for ( int j = branches[b].startIdx; j <= branches[b].endIdx; j++ ) {
								XTE_TokenItem subTok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, j);
								// 检查 break/continue 标志，如果已设置则跳出分支执行
								if ( (pBreakFlag && *pBreakFlag) || (pContinueFlag && *pContinueFlag) ) {
									break;
								}
								// 递归调用主逻辑处理各种 Token
								if ( subTok->Type == XTE_TK_TEXT ) {
									xrtBufferAppend(objBuf, subTok->Text, subTok->Size, XBUF_UTF8);
								} else if ( subTok->Type == XTE_TK_IF || subTok->Type == XTE_TK_FOR || subTok->Type == XTE_TK_FOREACH ) {
									// 嵌套控制语句，构建临时 Action 列表并递归执行
									int nestedEnd = xte_find_matching_end(arrAction, j + 1);
									if ( nestedEnd > 0 && nestedEnd <= arrAction->Count ) {
										xparray_struct tempAction = { 0, 0, 0 };
										xrtPtrArrayInit(&tempAction);
										for ( int k = j; k <= nestedEnd; k++ ) {
											xrtPtrArrayAppend(&tempAction, xrtPtrArrayGet_Inline(arrAction, k));
										}
										size_t nestedSize = 0;
										char* nestedResult = xteMakeActions_ex(&tempAction, objTemplate, tblVal, tblRoot, tblENV, tblInclude, &nestedSize, pBreakFlag, pContinueFlag);
										if ( nestedResult ) {
											xrtBufferAppend(objBuf, nestedResult, nestedSize, XBUF_UTF8);
											xrtFree(nestedResult);
										}
										xrtPtrArrayUnit(&tempAction);
										j = nestedEnd; // 跳过嵌套块
									}
								} else if ( subTok->Type == XTE_TK_BREAK ) {
									// 直接处理 break
									if ( pBreakFlag ) {
										*pBreakFlag = 1;
									}
									break;
								} else if ( subTok->Type == XTE_TK_CONTINUE ) {
									// 直接处理 continue
									if ( pContinueFlag ) {
										*pContinueFlag = 1;
									}
									break;
								} else {
									// 其他 Token，构建单元素 Action 列表执行
									xparray_struct singleAction = { 0, 0, 0 };
									xrtPtrArrayInit(&singleAction);
									xrtPtrArrayAppend(&singleAction, subTok);
									size_t singleSize = 0;
									char* singleResult = xteMakeActions_ex(&singleAction, objTemplate, tblVal, tblRoot, tblENV, tblInclude, &singleSize, pBreakFlag, pContinueFlag);
									if ( singleResult ) {
										xrtBufferAppend(objBuf, singleResult, singleSize, XBUF_UTF8);
										xrtFree(singleResult);
									}
									xrtPtrArrayUnit(&singleAction);
								}
							}
							break; // 只执行第一个为真的分支
						}
					}
					i = endIdx; // 跳过整个 if 结构
				}
			} else if ( objTok->Type == XTE_TK_FOR ) {
				// for 计次循环：{#for:start:end:step}
				int endIdx = xte_find_matching_end(arrAction, i + 1);
				if ( endIdx > 0 && objTok->ParamCount >= 2 ) {
					int64 forStart = xrtStrToI64(objTok->ParamText[0]);
					int64 forEnd = xrtStrToI64(objTok->ParamText[1]);
					int64 forStep = (objTok->ParamCount >= 3) ? xrtStrToI64(objTok->ParamText[2]) : 1;
					
					// 修复步长为0的无限循环问题
					if ( forStep == 0 ) {
						forStep = (forStart <= forEnd) ? 1 : -1;
					}
					// 修复步长方向不匹配的问题（防止无限循环）
					if ( (forStart < forEnd && forStep < 0) || (forStart > forEnd && forStep > 0) ) {
						// 方向不匹配，跳过循环体
						i = endIdx;
						continue;
					}
					
					// 构建循环体 Action 列表
					xparray_struct loopAction = { 0, 0, 0 };
					xrtPtrArrayInit(&loopAction);
					for ( int k = i + 1; k < endIdx; k++ ) {
						xrtPtrArrayAppend(&loopAction, xrtPtrArrayGet_Inline(arrAction, k));
					}
					
					// 创建循环环境变量
					xvalue loopEnv = xvoCreateTable();
					
					// 循环内部的 break/continue 标志
					int loopBreak = 0, loopContinue = 0;
					
					// 执行循环
					int loopIterations = 0;  // 迭代计数器
					if ( forStep > 0 ) {
						for ( int64 idx = forStart; idx <= forEnd; idx += forStep ) {
							if ( ++loopIterations > XTE_LOOP_MAX_ITERATIONS ) break;  // 超出最大迭代次数
							loopContinue = 0;  // 每次迭代重置 continue
							xvoTableSetInt(loopEnv, "__index__", 0, idx);
							size_t loopSize = 0;
							char* loopResult = xteMakeActions_ex(&loopAction, objTemplate, tblVal, tblRoot, loopEnv, tblInclude, &loopSize, &loopBreak, &loopContinue);
							if ( loopResult ) {
								xrtBufferAppend(objBuf, loopResult, loopSize, XBUF_UTF8);
								xrtFree(loopResult);
							}
							if ( loopBreak ) break;  // break 跳出循环
							// continue 已在下一次迭代开始时重置
						}
					} else {
						for ( int64 idx = forStart; idx >= forEnd; idx += forStep ) {
							if ( ++loopIterations > XTE_LOOP_MAX_ITERATIONS ) break;  // 超出最大迭代次数
							loopContinue = 0;  // 每次迭代重置 continue
							xvoTableSetInt(loopEnv, "__index__", 0, idx);
							size_t loopSize = 0;
							char* loopResult = xteMakeActions_ex(&loopAction, objTemplate, tblVal, tblRoot, loopEnv, tblInclude, &loopSize, &loopBreak, &loopContinue);
							if ( loopResult ) {
								xrtBufferAppend(objBuf, loopResult, loopSize, XBUF_UTF8);
								xrtFree(loopResult);
							}
							if ( loopBreak ) break;  // break 跳出循环
						}
					}
					
					xvoUnref(loopEnv);
					xrtPtrArrayUnit(&loopAction);
					i = endIdx; // 跳过循环体
				}
			} else if ( objTok->Type == XTE_TK_FOREACH ) {
				// foreach 迭代循环：{#foreach:items} 或 {#foreach:items:alias}
				int endIdx = xte_find_matching_end(arrAction, i + 1);
				if ( endIdx > 0 && objTok->ParamCount >= 1 ) {
					// 获取迭代对象
					xvalue iterObj = xteResolvePath(objTok->ParamText[0], objTok->ParamSize[0], tblVal, tblRoot, tblENV);
					
					if ( iterObj != &XVO_VALUE_NULL && (iterObj->Type == XVO_DT_ARRAY || iterObj->Type == XVO_DT_TABLE) ) {
						// 构建循环体 Action 列表
						xparray_struct loopAction = { 0, 0, 0 };
						xrtPtrArrayInit(&loopAction);
						for ( int k = i + 1; k < endIdx; k++ ) {
							xrtPtrArrayAppend(&loopAction, xrtPtrArrayGet_Inline(arrAction, k));
						}
						
						if ( iterObj->Type == XVO_DT_ARRAY ) {
						// 迭代数组
						int loopBreak = 0, loopContinue = 0;
						int loopLimit = (iterObj->vArray->Count > XTE_LOOP_MAX_ITERATIONS) ? XTE_LOOP_MAX_ITERATIONS : iterObj->vArray->Count;
						for ( int idx = 0; idx < loopLimit; idx++ ) {
							loopContinue = 0;  // 每次迭代重置 continue
							xvalue itemVal = xvoArrayGetValue(iterObj, idx);
							// 创建循环环境
							xvalue loopEnv = xvoCreateTable();
							xvoTableSetInt(loopEnv, "__index__", 0, idx);
							xvoTableSetValue(loopEnv, "__value__", 0, itemVal, FALSE);
							
							size_t loopSize = 0;
							char* loopResult = xteMakeActions_ex(&loopAction, objTemplate, itemVal, tblVal, loopEnv, tblInclude, &loopSize, &loopBreak, &loopContinue);
							if ( loopResult ) {
								xrtBufferAppend(objBuf, loopResult, loopSize, XBUF_UTF8);
								xrtFree(loopResult);
							}
							xvoUnref(loopEnv);
							if ( loopBreak ) break;  // break 跳出循环
						}
					} else {
						// 迭代表（使用 xrtDictWalk）
						int loopBreak = 0, loopContinue = 0;
						XTE_ForeachTableCtx foreachCtx = { objBuf, &loopAction, objTemplate, tblVal, tblInclude, 0, &loopBreak, &loopContinue, XTE_LOOP_MAX_ITERATIONS };
						xrtDictWalk(iterObj->vTable, (void*)xte_foreach_table_proc, &foreachCtx);
					}
						
						xrtPtrArrayUnit(&loopAction);
					}
					i = endIdx; // 跳过循环体
				}
			} else if ( objTok->Type == XTE_TK_BREAK ) {
				// {#break} - 跳出循环
				if ( pBreakFlag ) {
					*pBreakFlag = 1;
				}
				break;  // 立即跳出当前 Action 遍历
			} else if ( objTok->Type == XTE_TK_CONTINUE ) {
				// {#continue} - 继续下一轮循环
				if ( pContinueFlag ) {
					*pContinueFlag = 1;
				}
				break;  // 跳出当前 Action 遍历，由上层循环检查 continue 标志
			} else if ( objTok->Type == XTE_TK_SCRIPT ) {
				// 执行脚本
				printf("\t★★★ Token Type [%d] : XTE_TK_SCRIPT (%d)\n", i, objTok->Type);
			} else {
				printf("\t★★★ Error : Unknown Token Type ID [%d] : %d\n", i, objTok->Type);
			}
		}
	} else {
		// 其他值（只支持代入 __self__ 值）
		
		// 遍历模板 Action 生成内容
		for ( int i = 1; i <= arrAction->Count; i++ ) {
			// 检查 break/continue 标志，如果已设置则立即跳出
			if ( (pBreakFlag && *pBreakFlag) || (pContinueFlag && *pContinueFlag) ) {
				break;
			}
			XTE_TokenItem objTok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, i);
			if ( objTok->Type == XTE_TK_TEXT ) {
				// 文本节点
				xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
			} else if ( objTok->Type == XTE_TK_VAR ) {
				// 代入变量 - 转为字符串
				// 注: 使用 xvoTableGetValue 检查键是否存在，避免 xvoGetText 返回空字符串导致搜索停止
				xvalue varVal = &XVO_VALUE_NULL;
				if ( objTok->Text && objTok->Size == 8 && memcmp(objTok->Text, "__self__", 8) == 0 ) {
					varVal = tblVal;
				}
				if ( tblRoot && (varVal == &XVO_VALUE_NULL) ) {
					varVal = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varVal == &XVO_VALUE_NULL ) {
					varVal = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				if ( varVal != &XVO_VALUE_NULL ) {
					char* sTemp = xvoGetText(varVal);
					if ( sTemp ) {
						xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
					}
				}
			} else if ( objTok->Type == XTE_TK_NUM ) {
				// 代入数字 - 支持格式化
				xvalue varNum = &XVO_VALUE_NULL;
				if ( objTok->Text && objTok->Size == 8 && memcmp(objTok->Text, "__self__", 8) == 0 ) {
					varNum = tblVal;
				}
				if ( tblRoot && (varNum == &XVO_VALUE_NULL) ) {
					varNum = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varNum == &XVO_VALUE_NULL ) {
					varNum = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				str sFormat = (objTok->ParamCount > 0 && objTok->ParamText[0]) ? objTok->ParamText[0] : NULL;
				str sResult = NULL;
				if ( varNum == &XVO_VALUE_NULL || varNum->Type == XVO_DT_NULL ) {
					// NULL - 作为 0 处理
					sResult = xrtIntFormat(0, sFormat);
				} else if ( varNum->Type == XVO_DT_BOOL ) {
					// BOOL - TRUE=1, FALSE=0
					sResult = xrtIntFormat(varNum->vBool ? 1 : 0, sFormat);
				} else if ( varNum->Type == XVO_DT_INT ) {
					sResult = xrtIntFormat(varNum->vInt, sFormat);
				} else if ( varNum->Type == XVO_DT_FLOAT ) {
					sResult = xrtNumFormat(varNum->vFloat, sFormat);
				} else if ( varNum->Type == XVO_DT_TEXT && varNum->vText ) {
					// 字符串类型 - 使用 xrtParseNumSkipSpace 解析
					jnum_type_t numType = JNUM_NULL;
					jnum_value_t numValue;
					int parsed = xrtParseNumSkipSpace(varNum->vText, &numType, &numValue);
					if ( parsed > 0 ) {
						switch ( numType ) {
							case JNUM_INT: sResult = xrtIntFormat((int64)numValue.vint, sFormat); break;
							case JNUM_HEX: sResult = xrtIntFormat((int64)numValue.vhex, sFormat); break;
							case JNUM_LINT: sResult = xrtIntFormat(numValue.vlint, sFormat); break;
							case JNUM_LHEX: sResult = xrtIntFormat((int64)numValue.vlhex, sFormat); break;
							case JNUM_DOUBLE: sResult = xrtNumFormat(numValue.vdbl, sFormat); break;
							case JNUM_BOOL: sResult = xrtIntFormat(numValue.vbool ? 1 : 0, sFormat); break;
							default: sResult = xrtIntFormat(0, sFormat); break;
						}
					} else {
						sResult = xrtIntFormat(0, sFormat);
					}
				} else {
					sResult = xrtIntFormat(0, sFormat);
				}
				if ( sResult ) {
					xrtBufferAppend(objBuf, sResult, 0, XBUF_UTF8);
					xrtFree(sResult);
				}
			} else if ( objTok->Type == XTE_TK_TIME ) {
				// 代入时间 - 支持自定义格式
				xvalue varTime = &XVO_VALUE_NULL;
				if ( objTok->Text && objTok->Size == 8 && memcmp(objTok->Text, "__self__", 8) == 0 ) {
					varTime = tblVal;
				}
				if ( tblRoot && (varTime == &XVO_VALUE_NULL) ) {
					varTime = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varTime == &XVO_VALUE_NULL ) {
					varTime = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				if ( varTime != &XVO_VALUE_NULL ) {
					xtime iTime = 0;
					if ( varTime->Type == XVO_DT_INT ) {
						iTime = varTime->vInt;
					} else if ( varTime->Type == XVO_DT_TEXT && varTime->vText ) {
						iTime = xrtStrToI64(varTime->vText);
					}
					str sResult = NULL;
					if ( objTok->ParamCount > 0 && objTok->ParamText[0] && objTok->ParamSize[0] > 0 ) {
						// 使用自定义格式
						sResult = xrtTimeFormat(iTime, objTok->ParamText[0]);
					} else {
						// 默认格式
						sResult = xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME);
					}
					if ( sResult ) {
						xrtBufferAppend(objBuf, sResult, 0, XBUF_UTF8);
						xrtFree(sResult);
					}
				}
			} else if ( objTok->Type == XTE_TK_BOOL ) {
				// 根据逻辑结果决定代入什么内容
				xvalue varBool = &XVO_VALUE_NULL;
				if ( objTok->Text && objTok->Size == 8 && memcmp(objTok->Text, "__self__", 8) == 0 ) {
					varBool = tblVal;
				}
				if ( tblRoot && (varBool == &XVO_VALUE_NULL) ) {
					varBool = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varBool == &XVO_VALUE_NULL ) {
					varBool = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				int bRet = xvoGetBool(varBool);
				int idx = 1;
				if ( bRet ) {
					idx = 0;
				}
				// 修复: 添加 ParamText[idx] 空指针检查
				if ( objTok->ParamCount > idx && objTok->ParamText[idx] ) {
					if ( objTok->ParamText[idx][0] == '=' ) {
						// 参数首字符为 = 则作为模板生成
						do {
							xparray arrSubAction = xrtDictGet(&objTemplate->SubTemplates, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							if ( arrSubAction == NULL ) {
								xrtBufferAppend(objBuf, "(cannot find sub template : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							// 根据模板生成页面
							size_t iSizeRet = 0;
							char* sSubPage = xteMakeActions(arrSubAction, objTemplate, tblVal, NULL, tblENV, tblInclude, &iSizeRet);
							if ( sSubPage == NULL ) {
								xrtBufferAppend(objBuf, "(sub template generation failed : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							xrtBufferAppend(objBuf, sSubPage, iSizeRet, XBUF_UTF8);
							xrtFree(sSubPage);
						} while (0);
					} else if ( objTok->ParamText[idx][0] == '@' ) {
						// 参数首字符为 @ 则作为函数调用参数
						/*
						while ( 1 ) {
							XTE_FUNC pFunc = NULL;
							if ( strcmp(&objTok->ParamText[idx][1], "__self__") == 0 ) {
								pFunc = xvoGetFunc(tblVal);
							}
							if ( pFunc == NULL ) {
								pFunc = xvoTableGetFunc(tblENV, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							}
							if ( pFunc == NULL ) {
								xrtBufferAppend(objBuf, "(cannot find function : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
							// 传递参数，调用函数，代入返回值
							xvalue varParam = xvoCreateText(objTok->ParamText[2], FALSE);
							xvalue varRet = pFunc(tblVal, varParam);
							char* sTemp = xvoGetText(varRet);
							if ( sTemp ) {
								xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
							}
							break;
						}
						*/
					} else if ( objTok->ParamText[idx][0] == '$' ) {
						// 参数首字符为 $ 则作为字符串获取内容
						char* sRet = xvoTableGetText(tblVal, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
						if ( sRet ) {
							xrtBufferAppend(objBuf, sRet, 0, XBUF_UTF8);
						} else {
							sRet = xvoTableGetText(tblENV, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1);
							if ( sRet ) {
								xrtBufferAppend(objBuf, sRet, 0, XBUF_UTF8);
							}
						}
					} else if ( objTok->ParamText[idx][0] == ' ' ) {
						// 参数首字符为空格则跳过这个空格输出参数文本（忽略首空格）
						xrtBufferAppend(objBuf, &objTok->ParamText[idx][1], objTok->ParamSize[idx] - 1, XBUF_UTF8);
					} else {
						xrtBufferAppend(objBuf, objTok->ParamText[idx], objTok->ParamSize[idx], XBUF_UTF8);
					}
				}
			}
		}
	}
	// 移出 objBuf->Buffer 返回，销毁 objBuf
	char* sRet = objBuf->Buffer;
	if ( pRetSize ) {
		*pRetSize = objBuf->Length;
	}
	objBuf->Buffer = NULL;
	xrtBufferDestroy(objBuf);
	return sRet;
}

// 公共接口：生成模板内容
char* xteMakeActions(xparray arrAction, XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, xdict tblInclude, size_t* pRetSize)
{
	return xteMakeActions_ex(arrAction, objTemplate, tblVal, tblRoot, tblENV, tblInclude, pRetSize, NULL, NULL);
}

char* xteMake(XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblENV, xdict tblInclude, size_t* pRetSize)
{
	return xteMakeActions(&objTemplate->Actions, objTemplate, tblVal, NULL, tblENV, tblInclude, pRetSize);
}



/* ================================ 表达式解析器 (Expression Parser) ================================ */

// 表达式错误描述
static const char* XTE_EXPR_ERROR_SUCCESS = "success";
static const char* XTE_EXPR_ERROR_MALLOC = "malloc failed";
static const char* XTE_EXPR_ERROR_UNEXPECTED_CHAR = "unexpected character";
static const char* XTE_EXPR_ERROR_UNTERMINATED_STRING = "unterminated string";
static const char* XTE_EXPR_ERROR_UNEXPECTED_TOKEN = "unexpected token";
static const char* XTE_EXPR_ERROR_EXPECTED_RPAREN = "expected ')'";
static const char* XTE_EXPR_ERROR_EXPECTED_OPERAND = "expected operand";

// 词法分析器状态
typedef struct {
	const char* Expr;			// 表达式字符串
	size_t Len;					// 表达式长度
	size_t Pos;					// 当前位置
	XTE_ExprToken_Struct Current;	// 当前 Token
	const char* Error;			// 错误描述
} XTE_ExprLexer_Struct, *XTE_ExprLexer;

// 跳过空白字符
static inline void xte_expr_skip_whitespace(XTE_ExprLexer lex)
{
	while ( lex->Pos < lex->Len ) {
		char c = lex->Expr[lex->Pos];
		if ( c == ' ' || c == '\t' || c == '\r' || c == '\n' ) {
			lex->Pos++;
		} else {
			break;
		}
	}
}

// 判断字符是否为标识符字符
static inline int xte_expr_is_ident_char(char c, int first)
{
	if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ) {
		return 1;
	}
	if ( !first && ((c >= '0' && c <= '9') || c == '.' || c == '[' || c == ']') ) {
		return 1;
	}
	return 0;
}

// 解析数字
static void xte_expr_parse_number(XTE_ExprLexer lex)
{
	size_t start = lex->Pos;
	int isFloat = 0;
	int hasDigit = 0;
	
	// 负号
	if ( lex->Pos < lex->Len && lex->Expr[lex->Pos] == '-' ) {
		lex->Pos++;
	}
	
	// 整数部分
	while ( lex->Pos < lex->Len && lex->Expr[lex->Pos] >= '0' && lex->Expr[lex->Pos] <= '9' ) {
		lex->Pos++;
		hasDigit = 1;
	}
	
	// 小数点
	if ( lex->Pos < lex->Len && lex->Expr[lex->Pos] == '.' ) {
		lex->Pos++;
		isFloat = 1;
		while ( lex->Pos < lex->Len && lex->Expr[lex->Pos] >= '0' && lex->Expr[lex->Pos] <= '9' ) {
			lex->Pos++;
			hasDigit = 1;
		}
	}
	
	// 科学计数法 (e/E)
	if ( hasDigit && lex->Pos < lex->Len && (lex->Expr[lex->Pos] == 'e' || lex->Expr[lex->Pos] == 'E') ) {
		lex->Pos++;
		isFloat = 1;
		if ( lex->Pos < lex->Len && (lex->Expr[lex->Pos] == '+' || lex->Expr[lex->Pos] == '-') ) {
			lex->Pos++;
		}
		while ( lex->Pos < lex->Len && lex->Expr[lex->Pos] >= '0' && lex->Expr[lex->Pos] <= '9' ) {
			lex->Pos++;
		}
	}
	
	lex->Current.Type = XTE_ETK_NUM;
	lex->Current.IsFloat = isFloat;
	lex->Current.Pos = start;
	
	if ( isFloat ) {
		lex->Current.Value.NumVal = strtod(lex->Expr + start, NULL);
	} else {
		lex->Current.Value.IntVal = atoll(lex->Expr + start);
	}
}

// 解析字符串
static void xte_expr_parse_string(XTE_ExprLexer lex)
{
	char quote = lex->Expr[lex->Pos];
	size_t start = lex->Pos + 1;
	lex->Pos++;
	
	while ( lex->Pos < lex->Len ) {
		char c = lex->Expr[lex->Pos];
		if ( c == quote ) {
			// 结束引号
			lex->Current.Type = XTE_ETK_STR;
			lex->Current.Value.Str.Ptr = lex->Expr + start;
			lex->Current.Value.Str.Len = lex->Pos - start;
			lex->Current.Pos = start - 1;
			lex->Pos++;
			return;
		}
		if ( c == '\\' && lex->Pos + 1 < lex->Len ) {
			// 转义字符，跳过下一个
			lex->Pos += 2;
		} else {
			lex->Pos++;
		}
	}
	
	// 未结束的字符串
	lex->Error = XTE_EXPR_ERROR_UNTERMINATED_STRING;
	lex->Current.Type = XTE_ETK_EOF;
}

// 解析标识符或关键字
static void xte_expr_parse_ident(XTE_ExprLexer lex)
{
	size_t start = lex->Pos;
	int bracketDepth = 0;
	
	// 解析标识符，支持 a.b.c 和 arr[0] 语法
	while ( lex->Pos < lex->Len ) {
		char c = lex->Expr[lex->Pos];
		if ( c == '[' ) {
			bracketDepth++;
			lex->Pos++;
		} else if ( c == ']' ) {
			if ( bracketDepth > 0 ) {
				bracketDepth--;
				lex->Pos++;
			} else {
				break;
			}
		} else if ( bracketDepth > 0 ) {
			// 括号内允许数字和空格
			lex->Pos++;
		} else if ( xte_expr_is_ident_char(c, lex->Pos == start) ) {
			lex->Pos++;
		} else {
			break;
		}
	}
	
	size_t len = lex->Pos - start;
	const char* str = lex->Expr + start;
	
	// 检查关键字
	if ( len == 3 && strncasecmp(str, "and", 3) == 0 ) {
		lex->Current.Type = XTE_ETK_OP_AND;
	} else if ( len == 2 && strncasecmp(str, "or", 2) == 0 ) {
		lex->Current.Type = XTE_ETK_OP_OR;
	} else if ( len == 3 && strncasecmp(str, "not", 3) == 0 ) {
		lex->Current.Type = XTE_ETK_OP_NOT;
	} else if ( len == 4 && strncasecmp(str, "true", 4) == 0 ) {
		lex->Current.Type = XTE_ETK_BOOL;
		lex->Current.Value.BoolVal = 1;
	} else if ( len == 5 && strncasecmp(str, "false", 5) == 0 ) {
		lex->Current.Type = XTE_ETK_BOOL;
		lex->Current.Value.BoolVal = 0;
	} else {
		// 普通标识符
		lex->Current.Type = XTE_ETK_IDENT;
		lex->Current.Value.Str.Ptr = str;
		lex->Current.Value.Str.Len = len;
	}
	lex->Current.Pos = start;
}

// 获取下一个 Token
static void xte_expr_next_token(XTE_ExprLexer lex)
{
	xte_expr_skip_whitespace(lex);
	
	if ( lex->Pos >= lex->Len ) {
		lex->Current.Type = XTE_ETK_EOF;
		lex->Current.Pos = lex->Pos;
		return;
	}
	
	char c = lex->Expr[lex->Pos];
	
	// 数字
	if ( (c >= '0' && c <= '9') || (c == '-' && lex->Pos + 1 < lex->Len && lex->Expr[lex->Pos + 1] >= '0' && lex->Expr[lex->Pos + 1] <= '9') ) {
		xte_expr_parse_number(lex);
		return;
	}
	
	// 字符串
	if ( c == '"' || c == '\'' ) {
		xte_expr_parse_string(lex);
		return;
	}
	
	// 标识符或关键字
	if ( xte_expr_is_ident_char(c, 1) ) {
		xte_expr_parse_ident(lex);
		return;
	}
	
	// 运算符和括号
	size_t start = lex->Pos;
	switch ( c ) {
		case '(':
			lex->Current.Type = XTE_ETK_LPAREN;
			lex->Pos++;
			break;
		case ')':
			lex->Current.Type = XTE_ETK_RPAREN;
			lex->Pos++;
			break;
		case '=':
			lex->Current.Type = XTE_ETK_OP_EQ;
			lex->Pos++;
			break;
		case '!':
			if ( lex->Pos + 1 < lex->Len && lex->Expr[lex->Pos + 1] == '=' ) {
				lex->Current.Type = XTE_ETK_OP_NE;
				lex->Pos += 2;
			} else {
				lex->Error = XTE_EXPR_ERROR_UNEXPECTED_CHAR;
				lex->Current.Type = XTE_ETK_EOF;
			}
			break;
		case '~':
			if ( lex->Pos + 1 < lex->Len && lex->Expr[lex->Pos + 1] == '=' ) {
				lex->Current.Type = XTE_ETK_OP_AE;
				lex->Pos += 2;
			} else {
				lex->Error = XTE_EXPR_ERROR_UNEXPECTED_CHAR;
				lex->Current.Type = XTE_ETK_EOF;
			}
			break;
		case '>':
			if ( lex->Pos + 1 < lex->Len && lex->Expr[lex->Pos + 1] == '=' ) {
				lex->Current.Type = XTE_ETK_OP_GE;
				lex->Pos += 2;
			} else {
				lex->Current.Type = XTE_ETK_OP_GT;
				lex->Pos++;
			}
			break;
		case '<':
			if ( lex->Pos + 1 < lex->Len && lex->Expr[lex->Pos + 1] == '=' ) {
				lex->Current.Type = XTE_ETK_OP_LE;
				lex->Pos += 2;
			} else {
				lex->Current.Type = XTE_ETK_OP_LT;
				lex->Pos++;
			}
			break;
		default:
			lex->Error = XTE_EXPR_ERROR_UNEXPECTED_CHAR;
			lex->Current.Type = XTE_ETK_EOF;
			break;
	}
	lex->Current.Pos = start;
}

// 初始化词法分析器
static void xte_expr_lexer_init(XTE_ExprLexer lex, const char* expr, size_t len)
{
	lex->Expr = expr;
	lex->Len = (len > 0) ? len : strlen(expr);
	lex->Pos = 0;
	lex->Error = NULL;
	memset(&lex->Current, 0, sizeof(XTE_ExprToken_Struct));
	xte_expr_next_token(lex);
}



/* -------------------- 语法解析器 (Parser) -------------------- */

// 运算符优先级（数字越大优先级越低）
static int xte_expr_get_precedence(uint32 op)
{
	switch ( op ) {
		case XTE_ETK_OP_OR:		return 1;
		case XTE_ETK_OP_AND:	return 2;
		case XTE_ETK_OP_EQ:
		case XTE_ETK_OP_NE:
		case XTE_ETK_OP_AE:		return 3;
		case XTE_ETK_OP_GT:
		case XTE_ETK_OP_LT:
		case XTE_ETK_OP_GE:
		case XTE_ETK_OP_LE:		return 4;
		default:				return 0;
	}
}

// 创建字面量节点
static XTE_ASTNode xte_ast_create_literal_int(int64 val)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_LITERAL;
		node->Data.Literal.LitType = XTE_LIT_INT;
		node->Data.Literal.Val.IntVal = val;
	}
	return node;
}

static XTE_ASTNode xte_ast_create_literal_float(double val)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_LITERAL;
		node->Data.Literal.LitType = XTE_LIT_FLOAT;
		node->Data.Literal.Val.NumVal = val;
	}
	return node;
}

static XTE_ASTNode xte_ast_create_literal_bool(int val)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_LITERAL;
		node->Data.Literal.LitType = XTE_LIT_BOOL;
		node->Data.Literal.Val.BoolVal = val;
	}
	return node;
}

static XTE_ASTNode xte_ast_create_literal_string(const char* str, size_t len)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_LITERAL;
		node->Data.Literal.LitType = XTE_LIT_STRING;
		node->Data.Literal.Val.Str.Ptr = xrtMalloc(len + 1);
		if ( node->Data.Literal.Val.Str.Ptr ) {
			memcpy(node->Data.Literal.Val.Str.Ptr, str, len);
			node->Data.Literal.Val.Str.Ptr[len] = '\0';
			node->Data.Literal.Val.Str.Len = len;
		} else {
			xrtFree(node);
			return NULL;
		}
	}
	return node;
}

// 创建变量节点
static XTE_ASTNode xte_ast_create_variable(const char* path, size_t len)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_VARIABLE;
		node->Data.Variable.Path = xrtMalloc(len + 1);
		if ( node->Data.Variable.Path ) {
			memcpy(node->Data.Variable.Path, path, len);
			node->Data.Variable.Path[len] = '\0';
			node->Data.Variable.PathLen = len;
		} else {
			xrtFree(node);
			return NULL;
		}
	}
	return node;
}

// 创建一元运算节点
static XTE_ASTNode xte_ast_create_unary(uint32 op, XTE_ASTNode operand)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_UNARY;
		node->Data.Unary.Op = op;
		node->Data.Unary.Operand = operand;
	}
	return node;
}

// 创建二元运算节点
static XTE_ASTNode xte_ast_create_binary(uint32 op, XTE_ASTNode left, XTE_ASTNode right)
{
	XTE_ASTNode node = xrtMalloc(sizeof(XTE_ASTNode_Struct));
	if ( node ) {
		node->Type = XTE_AST_BINARY;
		node->Data.Binary.Op = op;
		node->Data.Binary.Left = left;
		node->Data.Binary.Right = right;
	}
	return node;
}

// 释放 AST 节点
static void xte_ast_free_node(XTE_ASTNode node)
{
	if ( node == NULL ) return;
	
	switch ( node->Type ) {
		case XTE_AST_LITERAL:
			if ( node->Data.Literal.LitType == XTE_LIT_STRING ) {
				xrtFree(node->Data.Literal.Val.Str.Ptr);
			}
			break;
		case XTE_AST_VARIABLE:
			xrtFree(node->Data.Variable.Path);
			break;
		case XTE_AST_UNARY:
			xte_ast_free_node(node->Data.Unary.Operand);
			break;
		case XTE_AST_BINARY:
			xte_ast_free_node(node->Data.Binary.Left);
			xte_ast_free_node(node->Data.Binary.Right);
			break;
	}
	xrtFree(node);
}

// 前向声明
static XTE_ASTNode xte_expr_parse_expression(XTE_ExprLexer lex, int minPrec);

// 解析原子表达式（字面量、变量、括号、一元运算）
static XTE_ASTNode xte_expr_parse_atom(XTE_ExprLexer lex)
{
	XTE_ASTNode node = NULL;
	
	switch ( lex->Current.Type ) {
		case XTE_ETK_NUM:
			if ( lex->Current.IsFloat ) {
				node = xte_ast_create_literal_float(lex->Current.Value.NumVal);
			} else {
				node = xte_ast_create_literal_int(lex->Current.Value.IntVal);
			}
			xte_expr_next_token(lex);
			break;
		
		case XTE_ETK_STR:
			node = xte_ast_create_literal_string(lex->Current.Value.Str.Ptr, lex->Current.Value.Str.Len);
			xte_expr_next_token(lex);
			break;
		
		case XTE_ETK_BOOL:
			node = xte_ast_create_literal_bool(lex->Current.Value.BoolVal);
			xte_expr_next_token(lex);
			break;
		
		case XTE_ETK_IDENT:
			node = xte_ast_create_variable(lex->Current.Value.Str.Ptr, lex->Current.Value.Str.Len);
			xte_expr_next_token(lex);
			break;
		
		case XTE_ETK_LPAREN:
			xte_expr_next_token(lex);	// 跳过 '('
			node = xte_expr_parse_expression(lex, 0);
			if ( lex->Current.Type != XTE_ETK_RPAREN ) {
				lex->Error = XTE_EXPR_ERROR_EXPECTED_RPAREN;
				xte_ast_free_node(node);
				return NULL;
			}
			xte_expr_next_token(lex);	// 跳过 ')'
			break;
		
		case XTE_ETK_OP_NOT:
			xte_expr_next_token(lex);	// 跳过 'not'
			{
				XTE_ASTNode operand = xte_expr_parse_atom(lex);
				if ( operand == NULL ) {
					return NULL;
				}
				node = xte_ast_create_unary(XTE_ETK_OP_NOT, operand);
				if ( node == NULL ) {
					xte_ast_free_node(operand);
					return NULL;
				}
			}
			break;
		
		default:
			lex->Error = XTE_EXPR_ERROR_EXPECTED_OPERAND;
			return NULL;
	}
	
	return node;
}

// 表达式解析最大递归深度
#define XTE_EXPR_MAX_DEPTH 64

// 解析表达式（优先级爬升法）- 带深度限制
static XTE_ASTNode xte_expr_parse_expression_depth(XTE_ExprLexer lex, int minPrec, int depth)
{
	// 检查递归深度，防止栈溢出
	if ( depth > XTE_EXPR_MAX_DEPTH ) {
		lex->Error = "expression too deeply nested";
		return NULL;
	}
	
	XTE_ASTNode left = xte_expr_parse_atom(lex);
	if ( left == NULL ) {
		return NULL;
	}
	
	// 处理二元运算符
	while ( 1 ) {
		uint32 op = lex->Current.Type;
		int prec = xte_expr_get_precedence(op);
		
		if ( prec == 0 || prec < minPrec ) {
			break;
		}
		
		xte_expr_next_token(lex);	// 跳过运算符
		
		XTE_ASTNode right = xte_expr_parse_expression_depth(lex, prec + 1, depth + 1);
		if ( right == NULL ) {
			xte_ast_free_node(left);
			return NULL;
		}
		
		XTE_ASTNode newNode = xte_ast_create_binary(op, left, right);
		if ( newNode == NULL ) {
			xte_ast_free_node(left);
			xte_ast_free_node(right);
			return NULL;
		}
		left = newNode;
	}
	
	return left;
}

// 解析表达式（优先级爬升法）
static XTE_ASTNode xte_expr_parse_expression(XTE_ExprLexer lex, int minPrec)
{
	return xte_expr_parse_expression_depth(lex, minPrec, 0);
}

// 解析表达式字符串，返回解析结果
XTE_ExprResult xteExprParse(const char* expr, size_t len)
{
	XTE_ExprResult result = xrtMalloc(sizeof(XTE_ExprResult_Struct));
	if ( result == NULL ) {
		return NULL;
	}
	
	// 初始化词法分析器
	XTE_ExprLexer_Struct lex;
	xte_expr_lexer_init(&lex, expr, len);
	
	// 检查词法错误
	if ( lex.Error ) {
		result->Success = 0;
		result->ErrorDesc = lex.Error;
		result->ErrorPos = lex.Current.Pos;
		result->Root = NULL;
		return result;
	}
	
	// 解析表达式
	result->Root = xte_expr_parse_expression(&lex, 0);
	
	if ( lex.Error ) {
		result->Success = 0;
		result->ErrorDesc = lex.Error;
		result->ErrorPos = lex.Current.Pos;
		return result;
	}
	
	if ( result->Root == NULL ) {
		result->Success = 0;
		result->ErrorDesc = XTE_EXPR_ERROR_MALLOC;
		result->ErrorPos = 0;
		return result;
	}
	
	// 检查是否有多余内容
	if ( lex.Current.Type != XTE_ETK_EOF ) {
		result->Success = 0;
		result->ErrorDesc = XTE_EXPR_ERROR_UNEXPECTED_TOKEN;
		result->ErrorPos = lex.Current.Pos;
		return result;
	}
	
	result->Success = 1;
	result->ErrorDesc = XTE_EXPR_ERROR_SUCCESS;
	result->ErrorPos = 0;
	return result;
}

// 释放表达式解析结果
void xteExprFree(XTE_ExprResult result)
{
	if ( result ) {
		xte_ast_free_node(result->Root);
		xrtFree(result);
	}
}



/* -------------------- 求值器 (Evaluator) -------------------- */

// 将 xvalue 转换为布尔值
static int xte_value_to_bool(xvalue val)
{
	if ( val == NULL || val->Type == XVO_DT_NULL ) {
		return 0;
	}
	switch ( val->Type ) {
		case XVO_DT_BOOL:
			return val->vBool ? 1 : 0;
		case XVO_DT_INT:
			return val->vInt != 0;
		case XVO_DT_FLOAT:
			return val->vFloat != 0.0;
		case XVO_DT_TEXT:
			return val->Size > 0;
		case XVO_DT_TIME:
			return val->vTime != 0;
		case XVO_DT_ARRAY:
		case XVO_DT_LIST:
		case XVO_DT_TABLE:
			return 1;	// 集合类型始终为真
		default:
			return 0;
	}
}

// 比较两个 xvalue
static int xte_value_compare(xvalue left, xvalue right, uint32 op)
{
	// NULL 处理
	if ( left == NULL || left->Type == XVO_DT_NULL ) {
		if ( right == NULL || right->Type == XVO_DT_NULL ) {
			return (op == XTE_ETK_OP_EQ || op == XTE_ETK_OP_AE) ? 1 : 0;
		}
		return (op == XTE_ETK_OP_NE) ? 1 : 0;
	}
	if ( right == NULL || right->Type == XVO_DT_NULL ) {
		return (op == XTE_ETK_OP_NE) ? 1 : 0;
	}
	
	// 数字比较
	if ( (left->Type == XVO_DT_INT || left->Type == XVO_DT_FLOAT) &&
		 (right->Type == XVO_DT_INT || right->Type == XVO_DT_FLOAT) ) {
		double lv = (left->Type == XVO_DT_INT) ? (double)left->vInt : left->vFloat;
		double rv = (right->Type == XVO_DT_INT) ? (double)right->vInt : right->vFloat;
		
		switch ( op ) {
			case XTE_ETK_OP_EQ:	return lv == rv;
			case XTE_ETK_OP_NE:	return lv != rv;
			case XTE_ETK_OP_AE:	return xrtNumApprox(lv, rv);
			case XTE_ETK_OP_GT:	return lv > rv;
			case XTE_ETK_OP_LT:	return lv < rv;
			case XTE_ETK_OP_GE:	return lv >= rv;
			case XTE_ETK_OP_LE:	return lv <= rv;
		}
	}
	
	// 字符串比较
	if ( left->Type == XVO_DT_TEXT && right->Type == XVO_DT_TEXT ) {
		const char* ls = left->vText;
		const char* rs = right->vText;
		size_t llen = left->Size;
		size_t rlen = right->Size;
		
		switch ( op ) {
			case XTE_ETK_OP_EQ:
				return (llen == rlen) && (memcmp(ls, rs, llen) == 0);
			case XTE_ETK_OP_NE:
				return (llen != rlen) || (memcmp(ls, rs, llen) != 0);
			case XTE_ETK_OP_AE:
				return xrtStrApprox((str)ls, llen, (str)rs, rlen);
			case XTE_ETK_OP_GT:
				return strcmp(ls, rs) > 0;
			case XTE_ETK_OP_LT:
				return strcmp(ls, rs) < 0;
			case XTE_ETK_OP_GE:
				return strcmp(ls, rs) >= 0;
			case XTE_ETK_OP_LE:
				return strcmp(ls, rs) <= 0;
		}
	}
	
	// 时间比较
	if ( left->Type == XVO_DT_TIME && right->Type == XVO_DT_TIME ) {
		xtime lt = left->vTime;
		xtime rt = right->vTime;
		
		switch ( op ) {
			case XTE_ETK_OP_EQ:	return lt == rt;
			case XTE_ETK_OP_NE:	return lt != rt;
			case XTE_ETK_OP_AE:	return xrtTimeApprox(lt, rt);
			case XTE_ETK_OP_GT:	return lt > rt;
			case XTE_ETK_OP_LT:	return lt < rt;
			case XTE_ETK_OP_GE:	return lt >= rt;
			case XTE_ETK_OP_LE:	return lt <= rt;
		}
	}
	
	// 布尔比较
	if ( left->Type == XVO_DT_BOOL && right->Type == XVO_DT_BOOL ) {
		int lb = left->vBool ? 1 : 0;
		int rb = right->vBool ? 1 : 0;
		switch ( op ) {
			case XTE_ETK_OP_EQ:
			case XTE_ETK_OP_AE:	return lb == rb;
			case XTE_ETK_OP_NE:	return lb != rb;
			default:			return 0;
		}
	}
	
	// 类型不匹配
	return (op == XTE_ETK_OP_NE) ? 1 : 0;
}

// 求值 AST 节点
xvalue xteExprEval(XTE_ASTNode ast, xvalue tblVal, xvalue tblRoot, xvalue tblENV)
{
	if ( ast == NULL ) {
		return xvoCreateBool(0);
	}
	
	switch ( ast->Type ) {
		case XTE_AST_LITERAL:
			switch ( ast->Data.Literal.LitType ) {
				case XTE_LIT_INT:
					return xvoCreateInt(ast->Data.Literal.Val.IntVal);
				case XTE_LIT_FLOAT:
					return xvoCreateFloat(ast->Data.Literal.Val.NumVal);
				case XTE_LIT_BOOL:
					return xvoCreateBool(ast->Data.Literal.Val.BoolVal);
				case XTE_LIT_STRING:
					return xvoCreateText(ast->Data.Literal.Val.Str.Ptr, ast->Data.Literal.Val.Str.Len, FALSE);
				default:
					return xvoCreateBool(0);
			}
		
		case XTE_AST_VARIABLE:
			{
				xvalue resolved = xteResolvePath(
					ast->Data.Variable.Path,
					ast->Data.Variable.PathLen,
					tblVal, tblRoot, tblENV
				);
				// 返回副本（调用者负责 unref）
				if ( resolved && resolved->Type != XVO_DT_NULL ) {
					xvoAddRef(resolved);
					return resolved;
				}
				return xvoCreateNull();
			}
		
		case XTE_AST_UNARY:
			{
				xvalue operand = xteExprEval(ast->Data.Unary.Operand, tblVal, tblRoot, tblENV);
				int boolVal = xte_value_to_bool(operand);
				xvoUnref(operand);
				// not 运算
				if ( ast->Data.Unary.Op == XTE_ETK_OP_NOT ) {
					return xvoCreateBool(!boolVal);
				}
				return xvoCreateBool(0);
			}
		
		case XTE_AST_BINARY:
			{
				uint32 op = ast->Data.Binary.Op;
				
				// 逻辑运算：短路求值
				if ( op == XTE_ETK_OP_AND ) {
					xvalue left = xteExprEval(ast->Data.Binary.Left, tblVal, tblRoot, tblENV);
					if ( !xte_value_to_bool(left) ) {
						xvoUnref(left);
						return xvoCreateBool(0);
					}
					xvoUnref(left);
					xvalue right = xteExprEval(ast->Data.Binary.Right, tblVal, tblRoot, tblENV);
					int result = xte_value_to_bool(right);
					xvoUnref(right);
					return xvoCreateBool(result);
				}
				
				if ( op == XTE_ETK_OP_OR ) {
					xvalue left = xteExprEval(ast->Data.Binary.Left, tblVal, tblRoot, tblENV);
					if ( xte_value_to_bool(left) ) {
						xvoUnref(left);
						return xvoCreateBool(1);
					}
					xvoUnref(left);
					xvalue right = xteExprEval(ast->Data.Binary.Right, tblVal, tblRoot, tblENV);
					int result = xte_value_to_bool(right);
					xvoUnref(right);
					return xvoCreateBool(result);
				}
				
				// 比较运算
				xvalue left = xteExprEval(ast->Data.Binary.Left, tblVal, tblRoot, tblENV);
				xvalue right = xteExprEval(ast->Data.Binary.Right, tblVal, tblRoot, tblENV);
				int result = xte_value_compare(left, right, op);
				xvoUnref(left);
				xvoUnref(right);
				return xvoCreateBool(result);
			}
		
		default:
			return xvoCreateBool(0);
	}
}

// 便捷函数：解析并求值表达式，返回布尔结果（带 AST 缓存）
int xteExprEvalBool(const char* expr, size_t len, xvalue tblVal, xvalue tblRoot, xvalue tblENV)
{
	if ( expr == NULL ) {
		return 0;
	}
	if ( len == 0 ) {
		len = strlen(expr);
	}
	
	XTE_ExprResult result = NULL;
	
	// 尝试从缓存获取
	if ( XTE_EXPR_CACHE ) {
		result = (XTE_ExprResult)xrtDictGetPtr(XTE_EXPR_CACHE, (str)expr, len);
	}
	
	if ( result == NULL ) {
		// 缓存未命中，解析表达式
		result = xteExprParse(expr, len);
		if ( result == NULL || !result->Success ) {
			xteExprFree(result);
			return 0;
		}
		// 存入缓存
		if ( XTE_EXPR_CACHE ) {
			XTE_ExprResult oldResult = NULL;
			xrtDictSetPtr(XTE_EXPR_CACHE, (str)expr, len, result, (ptr*)&oldResult);
			// 如果有旧值（理论上不会发生），释放它
			if ( oldResult ) {
				xteExprFree(oldResult);
			}
		}
	}
	
	// 求值并返回布尔结果
	xvalue val = xteExprEval(result->Root, tblVal, tblRoot, tblENV);
	int boolVal = xte_value_to_bool(val);
	xvoUnref(val);
	
	// 如果未缓存，释放 result
	if ( !XTE_EXPR_CACHE ) {
		xteExprFree(result);
	}
	return boolVal;
}

