#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "app/actions.h"
#include "ui/c_completion_view.h"
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "ui/workspace_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include "ui/configuration_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/toolchain_view.h"
#include "ui/language_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "ui/goto_view.h"
#include "ui/catalog_view.h"
#include <stdlib.h>
#include <string.h>
enum requirement { WORKBENCH, WORKSPACE_VIEW, WORKSPACE, NEW_PROJECT, IMPORT_VIEW, PROJECT, AVAILABLE_PROJECT, REFERENCES, RESOURCES, NEW_RESOURCE, ADD_FILE, REMOVE_FILE, RENAME_SELECTED_FILE, MIGRATE_PROJECT, PROPERTIES,
    DOCUMENTS, DOCUMENT, NAMED_DOCUMENT, RENAME_DOCUMENT, DIRTY_DOCUMENTS, UNDO, REDO, WRITABLE_DOCUMENT, SELECTED_TEXT, WRITABLE_SELECTION, NONEMPTY_DOCUMENT, LINE_COMMENT, COMPLETION, GOTO_VIEW, BUILD, RUN, TEST, STOP, INPUT, TASK_VIEW, FONT_VIEW, RECOVERY_VIEW, CHANGES_VIEW, TOOLCHAIN_VIEW, SAVED_CHECK, CURRENT_CHECK, CANCEL_CHECK, DEFINITION, HOVER, SOURCE_DECLARATION, SOURCE_DOCUMENT };
enum check_kind { NO_CHECK, THEME_CHECK, LANGUAGE_CHECK, STARTUP_CHECK };
typedef struct action_def { xide_action_info info; enum requirement need; enum check_kind check; int (*execute)(xide_app*,int); } action_def;
static int fontAction(xide_app* a,int id) { return id==XIDE_ACTION_UI_FONT ? xideFontViewOpenUi(a) : xideFontViewOpen(a); }
static int recoveryAction(xide_app* a,int id) { (void)id; return xideRecoveryViewOpen(a); }
static int toolchainAction(xide_app* a,int id) { (void)id; return xideToolchainViewOpen(a); }
static int resourceAction(xide_app* a,int id) { return id==XIDE_ACTION_NEW_RESOURCE ? xideResourceViewOpenNew(a) : xideResourceViewOpen(a); }
static int referencesAction(xide_app* a,int id) { (void)id; return xideReferencesViewOpen(a); }
static int renameAction(xide_app* a,int id) { (void)id; return xideRenameViewOpen(a); }
static int importAction(xide_app* a,int id) { (void)id; return xideImportViewOpen(a); }
static int changesAction(xide_app* a,int id) { (void)id; return xideFileChangesViewOpen(a); }
static int gotoAction(xide_app* a,int id) { (void)id; return xideGotoViewOpen(a); }
static int sourceAction(xide_app* a,int id) { return xideCatalogSourceOpen(a,id==XIDE_ACTION_SOURCE_DOCUMENT); }
#define A(id,name,title,shorttitle,chord,key,mods,scope,cmd,need,check,proc) {{id,name,title,shorttitle,chord,key,mods,scope,cmd},need,check,proc}
#define W(id,name,title,need) A(id,name,title,NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,need,NO_CHECK,xideWorkspaceAction)
#define E(id,name,title,chord,key,mods,need,scope,cmd) A(id,name,title,NULL,chord,key,mods,scope,cmd,need,NO_CHECK,xideEditorAction)
static const action_def definitions[]={
    A(XIDE_ACTION_RESET,"view.resetLayout","恢复默认布局","恢复布局",NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,NO_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_XLANG,"view.xlangDemo","查看 xlang 语法演示","xlang",NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,NO_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_C,"view.cDemo","查看 C 语法演示","C",NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,NO_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_THEME_CLASSIC,"view.theme.classic","经典浅色",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,THEME_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_THEME_GRAPHITE,"view.theme.graphite","石墨深色",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,THEME_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_THEME_PAPER,"view.theme.paper","暖纸白",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,THEME_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_THEME_OCEAN,"view.theme.ocean","海洋蓝",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,THEME_CHECK,xideWorkbenchAction),
    A(XIDE_ACTION_NEW_WORKSPACE,"workspace.new","新建工作空间…",NULL,"Ctrl+Shift+N",'N',XUI_MOD_CTRL|XUI_MOD_SHIFT,XIDE_SHORTCUT_GLOBAL,0,WORKSPACE_VIEW,NO_CHECK,xideWorkspaceAction),
    A(XIDE_ACTION_OPEN_WORKSPACE,"workspace.open","打开工作空间…",NULL,"Ctrl+Alt+O",'O',XUI_MOD_CTRL|XUI_MOD_ALT,XIDE_SHORTCUT_GLOBAL,0,WORKSPACE_VIEW,NO_CHECK,xideWorkspaceAction),
    A(XIDE_ACTION_SAVE_WORKSPACE,"workspace.save","保存工作空间和项目",NULL,"Ctrl+Alt+S",'S',XUI_MOD_CTRL|XUI_MOD_ALT,XIDE_SHORTCUT_GLOBAL,0,WORKSPACE,NO_CHECK,xideWorkspaceAction),
    W(XIDE_ACTION_SAVE_WORKSPACE_AS,"workspace.saveAs","工作空间另存为…",WORKSPACE),
    A(XIDE_ACTION_OPEN_PROJECT,"project.open","打开项目…",NULL,"Ctrl+Shift+O",'O',XUI_MOD_CTRL|XUI_MOD_SHIFT,XIDE_SHORTCUT_GLOBAL,0,WORKSPACE_VIEW,NO_CHECK,xideWorkspaceAction),
    W(XIDE_ACTION_ADD_PROJECT,"project.add","添加已有项目…",WORKSPACE),
    W(XIDE_ACTION_REMOVE_PROJECT,"project.remove","从工作空间移除项目",PROJECT),
    A(XIDE_ACTION_SET_STARTUP,"project.setStartup","设为启动项目",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,AVAILABLE_PROJECT,STARTUP_CHECK,xideWorkspaceAction),
    W(XIDE_ACTION_ADD_FILE,"project.addFile","添加已有文件…",ADD_FILE),
    W(XIDE_ACTION_REMOVE_FILE,"project.removeFile","移除所选文件引用",REMOVE_FILE),
    A(XIDE_ACTION_RENAME_SELECTED_FILE,"project.renameFile","重命名所选文件…",NULL,"F2",XUI_KEY_F2,0,XIDE_SHORTCUT_WORKSPACE,0,RENAME_SELECTED_FILE,NO_CHECK,xideWorkspaceAction),
    W(XIDE_ACTION_MIGRATE_PROJECT_MANIFEST,"project.moveManifest","迁移项目清单…",MIGRATE_PROJECT),
    W(XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY,"project.moveDirectory","迁移项目目录…",MIGRATE_PROJECT),
    A(XIDE_ACTION_IMPORT_LEGACY,"project.importLegacy","导入旧工作空间或项目…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,IMPORT_VIEW,NO_CHECK,importAction),
    W(XIDE_ACTION_NEW_PROJECT,"project.new","从模板新建项目…",NEW_PROJECT),
    W(XIDE_ACTION_CONFIGURATIONS,"project.configurations","构建、运行与测试配置…",AVAILABLE_PROJECT),
    W(XIDE_ACTION_APPLY_PROPERTIES,"project.applyProperties","应用项目属性",PROPERTIES),
    A(XIDE_ACTION_REFERENCES,"project.references","项目文件、资源与库引用…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,REFERENCES,NO_CHECK,referencesAction),
    A(XIDE_ACTION_RESOURCES,"project.resources","Windows 资源属性…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,RESOURCES,NO_CHECK,resourceAction),
    A(XIDE_ACTION_NEW_RESOURCE,"project.newResource","新建 Windows 资源配置…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,NEW_RESOURCE,NO_CHECK,resourceAction),
    E(XIDE_ACTION_NEW_FILE,"file.new","新建源文件","Ctrl+N",'N',XUI_MOD_CTRL,DOCUMENTS,XIDE_SHORTCUT_GLOBAL,0),
    A(XIDE_ACTION_OPEN_FILE,"file.open","打开文件…","打开","Ctrl+O",'O',XUI_MOD_CTRL,XIDE_SHORTCUT_GLOBAL,0,DOCUMENTS,NO_CHECK,xideEditorAction),
    A(XIDE_ACTION_SAVE_FILE,"file.save","保存文件","保存","Ctrl+S",'S',XUI_MOD_CTRL,XIDE_SHORTCUT_GLOBAL,0,DOCUMENT,NO_CHECK,xideEditorAction),
    E(XIDE_ACTION_SAVE_FILE_AS,"file.saveAs","文件另存为…","Ctrl+Shift+S",'S',XUI_MOD_CTRL|XUI_MOD_SHIFT,DOCUMENT,XIDE_SHORTCUT_GLOBAL,0),
    E(XIDE_ACTION_SAVE_ALL_FILES,"file.saveAll","保存所有文件","Ctrl+Shift+Alt+S",'S',XUI_MOD_CTRL|XUI_MOD_SHIFT|XUI_MOD_ALT,DIRTY_DOCUMENTS,XIDE_SHORTCUT_GLOBAL,0),
    E(XIDE_ACTION_CLOSE_FILE,"file.close","关闭文件","Ctrl+W",'W',XUI_MOD_CTRL,DOCUMENT,XIDE_SHORTCUT_GLOBAL,0),
    A(XIDE_ACTION_RENAME_FILE,"file.rename","重命名当前文件…",NULL,"F2",XUI_KEY_F2,0,XIDE_SHORTCUT_EDITOR,0,RENAME_DOCUMENT,NO_CHECK,renameAction),
    E(XIDE_ACTION_RELOAD_FILE,"file.reload","重新加载文件",NULL,0,0,NAMED_DOCUMENT,XIDE_SHORTCUT_GLOBAL,0),
    A(XIDE_ACTION_LANGUAGE_C,"editor.language.c","当前文件使用 C 语法",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,DOCUMENT,LANGUAGE_CHECK,xideEditorAction),
    A(XIDE_ACTION_LANGUAGE_XLANG,"editor.language.xlang","当前文件使用 xlang 语法",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,DOCUMENT,LANGUAGE_CHECK,xideEditorAction),
    A(XIDE_ACTION_LANGUAGE_TEXT,"editor.language.text","当前文件使用纯文本",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,DOCUMENT,LANGUAGE_CHECK,xideEditorAction),
    E(XIDE_ACTION_EDITOR_UNDO,"editor.undo","撤销","Ctrl+Z",'Z',XUI_MOD_CTRL,UNDO,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_UNDO),
    E(XIDE_ACTION_EDITOR_REDO,"editor.redo","重做","Ctrl+Y",'Y',XUI_MOD_CTRL,REDO,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_REDO),
    E(XIDE_ACTION_EDITOR_CUT,"editor.cut","剪切","Ctrl+X",'X',XUI_MOD_CTRL,WRITABLE_SELECTION,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_CUT),
    E(XIDE_ACTION_EDITOR_COPY,"editor.copy","复制","Ctrl+C",'C',XUI_MOD_CTRL,SELECTED_TEXT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_COPY),
    E(XIDE_ACTION_EDITOR_PASTE,"editor.paste","粘贴","Ctrl+V",'V',XUI_MOD_CTRL,WRITABLE_DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_PASTE),
    E(XIDE_ACTION_EDITOR_SELECT_ALL,"editor.selectAll","全选","Ctrl+A",'A',XUI_MOD_CTRL,NONEMPTY_DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_SELECT_ALL),
    E(XIDE_ACTION_EDITOR_INDENT,"editor.indent","增加缩进","Ctrl+]",']',XUI_MOD_CTRL,WRITABLE_DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_INDENT),
    E(XIDE_ACTION_EDITOR_OUTDENT,"editor.outdent","减少缩进","Ctrl+[",'[',XUI_MOD_CTRL,WRITABLE_DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_OUTDENT),
    E(XIDE_ACTION_EDITOR_COMMENT,"editor.toggleLineComment","切换行注释","Ctrl+/",'/',XUI_MOD_CTRL,LINE_COMMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT),
    E(XIDE_ACTION_EDITOR_FIND,"editor.find","查找…","Ctrl+F",'F',XUI_MOD_CTRL,DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_OPEN_FIND),
    E(XIDE_ACTION_EDITOR_REPLACE,"editor.replace","替换…","Ctrl+H",'H',XUI_MOD_CTRL,WRITABLE_DOCUMENT,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_OPEN_REPLACE),
    A(XIDE_ACTION_EDITOR_GOTO,"editor.gotoLine","跳转到行…",NULL,"Ctrl+G",'G',XUI_MOD_CTRL,XIDE_SHORTCUT_EDITOR,XUI_CODE_COMMAND_GOTO_LINE,GOTO_VIEW,NO_CHECK,gotoAction),
    E(XIDE_ACTION_EDITOR_COMPLETE,"editor.complete","代码补全","Ctrl+Space",XUI_KEY_SPACE,XUI_MOD_CTRL,COMPLETION,XIDE_SHORTCUT_EDITOR,0),
    A(XIDE_ACTION_BUILD,"task.build","构建所选项目","构建","F7",XUI_KEY_F7,0,XIDE_SHORTCUT_GLOBAL,0,BUILD,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_RUN,"task.run","构建并运行启动项目","运行","F5",XUI_KEY_F5,0,XIDE_SHORTCUT_GLOBAL,0,RUN,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_TEST,"task.test","运行所选项目测试","测试","Ctrl+F7",XUI_KEY_F7,XUI_MOD_CTRL,XIDE_SHORTCUT_GLOBAL,0,TEST,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_STOP,"task.stop","停止任务","停止","Shift+F5",XUI_KEY_F5,XUI_MOD_SHIFT,XIDE_SHORTCUT_GLOBAL,0,STOP,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_C_SERVER,"language.cServer","选择 C 语言服务器…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,NO_CHECK,xideCCompletionViewAction),
    A(XIDE_ACTION_COMPILER,"task.compiler","选择 xlang 编译器…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,TASK_VIEW,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_CLOSE_INPUT,"task.closeInput","关闭会话输入",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,INPUT,NO_CHECK,xideTaskViewAction),
    A(XIDE_ACTION_TOOLCHAIN_INFO,"task.toolchainInfo","工具链信息…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,TOOLCHAIN_VIEW,NO_CHECK,toolchainAction),
    A(XIDE_ACTION_CHECK_CURRENT,"language.check","检查当前文档（含未保存依赖）",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,CURRENT_CHECK,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_OUTLINE,"language.outline","文档大纲",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,WORKBENCH,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_HOVER,"language.hover","符号信息",NULL,"Ctrl+I",'I',XUI_MOD_CTRL,XIDE_SHORTCUT_EDITOR,0,HOVER,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_DEFINITION,"language.definition","转到定义",NULL,"F12",XUI_KEY_F12,0,XIDE_SHORTCUT_EDITOR,0,DEFINITION,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_CHECK_SAVED,"language.checkSaved","检查已保存的 xlang 源码",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,SAVED_CHECK,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_CANCEL_CHECK,"language.cancelCheck","取消源码检查",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,CANCEL_CHECK,NO_CHECK,xideLanguageViewAction),
    A(XIDE_ACTION_EDITOR_FONT,"view.codeFont","代码字体…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,FONT_VIEW,NO_CHECK,fontAction),
    A(XIDE_ACTION_UI_FONT,"view.uiFont","界面字体…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,FONT_VIEW,NO_CHECK,fontAction),
    A(XIDE_ACTION_RECOVERY,"view.recovery","会话与恢复…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,RECOVERY_VIEW,NO_CHECK,recoveryAction),
    A(XIDE_ACTION_FILE_CHANGES,"view.diskChanges","磁盘变化…",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,CHANGES_VIEW,NO_CHECK,changesAction),
    A(XIDE_ACTION_SOURCE_DECLARATION,"help.sourceDeclaration","查看声明来源",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,SOURCE_DECLARATION,NO_CHECK,sourceAction),
    A(XIDE_ACTION_SOURCE_DOCUMENT,"help.sourceDocument","查看文档来源",NULL,NULL,0,0,XIDE_SHORTCUT_GLOBAL,0,SOURCE_DOCUMENT,NO_CHECK,sourceAction)
};
#undef A
#undef E
#undef W
typedef struct binding { struct binding* next; xui_widget widget; int kind,action; xui_widget_event_proc keyHandler,contextHandler,pointerHandler; void *keyUser,*contextUser,*pointerUser; } binding;
struct xide_actions { binding* bindings; int depth; xui_widget_event_proc commandHandler; void* commandUser; };
size_t xideActionCount(void) { return sizeof(definitions)/sizeof(*definitions); }
const xide_action_info* xideActionAt(size_t index) { return index<xideActionCount() ? &definitions[index].info : NULL; }
static const action_def* definition(int id) { for(size_t i=0;i<xideActionCount();++i) if(definitions[i].info.id==id) return &definitions[i]; return NULL; }
const xide_action_info* xideActionFind(int id) { const action_def* d=definition(id); return d ? &d->info : NULL; }
const xide_action_info* xideActionNamed(const char* name) { if(name) for(size_t i=0;i<xideActionCount();++i) if(!strcmp(name,definitions[i].info.name)) return &definitions[i].info; return NULL; }
int xideActionsHasModal(xide_app* a)
{ return a && (xideFileDialogBusy(a) || xideMessageBusy(a) || xideCatalogSourceIsOpen(a) || xideGotoViewIsOpen(a) || xideToolchainViewIsOpen(a) || xideResourceViewIsOpen(a) || xideImportViewIsOpen(a) || xideRenameViewIsOpen(a) || xideRenameViewHasPending(a) || xideTemplateViewIsOpen(a) || xideReferencesViewIsOpen(a) || xideConfigurationViewIsOpen(a) || xideFontViewIsOpen(a) || xideRecoveryViewIsOpen(a) || xideFileChangesViewIsOpen(a)); }
xide_action_state xideActionQuery(xide_app* a,int id)
{
    const action_def* d=definition(id); xide_action_state state={0,0,"未知动作或工作台尚未就绪"};
    if(!a || !a->actions || !d || !a->context) return state;
    xide_document* doc=a->activeDocument && a->activeDocument->open ? a->activeDocument : NULL;
    xide_project* project=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);
    xide_project* startup=xideWorkspaceFindProject(&a->workspaceModel,a->workspaceModel.startupProject);
    if(d->check==THEME_CHECK) state.checked=a->theme==id-XIDE_ACTION_THEME_CLASSIC;
    else if(d->check==LANGUAGE_CHECK && doc) { const char* modes[]={"c","xlang","text"}; state.checked=!strcmp(doc->language,modes[id-XIDE_ACTION_LANGUAGE_C]); }
    else if(d->check==STARTUP_CHECK) state.checked=project && startup==project;
    if(id!=XIDE_ACTION_STOP && xideActionsHasModal(a)) { state.reason="请先完成或关闭当前设置窗口"; return state; }
    int loaded=a->workspaceView && a->workspaceModel.source,available=project && project->available;
    int writable=doc && !xuiCodeEditIsReadonly(doc->editor); const char* compiler=xideTaskViewCompiler(a);
    switch(d->need) {
    case SOURCE_DECLARATION: state.enabled=xideCatalogSourceAvailable(a,0); state.reason="请先选择命令"; break;
    case SOURCE_DOCUMENT: state.enabled=xideCatalogSourceAvailable(a,1); state.reason="当前命令没有选中的帮助文档"; break;
    case WORKBENCH: state.enabled=a->dock!=NULL; break;
    case WORKSPACE_VIEW: state.enabled=a->workspaceView!=NULL; break;
    case NEW_PROJECT: state.enabled=a->templateView && a->workspaceView && !xideWorkspaceViewHasDraft(a); state.reason="请先应用或取消项目属性修改"; break;
    case WORKSPACE: state.enabled=loaded; state.reason="请先打开或新建工作空间"; break;
    case PROJECT: state.enabled=a->workspaceView && project; state.reason="请选择项目"; break;
    case AVAILABLE_PROJECT: state.enabled=a->workspaceView && available; state.reason="请选择可用项目"; break;
    case RESOURCES: state.enabled=a->resourceView && available && project->resources.count && !xideWorkspaceViewHasDraft(a); state.reason="请选择含资源配置的可用项目并先应用项目属性"; break;
    case NEW_RESOURCE: state.enabled=a->resourceView && available && !xideWorkspaceViewHasDraft(a); state.reason="请选择可用项目并先应用属性草稿"; break;
    case REFERENCES: state.enabled=a->referencesView && available && !xideWorkspaceViewHasDraft(a); state.reason="请选择可用项目并先应用项目属性"; break;
    case ADD_FILE: state.enabled=loaded && (!project || available); state.reason="请先打开工作空间并选择可用目标"; break;
    case REMOVE_FILE: state.enabled=xideWorkspaceViewCanRemoveFile(a); state.reason="请选择文件引用"; break;
    case RENAME_SELECTED_FILE: state.enabled=xideWorkspaceViewCanRenameFile(a); state.reason="请选择可用的普通文件，并先处理项目属性"; break;
    case IMPORT_VIEW: state.enabled=a->importView && a->workspaceView && !xideWorkspaceViewHasDraft(a); state.reason="请先处理项目属性"; break;
    case MIGRATE_PROJECT: state.enabled=xideRenameViewCanMigrateProject(a); state.reason="请选择可用项目，并先处理项目属性"; break;
    case PROPERTIES: state.enabled=available && xideWorkspaceViewHasDraft(a); state.reason="当前项目没有待应用属性"; break;
    case DOCUMENTS: state.enabled=a->documents.context!=NULL; break;
    case DOCUMENT: state.enabled=doc!=NULL; state.reason="请先打开或新建文档"; break;
    case GOTO_VIEW: state.enabled=doc && a->gotoView; state.reason="请先打开或新建文档"; break;
    case NAMED_DOCUMENT: state.enabled=doc && doc->path; state.reason="未命名文档没有磁盘文件可重载"; break;
    case RENAME_DOCUMENT: state.enabled=xideRenameViewCanOpen(a); state.reason="请打开已命名的普通文件，并先处理项目属性；工程清单使用专用迁移操作"; break;
    case DIRTY_DOCUMENTS: state.enabled=xideDocumentsDirty(&a->documents); state.reason="没有待保存文档"; break;
    case UNDO: state.enabled=writable && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(doc->editor)); state.reason="当前文档没有可撤销编辑"; break;
    case REDO: state.enabled=writable && xuiCodeDocumentCanRedo(xuiCodeEditGetDocument(doc->editor)); state.reason="当前文档没有可重做编辑"; break;
    case WRITABLE_DOCUMENT: state.enabled=writable; state.reason="请选择可编辑文档"; break;
    case SELECTED_TEXT: state.enabled=doc && xuiCodeSelectionHasSelection(xuiCodeEditGetSelection(doc->editor)); state.reason="请先选择要复制的正文"; break;
    case WRITABLE_SELECTION: state.enabled=writable && xuiCodeSelectionHasSelection(xuiCodeEditGetSelection(doc->editor)); state.reason="请在可编辑文档中选择正文"; break;
    case NONEMPTY_DOCUMENT: state.enabled=doc && xuiCodeDocumentGetLength(xuiCodeEditGetDocument(doc->editor))>0; state.reason="当前文档没有正文"; break;
    case LINE_COMMENT: {
        xui_code_language_t language={0};
        state.enabled=writable && xuiCodeLanguageRegistryFind(xuiCodeEditGetLanguageRegistry(doc->editor),doc->language,&language)==XUI_OK && language.sLineComment && *language.sLineComment;
        state.reason="当前文档只读或当前语法没有行注释"; break;
    }
    case COMPLETION: state.enabled=writable && strcmp(doc->language,"text") && a->catalogView; state.reason="当前文档没有目录补全"; break;
    case BUILD: state.enabled=a->tasks && compiler && *compiler && available; state.reason="请选择可用项目和编译器"; break;
    case RUN: state.enabled=a->tasks && compiler && *compiler && startup && startup->available && !strcmp(startup->kind,"exe"); state.reason="运行需要可用的 exe 启动项目和编译器"; break;
    case TEST: {
        const char* id=a->configurationId ? a->configurationId : available ? project->activeConfiguration : NULL;
        xide_configuration* config=available ? xideProjectFindConfiguration(project,id) : NULL;
        if(!config && available && (!id || !*id) && project->configurationCount) config=&project->configurations[0];
        state.enabled=a->tasks && config && config->test.program && *config->test.program && xidePathHasExtension(config->test.program,".exe");
        state.reason="请为所选项目的当前配置填写测试程序"; break;
    }
    case STOP: state.enabled=a->tasks && xideTasksBusy(a->tasks); state.reason="没有活动或排队任务"; break;
    case INPUT: state.enabled=xideTaskInputOpen(xideTaskViewSelected(a)); state.reason="当前所选任务没有开放的会话输入"; break;
    case TASK_VIEW: state.enabled=a->taskView!=NULL; break;
    case FONT_VIEW: state.enabled=a->fontView!=NULL; break;
    case RECOVERY_VIEW: state.enabled=a->recoveryView!=NULL; break;
    case TOOLCHAIN_VIEW: state.enabled=xideToolchainViewCanOpen(a); state.reason="请先选择编译器，或等待上次查询结束"; break;
    case SAVED_CHECK: state.enabled=xideLanguageViewCanCheck(a); state.reason="请选择保存后的 .xl 文档与编译器，并等待上次检查结束"; break;
    case CURRENT_CHECK: state.enabled=xideLanguageViewCanCheckCurrent(a); state.reason="请选择命名的 C/xlang 文档并配置相应语言服务，先处理外部冲突"; break;
    case HOVER: state.enabled=xideLanguageViewCanHover(a); state.reason="请先完成当前文档分析，并配置支持语义索引的语言服务"; break;
    case DEFINITION: state.enabled=xideLanguageViewCanDefine(a); state.reason="请先完成当前文档分析，并配置支持语义索引的语言服务"; break;
    case CANCEL_CHECK: state.enabled=xideLanguageViewCanCancel(a); state.reason="没有正在进行或自动复查的源码检查"; break;
    case CHANGES_VIEW: state.enabled=a->fileChangesView!=NULL; break;
    }
    if(state.enabled) state.reason=NULL;
    return state;
}
int xideActionExecute(xide_app* app,int id)
{
    const action_def* d=definition(id); if(!d) return XIDE_ACTION_UNKNOWN;
    if(!xideActionQuery(app,id).enabled) return XIDE_ACTION_DISABLED;
    if(!app->actions->depth) xideMessageBeginAction(app,id);
    ++app->actions->depth; d->execute(app,id); --app->actions->depth;
    if(!app->actions->depth) xideMessageEndOperation(app);
    xideActionsRefresh(app); xgeRenderRequest(); return XIDE_ACTION_DISPATCHED;
}
void xideDispatchAction(xide_app* app,int id) { (void)xideActionExecute(app,id); }
static int command(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user; xide_actions* actions=app->actions;
    if(!actions->depth && event->iPhase==XUI_EVENT_PHASE_CAPTURE) {
        /* A native CodeEdit shortcut/context menu targets its own document.
           Plain fields, read-only help/demos and find inputs keep SDK behavior. */
        for(size_t j=0;j<app->documents.count;++j) {
            xide_document* doc=app->documents.items[j];
            if(!doc->open || event->pTarget!=doc->editor) continue;
            for(size_t i=0;i<xideActionCount();++i) if(definitions[i].info.codeCommand==event->iCommand && event->iCommand) {
                app->activeDocument=doc; xideActionExecute(app,definitions[i].info.id); return XUI_EVENT_DISPATCH_STOP;
            }
        }
    }
    return actions->commandHandler ? actions->commandHandler(widget,event,actions->commandUser) : XUI_OK;
}
int xideActionsInit(xide_app* app)
{
    if(!app || !app->context || !app->root) return XUI_ERROR_INVALID_ARGUMENT;
    if(app->actions) return XUI_OK;
    app->actions=calloc(1,sizeof(*app->actions)); if(!app->actions) return XUI_ERROR_OUT_OF_MEMORY;
    int ret=xuiWidgetGetEventHandler(app->root,XUI_EVENT_COMMAND,&app->actions->commandHandler,&app->actions->commandUser);
    if(ret==XUI_OK) ret=xuiWidgetSetEventHandler(app->root,XUI_EVENT_COMMAND,command,app);
    if(ret!=XUI_OK) { free(app->actions); app->actions=NULL; }
    return ret;
}
void xideActionsDestroy(xide_app* app)
{
    if(!app || !app->actions) return;
    binding* b=app->actions->bindings; while(b) { binding* next=b->next; free(b); b=next; }
    free(app->actions); app->actions=NULL;
}
static int bind(xide_app* app,xui_widget widget,int kind,int id)
{
    for(binding* b=app->actions->bindings;b;b=b->next) if(b->widget==widget) { b->kind=kind; b->action=id; return XUI_OK; }
    binding* b=calloc(1,sizeof(*b)); if(!b) return XUI_ERROR_OUT_OF_MEMORY;
    b->next=app->actions->bindings; b->widget=widget; b->kind=kind; b->action=id; app->actions->bindings=b; return XUI_OK;
}
static void selected(xui_widget widget,int index,int id,void* user) { (void)widget; (void)index; xideDispatchAction(user,id); }
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user; for(binding* b=app->actions->bindings;b;b=b->next) if(b->widget==widget && b->kind==2) { xideDispatchAction(app,b->action); return; }
}
int xideActionsMenu(xide_app* app,xui_widget menu,const int* ids,size_t count)
{
    if(!app || !app->actions || !menu || !ids || !count || count>64) return XUI_ERROR_INVALID_ARGUMENT;
    xui_menu_item_t items[64]={0};
    for(size_t i=0;i<count;++i) {
        if(!ids[i]) { items[i].iType=XUI_MENU_ITEM_SEPARATOR; continue; }
        const action_def* d=definition(ids[i]); if(!d) return XUI_ERROR_INVALID_ARGUMENT;
        items[i].sText=d->info.title; items[i].sShortcut=d->info.shortcut; items[i].iValue=ids[i];
        items[i].iType=d->check==NO_CHECK ? XUI_MENU_ITEM_NORMAL : d->check==STARTUP_CHECK ? XUI_MENU_ITEM_CHECK : XUI_MENU_ITEM_RADIO;
    }
    int ret=xuiMenuSetItems(menu,items,(int)count);
    if(ret==XUI_OK) ret=xuiMenuSetSelect(menu,selected,app);
    if(ret==XUI_OK) ret=bind(app,menu,0,0);
    xideActionsRefresh(app); return ret;
}
int xideActionsToolbar(xide_app* app,xui_widget toolbar,const int* ids,size_t count)
{
    if(!app || !app->actions || !toolbar || !ids || !count || count>64) return XUI_ERROR_INVALID_ARGUMENT;
    xui_toolbar_item_t items[64]={0};
    for(size_t i=0;i<count;++i) {
        if(!ids[i]) { items[i].iType=XUI_TOOLBAR_ITEM_SEPARATOR; continue; }
        const xide_action_info* d=xideActionFind(ids[i]); if(!d) return XUI_ERROR_INVALID_ARGUMENT;
        items[i].sText=d->toolbarTitle ? d->toolbarTitle : d->title; items[i].iValue=ids[i];
    }
    int ret=xuiToolbarSetItems(toolbar,items,(int)count);
    if(ret==XUI_OK) ret=xuiToolbarSetSelect(toolbar,selected,app);
    if(ret==XUI_OK) ret=bind(app,toolbar,1,0);
    xideActionsRefresh(app); return ret;
}
int xideActionsButton(xide_app* app,xui_widget button,int id)
{
    if(!app || !app->actions || !button || !definition(id)) return XUI_ERROR_INVALID_ARGUMENT;
    int ret=bind(app,button,2,id); if(ret==XUI_OK) ret=xuiButtonSetClick(button,clicked,app);
    xideActionsRefresh(app); return ret;
}
static xide_document* documentFor(xide_app* app,xui_widget widget)
{
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && app->documents.items[i]->editor==widget) return app->documents.items[i];
    return NULL;
}
static const action_def* codeDefinition(int command)
{ for(size_t i=0;i<xideActionCount();++i) if(command && definitions[i].info.codeCommand==command) return &definitions[i]; return NULL; }
static binding* widgetBinding(xide_app* app,xui_widget widget,int kind)
{ for(binding* b=app->actions->bindings;b;b=b->next) if(b->widget==widget && b->kind==kind) return b; return NULL; }
static xide_document* menuDocument(xide_app* app,xui_widget menu)
{ for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && xuiCodeEditGetMenuWidget(d->editor)==menu) return d; } return NULL; }
static const action_def* menuCodeDefinition(xui_widget menu,int index)
{
    const xui_menu_item_t* item=index>=0 ? xuiMenuGetItem(menu,index) : NULL;
    if(item && item->sText) for(size_t i=0;i<xideActionCount();++i) if(definitions[i].info.codeCommand && !strcmp(item->sText,definitions[i].info.title)) return &definitions[i];
    return NULL;
}
static int nativeMenuEvent(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user; binding* b=widgetBinding(app,widget,4);
    int choose=(event->iType==XUI_EVENT_POINTER_UP && event->iButton==XUI_POINTER_BUTTON_LEFT) || (event->iType==XUI_EVENT_KEY_DOWN && (event->iKey==XUI_KEY_ENTER || event->iKey==XUI_KEY_SPACE));
    const action_def* d=choose ? menuCodeDefinition(widget,xuiMenuGetHoverIndex(widget)) : NULL;
    if(d && event->iPhase==XUI_EVENT_PHASE_TARGET) {
        xide_document* doc=menuDocument(app,widget); xuiMenuClose(widget);
        if(doc) { app->activeDocument=doc; xideActionExecute(app,d->info.id); }
        return XUI_EVENT_DISPATCH_STOP;
    }
    xui_widget_event_proc original=b ? (event->iType==XUI_EVENT_KEY_DOWN ? b->keyHandler : b->pointerHandler) : NULL;
    void* data=b ? (event->iType==XUI_EVENT_KEY_DOWN ? b->keyUser : b->pointerUser) : NULL;
    return original ? original(widget,event,data) : XUI_OK;
}
static int bindNativeMenu(xide_app* app,xui_widget editor)
{
    xui_widget menu=xuiCodeEditGetMenuWidget(editor); if(!menu || widgetBinding(app,menu,4)) return XUI_OK;
    int fontResult=xideUiFontBindMenu(app,menu); if(fontResult!=XUI_OK) return fontResult;
    xui_widget_event_proc key=NULL,pointer=NULL; void *keyUser=NULL,*pointerUser=NULL;
    int ret=xuiWidgetGetEventHandler(menu,XUI_EVENT_KEY_DOWN,&key,&keyUser);
    if(ret==XUI_OK) ret=xuiWidgetGetEventHandler(menu,XUI_EVENT_POINTER_UP,&pointer,&pointerUser);
    if(ret==XUI_OK) ret=bind(app,menu,4,0);
    if(ret!=XUI_OK) return ret;
    binding* b=app->actions->bindings; b->keyHandler=key; b->keyUser=keyUser; b->pointerHandler=pointer; b->pointerUser=pointerUser;
    ret=xuiWidgetSetEventHandler(menu,XUI_EVENT_KEY_DOWN,nativeMenuEvent,app);
    if(ret==XUI_OK) ret=xuiWidgetSetEventHandler(menu,XUI_EVENT_POINTER_UP,nativeMenuEvent,app);
    if(ret!=XUI_OK) {
        xuiWidgetSetEventHandler(menu,XUI_EVENT_KEY_DOWN,key,keyUser); xuiWidgetSetEventHandler(menu,XUI_EVENT_POINTER_UP,pointer,pointerUser);
        app->actions->bindings=b->next; free(b);
    }
    return ret;
}
static int editorContext(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user; binding* b=widgetBinding(app,widget,3);
    int ret=b && b->contextHandler ? b->contextHandler(widget,event,b->contextUser) : XUI_OK;
    int attached=bindNativeMenu(app,widget); return attached==XUI_OK ? ret : attached;
}
static int editorKey(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user; binding* b=NULL;
    for(binding* item=app->actions->bindings;item;item=item->next) if(item->widget==widget && item->kind==3) { b=item; break; }
    if(event->iPhase==XUI_EVENT_PHASE_TARGET) {
        int key=event->iKey; if(key>='a' && key<='z') key-=('a'-'A');
        for(size_t i=0;i<xideActionCount();++i) {
            const xide_action_info* action=&definitions[i].info;
            if(action->shortcutScope!=XIDE_SHORTCUT_EDITOR || !action->key || key!=action->key || event->iModifiers!=action->modifiers) continue;
            xide_document* doc=documentFor(app,widget); if(doc) { app->activeDocument=doc; xideActionExecute(app,action->id); }
            /* The pinned SDK handles focus-target keys before root hotkeys.
               Consume registered editor chords even when disabled. */
            return XUI_EVENT_DISPATCH_STOP;
        }
    }
    return b && b->keyHandler ? b->keyHandler(widget,event,b->keyUser) : XUI_OK;
}
static int editorCommand(xui_widget widget,int command,const void* data,int* handled,void* user)
{
    (void)data; xide_app* app=user; const action_def* d=codeDefinition(command);
    if(handled) *handled=0;
    if(!d || !handled) return XUI_ERROR_UNSUPPORTED;
    if(app->actions->depth) return XUI_OK; /* Registry's own native command. */
    xide_document* doc=documentFor(app,widget); if(doc) { app->activeDocument=doc; xideActionExecute(app,d->info.id); }
    *handled=1; return XUI_OK;
}
static int editorCommandEnabled(xui_widget widget,int command,int* enabled,void* user)
{
    xide_app* app=user; const action_def* d=codeDefinition(command); if(!d || !enabled) return XUI_ERROR_UNSUPPORTED;
    xide_document* saved=app->activeDocument; app->activeDocument=documentFor(app,widget);
    *enabled=xideActionQuery(app,d->info.id).enabled; app->activeDocument=saved; return XUI_OK;
}
int xideActionsBindEditor(xide_app* app,xui_widget editor)
{
    if(!app || !app->actions || !editor) return XUI_ERROR_INVALID_ARGUMENT;
    binding* b=NULL; for(binding* item=app->actions->bindings;item;item=item->next) if(item->widget==editor && item->kind==3) { b=item; break; }
    if(!b) {
        xui_widget_event_proc original=NULL; void* data=NULL;
        int ret=xuiWidgetGetEventHandler(editor,XUI_EVENT_KEY_DOWN,&original,&data); if(ret!=XUI_OK) return ret;
        ret=bind(app,editor,3,0); if(ret!=XUI_OK) return ret;
        b=app->actions->bindings; b->keyHandler=original; b->keyUser=data;
        ret=xuiWidgetGetEventHandler(editor,XUI_EVENT_CONTEXT_MENU,&b->contextHandler,&b->contextUser);
        if(ret==XUI_OK) ret=xuiWidgetSetEventHandler(editor,XUI_EVENT_KEY_DOWN,editorKey,app);
        if(ret==XUI_OK) ret=xuiWidgetSetEventHandler(editor,XUI_EVENT_CONTEXT_MENU,editorContext,app);
        if(ret!=XUI_OK) {
            xuiWidgetSetEventHandler(editor,XUI_EVENT_KEY_DOWN,b->keyHandler,b->keyUser); xuiWidgetSetEventHandler(editor,XUI_EVENT_CONTEXT_MENU,b->contextHandler,b->contextUser);
            app->actions->bindings=b->next; free(b); return ret;
        }
    }
    xui_code_provider_set providers=xuiCodeEditGetProviders(editor);
    int ret=xuiCodeProviderSetCommand(providers,editorCommand,app);
    if(ret==XUI_OK) ret=xuiCodeProviderSetCommandEnabled(providers,editorCommandEnabled,app);
    for(size_t i=0;ret==XUI_OK && i<xideActionCount();++i) if(definitions[i].info.codeCommand) {
        /* Native edit commands need not have a context-menu title slot. */
        const char* title=xuiCodeEditGetMenuTitle(editor,definitions[i].info.codeCommand);
        if(title && *title) ret=xuiCodeEditSetMenuTitle(editor,definitions[i].info.codeCommand,definitions[i].info.title);
    }
    return ret;
}
void xideActionsRefresh(xide_app* app)
{
    if(!app || !app->actions || !app->context) return;
    for(binding* b=app->actions->bindings;b;b=b->next) {
        if(b->kind==3) { int ret=bindNativeMenu(app,b->widget); if(ret!=XUI_OK) app->frameError=ret; continue; }
        if(b->kind==4) {
            xide_document* saved=app->activeDocument; app->activeDocument=menuDocument(app,b->widget);
            for(int i=0;i<xuiMenuGetItemCount(b->widget);++i) {
                const action_def* d=menuCodeDefinition(b->widget,i); if(!d) continue;
                uint32_t wanted=xideActionQuery(app,d->info.id).enabled ? XUI_MENU_ITEM_ENABLED : 0;
                if((xuiMenuGetItemState(b->widget,i)&XUI_MENU_ITEM_ENABLED)!=wanted) xuiMenuSetItemState(b->widget,i,XUI_MENU_ITEM_ENABLED,wanted);
            }
            app->activeDocument=saved; continue;
        }
        if(b->kind==2) {
            int enabled=xideActionQuery(app,b->action).enabled;
            if(xuiWidgetGetEnabled(b->widget)!=enabled) xuiWidgetSetEnabled(b->widget,enabled);
            const uint32_t priorities[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
            uint32_t input=xuiWidgetGetInputState(b->widget),wanted=0;
            for(size_t i=0;i<sizeof(priorities)/sizeof(*priorities);++i) if(input&priorities[i]) { wanted=priorities[i]; break; }
            if(xuiWidgetGetStateId(b->widget)!=wanted) xuiWidgetSetStateId(b->widget,wanted);
            continue;
        }
        int count=b->kind ? xuiToolbarGetItemCount(b->widget) : xuiMenuGetItemCount(b->widget);
        for(int i=0;i<count;++i) {
            int id;
            if(b->kind) { const xui_toolbar_item_t* item=xuiToolbarGetItem(b->widget,i); id=item ? item->iValue : 0; }
            else { const xui_menu_item_t* item=xuiMenuGetItem(b->widget,i); id=item ? item->iValue : 0; }
            if(!definition(id)) continue;
            xide_action_state state=xideActionQuery(app,id);
            if(b->kind) { if(xuiToolbarIsItemEnabled(b->widget,i)!=state.enabled) xuiToolbarSetItemEnabled(b->widget,i,state.enabled); if(xuiToolbarGetItemChecked(b->widget,i)!=state.checked) xuiToolbarSetItemChecked(b->widget,i,state.checked); }
            else { uint32_t mask=XUI_MENU_ITEM_ENABLED|XUI_MENU_ITEM_CHECKED,wanted=(state.enabled ? XUI_MENU_ITEM_ENABLED : 0)|(state.checked ? XUI_MENU_ITEM_CHECKED : 0); if((xuiMenuGetItemState(b->widget,i)&mask)!=wanted) xuiMenuSetItemState(b->widget,i,mask,wanted); }
        }
    }
}
