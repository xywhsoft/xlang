


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
			if ( sText[i] == sBracket[0] ) {
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
						// 无法识别的 Token 类型
						XTE_OnLexerError(3);
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
			if ( (sText[i] == sBracket[0]) && (sText[i+1] == '#') && (sText[i+2] == 'e') && (sText[i+3] == 'n') && (sText[i+4] == 'd') && (sText[i+5] == sBracket[1]) ) {
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
						// 更新参数数据
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
	NULL,
	{
		NULL,
		0,
		0,
		0
	},
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
	// 保存 objToks 的 Tokens，而 objToks 本体将被释放
	objRet->Tokens = &objToks->Tokens;
	if ( objToks != &XTE_LEXER_ERROR_MALLOC ) {
		xrtFree(objToks);
	}
	// 初始化数据结构
	xrtPtrArrayInit(&objRet->Actions);
	xrtDictInit(&objRet->SubTemplates, sizeof(xparray_struct));
	// 遍历 Token 列表，添加到 Actions 列表
	XTE_TokenItem objRefTok = NULL;
	xparray objCurTemplate = NULL;
	for ( int i = 1; i <= objToks->Tokens.Count; i++ ) {
		XTE_TokenItem objTok = xrtArrayGet_Inline(&objToks->Tokens, i);
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
				objCurTemplate = NULL;
			} else {
				XTE_OnParseError(10);
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
	// 返回 Token 列表
	objRet->Success = -1;
	objRet->ErrorCode = 0;
	objRet->ErrorDesc = XTE_ERROR_DESC[0];
	return objRet;
}



// 解析返回语法列表
xarray XTE_LITE_IDENT_LIST = NULL;
XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket)
{
	// 创建可解析的标识符列表
	if ( XTE_LITE_IDENT_LIST == NULL ) {
		XTE_LITE_IDENT_LIST = xteCreateIdentList();
		if ( XTE_LITE_IDENT_LIST == NULL ) {
			return &XTE_LITE_ERROR_MALLOC;
		}
		xteAddIdentToList(XTE_LITE_IDENT_LIST, "end"	, 3, XTE_TK_END		, XTE_IDTPE_DEFAULT	, 0, 0);
		xteAddIdentToList(XTE_LITE_IDENT_LIST, "include", 7, XTE_TK_INCLUDE	, XTE_IDTPE_DEFAULT	, 1, 1);
		xteAddIdentToList(XTE_LITE_IDENT_LIST, "define"	, 6, XTE_TK_DEFINE	, XTE_IDTPE_DEFAULT	, 1, 1);
		xteAddIdentToList(XTE_LITE_IDENT_LIST, "script"	, 6, XTE_TK_SCRIPT	, XTE_IDTPE_BLOCK	, 1, 1);
	}
	// 词法解析
	XTE_TokenList objToks = xteLexer(sText, iSize, XTE_LITE_IDENT_LIST, sBracket);
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
		xte_private_free_tokenlist(objLite->Tokens);
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











// 根据 XTE_LiteObject 模板对象生成文档
int xte_private_Make_EachTableProc(Dict_Key* pKey, xvalue* ppVal, void* pArg)
{
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
	char* sEachPage = xteMakeActions(tblProcParam->Action, tblProcParam->Template, *ppVal, tblProcParam->RootEnv, tblProcParam->ENV, tblProcParam->Include, &iSizeRet);
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
char* xteMakeActions(xparray arrAction, XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblRoot, xvalue tblENV, xdict tblInclude, size_t* pRetSize)
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
				// 代入数字 - 转为数字（目前与转为字符串采用相同方式处理）
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
			} else if ( objTok->Type == XTE_TK_TIME ) {
				// 代入时间
				xvalue varTime = xvoTableGetValue(tblVal, objTok->Text, objTok->Size);
				if ( tblRoot && (varTime == &XVO_VALUE_NULL) ) {
					varTime = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varTime == &XVO_VALUE_NULL ) {
					varTime = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				if ( varTime != &XVO_VALUE_NULL ) {
					if ( varTime->Type == XVO_DT_INT ) {
						str sTime = xrtTimeToStr(varTime->vInt, XRT_TIME_FORMAT_DATETIME);
						xrtBufferAppend(objBuf, sTime, 0, XBUF_UTF8);
						xrtFree(sTime);
					} else if ( varTime->Type == XVO_DT_TEXT ) {
						if ( varTime->vText ) {
							xrtBufferAppend(objBuf, varTime->vText, 0, XBUF_UTF8);
						}
					} else {
						// 不支持其他类型转换为时间
						xrtBufferAppend(objBuf, "(conv time error : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
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
						while ( 1 ) {
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
							break;
						}
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
				while ( 1 ) {
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
								xrtBufferAppend(objBuf, "(foreach array generation failed : ", 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
								xrtBufferAppend(objBuf, " [", 2, XBUF_UTF8);
								char sk[8];
								xrtI32ToStr(k, sk);
								xrtBufferAppend(objBuf, sk, 0, XBUF_UTF8);
								xrtBufferAppend(objBuf, "])", 2, XBUF_UTF8);
								break;
							}
						}
					} else {
						xrtBufferAppend(objBuf, "(param type error : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
					}
					break;
				}
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
				while ( 1 ) {
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
					break;
				}
			} else if ( objTok->Type == XTE_TK_INCLUDE ) {
				// 引用外部模板
				while ( 1 ) {
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
					break;
				}
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
			XTE_TokenItem objTok = (XTE_TokenItem)xrtPtrArrayGet_Inline(arrAction, i);
			if ( objTok->Type == XTE_TK_TEXT ) {
				// 文本节点
				xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
			} else if ( objTok->Type == XTE_TK_VAR ) {
				// 代入变量 - 转为字符串
				char* sTemp = NULL;
				if ( strcmp(objTok->Text, "__self__") == 0 ) {
					sTemp = xvoGetText(tblVal);
				}
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
				// 代入数字 - 转为数字（目前与转为字符串采用相同方式处理）
				char* sTemp = NULL;
				if ( strcmp(objTok->Text, "__self__") == 0 ) {
					sTemp = xvoGetText(tblVal);
				}
				if ( tblRoot && (sTemp == NULL) ) {
					sTemp = xvoTableGetText(tblRoot, objTok->Text, objTok->Size);
				}
				if ( sTemp == NULL ) {
					sTemp = xvoTableGetText(tblENV, objTok->Text, objTok->Size);
				}
				if ( sTemp ) {
					xrtBufferAppend(objBuf, sTemp, 0, XBUF_UTF8);
				}
			} else if ( objTok->Type == XTE_TK_TIME ) {
				// 代入时间
				xvalue varTime = &XVO_VALUE_NULL;
				if ( strcmp(objTok->Text, "__self__") == 0 ) {
					varTime = tblVal;
				}
				if ( tblRoot && (varTime == &XVO_VALUE_NULL) ) {
					varTime = xvoTableGetValue(tblRoot, objTok->Text, objTok->Size);
				}
				if ( varTime == &XVO_VALUE_NULL ) {
					varTime = xvoTableGetValue(tblENV, objTok->Text, objTok->Size);
				}
				if ( varTime != &XVO_VALUE_NULL ) {
					if ( varTime->Type == XVO_DT_INT ) {
						str sTime = xrtTimeToStr(varTime->vInt, XRT_TIME_FORMAT_DATETIME);
						xrtBufferAppend(objBuf, sTime, 0, XBUF_UTF8);
						xrtFree(sTime);
					} else if ( varTime->Type == XVO_DT_TEXT ) {
						if ( varTime->vText ) {
							xrtBufferAppend(objBuf, varTime->vText, 0, XBUF_UTF8);
						}
					} else {
						// 不支持其他类型转换为时间
						xrtBufferAppend(objBuf, "(conv time error : ", 0, XBUF_UTF8);
						xrtBufferAppend(objBuf, objTok->Text, objTok->Size, XBUF_UTF8);
						xrtBufferAppend(objBuf, ")", 1, XBUF_UTF8);
					}
				}
			} else if ( objTok->Type == XTE_TK_BOOL ) {
				// 根据逻辑结果决定代入什么内容
				xvalue varBool = &XVO_VALUE_NULL;
				if ( strcmp(objTok->Text, "__self__") == 0 ) {
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
				if ( objTok->ParamCount > idx ) {
					if ( objTok->ParamText[idx][0] == '=' ) {
						// 参数首字符为 = 则作为模板生成
						while ( 1 ) {
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
							break;
						}
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
char* xteMake(XTE_LiteObject objTemplate, xvalue tblVal, xvalue tblENV, xdict tblInclude, size_t* pRetSize)
{
	return xteMakeActions(&objTemplate->Actions, objTemplate, tblVal, NULL, tblENV, tblInclude, pRetSize);
}


