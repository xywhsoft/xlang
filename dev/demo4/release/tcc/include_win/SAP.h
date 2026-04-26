


#define SAPAPI



// 类型定义
typedef void* SAP_GuiApplication;
typedef void* SAP_GuiConnection;
typedef void* SAP_GuiSession;
typedef void* SAP_Object;





// 创建 SAP_GuiApplication 对象
SAPAPI SAP_GuiApplication SAP_GUI_Create();

// 获取 SAP_GuiApplication 对象
SAPAPI SAP_GuiApplication SAP_GUI_GetObject();

// 连接到服务器
SAPAPI SAP_GuiConnection SAP_GUI_OpenConnectW(SAP_GuiApplication objSAP, wchar_t* sName);
SAPAPI SAP_GuiConnection SAP_GUI_OpenConnectA(SAP_GuiApplication objSAP, char* sName);



// 关闭连接
SAPAPI int SAP_Conn_Close(SAP_GuiConnection objConn);



// 发送命令
SAPAPI int SAP_Session_SendCMDW(SAP_GuiSession objSession, wchar_t* sCMD);
SAPAPI int SAP_Session_SendCMDA(SAP_GuiSession objSession, char* sCMD);



// 获取 Children
SAPAPI SAP_Object SAP_Children(SAP_Object objSAP, int iIdx);

// 判断元素是否存在
SAPAPI int SAP_ExistsW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_ExistsA(SAP_Object objSAP, char* sPath);

// 修改元素文本
SAPAPI int SAP_SetTextW(SAP_Object objSAP, wchar_t* sPath, wchar_t* sText);
SAPAPI int SAP_SetTextA(SAP_Object objSAP, char* sPath, char* sText);

// 获取元素文本
SAPAPI wchar_t* SAP_GetTextW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI char* SAP_GetTextA(SAP_Object objSAP, char* sPath);

// 点击元素
SAPAPI int SAP_ClickW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_ClickA(SAP_Object objSAP, char* sPath);

// 选择元素
SAPAPI int SAP_SelectW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_SelectA(SAP_Object objSAP, char* sPath);

// 设置元素选择状态
SAPAPI int SAP_SetCheckW(SAP_Object objSAP, wchar_t* sPath, int bCheck);
SAPAPI int SAP_SetCheckA(SAP_Object objSAP, char* sPath, int bCheck);

// 获取元素选择状态
SAPAPI int SAP_GetCheckW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_GetCheckA(SAP_Object objSAP, char* sPath);

// 发送按键
SAPAPI int SAP_SendKeyW(SAP_Object objSAP, wchar_t* sPath, int iKey);
SAPAPI int SAP_SendKeyA(SAP_Object objSAP, char* sPath, int iKey);

// 设置焦点
SAPAPI int SAP_SetFocusW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_SetFocusA(SAP_Object objSAP, char* sPath);

// 设置光标位置
SAPAPI int SAP_SetCaretPosW(SAP_Object objSAP, wchar_t* sPath, int iPos);
SAPAPI int SAP_SetCaretPosA(SAP_Object objSAP, char* sPath, int iPos);

// 关闭窗口
SAPAPI int SAP_CloseWindowW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_CloseWindowA(SAP_Object objSAP, char* sPath);

// 获取表格行数
SAPAPI int SAP_GridView_RowCountW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_GridView_RowCountA(SAP_Object objSAP, char* sPath);

// 获取表格列数
SAPAPI int SAP_GridView_ColCountW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_GridView_ColCountA(SAP_Object objSAP, char* sPath);

// 获取表格已显示行数
SAPAPI int SAP_GridView_VisibleRowCountW(SAP_Object objSAP, wchar_t* sPath);
SAPAPI int SAP_GridView_VisibleRowCountA(SAP_Object objSAP, char* sPath);

// 获取表格单元格内容
SAPAPI wchar_t* SAP_GridView_GetCellTextW(SAP_Object objSAP, wchar_t* sPath, int iRow, wchar_t* sCol);
SAPAPI char* SAP_GridView_GetCellTextA(SAP_Object objSAP, char* sPath, int iRow, char* sCol);



// 自动编码宏定义
#ifdef UNICODE
	#define SAP_GUI_OpenConnect SAP_GUI_OpenConnectW
	#define SAP_Session_SendCMD SAP_Session_SendCMDW
	#define SAP_Exists SAP_ExistsW
	#define SAP_SetText SAP_SetTextW
	#define SAP_GetText SAP_GetTextW
	#define SAP_Click SAP_ClickW
	#define SAP_Select SAP_SelectW
	#define SAP_SetCheck SAP_SetCheckW
	#define SAP_GetCheck SAP_GetCheckW
	#define SAP_SendKey SAP_SendKeyW
	#define SAP_SetFocus SAP_SetFocusW
	#define SAP_SetCaretPos SAP_SetCaretPosW
	#define SAP_CloseWindow SAP_CloseWindowW
	#define SAP_GridView_RowCount SAP_GridView_RowCountW
	#define SAP_GridView_ColCount SAP_GridView_ColCountW
	#define SAP_GridView_VisibleRowCount SAP_GridView_VisibleRowCountW
	#define SAP_GridView_GetCellText SAP_GridView_GetCellTextW
#else
	#define SAP_GUI_OpenConnect SAP_GUI_OpenConnectA
	#define SAP_Session_SendCMD SAP_Session_SendCMDA
	#define SAP_Exists SAP_ExistsA
	#define SAP_SetText SAP_SetTextA
	#define SAP_GetText SAP_GetTextA
	#define SAP_Click SAP_ClickA
	#define SAP_Select SAP_SelectA
	#define SAP_SetCheck SAP_SetCheckA
	#define SAP_GetCheck SAP_GetCheckA
	#define SAP_SendKey SAP_SendKeyA
	#define SAP_SetFocus SAP_SetFocusA
	#define SAP_SetCaretPos SAP_SetCaretPosA
	#define SAP_CloseWindow SAP_CloseWindowA
	#define SAP_GridView_RowCount SAP_GridView_RowCountA
	#define SAP_GridView_ColCount SAP_GridView_ColCountA
	#define SAP_GridView_VisibleRowCount SAP_GridView_VisibleRowCountA
	#define SAP_GridView_GetCellText SAP_GridView_GetCellTextA
#endif
