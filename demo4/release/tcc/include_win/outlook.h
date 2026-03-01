


#define EXCELAPI



// 类型定义
typedef void* Outlook_Application;
typedef void* Outlook_MailItem;
typedef void* Outlook_Folders;
typedef void* Outlook_Folder;
typedef void* Outlook_Items;



// 宏定义
#define EXCEL_TRY(func) if (FAILED(func)) { goto LB_Failed; }



// 关闭邮件时指定是否保存邮件
#define OutLook_Discard							1			// 放弃对文档的更改。
#define OutLook_PromptForSave					2			// 提示用户保存文档。
#define OutLook_Save							0			// 保存文档。

// 邮件正文格式
#define OutLook_FormatHTML						2			// HTML 格式
#define OutLook_FormatPlain						1			// 纯文本格式
#define OutLook_FormatRichText					3			// RTF 格式
#define OutLook_FormatUnspecified				0			// 未指定的格式

// 邮件的重要级别
#define OutLook_ImportanceHigh					2			// 项目的重要性标记为高。
#define OutLook_ImportanceLow					0			// 项目的重要性标记为低。
#define OutLook_ImportanceNormal				1			// 项目的重要性标记为中。

// 获取默认文件夹类型
#define OutLook_FolderCalendar					9			// "日历"文件夹。
#define OutLook_FolderConflicts					19			// "冲突"文件夹（"同步问题"文件夹的子文件夹）。 仅对 Exchange 帐户可用。
#define OutLook_FolderContacts					10			// "联系人"文件夹。
#define OutLook_FolderDeletedItems				3			// "已删除邮件"文件夹。
#define OutLook_FolderDrafts					16			// "草稿"文件夹。
#define OutLook_FolderInbox						6			// "收件箱"文件夹。
#define OutLook_FolderJournal					11			// "日记"文件夹。
#define OutLook_FolderJunk						23			// "垃圾电子邮件"文件夹。
#define OutLook_FolderLocalFailures				21			// "本地故障"文件夹（"同步问题"文件夹的子文件夹）。 仅对 Exchange 帐户可用。
#define OutLook_FolderManagedEmail				29			// 托管文件夹组中的顶级文件夹。 有关托管文件夹的详细信息，请参阅 Microsoft Outlook 中的"帮助"。 仅对 Exchange 帐户可用。
#define OutLook_FolderNotes						12			// "便笺"文件夹。
#define OutLook_FolderOutbox					4			// "发件箱"文件夹。
#define OutLook_FolderSentMail					5			// "已发送邮件"文件夹。
#define OutLook_FolderServerFailures			22			// "服务器故障"文件夹（"同步问题"文件夹的子文件夹）。 仅对 Exchange 帐户可用。
#define OutLook_FolderSuggestedContacts			30			// "建议的联系人"文件夹。
#define OutLook_FolderSyncIssues				20			// "同步问题"文件夹。 仅对 Exchange 帐户可用。
#define OutLook_FolderTasks						13			// "任务"文件夹。
#define OutLook_FolderToDo						28			// "待办事项"文件夹。
#define OutLook_PublicFoldersAllPublicFolders	18			// Exchange 公用文件夹存储中的"所有的公用文件夹"文件夹。 仅对 Exchange 帐户可用。
#define OutLook_FolderRssFeeds					25			// “RSS 源”文件夹。

// 项目类型
#define OutLook_AppointmentItem					1			// AppointmentItem 对象。
#define OutLook_ContactItem						2			// ContactItem 对象。
#define OutLook_DistributionListItem			7			// DistListItem 对象。
#define OutLook_JournalItem						4			// JournalItem 对象。
#define OutLook_MailItem						0			// MailItem 对象。
#define OutLook_NoteItem						5			// NoteItem 对象。
#define OutLook_PostItem						6			// PostItem 对象。
#define OutLook_TaskItem						3			// TaskItem 对象。



// 创建 Application 对象
EXCELAPI Outlook_Application Outlook_Create();

// 获取 Application 对象
EXCELAPI Outlook_Application Outlook_GetObject();

// 退出 Outlook
EXCELAPI int Outlook_Quit(Outlook_Application objApp);

// 发送邮件
EXCELAPI int Outlook_SendMail(Outlook_Application objApp, wchar_t* sTo, wchar_t* sTitle, wchar_t* sBody);

// 创建一封邮件
EXCELAPI Outlook_MailItem Outlook_CreateMail(Outlook_Application objApp);

// 设置邮件主题
EXCELAPI int MailItem_SetSubJect(Outlook_MailItem objMail, wchar_t* sTitle);

// 设置邮件正文格式
EXCELAPI int MailItem_SetBodyFormat(Outlook_MailItem objMail, int iFormat);

// 设置邮件正文(纯文本格式)
EXCELAPI int MailItem_SetBody(Outlook_MailItem objMail, wchar_t* sBody);

// 设置邮件正文(HTML格式)
EXCELAPI int MailItem_SetHTMLBody(Outlook_MailItem objMail, wchar_t* sBody);

// 设置收件人
EXCELAPI int MailItem_SetTo(Outlook_MailItem objMail, wchar_t* sTo);

// 设置抄送人
EXCELAPI int MailItem_SetCC(Outlook_MailItem objMail, wchar_t* sCC);

// 设置秘密抄送人
EXCELAPI int MailItem_SetBCC(Outlook_MailItem objMail, wchar_t* sCC);

// 添加收件人
EXCELAPI int MailItem_Recipients_Add(Outlook_MailItem objMail, wchar_t* sMail);

// 删除收件人（索引从1开始）
EXCELAPI int MailItem_Recipients_Remove(Outlook_MailItem objMail, int iIndex);

// 获取收件人数量
EXCELAPI int MailItem_Recipients_Count(Outlook_MailItem objMail);

// 设置邮件的重要级别
EXCELAPI int MailItem_SetImportance(Outlook_MailItem objMail, int iLevel);

// 添加附件
EXCELAPI int MailItem_Attachments_Add(Outlook_MailItem objMail, wchar_t* sFile);

// 删除附件
EXCELAPI int MailItem_Attachments_Remove(Outlook_MailItem objMail, int iIndex);

// 获取附件数量
EXCELAPI int MailItem_Attachments_Count(Outlook_MailItem objMail);

// 获取附件显示名称
EXCELAPI wchar_t* MailItem_Attachments_GetDisplayName(Outlook_MailItem objMail, int iIndex);

// 获取附件文件名
EXCELAPI wchar_t* MailItem_Attachments_GetFileName(Outlook_MailItem objMail, int iIndex);

// 获取附件完整路径
EXCELAPI wchar_t* MailItem_Attachments_GetPathName(Outlook_MailItem objMail, int iIndex);

// 获取附件文件大小
EXCELAPI int MailItem_Attachments_GetSize(Outlook_MailItem objMail, int iIndex);

// 保存附件
EXCELAPI int MailItem_Attachments_SaveAsFile(Outlook_MailItem objMail, int iIndex, wchar_t* sFile);

// 发送邮件
EXCELAPI int MailItem_Send(Outlook_MailItem objMail);

// 关闭邮件
EXCELAPI int MailItem_Close(Outlook_MailItem objMail, int iSave);

// 显示邮件
EXCELAPI int MailItem_Display(Outlook_MailItem objMail, int bModal);

// 转发邮件
EXCELAPI Outlook_MailItem MailItem_Forward(Outlook_MailItem objMail);

// 回复邮件
EXCELAPI Outlook_MailItem MailItem_Reply(Outlook_MailItem objMail);

// 回复所有人
EXCELAPI Outlook_MailItem MailItem_ReplyAll(Outlook_MailItem objMail);

// 返回收件人列表
EXCELAPI wchar_t* MailItem_ReplyRecipientNames(Outlook_MailItem objMail);

// 获取阅读状态
EXCELAPI int MailItem_GetUnRead(Outlook_MailItem objMail);

// 设置阅读状态
EXCELAPI int MailItem_SetUnRead(Outlook_MailItem objMail, int bRead);

// 保存邮件
EXCELAPI int MailItem_Save(Outlook_MailItem objMail);

// 另存为邮件
EXCELAPI int MailItem_SaveAs(Outlook_MailItem objMail, wchar_t* sFile);

// 获取登录的用户名
EXCELAPI wchar_t* MAPI_GetCurrentUser(Outlook_Application objApp);

// 获取默认文件夹
EXCELAPI Outlook_Folder MAPI_GetDefaultFolder(Outlook_Application objApp, int iType);

// 获取子目录对象
EXCELAPI Outlook_Folders MAPI_GetFolders(Outlook_Application objApp);

// 获取文件夹数量
EXCELAPI int Folders_GetCount(Outlook_Folders objFolders);

// 创建文件夹
EXCELAPI int Folders_Add(Outlook_Folders objFolders, wchar_t* sName, int iType);

// 删除文件夹
EXCELAPI int Folders_Remove(Outlook_Folders objFolders, int iIndex);

// 获取文件夹
EXCELAPI Outlook_Folder Folders_GetItem(Outlook_Folders objFolders, int iIndex);

// 获取子目录集合
EXCELAPI Outlook_Folders Folder_GetFolders(Outlook_Folder objFolder);

// 获取父对象（包含自身的目录集合）
EXCELAPI Outlook_Folders Folder_GetParent(Outlook_Folder objFolder);

// 获取文件夹名
EXCELAPI wchar_t* Folder_GetName(Outlook_Folder objFolder);

// 修改文件夹名
EXCELAPI int Folder_SetName(Outlook_Folder objFolder, wchar_t* sName);

// 获取文件夹路径
EXCELAPI wchar_t* Folder_GetFolderPath(Outlook_Folder objFolder);

// 获取文件夹默认的项目类型
EXCELAPI int Folder_DefaultItemType(Outlook_Folder objFolder);

// 获取邮件列表集合
EXCELAPI Outlook_Items Folder_GetItems(Outlook_Folder objFolder);

// 获取子项数量
EXCELAPI int Items_GetCount(Outlook_Items objItems);

// 获取子项
EXCELAPI Outlook_MailItem Items_GetItem(Outlook_Items objItems, int iIndex);


