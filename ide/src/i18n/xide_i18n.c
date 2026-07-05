#include "i18n/xide_i18n.h"

#include <string.h>

typedef struct xide_i18n_row_t {
	int iTextId;
	const char* sEn;
	const char* sZh;
	const char* sRu;
	const char* sJa;
	const char* sFr;
	const char* sEs;
} xide_i18n_row_t;

static const xide_i18n_row_t g_xideTexts[] = {
	{XIDE_TR_APP_TITLE, "xlang IDE", "xlang IDE", "xlang IDE", "xlang IDE", "xlang IDE", "xlang IDE"},
	{XIDE_TR_MENU_FILE, "File", "文件", "Файл", "ファイル", "Fichier", "Archivo"},
	{XIDE_TR_MENU_EDIT, "Edit", "编辑", "Правка", "編集", "Édition", "Editar"},
	{XIDE_TR_MENU_VIEW, "View", "视图", "Вид", "表示", "Affichage", "Ver"},
	{XIDE_TR_MENU_RUN, "Run", "运行", "Запуск", "実行", "Exécuter", "Ejecutar"},
	{XIDE_TR_MENU_TOOLS, "Tools", "工具", "Инструменты", "ツール", "Outils", "Herramientas"},
	{XIDE_TR_MENU_HELP, "Help", "帮助", "Справка", "ヘルプ", "Aide", "Ayuda"},
	{XIDE_TR_NEW_PROJECT, "New Project", "新建项目", "Новый проект", "新規プロジェクト", "Nouveau projet", "Nuevo proyecto"},
	{XIDE_TR_NEW_FILE, "New File", "新建文件", "Новый файл", "新規ファイル", "Nouveau fichier", "Nuevo archivo"},
	{XIDE_TR_OPEN, "Open", "打开", "Открыть", "開く", "Ouvrir", "Abrir"},
	{XIDE_TR_SAVE, "Save", "保存", "Сохранить", "保存", "Enregistrer", "Guardar"},
	{XIDE_TR_SAVE_ALL, "Save All", "全部保存", "Сохранить все", "すべて保存", "Tout enregistrer", "Guardar todo"},
	{XIDE_TR_CLOSE, "Close", "关闭", "Закрыть", "閉じる", "Fermer", "Cerrar"},
	{XIDE_TR_EXIT, "Exit", "退出", "Выход", "終了", "Quitter", "Salir"},
	{XIDE_TR_UNDO, "Undo", "撤销", "Отменить", "元に戻す", "Annuler", "Deshacer"},
	{XIDE_TR_REDO, "Redo", "重做", "Повторить", "やり直す", "Rétablir", "Rehacer"},
	{XIDE_TR_CUT, "Cut", "剪切", "Вырезать", "切り取り", "Couper", "Cortar"},
	{XIDE_TR_COPY, "Copy", "复制", "Копировать", "コピー", "Copier", "Copiar"},
	{XIDE_TR_PASTE, "Paste", "粘贴", "Вставить", "貼り付け", "Coller", "Pegar"},
	{XIDE_TR_FIND, "Find", "查找", "Найти", "検索", "Rechercher", "Buscar"},
	{XIDE_TR_HOME, "Home", "主页", "Главная", "ホーム", "Accueil", "Inicio"},
	{XIDE_TR_PROJECT, "Project", "项目", "Проект", "プロジェクト", "Projet", "Proyecto"},
	{XIDE_TR_COMMAND_LIST, "Command List", "命令列表", "Список команд", "コマンド一覧", "Liste des commandes", "Lista de comandos"},
	{XIDE_TR_HELP, "Help", "帮助", "Справка", "ヘルプ", "Aide", "Ayuda"},
	{XIDE_TR_OUTPUT, "Output", "输出", "Вывод", "出力", "Sortie", "Salida"},
	{XIDE_TR_TASKS, "Tasks", "任务", "Задачи", "タスク", "Tâches", "Tareas"},
	{XIDE_TR_DEBUG, "Debug", "调试", "Отладка", "デバッグ", "Débogage", "Depurar"},
	{XIDE_TR_TOOLS, "Tools", "工具", "Инструменты", "ツール", "Outils", "Herramientas"},
	{XIDE_TR_PROPERTIES, "Properties", "属性", "Свойства", "プロパティ", "Propriétés", "Propiedades"},
	{XIDE_TR_RESET_LAYOUT, "Reset Layout", "重置布局", "Сбросить макет", "レイアウトをリセット", "Réinitialiser la disposition", "Restablecer diseño"},
	{XIDE_TR_RUN, "Run", "运行", "Запуск", "実行", "Exécuter", "Ejecutar"},
	{XIDE_TR_BUILD, "Build", "构建", "Сборка", "ビルド", "Compiler", "Compilar"},
	{XIDE_TR_STOP, "Stop", "停止", "Остановить", "停止", "Arrêter", "Detener"},
	{XIDE_TR_OPTIONS, "Options", "选项", "Параметры", "オプション", "Options", "Opciones"},
	{XIDE_TR_TERMINAL, "Terminal", "终端", "Терминал", "ターミナル", "Terminal", "Terminal"},
	{XIDE_TR_PACKAGE_MANAGER, "Package Manager", "包管理器", "Менеджер пакетов", "パッケージ管理", "Gestionnaire de paquets", "Gestor de paquetes"},
	{XIDE_TR_XAPI_INDEX, "XAPI Index", "XAPI 索引", "Индекс XAPI", "XAPI インデックス", "Index XAPI", "Índice XAPI"},
	{XIDE_TR_DOCUMENTATION, "Documentation", "文档", "Документация", "ドキュメント", "Documentation", "Documentación"},
	{XIDE_TR_SHORTCUTS, "Shortcuts", "快捷键", "Сочетания клавиш", "ショートカット", "Raccourcis", "Atajos"},
	{XIDE_TR_ABOUT, "About", "关于", "О программе", "情報", "À propos", "Acerca de"},
	{XIDE_TR_TOOLTIP_NEW_FILE, "New file", "新建文件", "Новый файл", "新規ファイル", "Nouveau fichier", "Nuevo archivo"},
	{XIDE_TR_TOOLTIP_OPEN_FILE, "Open file", "打开文件", "Открыть файл", "ファイルを開く", "Ouvrir un fichier", "Abrir archivo"},
	{XIDE_TR_TOOLTIP_SAVE_FILE, "Save file", "保存文件", "Сохранить файл", "ファイルを保存", "Enregistrer le fichier", "Guardar archivo"},
	{XIDE_TR_TOOLTIP_RUN_CURRENT_FILE, "Run current file", "运行当前文件", "Запустить текущий файл", "現在のファイルを実行", "Exécuter le fichier courant", "Ejecutar archivo actual"},
	{XIDE_TR_TOOLTIP_BUILD_PROJECT, "Build project", "构建项目", "Собрать проект", "プロジェクトをビルド", "Compiler le projet", "Compilar proyecto"},
	{XIDE_TR_TOOLTIP_START_DEBUG, "Start debug", "开始调试", "Начать отладку", "デバッグ開始", "Démarrer le débogage", "Iniciar depuración"},
	{XIDE_TR_TOOLTIP_STOP_TASK, "Stop task", "停止任务", "Остановить задачу", "タスクを停止", "Arrêter la tâche", "Detener tarea"},
	{XIDE_TR_STATUS_READY, "Ready", "就绪", "Готово", "準備完了", "Prêt", "Listo"},
	{XIDE_TR_STATUS_SKELETON, "xlang IDE skeleton", "xlang IDE 骨架", "Каркас xlang IDE", "xlang IDE スケルトン", "Squelette xlang IDE", "Esqueleto xlang IDE"},
	{XIDE_TR_STATUS_CURSOR, "Ln 1 Col 1", "第 1 行 第 1 列", "Стр 1 Стлб 1", "1 行 1 列", "Ln 1 Col 1", "Lín 1 Col 1"},
	{XIDE_TR_HOME_BODY, "Minimal skeleton: project tree, command list, editor, output, tasks, debug, tools, properties.", "最小骨架：项目树、命令列表、编辑器、输出、任务、调试、工具、属性。", "Минимальный каркас: дерево проекта, команды, редактор, вывод, задачи, отладка, инструменты, свойства.", "最小スケルトン: プロジェクトツリー、コマンド一覧、エディタ、出力、タスク、デバッグ、ツール、プロパティ。", "Squelette minimal : projet, commandes, éditeur, sortie, tâches, débogage, outils, propriétés.", "Esqueleto mínimo: proyecto, comandos, editor, salida, tareas, depuración, herramientas, propiedades."},
	{XIDE_TR_HELP_BODY, "Integrated Help\n\nXAPI metadata and generated documentation will be mounted here.", "集成帮助\n\n这里将接入 XAPI 元数据和自动生成文档。", "Встроенная справка\n\nЗдесь будут XAPI-метаданные и сгенерированная документация.", "統合ヘルプ\n\nここに XAPI メタデータと生成ドキュメントを接続します。", "Aide intégrée\n\nLes métadonnées XAPI et la documentation générée seront montées ici.", "Ayuda integrada\n\nAquí se montarán metadatos XAPI y documentación generada."},
	{XIDE_TR_DEBUG_BODY, "Debug\n\nBreakpoints, stack, variables and DAP views will be connected here.", "调试\n\n这里将接入断点、调用栈、变量和 DAP 视图。", "Отладка\n\nЗдесь будут точки останова, стек, переменные и DAP.", "デバッグ\n\nブレークポイント、スタック、変数、DAP ビューを接続します。", "Débogage\n\nPoints d'arrêt, pile, variables et vues DAP seront connectés ici.", "Depuración\n\nAquí irán puntos de ruptura, pila, variables y vistas DAP."},
	{XIDE_TR_TOOLS_BODY, "Tools\n\nOptions, package manager, terminal and XAPI index entry points.", "工具\n\n选项、包管理器、终端和 XAPI 索引入口。", "Инструменты\n\nПараметры, менеджер пакетов, терминал и индекс XAPI.", "ツール\n\nオプション、パッケージ管理、ターミナル、XAPI インデックス。", "Outils\n\nOptions, gestionnaire de paquets, terminal et index XAPI.", "Herramientas\n\nOpciones, gestor de paquetes, terminal e índice XAPI."},
	{XIDE_TR_PROPERTIES_BODY, "Properties\n\nCurrent file, symbol, project and debugger properties.", "属性\n\n当前文件、符号、项目和调试器属性。", "Свойства\n\nТекущий файл, символ, проект и свойства отладчика.", "プロパティ\n\n現在のファイル、シンボル、プロジェクト、デバッガのプロパティ。", "Propriétés\n\nFichier courant, symbole, projet et propriétés du débogueur.", "Propiedades\n\nArchivo actual, símbolo, proyecto y propiedades del depurador."},
	{XIDE_TR_OUTPUT_INITIAL, "xlang IDE output\n> xl run main.xl\nready\n", "xlang IDE 输出\n> xl run main.xl\n就绪\n", "Вывод xlang IDE\n> xl run main.xl\nготово\n", "xlang IDE 出力\n> xl run main.xl\n準備完了\n", "Sortie xlang IDE\n> xl run main.xl\nprêt\n", "Salida xlang IDE\n> xl run main.xl\nlisto\n"},
	{XIDE_TR_TREE_WORKSPACE, "workspace", "工作区", "рабочая область", "ワークスペース", "espace de travail", "espacio de trabajo"},
	{XIDE_TR_TREE_PROJECT, "project", "项目", "проект", "プロジェクト", "projet", "proyecto"},
	{XIDE_TR_TREE_LANGUAGE, "Language", "语言", "Язык", "言語", "Langage", "Lenguaje"},
	{XIDE_TR_TREE_STANDARD_LIBRARY, "Standard Library", "标准库", "Стандартная библиотека", "標準ライブラリ", "Bibliothèque standard", "Biblioteca estándar"},
	{XIDE_TR_TASK_STATE, "State", "状态", "Состояние", "状態", "État", "Estado"},
	{XIDE_TR_TASK_COMMAND, "Command", "命令", "Команда", "コマンド", "Commande", "Comando"},
	{XIDE_TR_TASK_TARGET, "Target", "目标", "Цель", "対象", "Cible", "Destino"},
	{XIDE_TR_TASK_MESSAGE, "Message", "消息", "Сообщение", "メッセージ", "Message", "Mensaje"},
	{XIDE_TR_TASK_IDLE, "idle", "空闲", "ожидание", "待機", "inactif", "inactivo"},
	{XIDE_TR_TASK_WAITING, "waiting", "等待中", "ожидание", "待機中", "en attente", "esperando"},
	{XIDE_TR_TASK_NOT_CONNECTED, "not connected", "未连接", "не подключено", "未接続", "non connecté", "no conectado"}
};

static const xide_i18n_row_t* xideFindText(int iTextId)
{
	int i;
	for ( i = 0; i < (int)(sizeof(g_xideTexts) / sizeof(g_xideTexts[0])); i++ ) {
		if ( g_xideTexts[i].iTextId == iTextId ) return &g_xideTexts[i];
	}
	return NULL;
}

int xideI18nLanguageFromCode(const char* sCode)
{
	if ( sCode == NULL || sCode[0] == '\0' ) return -1;
	if ( strcmp(sCode, "en") == 0 || strcmp(sCode, "en-US") == 0 ) return XUI_LANGUAGE_EN;
	if ( strcmp(sCode, "zh") == 0 || strcmp(sCode, "zh-CN") == 0 || strcmp(sCode, "cn") == 0 ) return XUI_LANGUAGE_ZH;
	if ( strcmp(sCode, "ru") == 0 || strcmp(sCode, "ru-RU") == 0 ) return XUI_LANGUAGE_RU;
	if ( strcmp(sCode, "ja") == 0 || strcmp(sCode, "ja-JP") == 0 || strcmp(sCode, "jp") == 0 ) return XUI_LANGUAGE_JA;
	if ( strcmp(sCode, "fr") == 0 || strcmp(sCode, "fr-FR") == 0 ) return XUI_LANGUAGE_FR;
	if ( strcmp(sCode, "es") == 0 || strcmp(sCode, "es-ES") == 0 ) return XUI_LANGUAGE_ES;
	return -1;
}

const char* xideI18nLanguageCode(int iLanguageId)
{
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: return "en";
	case XUI_LANGUAGE_ZH: return "zh";
	case XUI_LANGUAGE_RU: return "ru";
	case XUI_LANGUAGE_JA: return "ja";
	case XUI_LANGUAGE_FR: return "fr";
	case XUI_LANGUAGE_ES: return "es";
	default: return "zh";
	}
}

const char* xideI18nLanguageName(int iLanguageId)
{
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: return "English";
	case XUI_LANGUAGE_ZH: return "Chinese";
	case XUI_LANGUAGE_RU: return "Russian";
	case XUI_LANGUAGE_JA: return "Japanese";
	case XUI_LANGUAGE_FR: return "French";
	case XUI_LANGUAGE_ES: return "Spanish";
	default: return "Chinese";
	}
}

const char* xideTrLang(int iLanguageId, int iTextId)
{
	const xide_i18n_row_t* row = xideFindText(iTextId);
	const char* sText = NULL;
	if ( row == NULL ) return "";
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: sText = row->sEn; break;
	case XUI_LANGUAGE_ZH: sText = row->sZh; break;
	case XUI_LANGUAGE_RU: sText = row->sRu; break;
	case XUI_LANGUAGE_JA: sText = row->sJa; break;
	case XUI_LANGUAGE_FR: sText = row->sFr; break;
	case XUI_LANGUAGE_ES: sText = row->sEs; break;
	default: sText = row->sZh; break;
	}
	return (sText != NULL && sText[0] != '\0') ? sText : row->sEn;
}

const char* xideTr(xui_context pContext, int iTextId)
{
	int language = pContext != NULL ? xuiGetLanguage(pContext) : xuiGetDefaultLanguage();
	return xideTrLang(language, iTextId);
}

int xideI18nSetLanguage(xui_context pContext, int iLanguageId)
{
	if ( iLanguageId < XUI_LANGUAGE_EN || iLanguageId >= XUI_LANGUAGE_COUNT ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiSetLanguage(pContext, iLanguageId);
}
