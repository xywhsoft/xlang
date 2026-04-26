


#define WBAPI



// 外置浏览器对象
//typedef int (*DeamonThreadProc)(LPWEBBOX_OBJECT objWebBox) WebBox_DeamonThreadProc;
typedef struct _WEBBOX_OBJECT
{
	DWORD PID;
	HWND hWin;
	HWND hWeb;
	HWND hParam1;
	HWND hParam2;
	HWND hParam3;
	HWND hParam4;
	void* pTable;
	//WebBox_DeamonThreadProc DeamonThreadProc;
} WEBBOX_OBJECT, *LPWEBBOX_OBJECT;



// 真实点击模式
#define WEBBOX_CLICK_RANDOM			0
#define WEBBOX_CLICK_LEFTBOTTOM		1
#define WEBBOX_CLICK_BOTTOM			2
#define WEBBOX_CLICK_RIGHTBOTTOM	3
#define WEBBOX_CLICK_LEFT			4
#define WEBBOX_CLICK_CENTER			5
#define WEBBOX_CLICK_RIGHT			6
#define WEBBOX_CLICK_LEFTTOP		7
#define WEBBOX_CLICK_TOP			8
#define WEBBOX_CLICK_RIGHTTOP		9

// 真实点击按键
#define WEBBOX_MB_LEFT				0
#define WEBBOX_MB_MIDDLE			0
#define WEBBOX_MB_RIGHT				0



// 外置浏览器初始化
WBAPI int WebBox_Setup(char* sDir);

// 外置浏览器创建
WBAPI LPWEBBOX_OBJECT WebBox_Create(void);

// 外置浏览器绑定
WBAPI LPWEBBOX_OBJECT WebBox_Bind(HWND hWin);

// 外置浏览器是否还在运行
WBAPI int WebBox_IsRunning(LPWEBBOX_OBJECT objWebBox);

// 显示
WBAPI int WebBox_Show(LPWEBBOX_OBJECT objWebBox);

// 隐藏
WBAPI int WebBox_Hide(LPWEBBOX_OBJECT objWebBox);

// 是否处于隐藏状态
WBAPI int WebBox_IsHide(LPWEBBOX_OBJECT objWebBox);

// 退出
WBAPI int WebBox_Close(LPWEBBOX_OBJECT objWebBox);

// 全部退出
WBAPI int WebBox_CloseAll(void);

// 释放 (释放控制权，使 WebBox_CloseAll 不会退出对应的浏览器)
WBAPI int WebBox_Release(LPWEBBOX_OBJECT objWebBox);

// 全部释放
WBAPI int WebBox_ReleaseAll(void);

// 设置窗口大小
WBAPI int WebBox_SetWinSize(LPWEBBOX_OBJECT objWebBox, int w, int h);

// 设置网页大小
WBAPI int WebBox_SetWebSize(LPWEBBOX_OBJECT objWebBox, int w, int h);

// 获取窗口大小
WBAPI int WebBox_GetWinSize(LPWEBBOX_OBJECT objWebBox, int *w, int *h);

// 获取网页大小
WBAPI int WebBox_GetWebSize(LPWEBBOX_OBJECT objWebBox, int *w, int *h);

// 获取网页页面大小
WBAPI int WebBox_GetPageSize(LPWEBBOX_OBJECT objWebBox, int *w, int *h);

// 获取窗口标题(需要释放内存)
WBAPI char* WebBox_GetTitle(LPWEBBOX_OBJECT objWebBox);

// 设置窗口标题
WBAPI int WebBox_SetTitle(LPWEBBOX_OBJECT objWebBox, char* sTitle);

// 获取网页标题(需要释放内存)
WBAPI char* WebBox_GetWebTitle(LPWEBBOX_OBJECT objWebBox);

// 加载网页
WBAPI int WebBox_LoadPage(LPWEBBOX_OBJECT objWebBox, char* url, char* ele, int flag, int ot);

// 等待网页加载
WBAPI int WebBox_WaitPage(LPWEBBOX_OBJECT objWebBox, char* ele, int flag, int ot);

// 保存网页
WBAPI int WebBox_SavePage(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 运行JS (需要释放内存)
WBAPI char* WebBox_RunJS(LPWEBBOX_OBJECT objWebBox, char* sScript, int bRtl);

// 运行JS [不需要返回值]
WBAPI int WebBox_RunJS_NoRet(LPWEBBOX_OBJECT objWebBox, char* sScript, int bRtl);

// 切换 Frame
WBAPI int WebBox_SetFrame(LPWEBBOX_OBJECT objWebBox, char* sMethod, char* sAttr, char* sValue, int iMode);

// 恢复 Frame
WBAPI int WebBox_ResetFrame(LPWEBBOX_OBJECT objWebBox);

// 获取 Cookies [全部]
WBAPI char* WebBox_GetCookies(LPWEBBOX_OBJECT objWebBox, char* sURL);

// 设置代理
WBAPI int WebBox_SetupProxy(LPWEBBOX_OBJECT objWebBox, char* sType, char* sAddr);

// 取消代理
WBAPI int WebBox_ClearProxy(LPWEBBOX_OBJECT objWebBox);

// 滚动页面到指定位置
WBAPI int WebBox_SetScroll(LPWEBBOX_OBJECT objWebBox, int x, int y);

// 获取滚动条位置
WBAPI int WebBox_GetScroll(LPWEBBOX_OBJECT objWebBox, int *x, int *y);

// 显示开发者工具
WBAPI int WebBox_ShowDevTool(LPWEBBOX_OBJECT objWebBox);

// 获取选项 [仅运行时] (9、10号选项需要释放内存，返回字符串指针)
WBAPI int WebBox_GetOpt(LPWEBBOX_OBJECT objWebBox, int id);
#define WebBox_GetOpt_Str (char*)WebBox_GetOpt

// 修改选项 [仅运行时]
WBAPI int WebBox_SetOpt(LPWEBBOX_OBJECT objWebBox, int id, int val);
#define WebBox_SetOpt_Str(objWebBox, id, val) WebBox_SetOpt(objWebBox, id, (int)val)

// 设置工具栏显示状态
#define WebBox_ShowTool(objWebBox, bShow) WebBox_SetOpt(objWebBox, 1, bShow)

// 跳到链接
WBAPI int WebBox_GoURL(LPWEBBOX_OBJECT objWebBox, char* sURL);

// 后退
WBAPI int WebBox_GoBack(LPWEBBOX_OBJECT objWebBox);

// 前进
WBAPI int WebBox_GoForward(LPWEBBOX_OBJECT objWebBox);

// 打开主页
WBAPI int WebBox_GoHome(LPWEBBOX_OBJECT objWebBox);

// 刷新
WBAPI int WebBox_Refresh(LPWEBBOX_OBJECT objWebBox);

// 停止
WBAPI int WebBox_Stop(LPWEBBOX_OBJECT objWebBox);

// 获取网页源代码 (需要释放内存)
WBAPI char* WebBox_GetHTML(LPWEBBOX_OBJECT objWebBox);

// 获取网页链接 (需要释放内存)
WBAPI char* WebBox_GetURL(LPWEBBOX_OBJECT objWebBox);

// 获取本地链接 (需要释放内存)
WBAPI char* WebBox_GetLocationURL(LPWEBBOX_OBJECT objWebBox);

// 获取本地Name (需要释放内存)
WBAPI char* WebBox_GetLocationName(LPWEBBOX_OBJECT objWebBox);

// 完整截图
WBAPI int WebBox_ScreenShotFull(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 区域截图
WBAPI int WebBox_ScreenShot(LPWEBBOX_OBJECT objWebBox, int x, int y, int w, int h, char* sPath);

// 获取变量
WBAPI char* WebBox_GetVar(LPWEBBOX_OBJECT objWebBox, char* k);

// 设置变量
WBAPI int WebBox_SetVar(LPWEBBOX_OBJECT objWebBox, char* k, char* v);

// 获取表
WBAPI void* WebBox_GetTable(LPWEBBOX_OBJECT objWebBox);

// HTTP GET
WBAPI char* WebBox_HttpGet(LPWEBBOX_OBJECT objWebBox, char* sURL);

// HTTP POST
WBAPI char* WebBox_HttpPost(LPWEBBOX_OBJECT objWebBox, char* sURL, char* sForm);

// HTTP GET (二进制)
WBAPI char* WebBox_HttpGetBinary(LPWEBBOX_OBJECT objWebBox, char* sURL);

// HTTP POST (二进制)
WBAPI char* WebBox_HttpPostBinary(LPWEBBOX_OBJECT objWebBox, char* sURL, char* sForm);

// HTTP GET (保存到文件)
WBAPI size_t WebBox_HttpGetFile(LPWEBBOX_OBJECT objWebBox, char* sURL, char* sFile);

// HTTP POST (保存到文件)
WBAPI size_t WebBox_HttpPostFile(LPWEBBOX_OBJECT objWebBox, char* sURL, char* sForm, char* sFile);

// MXPath 获取 JavaScript 代码
WBAPI int WebBox_MXPath_GenPath(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sRoot, char** sJS, char** sVar);

// MXPath 获取 JavaScript 代码 (使用自定义的变量名，编译后续处理和替换)
WBAPI int WebBox_MXPath_GenPath_UseVar(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sRoot, char* sVar, char** sJS);

// MXPath 获取元素调用的 JavaScript 代码 (如果指定了 sJS 或 sVar 需要释放内存)
// sPath:MXPath、sCode:代码模板,使用{$var}代入元素、sVal:替换值,使用{$val}代入、sVal2:替换值,使用{$val2}代入
// bOnce:为TRUE时不管找到多少元素,只操作第一个元素、bEleVar:为TRUE时返回的JS代码变量名仍为{$var},可自行替换
WBAPI int WebBox_MXPath_GenEleJS(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sCode, char* sVal, char* sVal2, int bOnce, int bEleVar, char** sJS, char** sVar);

// 基于指定元素运行JS (如果指定了 sRet 需要释放内存)
WBAPI int WebBox_Element_RunJS(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sCode, char* sVal, char* sVal2, int bOnce, char** sRet);

// 判断元素是否存在 (假定 MXPath 合法)
WBAPI int WebBox_Element_IsExists(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 获取元素的数量 (假定 MXPath 合法)
WBAPI int WebBox_Element_GetCount(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 点击元素
WBAPI int WebBox_Element_Click(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素的 Value
WBAPI int WebBox_Element_SetValue(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sVal);

// 获取元素的 Value (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetValue(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素的 outerHTML
WBAPI int WebBox_Element_SetOuterHtml(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sVal);

// 获取元素的 outerHTML (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetOuterHtml(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素的 innerHTML
WBAPI int WebBox_Element_SetInnerHtml(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sVal);

// 获取元素的 innerHTML (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetInnerHtml(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素的文本
WBAPI int WebBox_Element_SetText(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sVal);

// 获取元素的文本 (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetText(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 获取元素的 outerText (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetOuterText(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 获取元素的 innerText (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetInnerText(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素的选择状态
WBAPI int WebBox_Element_SetCheck(LPWEBBOX_OBJECT objWebBox, char* sPath, int bVal);

// 获取元素的选择状态 (假定 MXPath 合法)
WBAPI int WebBox_Element_GetCheck(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 按索引修改元素的选择状态
WBAPI int WebBox_Element_SetSelect(LPWEBBOX_OBJECT objWebBox, char* sPath, int nIdx);

// 按索引修改元素的选择状态
WBAPI int WebBox_Element_SetSelectText(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sText);

// 按Value修改元素的选择状态
#define WebBox_Element_SetSelectValue WebBox_Element_SetValue

// 获取列表元素的选择Value
#define WebBox_Element_GetSelectValue WebBox_Element_GetValue

// 删除元素
WBAPI int WebBox_Element_Remove(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 修改元素属性
WBAPI int WebBox_Element_SetAttribute(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sAttr, char* sVal);

// 获取元素属性 (假定 MXPath 合法) (需要释放内存)
WBAPI char* WebBox_Element_GetAttribute(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sAttr);

// 获取元素位置
WBAPI int WebBox_Element_GetRect(LPWEBBOX_OBJECT objWebBox, char* sPath, int* left, int* top, int* right, int* bottom);

// 显示元素
WBAPI int WebBox_Element_Show(LPWEBBOX_OBJECT objWebBox, char* sPath);

// 点击元素（真实鼠标点击）
WBAPI int WebBox_Element_RealClick(LPWEBBOX_OBJECT objWebBox, char* sPath, int iBtn, int mode, int px, int py);

// 设置元素文本（真实键盘输入）
WBAPI int WebBox_Element_RealPaste(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sText);

// 输入元素文本（真实键盘输入）
WBAPI int WebBox_Element_RealInput(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sText);

// 元素截图
WBAPI int WebBox_Element_ScreenShot(LPWEBBOX_OBJECT objWebBox, char* sPath, char* sFile);


