


#define EXCELAPI



// 类型定义
typedef void* Excel_Application;
typedef void* Excel_WorkBooks;
typedef void* Excel_WorkBook;
typedef void* Excel_WorkSheets;
typedef void* Excel_WorkSheet;
typedef void* Excel_Range;



// 宏定义
#define EXCEL_TRY(func) if (FAILED(func)) { goto LB_Failed; }



// Excel 保存格式
// 参考自 : https://docs.microsoft.com/zh-cn/office/vba/api/excel.xlfileformat
#define Excel_Format_AddIn							18			// Microsoft Excel 97-2003 外接程序	*.xla
#define Excel_Format_AddIn8							18			// Microsoft Excel 97-2003 外接程序	*.xla
#define Excel_Format_CSV							6			// CSV	*.csv
#define Excel_Format_CSVMac							22			// Macintosh CSV	*.csv
#define Excel_Format_CSVMSDOS						24			// MSDOS CSV	*.csv
#define Excel_Format_CSVUTF8						62			// UTF8 CSV	*.csv
#define Excel_Format_CSVWindows						23			// Windows CSV	*.csv
#define Excel_Format_CurrentPlatformText			-4158		// 当前平台文本	*.txt
#define Excel_Format_DBF2							7			// Dbase 2 格式	*.dbf
#define Excel_Format_DBF3							8			// Dbase 3 格式	*.dbf
#define Excel_Format_DBF4							11			// Dbase 4 格式	*.dbf
#define Excel_Format_DIF							9			// 数据交换格式	*.dif
#define Excel_Format_Excel12						50			// Excel 二进制工作簿	*.xlsb
#define Excel_Format_Excel2							16			// Excel 版本 2.0 (1987)	*.xls
#define Excel_Format_Excel2FarEast					27			// Excel 版本 2.0 中文 (1987)	*.xls
#define Excel_Format_Excel3							29			// Excel 版本 3.0 (1990)	*.xls
#define Excel_Format_Excel4							33			// Excel 版本 4.0 (1992)	*.xls
#define Excel_Format_Excel4Workbook					35			// Excel 版本 4.0 工作簿格式 (1992)	*.xlw
#define Excel_Format_Excel5							39			// Excel 版本 5.0 (1994)	*.xls
#define Excel_Format_Excel7							39			// Excel 95（版本 7.0）	*.xls
#define Excel_Format_Excel8							56			// Excel 97-2003 工作簿	*.xls
#define Excel_Format_Excel9795						43			// Excel 版本 95 和 97	*.xls
#define Excel_Format_Html							44			// HTML 格式	.htm；.html
#define Excel_Format_IntlAddIn						26			// 国际外接程序	无文件扩展名
#define Excel_Format_IntlMacro						25			// 国际宏	无文件扩展名
#define Excel_Format_OpenDocumentSpreadsheet		60			// OpenDocument 电子表格	*.ods
#define Excel_Format_OpenXMLAddIn					55			// Open XML 外接程序	*.xlam
#define Excel_Format_OpenXMLStrictWorkbook			61			// (&H3D) Strict Open XML 文件	*.xlsx
#define Excel_Format_OpenXMLTemplate				54			// Open XML 模板	*.xltx
#define Excel_Format_OpenXMLTemplateMacroEnabled	53			// 启用 Open XML 模板宏	*.xltm
#define Excel_Format_OpenXMLWorkbook				51			// Open XML 工作簿	*.xlsx
#define Excel_Format_OpenXMLWorkbookMacroEnabled	52			// 启用 Open XML 工作簿宏	*.xlsm
#define Excel_Format_SYLK							2			// 符号链接格式	*.slk
#define Excel_Format_Template						17			// Excel 模板格式	*.xlt
#define Excel_Format_Template8						17			// 模板 8	*.xlt
#define Excel_Format_TextMac						19			// Macintosh 文本	*.txt
#define Excel_Format_TextMSDOS						21			// MSDOS 文本	*.txt
#define Excel_Format_TextPrinter					36			// 打印机文本	*.prn
#define Excel_Format_TextWindows					20			// Windows 文本	*.txt
#define Excel_Format_UnicodeText					42			// Unicode 文本	无文件扩展名；*.txt
#define Excel_Format_WebArchive						45			// Web 档案	.mh；.mhtml
#define Excel_Format_WJ2WD1							14			// 日语 1-2-3	*.wj2
#define Excel_Format_WJ3							40			// 日语 1-2-3	*.wj3
#define Excel_Format_WJ3FJ3							41			// 日语 1-2-3 格式	*.wj3
#define Excel_Format_WK1							5			// Lotus 1-2-3 格式	*.wk1
#define Excel_Format_WK1ALL							31			// Lotus 1-2-3 格式	*.wk1
#define Excel_Format_WK1FMT							30			// Lotus 1-2-3 格式	*.wk1
#define Excel_Format_WK3							15			// Lotus 1-2-3 格式	*.wk3
#define Excel_Format_WK3FM3							32			// Lotus 1-2-3 格式	*.wk3
#define Excel_Format_WK4							38			// Lotus 1-2-3 格式	*.wk4
#define Excel_Format_WKS							4			// Lotus 1-2-3 格式	*.wks
#define Excel_Format_WorkbookDefault				51			// 默认工作簿	*.xlsx
#define Excel_Format_WorkbookNormal					-4143		// 常规工作簿	*.xls
#define Excel_Format_Works2FarEast					28			// Microsoft Works 2.0 两端对齐格式	*.wks
#define Excel_Format_WQ1							34			// Quattro Pro 格式	*.wq1
#define Excel_Format_XMLSpreadsheet					46			// XML 电子表格	*.xml

// Excel 区域删除位移选项
// https://docs.microsoft.com/zh-cn/office/vba/api/excel.xldeleteshiftdirection
#define Excel_Delete_ShiftToLeft					-4159		// 单元格向左移动。
#define Excel_Delete_ShiftUp						-4162		// 单元格向上移动。

// Excel 粘贴选项
// https://docs.microsoft.com/zh-cn/office/vba/api/excel.xlpastetype
#define Excel_Paste_All								-4104		// 粘贴全部内容。
#define Excel_Paste_AllExceptBorders				7			// 粘贴除边框外的全部内容。
#define Excel_Paste_AllMergingConditionalFormats	14			// 将粘贴所有内容，并且将合并条件格式。
#define Excel_Paste_AllUsingSourceTheme				13			// 使用源主题粘贴全部内容。
#define Excel_Paste_ColumnWidths					8			// 粘贴复制的列宽。
#define Excel_Paste_Comments						-4144		// 粘贴批注。
#define Excel_Paste_Formats							-4122		// 粘贴复制的源格式。
#define Excel_Paste_Formulas						-4123		// 粘贴公式。
#define Excel_Paste_FormulasAndNumberFormats		11			// 粘贴公式和数字格式。
#define Excel_Paste_Validation						6			// 粘贴有效性。
#define Excel_Paste_Values							-4163		// 粘贴值。
#define Excel_Paste_ValuesAndNumberFormats			12			// 粘贴值和数字格式。



// 创建 Application 对象
EXCELAPI Excel_Application Excel_Create();

// 获取 Application 对象
EXCELAPI Excel_Application Excel_GetObject();

// 释放 Application 对象
EXCELAPI int Excel_Free(Excel_Application objExcel);

// 修改 Application 的显示状态
EXCELAPI int Excel_Visible(Excel_Application objExcel, int bShow);

// 修改 Application 的 DisplayAlerts 状态
EXCELAPI int Excel_DisplayAlerts(Excel_Application objExcel, int bAlert);

// 修改 Application 的窗口标题
EXCELAPI int Excel_SetCaptionW(Excel_Application objExcel, wchar_t* sTitle);
EXCELAPI int Excel_SetCaptionA(Excel_Application objExcel, char* sTitle);

// 退出 Application
EXCELAPI int Excel_Quit(Excel_Application objExcel);

// 获取 Application 的 Workbooks 对象
EXCELAPI Excel_WorkBooks Excel_Books(Excel_Application objExcel);

// 创建工作簿
EXCELAPI Excel_WorkBook Excel_NewBook(Excel_Application objExcel);

// 打开工作簿
EXCELAPI Excel_WorkBook Excel_OpenBookW(Excel_Application objExcel, wchar_t* sPath, int bReadOnly);
EXCELAPI Excel_WorkBook Excel_OpenBookA(Excel_Application objExcel, char* sPath, int bReadOnly);

// 打开工作簿 (允许使用密码)
EXCELAPI Excel_WorkBook Excel_OpenBookExW(Excel_Application objExcel, wchar_t* sPath, int bReadOnly, wchar_t* sPwd, wchar_t* sWritePwd);
EXCELAPI Excel_WorkBook Excel_OpenBookExA(Excel_Application objExcel, char* sPath, int bReadOnly, char* sPwd, char* sWritePwd);

// 获取工作簿（通过文件名）
EXCELAPI Excel_WorkBook Excel_GetBookW(Excel_Application objExcel, wchar_t* sPath);
EXCELAPI Excel_WorkBook Excel_GetBookA(Excel_Application objExcel, char* sPath);

// 获取工作簿（通过索引号）
EXCELAPI Excel_WorkBook Excel_GetBook_Index(Excel_Application objExcel, int iIndex);

// 获取活动的工作簿
EXCELAPI Excel_WorkBook Excel_ActiveBook(Excel_Application objExcel);

// 获取工作簿数量
EXCELAPI int Excel_GetBookCount(Excel_Application objExcel);

// 关闭所有工作簿
EXCELAPI int Excel_CloseAllBook(Excel_Application objExcel);



// 关闭工作簿
EXCELAPI int Book_Close(Excel_WorkBook objBook, int bSave);

// 保存工作簿
EXCELAPI int Book_Save(Excel_WorkBook objBook);

// 另存为工作簿 ( iFormat 默认为 Excel_Format_WorkbookDefault )
EXCELAPI int Book_SaveAsW(Excel_WorkBook objBook, wchar_t* sPath, int iFormat);
EXCELAPI int Book_SaveAsA(Excel_WorkBook objBook, char* sPath, int iFormat);

// 修改 WorkBook 的 Saved 状态
EXCELAPI int Book_Saved(Excel_WorkBook objBook, int bSaved);

// 获取工作簿的文件格式
EXCELAPI int Book_FileFormat(Excel_WorkBook objBook);

// 获取工作簿的完整名称（ 需要使用 dhFreeString 或 SysFreeString 释放 ）
EXCELAPI wchar_t* Book_FullNameW(Excel_WorkBook objBook);
EXCELAPI char* Book_FullNameA(Excel_WorkBook objBook);

// 获取工作簿的名称（ 需要使用 dhFreeString 或 SysFreeString 释放 ）
EXCELAPI wchar_t* Book_NameW(Excel_WorkBook objBook);
EXCELAPI char* Book_NameA(Excel_WorkBook objBook);

// 获取工作簿的文件路径（ 需要使用 dhFreeString 或 SysFreeString 释放 ）
EXCELAPI wchar_t* Book_PathW(Excel_WorkBook objBook);
EXCELAPI char* Book_PathA(Excel_WorkBook objBook);

// 激活工作簿窗口
EXCELAPI int Book_ActivateW(Excel_WorkBook objBook, wchar_t* sName);
EXCELAPI int Book_ActivateA(Excel_WorkBook objBook, char* sName);

// 激活工作簿（通过索引号）
EXCELAPI int Book_Activate_Index(Excel_WorkBook objBook, int iIndex);

// 获取激活的工作表
EXCELAPI Excel_WorkSheet Book_ActiveSheet(Excel_WorkBook objBook);

// 获取 WorkBook 的 Sheets 对象
EXCELAPI Excel_WorkSheets Book_Sheets(Excel_WorkBook objBook);

// 创建工作表 (iPos位置从0开始，代表插入到第一个之前、-1代表插入到活动工作表之前)
EXCELAPI Excel_WorkSheet Book_AddSheet(Excel_WorkBook objBook, int iPos);

// 复制工作表
EXCELAPI Excel_WorkSheet Book_CopySheetW(Excel_WorkBook objBook, wchar_t* sName, int iPos);
EXCELAPI Excel_WorkSheet Book_CopySheetA(Excel_WorkBook objBook, char* sName, int iPos);

// 复制工作表（通过索引号）
EXCELAPI Excel_WorkSheet Book_CopySheet_Index(Excel_WorkBook objBook, int iIndex, int iPos);

// 获取工作表（通过表名）
EXCELAPI Excel_WorkSheet Book_GetSheetW(Excel_WorkBook objBook, wchar_t* sName);
EXCELAPI Excel_WorkSheet Book_GetSheetA(Excel_WorkBook objBook, char* sName);

// 获取工作表（通过索引号）
EXCELAPI Excel_WorkSheet Book_GetSheet_Index(Excel_WorkBook objBook, int iIndex);

// 获取工作表数量
EXCELAPI int Book_GetSheetCount(Excel_WorkBook objBook);

// 获取工作表名称（ 需要使用 dhFreeString 或 SysFreeString 释放 ）
EXCELAPI wchar_t* Book_GetSheetNameW(Excel_WorkBook objBook, int iIndex);
EXCELAPI char* Book_GetSheetNameA(Excel_WorkBook objBook, int iIndex);

// 修改工作表名称
EXCELAPI int Book_SetSheetNameW(Excel_WorkBook objBook, wchar_t* sOldName, wchar_t* sNewName);
EXCELAPI int Book_SetSheetNameA(Excel_WorkBook objBook, char* sOldName, char* sNewName);

// 修改工作表名称（通过索引号）
EXCELAPI int Book_SetSheetName_IndexW(Excel_WorkBook objBook, int iIndex, wchar_t* sName);
EXCELAPI int Book_SetSheetName_IndexA(Excel_WorkBook objBook, int iIndex, char* sName);

// 移动工作表
EXCELAPI int Book_MoveSheetW(Excel_WorkBook objBook, wchar_t* sName, int iPos);
EXCELAPI int Book_MoveSheetA(Excel_WorkBook objBook, char* sName, int iPos);

// 移动工作表（通过索引号）
EXCELAPI int Book_MoveSheet_Index(Excel_WorkBook objBook, int iIndex, int iPos);

// 删除工作表
EXCELAPI int Book_DeleteSheetW(Excel_WorkBook objBook, wchar_t* sName);
EXCELAPI int Book_DeleteSheetA(Excel_WorkBook objBook, char* sName);

// 删除工作表（通过索引号）
EXCELAPI int Book_DeleteSheet_Index(Excel_WorkBook objBook, int iIndex);



// 获取工作表行数
EXCELAPI int Sheet_RowCount(Excel_WorkSheet objSheet);

// 获取工作表列数
EXCELAPI int Sheet_ColCount(Excel_WorkSheet objSheet);

// 插入行
EXCELAPI int Sheet_InsertRow(Excel_WorkSheet objSheet, int iPos);

// 插入列
EXCELAPI int Sheet_InsertCol(Excel_WorkSheet objSheet, int iPos);

// 删除行
EXCELAPI int Sheet_DeleteRow(Excel_WorkSheet objSheet, int iPos);

// 删除列
EXCELAPI int Sheet_DeleteCol(Excel_WorkSheet objSheet, int iPos);

// 获取全部数据
EXCELAPI Excel_Range Sheet_UsedRange(Excel_WorkSheet objSheet);

// 获取范围
EXCELAPI Excel_Range Sheet_GetRange(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE);

// 获取范围(用描述字符串)
EXCELAPI Excel_Range Sheet_GetRangeSW(Excel_WorkSheet objSheet, wchar_t* sRange);
EXCELAPI Excel_Range Sheet_GetRangeSA(Excel_WorkSheet objSheet, char* sRange);

// 获取行
EXCELAPI Excel_Range Sheet_GetRow(Excel_WorkSheet objSheet, int iRow);

// 获取列
EXCELAPI Excel_Range Sheet_GetCol(Excel_WorkSheet objSheet, int iRow);

// 获取单元格
EXCELAPI Excel_Range Sheet_GetCell(Excel_WorkSheet objSheet, int iRow, int iCol);

// 设置行高（仅单行）[使用Range对象可以控制多行]
EXCELAPI int Sheet_SetRowHeight(Excel_WorkSheet objSheet, int iRow, double dHeight);

// 设置列宽（仅单列）[使用Range对象可以控制多列]
EXCELAPI int Sheet_SetColWidth(Excel_WorkSheet objSheet, int iCol, double dWidth);

// 读取单元格文本
EXCELAPI wchar_t* Sheet_GetCellTextW(Excel_WorkSheet objSheet, int iRow, int iCol);
EXCELAPI char* Sheet_GetCellTextA(Excel_WorkSheet objSheet, int iRow, int iCol);

// 设置单元格文本（仅单个单元格）[使用Range对象可以批量控制]
EXCELAPI int Sheet_SetCellTextW(Excel_WorkSheet objSheet, int iRow, int iCol, wchar_t* sText);
EXCELAPI int Sheet_SetCellTextA(Excel_WorkSheet objSheet, int iRow, int iCol, char* sText);

// 设置范围文本
EXCELAPI int Sheet_SetRangeTextW(Excel_WorkSheet objSheet, wchar_t* sRange, wchar_t* sText);
EXCELAPI int Sheet_SetRangeTextA(Excel_WorkSheet objSheet, char* sRange, char* sText);

// 读取单元格数字 (浮点)
EXCELAPI double Sheet_GetCellNumber(Excel_WorkSheet objSheet, int iRow, int iCol);

// 设置单元格数字 (浮点)（仅单个单元格）[使用Range对象可以批量控制]
EXCELAPI int Sheet_SetCellNumber(Excel_WorkSheet objSheet, int iRow, int iCol, double dData);

// 读取单元格数字
EXCELAPI double Sheet_GetCellInt(Excel_WorkSheet objSheet, int iRow, int iCol);

// 设置单元格数字（仅单个单元格）[使用Range对象可以批量控制]
EXCELAPI int Sheet_SetCellInt(Excel_WorkSheet objSheet, int iRow, int iCol, int iData);

// 设置范围数字 (浮点)
EXCELAPI int Sheet_SetRangeNumber(Excel_WorkSheet objSheet, wchar_t* sRange, double dData);

// 设置范围数字
EXCELAPI int Sheet_SetRangeInt(Excel_WorkSheet objSheet, wchar_t* sRange, int iData);

// 读取单元格公式
EXCELAPI wchar_t* Sheet_GetCellFormulaW(Excel_WorkSheet objSheet, int iRow, int iCol);
EXCELAPI char* Sheet_GetCellFormulaA(Excel_WorkSheet objSheet, int iRow, int iCol);

// 设置单元格公式（仅单个单元格）[使用Range对象可以批量控制]
EXCELAPI int Sheet_SetCellFormulaW(Excel_WorkSheet objSheet, int iRow, int iCol, wchar_t* sFormula);
EXCELAPI int Sheet_SetCellFormulaA(Excel_WorkSheet objSheet, int iRow, int iCol, char* sFormula);

// 设置单元格格式（仅单个单元格）[使用Range对象可以批量控制]
EXCELAPI int Sheet_SetCellFormatW(Excel_WorkSheet objSheet, int iRow, int iCol, wchar_t* sFormat);
EXCELAPI int Sheet_SetCellFormatA(Excel_WorkSheet objSheet, int iRow, int iCol, char* sFormat);

// 复制范围
EXCELAPI int Sheet_CopyRange(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE);

// 复制范围(用描述字符串)
EXCELAPI int Sheet_CopyRangeSW(Excel_WorkSheet objSheet, wchar_t* sRange);
EXCELAPI int Sheet_CopyRangeSA(Excel_WorkSheet objSheet, char* sRange);

// 粘贴数据到指定位置
EXCELAPI int Sheet_Paste(Excel_WorkSheet objSheet, int iRow, int iCol, int iPaste);

// 粘贴数据到指定位置(用描述字符串)
EXCELAPI int Sheet_PasteSW(Excel_WorkSheet objSheet, wchar_t* sRange, int iPaste);
EXCELAPI int Sheet_PasteSA(Excel_WorkSheet objSheet, char* sRange, int iPaste);

// 粘贴数据到表格末尾
EXCELAPI int Sheet_PasteAppend(Excel_WorkSheet objSheet);

// 清除范围
EXCELAPI int Sheet_ClearRange(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE);

// 清除范围(用描述字符串)
EXCELAPI int Sheet_ClearRangeSW(Excel_WorkSheet objSheet, wchar_t* sRange);
EXCELAPI int Sheet_ClearRangeSA(Excel_WorkSheet objSheet, char* sRange);

// 清空范围内容
EXCELAPI int Sheet_ClearRangeText(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE);

// 清空范围内容(用描述字符串)
EXCELAPI int Sheet_ClearRangeTextSW(Excel_WorkSheet objSheet, wchar_t* sRange);
EXCELAPI int Sheet_ClearRangeTextSA(Excel_WorkSheet objSheet, char* sRange);

// 清空范围格式
EXCELAPI int Sheet_ClearRangeFormat(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE);

// 清空范围格式(用描述字符串)
EXCELAPI int Sheet_ClearRangeFormatSW(Excel_WorkSheet objSheet, wchar_t* sRange);
EXCELAPI int Sheet_ClearRangeFormatSA(Excel_WorkSheet objSheet, char* sRange);

// 删除区域
EXCELAPI int Sheet_DeleteRange(Excel_WorkSheet objSheet, int iRowS, int iColS, int iRowE, int iColE, int iShift);

// 删除区域(用描述字符串)
EXCELAPI int Sheet_DeleteRangeSW(Excel_WorkSheet objSheet, wchar_t* sRange, int iShift);
EXCELAPI int Sheet_DeleteRangeSA(Excel_WorkSheet objSheet, char* sRange, int iShift);

// 设置页眉
EXCELAPI int Sheet_SetPageHeaderW(Excel_WorkSheet objSheet, wchar_t* sText);
EXCELAPI int Sheet_SetPageHeaderA(Excel_WorkSheet objSheet, char* sText);

// 设置页脚
EXCELAPI int Sheet_SetPageFooterW(Excel_WorkSheet objSheet, wchar_t* sText);
EXCELAPI int Sheet_SetPageFooterA(Excel_WorkSheet objSheet, char* sText);

// 设置页面边距
EXCELAPI int Sheet_SetPageSetup(Excel_WorkSheet objSheet, double iTop, double iBottom, double iLeft, double iRight, double iHeader, double iFooter, int bCenterH, int bMiddleV);

// 打印预览
EXCELAPI int Sheet_PrintPreview(Excel_WorkSheet objSheet);

// 打印
EXCELAPI int Sheet_Print(Excel_WorkSheet objSheet);



// 添加批注
EXCELAPI int Range_AddCommentW(Excel_Range objRange, wchar_t* sText);
EXCELAPI int Range_AddCommentA(Excel_Range objRange, char* sText);

// 清除批注
EXCELAPI int Range_ClearComments(Excel_Range objRange);

// 复制
EXCELAPI int Range_Copy(Excel_Range objRange);

// 剪切
EXCELAPI int Range_Cut(Excel_Range objRange);

// 粘贴
EXCELAPI int Range_Paste(Excel_Range objRange, int iPaste);

// 删除（与清除的区别在于可以指定删除后其他单元格的位移）
EXCELAPI int Range_Delete(Excel_Range objRange, int iShift);

// 清除
EXCELAPI int Range_Clear(Excel_Range objRange);

// 清除内容
EXCELAPI int Range_ClearContents(Excel_Range objRange);

// 清除格式
EXCELAPI int Range_ClearFormats(Excel_Range objRange);

// 选择
EXCELAPI int Range_Select(Excel_Range objRange);

// 打印预览
EXCELAPI int Range_PrintPreview(Excel_Range objRange);

// 打印
EXCELAPI int Range_Print(Excel_Range objRange);

// 选区有多少行
EXCELAPI int Range_RowCount(Excel_Range objRange);

// 选区有多少列
EXCELAPI int Range_ColCount(Excel_Range objRange);

// 选区单元格数量
EXCELAPI int Range_CellCount(Excel_Range objRange);

// 设置选区字体
EXCELAPI int Range_SetFontW(Excel_Range objRange, wchar_t* sFont, int iSize, int bBold, int bItalic, int bUnder, int bStrike);
EXCELAPI int Range_SetFontA(Excel_Range objRange, char* sFont, int iSize, int bBold, int bItalic, int bUnder, int bStrike);

// 设置选区颜色
EXCELAPI int Range_SetColor(Excel_Range objRange, int iTextColor, int iBackColor, int iBorderColor);

// 设置选区格式
EXCELAPI int Range_SetFormatW(Excel_Range objRange, wchar_t* sFormat);
EXCELAPI int Range_SetFormatA(Excel_Range objRange, char* sFormat);

// 设置选区公式
EXCELAPI int Range_SetFormulaW(Excel_Range objRange, wchar_t* sFormula);
EXCELAPI int Range_SetFormulaA(Excel_Range objRange, char* sFormula);

// 判断选区是否为公式
EXCELAPI int Range_HasFormula(Excel_Range objRange);

// 获取选区公式
EXCELAPI wchar_t* Range_GetFormulaW(Excel_Range objRange);
EXCELAPI char* Range_GetFormulaA(Excel_Range objRange);

// 获取文本
EXCELAPI wchar_t* Range_GetTextW(Excel_Range objRange);
EXCELAPI char* Range_GetTextA(Excel_Range objRange);

// 设置文本
EXCELAPI int Range_SetTextW(Excel_Range objRange, wchar_t* sText);
EXCELAPI int Range_SetTextA(Excel_Range objRange, char* sText);

// 获取数字 (浮点)
EXCELAPI double Range_GetNumber(Excel_Range objRange);

// 设置数字 (浮点)
EXCELAPI int Range_SetNumber(Excel_Range objRange, double dData);

// 获取数字
EXCELAPI int Range_GetInt(Excel_Range objRange);

// 设置数字
EXCELAPI int Range_SetInt(Excel_Range objRange, int iData);

// 设置自动换行
EXCELAPI int Range_SetWrapText(Excel_Range objRange, int bWrap);



// 自动编码宏定义
#ifdef UNICODE
	#define Excel_SetCaption Excel_SetCaptionW
	#define Excel_OpenBook Excel_OpenBookW
	#define Excel_OpenBookEx Excel_OpenBookExW
	#define Excel_GetBook Excel_GetBookW
	#define Book_SaveAs Book_SaveAsW
	#define Book_FullName Book_FullNameW
	#define Book_Name Book_NameW
	#define Book_Path Book_PathW
	#define Book_Activate Book_ActivateW
	#define Book_CopySheet Book_CopySheetW
	#define Book_GetSheet Book_GetSheetW
	#define Book_GetSheetName Book_GetSheetNameW
	#define Book_SetSheetName Book_SetSheetNameW
	#define Book_SetSheetName_Index Book_SetSheetName_IndexW
	#define Book_MoveSheet Book_MoveSheetW
	#define Book_DeleteSheet Book_DeleteSheetW
	#define Sheet_GetRangeS Sheet_GetRangeSW
	#define Sheet_GetCellText Sheet_GetCellTextW
	#define Sheet_SetCellText Sheet_SetCellTextW
	#define Sheet_SetRangeText Sheet_SetRangeTextW
	#define Sheet_GetCellFormula Sheet_GetCellFormulaW
	#define Sheet_SetCellFormula Sheet_SetCellFormulaW
	#define Sheet_SetCellFormat Sheet_SetCellFormatW
	#define Sheet_CopyRangeS Sheet_CopyRangeSW
	#define Sheet_PasteS Sheet_PasteSW
	#define Sheet_ClearRangeS Sheet_ClearRangeSW
	#define Sheet_ClearRangeTextS Sheet_ClearRangeTextSW
	#define Sheet_ClearRangeFormatS Sheet_ClearRangeFormatSW
	#define Sheet_DeleteRangeS Sheet_DeleteRangeSW
	#define Sheet_SetPageHeader Sheet_SetPageHeaderW
	#define Sheet_SetPageFooter Sheet_SetPageFooterW
	#define Range_AddComment Range_AddCommentW
	#define Range_SetFont Range_SetFontW
	#define Range_SetFormat Range_SetFormatW
	#define Range_SetFormula Range_SetFormulaW
	#define Range_GetFormula Range_GetFormulaW
	#define Range_GetText Range_GetTextW
	#define Range_SetText Range_SetTextW
#else
	#define Excel_SetCaption Excel_SetCaptionA
	#define Excel_OpenBook Excel_OpenBookA
	#define Excel_OpenBookEx Excel_OpenBookExA
	#define Excel_GetBook Excel_GetBookA
	#define Book_SaveAs Book_SaveAsA
	#define Book_FullName Book_FullNameA
	#define Book_Name Book_NameA
	#define Book_Path Book_PathA
	#define Book_Activate Book_ActivateA
	#define Book_CopySheet Book_CopySheetA
	#define Book_GetSheet Book_GetSheetA
	#define Book_GetSheetName Book_GetSheetNameA
	#define Book_SetSheetName Book_SetSheetNameA
	#define Book_SetSheetName_Index Book_SetSheetName_IndexA
	#define Book_MoveSheet Book_MoveSheetA
	#define Book_DeleteSheet Book_DeleteSheetA
	#define Sheet_GetRangeS Sheet_GetRangeSA
	#define Sheet_GetCellText Sheet_GetCellTextA
	#define Sheet_SetCellText Sheet_SetCellTextA
	#define Sheet_SetRangeText Sheet_SetRangeTextA
	#define Sheet_GetCellFormula Sheet_GetCellFormulaA
	#define Sheet_SetCellFormula Sheet_SetCellFormulaA
	#define Sheet_SetCellFormat Sheet_SetCellFormatA
	#define Sheet_CopyRangeS Sheet_CopyRangeSA
	#define Sheet_PasteS Sheet_PasteSA
	#define Sheet_ClearRangeS Sheet_ClearRangeSA
	#define Sheet_ClearRangeTextS Sheet_ClearRangeTextSA
	#define Sheet_ClearRangeFormatS Sheet_ClearRangeFormatSA
	#define Sheet_DeleteRangeS Sheet_DeleteRangeSA
	#define Sheet_SetPageHeader Sheet_SetPageHeaderA
	#define Sheet_SetPageFooter Sheet_SetPageFooterA
	#define Range_AddComment Range_AddCommentA
	#define Range_SetFont Range_SetFontA
	#define Range_SetFormat Range_SetFormatA
	#define Range_SetFormula Range_SetFormulaA
	#define Range_GetFormula Range_GetFormulaA
	#define Range_GetText Range_GetTextA
	#define Range_SetText Range_SetTextA
#endif
