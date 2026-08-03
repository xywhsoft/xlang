#include "xui_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define XUI_CONTEXT_REQUIRED_CAPS \
	(XUI_PROXY_CAP_SURFACE_TARGET | \
	 XUI_PROXY_CAP_SURFACE_READ | \
	 XUI_PROXY_CAP_SURFACE_QUAD | \
	 XUI_PROXY_CAP_SURFACE_CLEAR_RECT | \
	 XUI_PROXY_CAP_SURFACE_SAMPLER | \
	 XUI_PROXY_CAP_DRAW_CONTEXT | \
	 XUI_PROXY_CAP_SHAPE | \
	 XUI_PROXY_CAP_FONT_TTF | \
	 XUI_PROXY_CAP_FONT_XRF | \
	 XUI_PROXY_CAP_TEXT | \
	 XUI_PROXY_CAP_PATH_FILL | \
	 XUI_PROXY_CAP_PATH_STROKE | \
	 XUI_PROXY_CAP_PATH_DASH | \
	 XUI_PROXY_CAP_PATH_AA)

struct xui_resource_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_resource pNext;
	char* sName;
	int iKind;
	void* pHandle;
	void* pUser;
	void (*onDestroy)(xui_context pContext, void* pHandle, void* pUser);
	uint32_t iGeneration;
	int iRefCount;
	xui_resource_dependency_t* pDependencies;
};

struct xui_resource_dependency_t {
	xui_resource pResource;
	xui_resource_dependency_t* pNext;
};

struct xui_path_t {
	uint32_t iMagic;
	xui_path_command_t* pCommands;
	int iCommandCount;
	int iCommandCapacity;
};

typedef struct xui_vector_icon_def_t {
	const char* sName;
	const char* sPath;
	float fViewWidth;
	float fViewHeight;
} xui_vector_icon_def_t;

typedef struct xui_i18n_builtin_entry_t {
	int iTextId;
	const char* sEN;
	const char* sZH;
	const char* sRU;
	const char* sJA;
	const char* sFR;
	const char* sES;
} xui_i18n_builtin_entry_t;

static const xui_vector_icon_def_t g_arrXuiVectorIcons[] = {
	{"check", "M20 6 L9 17 L4 12", 24.0f, 24.0f},
	{"close", "M6 6 L18 18 M18 6 L6 18", 24.0f, 24.0f},
	{"chevron_down", "M6 9 L12 15 L18 9", 24.0f, 24.0f},
	{"search", "M10 18 C5.58 18 2 14.42 2 10 C2 5.58 5.58 2 10 2 C14.42 2 18 5.58 18 10 C18 12.1 17.19 14 15.88 15.88 L22 22", 24.0f, 24.0f},
	{"user", "M20 21 C20 17.69 16.42 15 12 15 C7.58 15 4 17.69 4 21 M12 12 C9.79 12 8 10.21 8 8 C8 5.79 9.79 4 12 4 C14.21 4 16 5.79 16 8 C16 10.21 14.21 12 12 12", 24.0f, 24.0f},
	{"lock", "M7 11 V8 C7 5.24 9.24 3 12 3 C14.76 3 17 5.24 17 8 V11 M6 11 H18 V21 H6 Z", 24.0f, 24.0f},
	{"eye", "M2 12 C5 7 8.5 5 12 5 C15.5 5 19 7 22 12 C19 17 15.5 19 12 19 C8.5 19 5 17 2 12 Z M12 15 C10.34 15 9 13.66 9 12 C9 10.34 10.34 9 12 9 C13.66 9 15 10.34 15 12 C15 13.66 13.66 15 12 15 Z", 24.0f, 24.0f}
};

static const xui_i18n_builtin_entry_t g_arrXuiBuiltinText[] = {
	{XUI_TR_EDIT_UNDO, "Undo", "\xE6\x92\xA4\xE9\x94\x80", "\xD0\x9E\xD1\x82\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C", "\xE5\x85\x83\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99", "Annuler", "Deshacer"},
	{XUI_TR_EDIT_REDO, "Redo", "\xE9\x87\x8D\xE5\x81\x9A", "\xD0\x9F\xD0\xBE\xD0\xB2\xD1\x82\xD0\xBE\xD1\x80\xD0\xB8\xD1\x82\xD1\x8C", "\xE3\x82\x84\xE3\x82\x8A\xE7\x9B\xB4\xE3\x81\x97", "Retablir", "Rehacer"},
	{XUI_TR_EDIT_CUT, "Cut", "\xE5\x89\xAA\xE5\x88\x87", "\xD0\x92\xD1\x8B\xD1\x80\xD0\xB5\xD0\xB7\xD0\xB0\xD1\x82\xD1\x8C", "\xE5\x88\x87\xE3\x82\x8A\xE5\x8F\x96\xE3\x82\x8A", "Couper", "Cortar"},
	{XUI_TR_EDIT_COPY, "Copy", "\xE5\xA4\x8D\xE5\x88\xB6", "\xD0\x9A\xD0\xBE\xD0\xBF\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x82\xD1\x8C", "\xE3\x82\xB3\xE3\x83\x94\xE3\x83\xBC", "Copier", "Copiar"},
	{XUI_TR_EDIT_PASTE, "Paste", "\xE7\xB2\x98\xE8\xB4\xB4", "\xD0\x92\xD1\x81\xD1\x82\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C", "\xE8\xB2\xBC\xE3\x82\x8A\xE4\xBB\x98\xE3\x81\x91", "Coller", "Pegar"},
	{XUI_TR_EDIT_DELETE, "Delete", "\xE5\x88\xA0\xE9\x99\xA4", "\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C", "\xE5\x89\x8A\xE9\x99\xA4", "Supprimer", "Eliminar"},
	{XUI_TR_EDIT_SELECT_ALL, "Select All", "\xE5\x85\xA8\xE9\x80\x89", "\xD0\x92\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C \xD0\xB2\xD1\x81\xD1\x91", "\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE9\x81\xB8\xE6\x8A\x9E", "Tout selectionner", "Seleccionar todo"},
	{XUI_TR_FIND_TITLE, "Find", "\xE6\x9F\xA5\xE6\x89\xBE", "\xD0\x9D\xD0\xB0\xD0\xB9\xD1\x82\xD0\xB8", "\xE6\xA4\x9C\xE7\xB4\xA2", "Rechercher", "Buscar"},
	{XUI_TR_REPLACE_TITLE, "Replace", "\xE6\x9B\xBF\xE6\x8D\xA2", "\xD0\x97\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C", "\xE7\xBD\xAE\xE6\x8F\x9B", "Remplacer", "Reemplazar"},
	{XUI_TR_FIND_PLACEHOLDER, "Find", "\xE6\x9F\xA5\xE6\x89\xBE", "\xD0\x9D\xD0\xB0\xD0\xB9\xD1\x82\xD0\xB8", "\xE6\xA4\x9C\xE7\xB4\xA2", "Rechercher", "Buscar"},
	{XUI_TR_REPLACE_PLACEHOLDER, "Replace", "\xE6\x9B\xBF\xE6\x8D\xA2", "\xD0\x97\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C", "\xE7\xBD\xAE\xE6\x8F\x9B", "Remplacer", "Reemplazar"},
	{XUI_TR_FIND_PREVIOUS, "Prev", "\xE4\xB8\x8A\xE4\xB8\x80\xE4\xB8\xAA", "\xD0\x9D\xD0\xB0\xD0\xB7\xD0\xB0\xD0\xB4", "\xE5\x89\x8D\xE3\x81\xB8", "Precedent", "Anterior"},
	{XUI_TR_FIND_NEXT, "Next", "\xE4\xB8\x8B\xE4\xB8\x80\xE4\xB8\xAA", "\xD0\x94\xD0\xB0\xD0\xBB\xD0\xB5\xD0\xB5", "\xE6\xAC\xA1\xE3\x81\xB8", "Suivant", "Siguiente"},
	{XUI_TR_FIND_ALL, "Find All", "\xE6\x9F\xA5\xE6\x89\xBE\xE5\x85\xA8\xE9\x83\xA8", "\xD0\x9D\xD0\xB0\xD0\xB9\xD1\x82\xD0\xB8 \xD0\xB2\xD1\x81\xD0\xB5", "\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE6\xA4\x9C\xE7\xB4\xA2", "Tout rechercher", "Buscar todo"},
	{XUI_TR_REPLACE_CURRENT, "Replace", "\xE6\x9B\xBF\xE6\x8D\xA2", "\xD0\x97\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C", "\xE7\xBD\xAE\xE6\x8F\x9B", "Remplacer", "Reemplazar"},
	{XUI_TR_REPLACE_ALL, "Replace All", "\xE5\x85\xA8\xE9\x83\xA8\xE6\x9B\xBF\xE6\x8D\xA2", "\xD0\x97\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C \xD0\xB2\xD1\x81\xD0\xB5", "\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE7\xBD\xAE\xE6\x8F\x9B", "Tout remplacer", "Reemplazar todo"},
	{XUI_TR_FIND_CASE, "Case", "\xE5\x8C\xBA\xE5\x88\x86\xE5\xA4\xA7\xE5\xB0\x8F\xE5\x86\x99", "\xD0\xA0\xD0\xB5\xD0\xB3\xD0\xB8\xD1\x81\xD1\x82\xD1\x80", "\xE5\xA4\xA7\xE6\x96\x87\xE5\xAD\x97\xE5\xB0\x8F\xE6\x96\x87\xE5\xAD\x97", "Casse", "Mayusculas"},
	{XUI_TR_FIND_WORD, "Word", "\xE5\x85\xA8\xE5\xAD\x97\xE5\x8C\xB9\xE9\x85\x8D", "\xD0\xA1\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE", "\xE5\x8D\x98\xE8\xAA\x9E", "Mot entier", "Palabra"},
	{XUI_TR_FIND_REGEX, "Regex", "\xE6\xAD\xA3\xE5\x88\x99", "\xD0\xA0\xD0\xB5\xD0\xB3\xD0\xB2\xD1\x8B\xD1\x80", "\xE6\xAD\xA3\xE8\xA6\x8F\xE8\xA1\xA8\xE7\x8F\xBE", "Regex", "Regex"},
	{XUI_TR_FIND_ESCAPE, "Esc", "\xE8\xBD\xAC\xE4\xB9\x89", "\xD0\xAD\xD0\xBA\xD1\x80", "\xE3\x82\xA8\xE3\x82\xB9\xE3\x82\xB1\xE3\x83\xBC\xE3\x83\x97", "Echap.", "Escape"},
	{XUI_TR_FIND_SELECTION, "Selection", "\xE9\x80\x89\xE5\x8C\xBA", "\xD0\x92\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB5", "\xE9\x81\xB8\xE6\x8A\x9E\xE7\xAF\x84\xE5\x9B\xB2", "Selection", "Seleccion"},
	{XUI_TR_FIND_SCOPE, "Scope", "\xE8\x8C\x83\xE5\x9B\xB4", "\xD0\x9E\xD0\xB1\xD0\xBB\xD0\xB0\xD1\x81\xD1\x82\xD1\x8C", "\xE3\x82\xB9\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x97", "Portee", "Ambito"},
	{XUI_TR_FIND_COL_FILE, "File", "\xE6\x96\x87\xE4\xBB\xB6", "\xD0\xA4\xD0\xB0\xD0\xB9\xD0\xBB", "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB", "Fichier", "Archivo"},
	{XUI_TR_FIND_COL_POSITION, "Position", "\xE4\xBD\x8D\xE7\xBD\xAE", "\xD0\x9F\xD0\xBE\xD0\xB7\xD0\xB8\xD1\x86\xD0\xB8\xD1\x8F", "\xE4\xBD\x8D\xE7\xBD\xAE", "Position", "Posicion"},
	{XUI_TR_FIND_COL_CONTENT, "Content", "\xE5\x86\x85\xE5\xAE\xB9", "\xD0\xA1\xD0\xBE\xD0\xB4\xD0\xB5\xD1\x80\xD0\xB6\xD0\xB8\xD0\xBC\xD0\xBE\xD0\xB5", "\xE5\x86\x85\xE5\xAE\xB9", "Contenu", "Contenido"},
	{XUI_TR_FIND_MATCHES_FMT, "%d matches", "%d \xE4\xB8\xAA\xE5\x8C\xB9\xE9\x85\x8D\xE9\xA1\xB9", "%d \xD1\x81\xD0\xBE\xD0\xB2\xD0\xBF\xD0\xB0\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB9", "%d \xE4\xBB\xB6\xE4\xB8\x80\xE8\x87\xB4", "%d correspondances", "%d coincidencias"},
	{XUI_TR_FIND_REPLACED_FMT, "%d replaced", "\xE5\xB7\xB2\xE6\x9B\xBF\xE6\x8D\xA2 %d \xE5\xA4\x84", "%d \xD0\xB7\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB5\xD0\xBD\xD0\xBE", "%d \xE4\xBB\xB6\xE7\xBD\xAE\xE6\x8F\x9B", "%d remplacements", "%d reemplazos"},
	{XUI_TR_FIND_INVALID_PATTERN, "Invalid pattern", "\xE6\x97\xA0\xE6\x95\x88\xE6\x9F\xA5\xE6\x89\xBE\xE8\xA1\xA8\xE8\xBE\xBE\xE5\xBC\x8F", "\xD0\x9D\xD0\xB5\xD0\xB4\xD0\xBE\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xB8\xD0\xBC\xD1\x8B\xD0\xB9 \xD1\x88\xD0\xB0\xD0\xB1\xD0\xBB\xD0\xBE\xD0\xBD", "\xE7\x84\xA1\xE5\x8A\xB9\xE3\x81\xAA\xE3\x83\x91\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\xB3", "Motif invalide", "Patron no valido"},
	{XUI_TR_FIND_NOT_FOUND, "Not found", "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0", "\xD0\x9D\xD0\xB5 \xD0\xBD\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xBE", "\xE8\xA6\x8B\xE3\x81\xA4\xE3\x81\x8B\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93", "Introuvable", "No encontrado"},
	{XUI_TR_FIND_NO_SEARCH_RANGE, "No searchable range", "\xE6\xB2\xA1\xE6\x9C\x89\xE5\x8F\xAF\xE6\x9F\xA5\xE6\x89\xBE\xE8\x8C\x83\xE5\x9B\xB4", "\xD0\x9D\xD0\xB5\xD1\x82 \xD0\xBE\xD0\xB1\xD0\xBB\xD0\xB0\xD1\x81\xD1\x82\xD0\xB8 \xD0\xBF\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA\xD0\xB0", "\xE6\xA4\x9C\xE7\xB4\xA2\xE7\xAF\x84\xE5\x9B\xB2\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93", "Aucune zone de recherche", "Sin ambito de busqueda"},
	{XUI_TR_CODE_GOTO_LINE, "Go To Line", "\xE8\xBD\xAC\xE5\x88\xB0\xE8\xA1\x8C", "\xD0\x9F\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB9\xD1\x82\xD0\xB8 \xD0\xBA \xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB5", "\xE8\xA1\x8C\xE3\x81\xB8\xE7\xA7\xBB\xE5\x8B\x95", "Aller a la ligne", "Ir a linea"},
	{XUI_TR_CODE_TOGGLE_COMMENT, "Toggle Comment", "\xE5\x88\x87\xE6\x8D\xA2\xE6\xB3\xA8\xE9\x87\x8A", "\xD0\x9F\xD0\xB5\xD1\x80\xD0\xB5\xD0\xBA\xD0\xBB. \xD0\xBA\xD0\xBE\xD0\xBC\xD0\xBC\xD0\xB5\xD0\xBD\xD1\x82.", "\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\xE5\x88\x87\xE6\x9B\xBF", "Basculer commentaire", "Alternar comentario"},
	{XUI_TR_CODE_TOGGLE_FOLD, "Toggle Fold", "\xE5\x88\x87\xE6\x8D\xA2\xE6\x8A\x98\xE5\x8F\xA0", "\xD0\xA1\xD0\xB2\xD0\xB5\xD1\x80\xD0\xBD\xD1\x83\xD1\x82\xD1\x8C/\xD1\x80\xD0\xB0\xD0\xB7\xD0\xB2.", "\xE6\x8A\x98\xE3\x82\x8A\xE3\x81\x9F\xE3\x81\x9F\xE3\x81\xBF\xE5\x88\x87\xE6\x9B\xBF", "Basculer pliage", "Alternar plegado"},
	{XUI_TR_DOCK_CLOSE, "Close", "\xE5\x85\xB3\xE9\x97\xAD", "\xD0\x97\xD0\xB0\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C", "\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B", "Fermer", "Cerrar"},
	{XUI_TR_DOCK_AUTO_HIDE, "Auto hide", "\xE8\x87\xAA\xE5\x8A\xA8\xE9\x9A\x90\xE8\x97\x8F", "\xD0\x90\xD0\xB2\xD1\x82\xD0\xBE\xD1\x81\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5", "\xE8\x87\xAA\xE5\x8B\x95\xE9\x9D\x9E\xE8\xA1\xA8\xE7\xA4\xBA", "Masquer auto", "Ocultar auto"},
	{XUI_TR_DOCK_DOCK, "Dock", "\xE5\x81\x9C\xE9\x9D\xA0", "\xD0\x97\xD0\xB0\xD0\xBA\xD1\x80\xD0\xB5\xD0\xBF\xD0\xB8\xD1\x82\xD1\x8C", "\xE3\x83\x89\xE3\x83\x83\xE3\x82\xAD\xE3\x83\xB3\xE3\x82\xB0", "Ancrer", "Acoplar"},
	{XUI_TR_DOCK_OPTIONS, "Options", "\xE9\x80\x89\xE9\xA1\xB9", "\xD0\x9F\xD0\xB0\xD1\x80\xD0\xB0\xD0\xBC\xD0\xB5\xD1\x82\xD1\x80\xD1\x8B", "\xE3\x82\xAA\xE3\x83\x97\xE3\x82\xB7\xE3\x83\xA7\xE3\x83\xB3", "Options", "Opciones"},
	{XUI_TR_DOCK_MORE_TABS, "More tabs", "\xE6\x9B\xB4\xE5\xA4\x9A\xE6\xA0\x87\xE7\xAD\xBE", "\xD0\x95\xD1\x89\xD1\x91 \xD0\xB2\xD0\xBA\xD0\xBB\xD0\xB0\xD0\xB4\xD0\xBA\xD0\xB8", "\xE3\x81\x9D\xE3\x81\xAE\xE4\xBB\x96\xE3\x81\xAE\xE3\x82\xBF\xE3\x83\x96", "Plus d'onglets", "Mas pestanas"},
	{XUI_TR_DOCK_FLOAT, "Float", "\xE6\xB5\xAE\xE5\x8A\xA8", "\xD0\x9F\xD0\xBB\xD0\xB0\xD0\xB2\xD0\xB0\xD1\x8E\xD1\x89\xD0\xB5\xD0\xB5", "\xE3\x83\x95\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x88", "Flottant", "Flotar"},
	{XUI_TR_DOCK_CLOSE_OTHERS, "Close Others", "\xE5\x85\xB3\xE9\x97\xAD\xE5\x85\xB6\xE4\xBB\x96", "\xD0\x97\xD0\xB0\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C \xD0\xBE\xD1\x81\xD1\x82\xD0\xB0\xD0\xBB\xD1\x8C\xD0\xBD\xD1\x8B\xD0\xB5", "\xE4\xBB\x96\xE3\x82\x92\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B", "Fermer les autres", "Cerrar otros"},
	{XUI_TR_DOCK_CLOSE_ALL, "Close All", "\xE5\x85\xA8\xE9\x83\xA8\xE5\x85\xB3\xE9\x97\xAD", "\xD0\x97\xD0\xB0\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C \xD0\xB2\xD1\x81\xD0\xB5", "\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE9\x96\x89\xE3\x81\x98\xE3\x82\x8B", "Tout fermer", "Cerrar todo"},
	{XUI_TR_TERMINAL_CLEAR_SCREEN, "Clear Screen", "\xE6\xB8\x85\xE5\xB1\x8F", "\xD0\x9E\xD1\x87\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD1\x82\xD1\x8C \xD1\x8D\xD0\xBA\xD1\x80\xD0\xB0\xD0\xBD", "\xE7\x94\xBB\xE9\x9D\xA2\xE3\x82\x92\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xA2", "Effacer l'ecran", "Limpiar pantalla"},
	{XUI_TR_TERMINAL_CLEAR_SCROLLBACK, "Clear Scrollback", "\xE6\xB8\x85\xE7\xA9\xBA\xE5\x9B\x9E\xE6\xBB\x9A", "\xD0\x9E\xD1\x87\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD1\x82\xD1\x8C \xD0\xB1\xD1\x83\xD1\x84\xD0\xB5\xD1\x80", "\xE3\x82\xB9\xE3\x82\xAF\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\xAB\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF\xE3\x82\x92\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xA2", "Effacer l'historique", "Limpiar historial"},
	{XUI_TR_FILE_PATH, "Path", "\xE8\xB7\xAF\xE5\xBE\x84", "\xD0\x9F\xD1\x83\xD1\x82\xD1\x8C", "\xE3\x83\x91\xE3\x82\xB9", "Chemin", "Ruta"},
	{XUI_TR_FILE_TYPE, "Type", "\xE7\xB1\xBB\xE5\x9E\x8B", "\xD0\xA2\xD0\xB8\xD0\xBF", "\xE7\xA8\xAE\xE9\xA1\x9E", "Type", "Tipo"},
	{XUI_TR_FILE_CANCEL, "Cancel", "\xE5\x8F\x96\xE6\xB6\x88", "\xD0\x9E\xD1\x82\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB0", "\xE3\x82\xAD\xE3\x83\xA3\xE3\x83\xB3\xE3\x82\xBB\xE3\x83\xAB", "Annuler", "Cancelar"},
	{XUI_TR_FILE_CONFIRM_SAVE_AS, "Confirm Save As", "\xE7\xA1\xAE\xE8\xAE\xA4\xE5\x8F\xA6\xE5\xAD\x98\xE4\xB8\xBA", "\xD0\x9F\xD0\xBE\xD0\xB4\xD1\x82\xD0\xB2\xD0\xB5\xD1\x80\xD0\xB4\xD0\xB8\xD1\x82\xD1\x8C \xD0\xA1\xD0\xBE\xD1\x85\xD1\x80. \xD0\xBA\xD0\xB0\xD0\xBA", "\xE5\x90\x8D\xE5\x89\x8D\xE3\x82\x92\xE4\xBB\x98\xE3\x81\x91\xE3\x81\xA6\xE4\xBF\x9D\xE5\xAD\x98\xE3\x81\xAE\xE7\xA2\xBA\xE8\xAA\x8D", "Confirmer Enregistrer sous", "Confirmar Guardar como"},
	{XUI_TR_FILE_PATH_NOT_EXIST_FMT, "Path does not exist: %s", "\xE8\xB7\xAF\xE5\xBE\x84\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8\xEF\xBC\x9A%s", "\xD0\x9F\xD1\x83\xD1\x82\xD1\x8C \xD0\xBD\xD0\xB5 \xD1\x81\xD1\x83\xD1\x89\xD0\xB5\xD1\x81\xD1\x82\xD0\xB2\xD1\x83\xD0\xB5\xD1\x82: %s", "\xE3\x83\x91\xE3\x82\xB9\xE3\x81\x8C\xE5\xAD\x98\xE5\x9C\xA8\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93: %s", "Chemin introuvable : %s", "La ruta no existe: %s"},
	{XUI_TR_FILE_EMPTY, "(empty)", "(\xE7\xA9\xBA)", "(\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xBE)", "(\xE7\xA9\xBA)", "(vide)", "(vacio)"},
	{XUI_TR_FILE_OPEN_FILE, "Open File", "\xE6\x89\x93\xE5\xBC\x80\xE6\x96\x87\xE4\xBB\xB6", "\xD0\x9E\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C \xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB", "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB\xE3\x82\x92\xE9\x96\x8B\xE3\x81\x8F", "Ouvrir un fichier", "Abrir archivo"},
	{XUI_TR_FILE_SAVE_FILE, "Save File", "\xE4\xBF\x9D\xE5\xAD\x98\xE6\x96\x87\xE4\xBB\xB6", "\xD0\xA1\xD0\xBE\xD1\x85\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C \xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB", "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB\xE3\x82\x92\xE4\xBF\x9D\xE5\xAD\x98", "Enregistrer le fichier", "Guardar archivo"},
	{XUI_TR_FILE_SELECT_FOLDER, "Select Folder", "\xE9\x80\x89\xE6\x8B\xA9\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9", "\xD0\x92\xD1\x8B\xD0\xB1\xD1\x80\xD0\xB0\xD1\x82\xD1\x8C \xD0\xBF\xD0\xB0\xD0\xBF\xD0\xBA\xD1\x83", "\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x80\xE3\x83\xBC\xE3\x82\x92\xE9\x81\xB8\xE6\x8A\x9E", "Selectionner un dossier", "Seleccionar carpeta"},
	{XUI_TR_FILE_OPEN, "Open", "\xE6\x89\x93\xE5\xBC\x80", "\xD0\x9E\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C", "\xE9\x96\x8B\xE3\x81\x8F", "Ouvrir", "Abrir"},
	{XUI_TR_FILE_SAVE, "Save", "\xE4\xBF\x9D\xE5\xAD\x98", "\xD0\xA1\xD0\xBE\xD1\x85\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C", "\xE4\xBF\x9D\xE5\xAD\x98", "Enregistrer", "Guardar"},
	{XUI_TR_FILE_SELECT, "Select", "\xE9\x80\x89\xE6\x8B\xA9", "\xD0\x92\xD1\x8B\xD0\xB1\xD1\x80\xD0\xB0\xD1\x82\xD1\x8C", "\xE9\x81\xB8\xE6\x8A\x9E", "Selectionner", "Seleccionar"},
	{XUI_TR_FILE_FILE, "File", "\xE6\x96\x87\xE4\xBB\xB6", "\xD0\xA4\xD0\xB0\xD0\xB9\xD0\xBB", "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB", "Fichier", "Archivo"},
	{XUI_TR_FILE_FOLDER, "Folder", "\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9", "\xD0\x9F\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0", "\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x80\xE3\x83\xBC", "Dossier", "Carpeta"},
	{XUI_TR_FILE_SELECT_FOLDER_FIRST, "Select a folder first.", "\xE8\xAF\xB7\xE5\x85\x88\xE9\x80\x89\xE6\x8B\xA9\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9\xE3\x80\x82", "\xD0\xA1\xD0\xBD\xD0\xB0\xD1\x87\xD0\xB0\xD0\xBB\xD0\xB0 \xD0\xB2\xD1\x8B\xD0\xB1\xD0\xB5\xD1\x80\xD0\xB8\xD1\x82\xD0\xB5 \xD0\xBF\xD0\xB0\xD0\xBF\xD0\xBA\xD1\x83.", "\xE5\x85\x88\xE3\x81\xAB\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x80\xE3\x83\xBC\xE3\x82\x92\xE9\x81\xB8\xE6\x8A\x9E\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x8F\xE3\x81\xA0\xE3\x81\x95\xE3\x81\x84\xE3\x80\x82", "Selectionnez d'abord un dossier.", "Seleccione una carpeta primero."},
	{XUI_TR_FILE_OVERWRITE_MESSAGE_FMT, "The file already exists.\nDo you want to replace it?\n\n%s", "\xE6\x96\x87\xE4\xBB\xB6\xE5\xB7\xB2\xE5\xAD\x98\xE5\x9C\xA8\xE3\x80\x82\n\xE8\xA6\x81\xE6\x9B\xBF\xE6\x8D\xA2\xE5\xAE\x83\xE5\x90\x97\xEF\xBC\x9F\n\n%s", "\xD0\xA4\xD0\xB0\xD0\xB9\xD0\xBB \xD1\x83\xD0\xB6\xD0\xB5 \xD1\x81\xD1\x83\xD1\x89\xD0\xB5\xD1\x81\xD1\x82\xD0\xB2\xD1\x83\xD0\xB5\xD1\x82.\n\xD0\x97\xD0\xB0\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C \xD0\xB5\xD0\xB3\xD0\xBE?\n\n%s", "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB\xE3\x81\xAF\xE6\x97\xA2\xE3\x81\xAB\xE5\xAD\x98\xE5\x9C\xA8\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99\xE3\x80\x82\n\xE7\xBD\xAE\xE3\x81\x8D\xE6\x8F\x9B\xE3\x81\x88\xE3\x81\xBE\xE3\x81\x99\xE3\x81\x8B\xEF\xBC\x9F\n\n%s", "Le fichier existe deja.\nVoulez-vous le remplacer ?\n\n%s", "El archivo ya existe.\nDesea reemplazarlo?\n\n%s"},
	{XUI_TR_TIMELINE_ADD_LAYER, "Add Layer", "\xE6\xB7\xBB\xE5\x8A\xA0\xE5\x9B\xBE\xE5\xB1\x82", "\xD0\x94\xD0\xBE\xD0\xB1. \xD1\x81\xD0\xBB\xD0\xBE\xD0\xB9", "\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE8\xBF\xBD\xE5\x8A\xA0", "Ajouter calque", "Agregar capa"},
	{XUI_TR_TIMELINE_DELETE_LAYER, "Delete Layer", "\xE5\x88\xA0\xE9\x99\xA4\xE5\x9B\xBE\xE5\xB1\x82", "\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB. \xD1\x81\xD0\xBB\xD0\xBE\xD0\xB9", "\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE5\x89\x8A\xE9\x99\xA4", "Supprimer calque", "Eliminar capa"},
	{XUI_TR_TIMELINE_RENAME, "Rename", "\xE9\x87\x8D\xE5\x91\xBD\xE5\x90\x8D", "\xD0\x9F\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB8\xD0\xBC\xD0\xB5\xD0\xBD.", "\xE5\x90\x8D\xE5\x89\x8D\xE5\xA4\x89\xE6\x9B\xB4", "Renommer", "Renombrar"},
	{XUI_TR_TIMELINE_VISIBLE, "Visible", "\xE5\x8F\xAF\xE8\xA7\x81", "\xD0\x92\xD0\xB8\xD0\xB4\xD0\xB8\xD0\xBC\xD1\x8B\xD0\xB9", "\xE8\xA1\xA8\xE7\xA4\xBA", "Visible", "Visible"},
	{XUI_TR_TIMELINE_LOCKED, "Locked", "\xE9\x94\x81\xE5\xAE\x9A", "\xD0\x97\xD0\xB0\xD0\xB1\xD0\xBB\xD0\xBE\xD0\xBA.", "\xE3\x83\xAD\xE3\x83\x83\xE3\x82\xAF", "Verrouille", "Bloqueado"},
	{XUI_TR_TIMELINE_MOVE_UP, "Move Up", "\xE4\xB8\x8A\xE7\xA7\xBB", "\xD0\x92\xD0\xB2\xD0\xB5\xD1\x80\xD1\x85", "\xE4\xB8\x8A\xE3\x81\xB8", "Monter", "Subir"},
	{XUI_TR_TIMELINE_MOVE_DOWN, "Move Down", "\xE4\xB8\x8B\xE7\xA7\xBB", "\xD0\x92\xD0\xBD\xD0\xB8\xD0\xB7", "\xE4\xB8\x8B\xE3\x81\xB8", "Descendre", "Bajar"},
	{XUI_TR_TIMELINE_INSERT_FRAME, "Insert Frame", "\xE6\x8F\x92\xE5\x85\xA5\xE5\xB8\xA7", "\xD0\x92\xD1\x81\xD1\x82. \xD0\xBA\xD0\xB0\xD0\xB4\xD1\x80", "\xE3\x83\x95\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\xA0\xE6\x8C\xBF\xE5\x85\xA5", "Inserer image", "Insertar fotograma"},
	{XUI_TR_TIMELINE_INSERT_KEYFRAME, "Insert Keyframe", "\xE6\x8F\x92\xE5\x85\xA5\xE5\x85\xB3\xE9\x94\xAE\xE5\xB8\xA7", "\xD0\x92\xD1\x81\xD1\x82. \xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87. \xD0\xBA\xD0\xB0\xD0\xB4\xD1\x80", "\xE3\x82\xAD\xE3\x83\xBC\xE3\x83\x95\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\xA0\xE6\x8C\xBF\xE5\x85\xA5", "Inserer image cle", "Insertar clave"},
	{XUI_TR_TIMELINE_INSERT_BLANK_KEYFRAME, "Insert Blank Keyframe", "\xE6\x8F\x92\xE5\x85\xA5\xE7\xA9\xBA\xE7\x99\xBD\xE5\x85\xB3\xE9\x94\xAE\xE5\xB8\xA7", "\xD0\x92\xD1\x81\xD1\x82. \xD0\xBF\xD1\x83\xD1\x81\xD1\x82. \xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87.", "\xE7\xA9\xBA\xE3\x81\xAE\xE3\x82\xAD\xE3\x83\xBC\xE3\x83\x95\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\xA0\xE6\x8C\xBF\xE5\x85\xA5", "Inserer image cle vide", "Insertar clave vacia"},
	{XUI_TR_TIMELINE_CLEAR_KEYFRAME, "Clear Keyframe", "\xE6\xB8\x85\xE9\x99\xA4\xE5\x85\xB3\xE9\x94\xAE\xE5\xB8\xA7", "\xD0\x9E\xD1\x87\xD0\xB8\xD1\x81\xD1\x82. \xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87.", "\xE3\x82\xAD\xE3\x83\xBC\xE3\x83\x95\xE3\x83\xAC\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\x92\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xA2", "Effacer image cle", "Borrar clave"},
	{XUI_TR_TIMELINE_CREATE_SPAN, "Create Span", "\xE5\x88\x9B\xE5\xBB\xBA\xE7\x89\x87\xE6\xAE\xB5", "\xD0\xA1\xD0\xBE\xD0\xB7\xD0\xB4. \xD0\xB4\xD0\xB8\xD0\xB0\xD0\xBF\xD0\xB0\xD0\xB7\xD0\xBE\xD0\xBD", "\xE3\x82\xB9\xE3\x83\x91\xE3\x83\xB3\xE4\xBD\x9C\xE6\x88\x90", "Creer segment", "Crear tramo"},
	{XUI_TR_TIMELINE_CREATE_SPAN_FROM_SELECTION, "Create Span From Selection", "\xE4\xBB\x8E\xE9\x80\x89\xE5\x8C\xBA\xE5\x88\x9B\xE5\xBB\xBA\xE7\x89\x87\xE6\xAE\xB5", "\xD0\xA1\xD0\xBE\xD0\xB7\xD0\xB4. \xD0\xB8\xD0\xB7 \xD0\xB2\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB.", "\xE9\x81\xB8\xE6\x8A\x9E\xE7\xAF\x84\xE5\x9B\xB2\xE3\x81\x8B\xE3\x82\x89\xE4\xBD\x9C\xE6\x88\x90", "Creer depuis selection", "Crear desde seleccion"},
	{XUI_TR_TIMELINE_CLEAR_SPAN, "Clear Span", "\xE6\xB8\x85\xE9\x99\xA4\xE7\x89\x87\xE6\xAE\xB5", "\xD0\x9E\xD1\x87\xD0\xB8\xD1\x81\xD1\x82. \xD0\xB4\xD0\xB8\xD0\xB0\xD0\xBF.", "\xE3\x82\xB9\xE3\x83\x91\xE3\x83\xB3\xE3\x82\x92\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xA2", "Effacer segment", "Borrar tramo"},
	{XUI_TR_MESSAGE_COPY, "Copy", "\xE5\xA4\x8D\xE5\x88\xB6", "\xD0\x9A\xD0\xBE\xD0\xBF\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x82\xD1\x8C", "\xE3\x82\xB3\xE3\x83\x94\xE3\x83\xBC", "Copier", "Copiar"},
	{XUI_TR_MESSAGE_TOOL, "Tool", "\xE5\xB7\xA5\xE5\x85\xB7", "\xD0\x98\xD0\xBD\xD1\x81\xD1\x82\xD1\x80\xD1\x83\xD0\xBC\xD0\xB5\xD0\xBD\xD1\x82", "\xE3\x83\x84\xE3\x83\xBC\xE3\x83\xAB", "Outil", "Herramienta"},
	{XUI_TR_MESSAGE_THINKING, "Thinking", "\xE6\x80\x9D\xE8\x80\x83", "\xD0\xA0\xD0\xB0\xD0\xB7\xD0\xBC\xD1\x8B\xD1\x88\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB5", "\xE6\x80\x9D\xE8\x80\x83", "R\xC3\xA9" "flexion", "Razonamiento"}
};

static int g_iXuiDefaultLanguage = XUI_LANGUAGE_EN;

static void __xuiContextDestroyLanguages(xui_context pContext);

static void __xuiContextInitIcons(xui_context pContext)
{
	xrtDictInit(&pContext->mapIconCategories, sizeof(void*), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pContext->arrIconCategories, sizeof(xui_icon_category), XRT_OBJMODE_LOCAL);
	pContext->iIconGeneration = 1;
}

static void __xuiContextDestroyIcons(xui_context pContext)
{
	if ( pContext->onDestroyIcons != NULL ) {
		pContext->onDestroyIcons(pContext);
	}
	xrtDictUnit(&pContext->mapIconCategories);
	xrtArrayUnit(&pContext->arrIconCategories);
	pContext->onDestroyIcons = NULL;
}

static int __xuiContextValid(xui_context pContext)
{
	return (pContext != NULL) && (pContext->iMagic == XUI_CONTEXT_MAGIC);
}

static int __xuiResourceValid(xui_resource pResource)
{
	return (pResource != NULL) && (pResource->iMagic == XUI_RESOURCE_MAGIC);
}

static char* __xuiCoreStringDuplicate(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText) + 1u;
	sCopy = (char*)xrtMalloc(iSize);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iSize);
	return sCopy;
}

static int __xuiLanguageValid(xui_language pLanguage)
{
	return (pLanguage != NULL) && (pLanguage->iMagic == XUI_LANGUAGE_MAGIC);
}

static int __xuiBuiltinLanguageValid(int iLanguageId)
{
	return (iLanguageId >= 0) && (iLanguageId < XUI_LANGUAGE_COUNT);
}

static const char* __xuiBuiltinLanguageCode(int iLanguageId)
{
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: return "en";
	case XUI_LANGUAGE_ZH: return "zh";
	case XUI_LANGUAGE_RU: return "ru";
	case XUI_LANGUAGE_JA: return "ja";
	case XUI_LANGUAGE_FR: return "fr";
	case XUI_LANGUAGE_ES: return "es";
	default: return "custom";
	}
}

static const char* __xuiBuiltinLanguageName(int iLanguageId)
{
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: return "English";
	case XUI_LANGUAGE_ZH: return "Chinese";
	case XUI_LANGUAGE_RU: return "Russian";
	case XUI_LANGUAGE_JA: return "Japanese";
	case XUI_LANGUAGE_FR: return "French";
	case XUI_LANGUAGE_ES: return "Spanish";
	default: return "Custom";
	}
}

static xui_language_text_t* __xuiLanguageGetTextSlot(xui_language pLanguage, int iTextId)
{
	if ( !__xuiLanguageValid(pLanguage) || (iTextId <= XUI_TR_NONE) || (iTextId >= XUI_TR_COUNT) ) {
		return NULL;
	}
	if ( pLanguage->arrTexts.Count < (uint32_t)iTextId ) {
		return NULL;
	}
	return (xui_language_text_t*)xrtArrayGet_Unsafe(&pLanguage->arrTexts, (uint32_t)iTextId);
}

static const char* __xuiBuiltinTextForLanguage(const xui_i18n_builtin_entry_t* pEntry, int iLanguageId)
{
	if ( pEntry == NULL ) {
		return NULL;
	}
	switch ( iLanguageId ) {
	case XUI_LANGUAGE_EN: return pEntry->sEN;
	case XUI_LANGUAGE_ZH: return pEntry->sZH;
	case XUI_LANGUAGE_RU: return pEntry->sRU;
	case XUI_LANGUAGE_JA: return pEntry->sJA;
	case XUI_LANGUAGE_FR: return pEntry->sFR;
	case XUI_LANGUAGE_ES: return pEntry->sES;
	default: return pEntry->sEN;
	}
}

static void __xuiLanguageFreeTextArray(xui_language pLanguage)
{
	xui_language_text_t* pText;
	uint32_t i;

	if ( !__xuiLanguageValid(pLanguage) ) {
		return;
	}
	for ( i = 1u; i <= pLanguage->arrTexts.Count; ++i ) {
		pText = (xui_language_text_t*)xrtArrayGet_Unsafe(&pLanguage->arrTexts, i);
		if ( pText->bOwned && pText->sText != NULL ) {
			xrtFree(pText->sText);
		}
		pText->sText = NULL;
		pText->bOwned = 0;
	}
	xrtArrayUnit(&pLanguage->arrTexts);
}

static void __xuiLanguageDestroyOne(xui_language pLanguage)
{
	if ( !__xuiLanguageValid(pLanguage) ) {
		return;
	}
	__xuiLanguageFreeTextArray(pLanguage);
	if ( pLanguage->sCode != NULL ) {
		xrtFree(pLanguage->sCode);
	}
	if ( pLanguage->sName != NULL ) {
		xrtFree(pLanguage->sName);
	}
	pLanguage->iMagic = 0;
	xrtFree(pLanguage);
}

static xui_language __xuiContextFindLanguage(xui_context pContext, int iLanguageId)
{
	xui_language* ppLanguage;
	uint32_t i;

	if ( !__xuiContextValid(pContext) ) {
		return NULL;
	}
	for ( i = 1u; i <= pContext->arrLanguages.Count; ++i ) {
		ppLanguage = (xui_language*)xrtArrayGet_Unsafe(&pContext->arrLanguages, i);
		if ( __xuiLanguageValid(*ppLanguage) && (*ppLanguage)->iLanguageId == iLanguageId ) {
			return *ppLanguage;
		}
	}
	return NULL;
}

static int __xuiLanguageEnsureTextSlots(xui_language pLanguage)
{
	xui_language_text_t* pText;
	uint32_t iPos;
	int i;

	if ( !__xuiLanguageValid(pLanguage) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	while ( pLanguage->arrTexts.Count < (uint32_t)(XUI_TR_COUNT - 1) ) {
		iPos = xrtArrayAppend(&pLanguage->arrTexts, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pText = (xui_language_text_t*)xrtArrayGet_Unsafe(&pLanguage->arrTexts, iPos);
		memset(pText, 0, sizeof(*pText));
		pText->iTextId = (int)iPos;
	}
	for ( i = 1; i < XUI_TR_COUNT; ++i ) {
		pText = __xuiLanguageGetTextSlot(pLanguage, i);
		if ( pText != NULL ) {
			pText->iTextId = i;
		}
	}
	return XUI_OK;
}

static xui_language __xuiLanguageCreateRaw(xui_context pContext, int iLanguageId, const char* sCode, const char* sName, int iFallbackLanguageId, int bBuiltin)
{
	xui_language pLanguage;
	xui_language* ppSlot;
	uint32_t iPos;

	if ( !__xuiContextValid(pContext) || sCode == NULL || sName == NULL || __xuiContextFindLanguage(pContext, iLanguageId) != NULL ) {
		return NULL;
	}
	pLanguage = (xui_language)xrtMalloc(sizeof(*pLanguage));
	if ( pLanguage == NULL ) {
		return NULL;
	}
	memset(pLanguage, 0, sizeof(*pLanguage));
	pLanguage->iMagic = XUI_LANGUAGE_MAGIC;
	pLanguage->pContext = pContext;
	pLanguage->iLanguageId = iLanguageId;
	pLanguage->iFallbackLanguageId = iFallbackLanguageId;
	pLanguage->bBuiltin = bBuiltin ? 1 : 0;
	xrtArrayInit(&pLanguage->arrTexts, sizeof(xui_language_text_t), XRT_OBJMODE_LOCAL);
	pLanguage->sCode = __xuiCoreStringDuplicate(sCode);
	pLanguage->sName = __xuiCoreStringDuplicate(sName);
	if ( pLanguage->sCode == NULL || pLanguage->sName == NULL || __xuiLanguageEnsureTextSlots(pLanguage) != XUI_OK ) {
		__xuiLanguageDestroyOne(pLanguage);
		return NULL;
	}
	iPos = xrtArrayAppend(&pContext->arrLanguages, 1u);
	if ( iPos == 0u ) {
		__xuiLanguageDestroyOne(pLanguage);
		return NULL;
	}
	ppSlot = (xui_language*)xrtArrayGet_Unsafe(&pContext->arrLanguages, iPos);
	*ppSlot = pLanguage;
	return pLanguage;
}

static void __xuiLanguageSetBuiltinText(xui_language pLanguage, int iTextId, const char* sText)
{
	xui_language_text_t* pSlot;

	pSlot = __xuiLanguageGetTextSlot(pLanguage, iTextId);
	if ( pSlot == NULL ) {
		return;
	}
	if ( pSlot->bOwned && pSlot->sText != NULL ) {
		xrtFree(pSlot->sText);
	}
	pSlot->sText = (char*)sText;
	pSlot->bOwned = 0;
}

static int __xuiContextInitLanguages(xui_context pContext)
{
	xui_language pLanguage;
	int iLanguage;
	size_t i;

	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	xrtArrayInit(&pContext->arrLanguages, sizeof(xui_language), XRT_OBJMODE_LOCAL);
	pContext->iCurrentLanguageId = XUI_LANGUAGE_EN;
	pContext->iNextCustomLanguageId = XUI_LANGUAGE_CUSTOM_BASE;
	pContext->iLanguageRevision = 1;
	for ( iLanguage = 0; iLanguage < XUI_LANGUAGE_COUNT; ++iLanguage ) {
		pLanguage = __xuiLanguageCreateRaw(pContext, iLanguage, __xuiBuiltinLanguageCode(iLanguage), __xuiBuiltinLanguageName(iLanguage), XUI_LANGUAGE_EN, 1);
		if ( pLanguage == NULL ) {
			__xuiContextDestroyLanguages(pContext);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		for ( i = 0; i < sizeof(g_arrXuiBuiltinText) / sizeof(g_arrXuiBuiltinText[0]); ++i ) {
			__xuiLanguageSetBuiltinText(pLanguage, g_arrXuiBuiltinText[i].iTextId, __xuiBuiltinTextForLanguage(&g_arrXuiBuiltinText[i], iLanguage));
		}
	}
	if ( __xuiContextFindLanguage(pContext, g_iXuiDefaultLanguage) != NULL ) {
		pContext->iCurrentLanguageId = g_iXuiDefaultLanguage;
	}
	return XUI_OK;
}

static void __xuiContextDestroyLanguages(xui_context pContext)
{
	xui_language* ppLanguage;
	uint32_t i;

	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	for ( i = 1u; i <= pContext->arrLanguages.Count; ++i ) {
		ppLanguage = (xui_language*)xrtArrayGet_Unsafe(&pContext->arrLanguages, i);
		__xuiLanguageDestroyOne(*ppLanguage);
		*ppLanguage = NULL;
	}
	xrtArrayUnit(&pContext->arrLanguages);
	pContext->iCurrentLanguageId = XUI_LANGUAGE_EN;
	pContext->iNextCustomLanguageId = XUI_LANGUAGE_CUSTOM_BASE;
	pContext->iLanguageRevision = 0;
}

static void __xuiContextTouchLanguage(xui_context pContext, xui_language pLanguage)
{
	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	if ( __xuiLanguageValid(pLanguage) ) {
		pLanguage->iRevision++;
		if ( pLanguage->iRevision == 0u ) pLanguage->iRevision = 1u;
	}
	pContext->iLanguageRevision++;
	if ( pContext->iLanguageRevision == 0u ) pContext->iLanguageRevision = 1u;
	if ( pContext->pRoot != NULL ) {
		(void)xuiWidgetInvalidate(pContext->pRoot, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( pContext->pOverlayRoot != NULL ) {
		(void)xuiWidgetInvalidate(pContext->pOverlayRoot, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	(void)xuiInternalContextInvalidateAll(pContext);
}

XUI_API int xuiSetDefaultLanguage(int iLanguageId)
{
	if ( !__xuiBuiltinLanguageValid(iLanguageId) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	g_iXuiDefaultLanguage = iLanguageId;
	return XUI_OK;
}

XUI_API int xuiGetDefaultLanguage(void)
{
	return g_iXuiDefaultLanguage;
}

XUI_API int xuiSetLanguage(xui_context pContext, int iLanguageId)
{
	xui_language pLanguage;

	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pLanguage = __xuiContextFindLanguage(pContext, iLanguageId);
	if ( pLanguage == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->iCurrentLanguageId == iLanguageId ) {
		return XUI_OK;
	}
	pContext->iCurrentLanguageId = iLanguageId;
	__xuiContextTouchLanguage(pContext, pLanguage);
	return XUI_OK;
}

XUI_API int xuiGetLanguage(xui_context pContext)
{
	return __xuiContextValid(pContext) ? pContext->iCurrentLanguageId : XUI_LANGUAGE_EN;
}

XUI_API uint32_t xuiGetLanguageRevision(xui_context pContext)
{
	return __xuiContextValid(pContext) ? pContext->iLanguageRevision : 0u;
}

XUI_API xui_language xuiGetLanguagePack(xui_context pContext, int iLanguageId)
{
	return __xuiContextFindLanguage(pContext, iLanguageId);
}

XUI_API xui_language xuiCreateLanguage(xui_context pContext, const char* sCode, const char* sName, int iFallbackLanguageId)
{
	xui_language pLanguage;
	int iLanguageId;

	if ( !__xuiContextValid(pContext) || sCode == NULL || sName == NULL ) {
		return NULL;
	}
	if ( __xuiContextFindLanguage(pContext, iFallbackLanguageId) == NULL ) {
		iFallbackLanguageId = XUI_LANGUAGE_EN;
	}
	iLanguageId = pContext->iNextCustomLanguageId++;
	if ( pContext->iNextCustomLanguageId < XUI_LANGUAGE_CUSTOM_BASE ) {
		pContext->iNextCustomLanguageId = XUI_LANGUAGE_CUSTOM_BASE;
	}
	pLanguage = __xuiLanguageCreateRaw(pContext, iLanguageId, sCode, sName, iFallbackLanguageId, 0);
	if ( pLanguage != NULL ) {
		__xuiContextTouchLanguage(pContext, pLanguage);
	}
	return pLanguage;
}

XUI_API int xuiGetLanguageId(xui_language pLanguage)
{
	return __xuiLanguageValid(pLanguage) ? pLanguage->iLanguageId : XUI_LANGUAGE_EN;
}

XUI_API int xuiLanguageSetText(xui_context pContext, xui_language pLanguage, int iTextId, const char* sText)
{
	xui_language_text_t* pSlot;
	char* sCopy;

	if ( !__xuiContextValid(pContext) || !__xuiLanguageValid(pLanguage) || pLanguage->pContext != pContext ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSlot = __xuiLanguageGetTextSlot(pLanguage, iTextId);
	if ( pSlot == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = NULL;
	if ( sText != NULL ) {
		sCopy = __xuiCoreStringDuplicate(sText);
		if ( sCopy == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( pSlot->bOwned && pSlot->sText != NULL ) {
		xrtFree(pSlot->sText);
	}
	pSlot->sText = sCopy;
	pSlot->bOwned = (sCopy != NULL) ? 1 : 0;
	__xuiContextTouchLanguage(pContext, pLanguage);
	return XUI_OK;
}

XUI_API void xuiLanguageTouch(xui_context pContext, xui_language pLanguage)
{
	if ( !__xuiContextValid(pContext) || !__xuiLanguageValid(pLanguage) || pLanguage->pContext != pContext ) {
		return;
	}
	__xuiContextTouchLanguage(pContext, pLanguage);
}

XUI_API xarray xuiGetLanguageTextArray(xui_language pLanguage)
{
	return __xuiLanguageValid(pLanguage) ? &pLanguage->arrTexts : NULL;
}

XUI_API const char* xuiGetLanguageCode(xui_language pLanguage)
{
	return __xuiLanguageValid(pLanguage) ? pLanguage->sCode : "";
}

XUI_API const char* xuiGetLanguageName(xui_language pLanguage)
{
	return __xuiLanguageValid(pLanguage) ? pLanguage->sName : "";
}

XUI_API const char* xuiTranslate(xui_context pContext, int iTextId)
{
	xui_language pLanguage;
	xui_language_text_t* pSlot;
	int iFallback;

	if ( !__xuiContextValid(pContext) || (iTextId <= XUI_TR_NONE) || (iTextId >= XUI_TR_COUNT) ) {
		return "";
	}
	pLanguage = __xuiContextFindLanguage(pContext, pContext->iCurrentLanguageId);
	if ( pLanguage != NULL ) {
		pSlot = __xuiLanguageGetTextSlot(pLanguage, iTextId);
		if ( pSlot != NULL && pSlot->sText != NULL && pSlot->sText[0] != '\0' ) {
			return pSlot->sText;
		}
		iFallback = pLanguage->iFallbackLanguageId;
		if ( iFallback != pLanguage->iLanguageId ) {
			pLanguage = __xuiContextFindLanguage(pContext, iFallback);
			if ( pLanguage != NULL ) {
				pSlot = __xuiLanguageGetTextSlot(pLanguage, iTextId);
				if ( pSlot != NULL && pSlot->sText != NULL && pSlot->sText[0] != '\0' ) {
					return pSlot->sText;
				}
			}
		}
	}
	pLanguage = __xuiContextFindLanguage(pContext, XUI_LANGUAGE_EN);
	pSlot = __xuiLanguageGetTextSlot(pLanguage, iTextId);
	return (pSlot != NULL && pSlot->sText != NULL) ? pSlot->sText : "";
}

static int __xuiPainterValid(xui_painter pPainter)
{
	return (pPainter != NULL) && (pPainter->iMagic == XUI_PAINTER_MAGIC);
}

static int __xuiPathValid(xui_path pPath)
{
	return (pPath != NULL) && (pPath->iMagic == XUI_PATH_MAGIC);
}

static int __xuiPathReserve(xui_path pPath, int iNeeded)
{
	xui_path_command_t* pCommands;
	int iCapacity;

	if ( !__xuiPathValid(pPath) || (iNeeded < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pPath->iCommandCapacity ) {
		return XUI_OK;
	}
	iCapacity = (pPath->iCommandCapacity > 0) ? pPath->iCommandCapacity : 16;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pCommands = (xui_path_command_t*)xrtRealloc(pPath->pCommands, sizeof(*pCommands) * (size_t)iCapacity);
	if ( pCommands == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pPath->pCommands = pCommands;
	pPath->iCommandCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiPathAddCommand(xui_path pPath, int iCommand, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC)
{
	xui_path_command_t* pCommand;
	int iRet;

	if ( !__xuiPathValid(pPath) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiPathReserve(pPath, pPath->iCommandCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pCommand = &pPath->pCommands[pPath->iCommandCount++];
	memset(pCommand, 0, sizeof(*pCommand));
	pCommand->iCommand = iCommand;
	pCommand->arrPoints[0] = tA;
	pCommand->arrPoints[1] = tB;
	pCommand->arrPoints[2] = tC;
	return XUI_OK;
}

static int __xuiFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiRectValid(xui_rect_i_t tRect)
{
	return (tRect.iW > 0) && (tRect.iH > 0);
}

static int __xuiContextCeilPositive(float fValue)
{
	if ( fValue <= 0.0f ) {
		return 0;
	}
	return xuiInternalPixelCeil(fValue);
}

static xui_rect_i_t __xuiRectUnion(xui_rect_i_t tA, xui_rect_i_t tB)
{
	xui_rect_i_t tRet;
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;

	iLeft = (tA.iX < tB.iX) ? tA.iX : tB.iX;
	iTop = (tA.iY < tB.iY) ? tA.iY : tB.iY;
	iRight = ((tA.iX + tA.iW) > (tB.iX + tB.iW)) ? (tA.iX + tA.iW) : (tB.iX + tB.iW);
	iBottom = ((tA.iY + tA.iH) > (tB.iY + tB.iH)) ? (tA.iY + tA.iH) : (tB.iY + tB.iH);
	tRet.iX = iLeft;
	tRet.iY = iTop;
	tRet.iW = iRight - iLeft;
	tRet.iH = iBottom - iTop;
	return tRet;
}

static int __xuiRectTouchesOrIntersects(xui_rect_i_t tA, xui_rect_i_t tB)
{
	if ( (tA.iX > (tB.iX + tB.iW)) || (tB.iX > (tA.iX + tA.iW)) ) {
		return 0;
	}
	if ( (tA.iY > (tB.iY + tB.iH)) || (tB.iY > (tA.iY + tA.iH)) ) {
		return 0;
	}
	return 1;
}

static int __xuiContextClipToViewport(xui_context pContext, xui_rect_i_t* pRect)
{
	int iWidth;
	int iHeight;
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;

	iWidth = __xuiContextCeilPositive(pContext->fViewportWidth);
	iHeight = __xuiContextCeilPositive(pContext->fViewportHeight);
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return 1;
	}
	iLeft = pRect->iX;
	iTop = pRect->iY;
	iRight = pRect->iX + pRect->iW;
	iBottom = pRect->iY + pRect->iH;
	if ( iLeft < 0 ) {
		iLeft = 0;
	}
	if ( iTop < 0 ) {
		iTop = 0;
	}
	if ( iRight > iWidth ) {
		iRight = iWidth;
	}
	if ( iBottom > iHeight ) {
		iBottom = iHeight;
	}
	pRect->iX = iLeft;
	pRect->iY = iTop;
	pRect->iW = iRight - iLeft;
	pRect->iH = iBottom - iTop;
	return __xuiRectValid(*pRect);
}

static int __xuiContextDamageReserve(xui_context pContext, int iCapacity)
{
	xui_rect_i_t* pDamage;

	if ( iCapacity <= pContext->iDamageCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pContext->iDamageCapacity * 2) ) {
		iCapacity = pContext->iDamageCapacity * 2;
	}
	if ( pContext->pDamage == pContext->arrInlineDamage ) {
		pDamage = (xui_rect_i_t*)xrtMalloc(sizeof(*pDamage) * (size_t)iCapacity);
		if ( pDamage == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pDamage, pContext->pDamage, sizeof(*pDamage) * (size_t)pContext->iDamageCount);
	} else {
		pDamage = (xui_rect_i_t*)xrtRealloc(pContext->pDamage, sizeof(*pDamage) * (size_t)iCapacity);
		if ( pDamage == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	pContext->pDamage = pDamage;
	pContext->iDamageCapacity = iCapacity;
	return XUI_OK;
}

static void __xuiContextBumpGeneration(xui_context pContext)
{
	pContext->iGeneration++;
	if ( pContext->iGeneration == 0 ) {
		pContext->iGeneration = 1;
	}
}

static void __xuiContextRemoveDamage(xui_context pContext, int iIndex)
{
	int iMoveCount;

	iMoveCount = pContext->iDamageCount - iIndex - 1;
	if ( iMoveCount > 0 ) {
		memmove(&pContext->pDamage[iIndex], &pContext->pDamage[iIndex + 1], sizeof(pContext->pDamage[0]) * (size_t)iMoveCount);
	}
	pContext->iDamageCount--;
}

static int __xuiContextAddDamage(xui_context pContext, xui_rect_i_t tRect)
{
	int i;
	int j;
	int iRet;

	if ( !__xuiRectValid(tRect) ) {
		return XUI_OK;
	}
	if ( !__xuiContextClipToViewport(pContext, &tRect) ) {
		return XUI_OK;
	}
	for ( i = 0; i < pContext->iDamageCount; i++ ) {
		if ( __xuiRectTouchesOrIntersects(pContext->pDamage[i], tRect) ) {
			pContext->pDamage[i] = __xuiRectUnion(pContext->pDamage[i], tRect);
			j = i + 1;
			while ( j < pContext->iDamageCount ) {
				if ( __xuiRectTouchesOrIntersects(pContext->pDamage[i], pContext->pDamage[j]) ) {
					pContext->pDamage[i] = __xuiRectUnion(pContext->pDamage[i], pContext->pDamage[j]);
					__xuiContextRemoveDamage(pContext, j);
				} else {
					j++;
				}
			}
			__xuiContextBumpGeneration(pContext);
			return XUI_OK;
		}
	}
	iRet = __xuiContextDamageReserve(pContext, pContext->iDamageCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pContext->pDamage[pContext->iDamageCount++] = tRect;
	__xuiContextBumpGeneration(pContext);
	return XUI_OK;
}

int xuiInternalContextIsValid(xui_context pContext)
{
	return __xuiContextValid(pContext);
}

int xuiInternalContextHasProxy(xui_context pContext)
{
	return __xuiContextValid(pContext) && pContext->bHasProxy;
}

xui_proxy xuiInternalContextGetProxy(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) || !pContext->bHasProxy ) {
		return NULL;
	}
	return &pContext->tProxy;
}

int xuiInternalContextInvalidateRect(xui_context pContext, xui_rect_i_t tRect)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiContextAddDamage(pContext, tRect);
}

int xuiInternalContextInvalidateAll(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiInvalidateAll(pContext);
}

void xuiInternalContextBumpGeneration(xui_context pContext)
{
	if ( __xuiContextValid(pContext) ) {
		__xuiContextBumpGeneration(pContext);
	}
}

void xuiInternalContextDestroyRenderTree(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	if ( pContext->pRenderNodes != NULL ) {
		xrtFree(pContext->pRenderNodes);
	}
	pContext->pRenderNodes = NULL;
	pContext->iRenderNodeCount = 0;
	pContext->iRenderNodeCapacity = 0;
	pContext->iRenderTreeGeneration = 0;
}

void xuiInternalContextDestroyInput(xui_context pContext)
{
	int i;

	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	for ( i = 0; i < pContext->iHotkeyCount; i++ ) {
		if ( pContext->pHotkeys[i].sCommand != NULL ) {
			xrtFree(pContext->pHotkeys[i].sCommand);
		}
	}
	if ( pContext->pHotkeys != pContext->arrInlineHotkeys ) {
		xrtFree(pContext->pHotkeys);
	}
	pContext->pHotkeys = pContext->arrInlineHotkeys;
	pContext->iHotkeyCount = 0;
	pContext->iHotkeyCapacity = XUI_CONTEXT_HOTKEY_INLINE;
	pContext->pDragWidget = NULL;
	pContext->bDragActive = 0;
	pContext->pLastClickWidget = NULL;
	memset(pContext->arrPointerStates, 0, sizeof(pContext->arrPointerStates));
	pContext->iPointerStateCount = 0;
	pContext->iInputPointerId = XUI_POINTER_ID_MOUSE;
	pContext->iInputPointerType = XUI_POINTER_TYPE_MOUSE;
	pContext->iInputDispatchDepth = 0;
	if ( pContext->sActiveTooltipText != NULL ) {
		xrtFree(pContext->sActiveTooltipText);
		pContext->sActiveTooltipText = NULL;
	}
	pContext->pTooltipOwner = NULL;
	pContext->pTooltipPopupWidget = NULL;
	pContext->bTooltipOpen = 0;
	pContext->fTooltipHoverTime = 0.0f;
	memset(&pContext->tActiveTooltip, 0, sizeof(pContext->tActiveTooltip));
	pContext->bImeEnabled = 0;
	pContext->bHasImeCandidateRect = 0;
}

static void __xuiResourceDestroyOne(xui_resource pResource)
{
	xui_resource_dependency_t* pDependency;
	xui_resource_dependency_t* pNextDependency;

	if ( !__xuiResourceValid(pResource) ) {
		return;
	}
	pDependency = pResource->pDependencies;
	while ( pDependency != NULL ) {
		pNextDependency = pDependency->pNext;
		xrtFree(pDependency);
		pDependency = pNextDependency;
	}
	pResource->pDependencies = NULL;
	if ( pResource->onDestroy != NULL ) {
		pResource->onDestroy(pResource->pContext, pResource->pHandle, pResource->pUser);
	}
	if ( pResource->sName != NULL ) {
		xrtFree(pResource->sName);
	}
	pResource->iMagic = 0;
	xrtFree(pResource);
}

static void __xuiResourceRemoveDependencyReferences(xui_context pContext, xui_resource pRemoved)
{
	xui_resource pResource;
	xui_resource_dependency_t* pDependency;
	xui_resource_dependency_t* pPrev;
	xui_resource_dependency_t* pNext;

	for ( pResource = pContext->pResources; pResource != NULL; pResource = pResource->pNext ) {
		if ( pResource == pRemoved ) {
			continue;
		}
		pPrev = NULL;
		pDependency = pResource->pDependencies;
		while ( pDependency != NULL ) {
			pNext = pDependency->pNext;
			if ( pDependency->pResource == pRemoved ) {
				if ( pPrev != NULL ) {
					pPrev->pNext = pNext;
				} else {
					pResource->pDependencies = pNext;
				}
				xrtFree(pDependency);
				pResource->iGeneration = pContext->iNextResourceGeneration++;
				if ( pContext->iNextResourceGeneration == 0 ) {
					pContext->iNextResourceGeneration = 1;
				}
			} else {
				pPrev = pDependency;
			}
			pDependency = pNext;
		}
	}
}

void xuiInternalContextDestroyResources(xui_context pContext)
{
	xui_resource pResource;
	xui_resource pNext;

	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	pResource = pContext->pResources;
	while ( pResource != NULL ) {
		pNext = pResource->pNext;
		__xuiResourceDestroyOne(pResource);
		pResource = pNext;
	}
	pContext->pResources = NULL;
}

static int __xuiContextWidgetContains(xui_widget pRoot, xui_widget pWidget)
{
	xui_widget pScan;

	if ( (pRoot == NULL) || (pWidget == NULL) ) {
		return 0;
	}
	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan == pRoot ) {
			return 1;
		}
	}
	return 0;
}

static void __xuiContextClearOverlayOwners(xui_widget pScan, xui_widget pRemoved)
{
	xui_widget pChild;

	if ( pScan == NULL ) {
		return;
	}
	if ( __xuiContextWidgetContains(pRemoved, pScan->pOverlayOwner) ) {
		pScan->pOverlayOwner = NULL;
	}
	for ( pChild = pScan->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiContextClearOverlayOwners(pChild, pRemoved);
	}
}

void xuiInternalContextDetachWidget(xui_context pContext, xui_widget pWidget)
{
	int i;

	if ( !__xuiContextValid(pContext) || (pWidget == NULL) ) {
		return;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pHoverWidget) ) {
		if ( pContext->pHoverWidget != NULL ) {
			pContext->pHoverWidget->iInputState &= ~XUI_WIDGET_STATE_HOVER;
		}
		pContext->pHoverWidget = NULL;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pActiveWidget) ) {
		if ( pContext->pActiveWidget != NULL ) {
			pContext->pActiveWidget->iInputState &= ~XUI_WIDGET_STATE_ACTIVE;
		}
		pContext->pActiveWidget = NULL;
		pContext->iActiveButton = 0;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pFocusWidget) ) {
		if ( pContext->pFocusWidget != NULL ) {
			pContext->pFocusWidget->iInputState &= ~XUI_WIDGET_STATE_FOCUS;
		}
		pContext->pFocusWidget = NULL;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pPointerCaptureWidget) ) {
		pContext->pPointerCaptureWidget = NULL;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pDragWidget) ) {
		pContext->pDragWidget = NULL;
		pContext->bDragActive = 0;
		pContext->iDragButton = 0;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pLastClickWidget) ) {
		pContext->pLastClickWidget = NULL;
		pContext->iLastClickButton = 0;
		pContext->fLastClickTime = 0.0;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pContextPressWidget) ) {
		xuiInternalContextPressCancel(pContext);
	}
	for ( i = 0; i < XUI_POINTER_MAX; i++ ) {
		xui_pointer_state_t* pState = &pContext->arrPointerStates[i];
		if ( !pState->bAllocated ) {
			continue;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pHoverWidget) ) {
			pState->pHoverWidget = NULL;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pActiveWidget) ) {
			pState->pActiveWidget = NULL;
			pState->iActiveButton = 0;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pPointerCaptureWidget) ) {
			pState->pPointerCaptureWidget = NULL;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pDragWidget) ) {
			pState->pDragWidget = NULL;
			pState->bDragActive = 0;
			pState->iDragButton = 0;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pLastClickWidget) ) {
			pState->pLastClickWidget = NULL;
			pState->iLastClickButton = 0;
			pState->fLastClickTime = 0.0;
		}
		if ( __xuiContextWidgetContains(pWidget, pState->pContextPressWidget) ) {
			pState->pContextPressWidget = NULL;
			pState->bContextPressActive = 0;
			pState->bContextPressMoved = 0;
			pState->bContextPressFired = 0;
			pState->fContextPressTime = 0.0f;
		}
	}
	xuiInternalTooltipDetachWidget(pContext, pWidget);
	for ( i = 0; i < pContext->iHotkeyCount; i++ ) {
		if ( __xuiContextWidgetContains(pWidget, pContext->pHotkeys[i].pWidget) ) {
			pContext->pHotkeys[i].pWidget = NULL;
		}
	}
	__xuiContextClearOverlayOwners(pContext->pOverlayRoot, pWidget);
	for ( i = pContext->iEventRead; i < pContext->iEventCount; i++ ) {
		if ( __xuiContextWidgetContains(pWidget, pContext->pEvents[i].pTarget) ) {
			pContext->pEvents[i].pTarget = NULL;
		}
		if ( __xuiContextWidgetContains(pWidget, pContext->pEvents[i].pRelated) ) {
			pContext->pEvents[i].pRelated = NULL;
		}
	}
}

static xui_rect_i_t __xuiContextFullRect(xui_context pContext)
{
	xui_rect_i_t tRect;

	tRect.iX = 0;
	tRect.iY = 0;
	tRect.iW = __xuiContextCeilPositive(pContext->fViewportWidth);
	tRect.iH = __xuiContextCeilPositive(pContext->fViewportHeight);
	return tRect;
}

static int __xuiProxyRequiredCallbacks(const xui_proxy_t* pProxy)
{
	return (pProxy->getCaps != NULL) &&
	       (pProxy->clipboardSetText != NULL) &&
	       (pProxy->clipboardGetText != NULL) &&
	       (pProxy->imeGetEnabled != NULL) &&
	       (pProxy->imeSetEnabled != NULL) &&
	       (pProxy->imeSetCandidateRect != NULL) &&
	       (pProxy->surfaceCreate != NULL) &&
	       (pProxy->surfaceCreateRGBA != NULL) &&
	       (pProxy->surfaceLoadFile != NULL) &&
	       (pProxy->surfaceLoadMemory != NULL) &&
	       (pProxy->surfaceUpdateRGBA != NULL) &&
	       (pProxy->surfaceReadRGBA != NULL) &&
	       (pProxy->surfaceGetDesc != NULL) &&
	       (pProxy->surfaceDraw != NULL) &&
	       (pProxy->surfaceClear != NULL) &&
	       (pProxy->surfaceClearRect != NULL) &&
	       (pProxy->surfaceDrawTo != NULL) &&
	       (pProxy->surfaceDrawQuad != NULL) &&
	       (pProxy->surfaceDrawQuadTo != NULL) &&
	       (pProxy->surfaceGetSampler != NULL) &&
	       (pProxy->surfaceSetSampler != NULL) &&
	       (pProxy->surfaceGetGeneration != NULL) &&
	       (pProxy->surfaceDestroy != NULL) &&
	       (pProxy->shapePoint != NULL) &&
	       (pProxy->shapeLine != NULL) &&
	       (pProxy->shapeTriangleFill != NULL) &&
	       (pProxy->shapeTriangleStroke != NULL) &&
	       (pProxy->shapeRectFill != NULL) &&
	       (pProxy->shapeRectStroke != NULL) &&
	       (pProxy->shapeCircleFill != NULL) &&
	       (pProxy->shapeCircleStroke != NULL) &&
	       (pProxy->fontLoadFile != NULL) &&
	       (pProxy->fontLoadMemory != NULL) &&
	       (pProxy->fontGetMetrics != NULL) &&
	       (pProxy->fontDestroy != NULL) &&
	       (pProxy->textMeasure != NULL) &&
	       (pProxy->textDraw != NULL) &&
	       (pProxy->drawBegin != NULL) &&
	       (pProxy->drawEnd != NULL) &&
	       (pProxy->drawClearRect != NULL) &&
	       (pProxy->drawSurface != NULL) &&
	       (pProxy->drawSurfaceQuad != NULL) &&
	       (pProxy->drawPath != NULL) &&
	       (pProxy->drawSvgPath != NULL) &&
	       (pProxy->drawPoint != NULL) &&
	       (pProxy->drawLine != NULL) &&
	       (pProxy->drawTriangleFill != NULL) &&
	       (pProxy->drawTriangleStroke != NULL) &&
	       (pProxy->drawRectFill != NULL) &&
	       (pProxy->drawRectStroke != NULL) &&
	       (pProxy->drawCircleFill != NULL) &&
	       (pProxy->drawCircleStroke != NULL) &&
	       (pProxy->drawText != NULL);
}

static int __xuiContextValidateProxy(const xui_proxy_t* pProxy, xui_proxy_caps_t* pCaps)
{
	int iRet;

	if ( (pProxy == NULL) || (pCaps == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pProxy->iSize < sizeof(*pProxy)) || (pProxy->iVersion != XUI_PROXY_VERSION) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( !__xuiProxyRequiredCallbacks(pProxy) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	memset(pCaps, 0, sizeof(*pCaps));
	iRet = pProxy->getCaps((xui_proxy)pProxy, pCaps);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pCaps->iSize < sizeof(*pCaps) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( (pCaps->iCaps & XUI_CONTEXT_REQUIRED_CAPS) != XUI_CONTEXT_REQUIRED_CAPS ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( (pCaps->iSurfaceFormat != XUI_SURFACE_FORMAT_RGBA8) ||
	     (pCaps->iInternalAlpha != XUI_SURFACE_ALPHA_PREMULTIPLIED) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	if ( ((pCaps->tDefaultSampler.iMinFilter != XUI_SURFACE_FILTER_NEAREST) &&
	      (pCaps->tDefaultSampler.iMinFilter != XUI_SURFACE_FILTER_LINEAR)) ||
	     ((pCaps->tDefaultSampler.iMagFilter != XUI_SURFACE_FILTER_NEAREST) &&
	      (pCaps->tDefaultSampler.iMagFilter != XUI_SURFACE_FILTER_LINEAR)) ||
	     ((pCaps->tDefaultSampler.iWrapS != XUI_SURFACE_WRAP_CLAMP) &&
	      (pCaps->tDefaultSampler.iWrapS != XUI_SURFACE_WRAP_REPEAT)) ||
	     ((pCaps->tDefaultSampler.iWrapT != XUI_SURFACE_WRAP_CLAMP) &&
	      (pCaps->tDefaultSampler.iWrapT != XUI_SURFACE_WRAP_REPEAT)) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	return XUI_OK;
}

XUI_API int xuiCreate(xui_context* ppContext)
{
	xui_context pContext;

	if ( ppContext == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppContext = NULL;
	pContext = (xui_context)xrtMalloc(sizeof(*pContext));
	if ( pContext == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pContext, 0, sizeof(*pContext));
	pContext->iMagic = XUI_CONTEXT_MAGIC;
	pContext->fDpiScale = 1.0f;
	pContext->iGeneration = 1;
	pContext->iNextStylePropertyId = 1;
	pContext->iNextResourceGeneration = 1;
	pContext->pDamage = pContext->arrInlineDamage;
	pContext->iDamageCapacity = XUI_CONTEXT_DAMAGE_INLINE;
	pContext->pEvents = pContext->arrInlineEvents;
	pContext->iEventCapacity = XUI_CONTEXT_EVENT_INLINE;
	pContext->pHotkeys = pContext->arrInlineHotkeys;
	pContext->iHotkeyCapacity = XUI_CONTEXT_HOTKEY_INLINE;
	pContext->pFonts = pContext->arrInlineFonts;
	pContext->iFontCapacity = XUI_CONTEXT_FONT_INLINE;
	__xuiContextInitIcons(pContext);
	if ( __xuiContextInitLanguages(pContext) != XUI_OK ) {
		__xuiContextDestroyIcons(pContext);
		pContext->iMagic = 0;
		xrtFree(pContext);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	xuiThemeDefault(&pContext->tTheme);
	pContext->tChromeStyle.iSize = sizeof(pContext->tChromeStyle);
	pContext->tChromeStyle.iBackdropColor = XUI_COLOR_RGBA(18, 20, 24, 255);
	pContext->tChromeStyle.iPopupColor = XUI_COLOR_RGBA(248, 249, 251, 255);
	pContext->tChromeStyle.iPopupBorderColor = XUI_COLOR_RGBA(120, 126, 138, 255);
	pContext->tChromeStyle.iTooltipColor = XUI_COLOR_RGBA(28, 32, 38, 245);
	pContext->tChromeStyle.iTooltipTextColor = XUI_COLOR_RGBA(248, 250, 252, 255);
	pContext->tChromeStyle.iModalOverlayColor = XUI_COLOR_RGBA(0, 0, 0, 96);
	pContext->tChromeStyle.fBorderWidth = 1.0f;
	pContext->tChromeStyle.fShadowSize = 8.0f;
	*ppContext = pContext;
	return XUI_OK;
}

static void __xuiContextDestroyFonts(xui_context pContext)
{
	int i;

	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	for ( i = 0; i < pContext->iFontCount; i++ ) {
		if ( pContext->pFonts[i].sName != NULL ) {
			xrtFree(pContext->pFonts[i].sName);
		}
	}
	if ( pContext->pFonts != pContext->arrInlineFonts ) {
		xrtFree(pContext->pFonts);
	}
	pContext->pFonts = pContext->arrInlineFonts;
	pContext->iFontCount = 0;
	pContext->iFontCapacity = XUI_CONTEXT_FONT_INLINE;
	pContext->pDefaultFont = NULL;
}

XUI_API void xuiDestroy(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	if ( pContext->pRoot != NULL ) {
		xuiWidgetDestroy(pContext->pRoot);
		pContext->pRoot = NULL;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		xuiWidgetDestroy(pContext->pOverlayRoot);
		pContext->pOverlayRoot = NULL;
	}
	if ( pContext->pDamage != pContext->arrInlineDamage ) {
		xrtFree(pContext->pDamage);
	}
	if ( pContext->pEvents != pContext->arrInlineEvents ) {
		xrtFree(pContext->pEvents);
	}
	xuiInternalContextDestroyInput(pContext);
	xuiInternalContextDestroyRenderTree(pContext);
	__xuiContextDestroyIcons(pContext);
	xuiInternalContextDestroyResources(pContext);
	xuiInternalContextDestroyStyles(pContext);
	xuiInternalContextDestroyWidgetTypes(pContext);
	__xuiContextDestroyLanguages(pContext);
	__xuiContextDestroyFonts(pContext);
	pContext->iMagic = 0;
	xrtFree(pContext);
}

XUI_API int xuiSetProxy(xui_context pContext, const xui_proxy_t* pProxy)
{
	xui_proxy_caps_t tCaps;
	int iRet;

	if ( !__xuiContextValid(pContext) || (pProxy == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->bHasProxy ) {
		return XUI_ERROR_ALREADY_INITIALIZED;
	}
	iRet = __xuiContextValidateProxy(pProxy, &tCaps);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pContext->tProxy = *pProxy;
	pContext->tProxyCaps = tCaps;
	pContext->bHasProxy = 1;
	__xuiContextBumpGeneration(pContext);
	return XUI_OK;
}

XUI_API int xuiGetProxy(xui_context pContext, xui_proxy_t* pProxy)
{
	if ( !__xuiContextValid(pContext) || (pProxy == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	*pProxy = pContext->tProxy;
	return XUI_OK;
}

XUI_API int xuiGetProxyCaps(xui_context pContext, xui_proxy_caps_t* pCaps)
{
	if ( !__xuiContextValid(pContext) || (pCaps == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	*pCaps = pContext->tProxyCaps;
	return XUI_OK;
}

XUI_API int xuiSetViewportSize(xui_context pContext, float fWidth, float fHeight)
{
	if ( !__xuiContextValid(pContext) || !__xuiFloatValid(fWidth) || !__xuiFloatValid(fHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->fViewportWidth == fWidth) && (pContext->fViewportHeight == fHeight) ) {
		return XUI_OK;
	}
	pContext->fViewportWidth = fWidth;
	pContext->fViewportHeight = fHeight;
	pContext->iDamageCount = 0;
	if ( (__xuiContextCeilPositive(fWidth) <= 0) || (__xuiContextCeilPositive(fHeight) <= 0) ) {
		__xuiContextBumpGeneration(pContext);
		return XUI_OK;
	}
	return __xuiContextAddDamage(pContext, __xuiContextFullRect(pContext));
}

XUI_API xui_vec2_t xuiGetViewportSize(xui_context pContext)
{
	xui_vec2_t tSize;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( __xuiContextValid(pContext) ) {
		tSize.fX = pContext->fViewportWidth;
		tSize.fY = pContext->fViewportHeight;
	}
	return tSize;
}

XUI_API int xuiSetVirtualDpi(xui_context pContext, float fDpiScale)
{
	if ( !__xuiContextValid(pContext) || (fDpiScale != fDpiScale) || (fDpiScale <= 0.0f) || (fDpiScale > 64.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->fDpiScale == fDpiScale ) {
		return XUI_OK;
	}
	pContext->fDpiScale = fDpiScale;
	pContext->iDamageCount = 0;
	if ( (__xuiContextCeilPositive(pContext->fViewportWidth) <= 0) ||
	     (__xuiContextCeilPositive(pContext->fViewportHeight) <= 0) ) {
		__xuiContextBumpGeneration(pContext);
		return XUI_OK;
	}
	return __xuiContextAddDamage(pContext, __xuiContextFullRect(pContext));
}

XUI_API float xuiGetVirtualDpi(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return 0.0f;
	}
	return pContext->fDpiScale;
}

XUI_API void xuiThemeDefault(xui_theme_t* pTheme)
{
	if ( pTheme == NULL ) {
		return;
	}
	memset(pTheme, 0, sizeof(*pTheme));
	pTheme->iSize = sizeof(*pTheme);
	pTheme->iTextColor = XUI_COLOR_RGBA(34, 37, 41, 255);
	pTheme->iBackgroundColor = XUI_COLOR_RGBA(245, 247, 250, 255);
	pTheme->iPanelColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pTheme->iBorderColor = XUI_COLOR_RGBA(188, 195, 205, 255);
	pTheme->iAccentColor = XUI_COLOR_RGBA(0, 112, 210, 255);
	pTheme->iSelectionColor = XUI_COLOR_RGBA(0, 112, 210, 72);
	pTheme->iStateNormalColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pTheme->iStateHoverColor = XUI_COLOR_RGBA(236, 244, 252, 255);
	pTheme->iStateActiveColor = XUI_COLOR_RGBA(214, 232, 248, 255);
	pTheme->iStateFocusColor = XUI_COLOR_RGBA(0, 112, 210, 255);
	pTheme->iStateDisabledColor = XUI_COLOR_RGBA(162, 168, 176, 255);
	pTheme->fPadding = 6.0f;
	pTheme->fSpacing = 6.0f;
	pTheme->fBorderWidth = 1.0f;
}

static int __xuiCoreSetColorToken(xui_context pContext, const char* sName, uint32_t iColor)
{
	xui_style_value_t tValue;

	memset(&tValue, 0, sizeof(tValue));
	tValue.iSize = sizeof(tValue);
	tValue.iType = XUI_STYLE_VALUE_COLOR;
	tValue.iColor = iColor;
	return xuiStyleSetToken(pContext, sName, &tValue);
}

static int __xuiCoreSetFloatToken(xui_context pContext, const char* sName, float fValue)
{
	xui_style_value_t tValue;

	memset(&tValue, 0, sizeof(tValue));
	tValue.iSize = sizeof(tValue);
	tValue.iType = XUI_STYLE_VALUE_FLOAT;
	tValue.fFloat = fValue;
	return xuiStyleSetToken(pContext, sName, &tValue);
}

XUI_API int xuiSetTheme(xui_context pContext, const xui_theme_t* pTheme)
{
	xui_theme_t tTheme;
	int iRet;

	if ( !__xuiContextValid(pContext) || (pTheme == NULL) ||
	     ((pTheme->iSize != 0) && (pTheme->iSize < sizeof(*pTheme))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tTheme = *pTheme;
	tTheme.iSize = sizeof(tTheme);
	pContext->tTheme = tTheme;
	pContext->pDefaultFont = tTheme.pFont;
	iRet = xuiStyleBeginUpdate(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiCoreSetColorToken(pContext, "theme.text", tTheme.iTextColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.background", tTheme.iBackgroundColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.panel", tTheme.iPanelColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.border", tTheme.iBorderColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.accent", tTheme.iAccentColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.selection", tTheme.iSelectionColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.state.normal", tTheme.iStateNormalColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.state.hover", tTheme.iStateHoverColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.state.active", tTheme.iStateActiveColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.state.focus", tTheme.iStateFocusColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "theme.state.disabled", tTheme.iStateDisabledColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetFloatToken(pContext, "theme.padding", tTheme.fPadding);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetFloatToken(pContext, "theme.spacing", tTheme.fSpacing);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetFloatToken(pContext, "theme.border_width", tTheme.fBorderWidth);
	if ( iRet != XUI_OK ) {
		(void)xuiStyleEndUpdate(pContext);
		return iRet;
	}
	return xuiStyleEndUpdate(pContext);
}

XUI_API int xuiGetTheme(xui_context pContext, xui_theme_t* pTheme)
{
	if ( !__xuiContextValid(pContext) || (pTheme == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pTheme = pContext->tTheme;
	pTheme->iSize = sizeof(*pTheme);
	return XUI_OK;
}

XUI_API int xuiSetChromeStyle(xui_context pContext, const xui_chrome_style_t* pChrome)
{
	xui_chrome_style_t tChrome;
	int iRet;

	if ( !__xuiContextValid(pContext) || (pChrome == NULL) ||
	     ((pChrome->iSize != 0) && (pChrome->iSize < sizeof(*pChrome))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tChrome = *pChrome;
	tChrome.iSize = sizeof(tChrome);
	pContext->tChromeStyle = tChrome;
	iRet = xuiStyleBeginUpdate(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiCoreSetColorToken(pContext, "chrome.backdrop", tChrome.iBackdropColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "chrome.popup", tChrome.iPopupColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "chrome.popup_border", tChrome.iPopupBorderColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "chrome.tooltip", tChrome.iTooltipColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "chrome.tooltip_text", tChrome.iTooltipTextColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetColorToken(pContext, "chrome.modal_overlay", tChrome.iModalOverlayColor);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetFloatToken(pContext, "chrome.border_width", tChrome.fBorderWidth);
	if ( iRet == XUI_OK ) iRet = __xuiCoreSetFloatToken(pContext, "chrome.shadow_size", tChrome.fShadowSize);
	if ( iRet != XUI_OK ) {
		(void)xuiStyleEndUpdate(pContext);
		return iRet;
	}
	return xuiStyleEndUpdate(pContext);
}

XUI_API int xuiGetChromeStyle(xui_context pContext, xui_chrome_style_t* pChrome)
{
	if ( !__xuiContextValid(pContext) || (pChrome == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pChrome = pContext->tChromeStyle;
	pChrome->iSize = sizeof(*pChrome);
	return XUI_OK;
}

XUI_API int xuiSetDefaultFont(xui_context pContext, xui_font pFont)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->pDefaultFont = pFont;
	pContext->tTheme.pFont = pFont;
	xuiInternalContextBumpGeneration(pContext);
	return XUI_OK;
}

XUI_API xui_font xuiGetDefaultFont(xui_context pContext)
{
	return __xuiContextValid(pContext) ? pContext->pDefaultFont : NULL;
}

static int __xuiCoreReserveFonts(xui_context pContext, int iCapacity)
{
	xui_font_entry_t* pFonts;

	if ( iCapacity <= pContext->iFontCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pContext->iFontCapacity * 2) ) {
		iCapacity = pContext->iFontCapacity * 2;
	}
	if ( pContext->pFonts == pContext->arrInlineFonts ) {
		pFonts = (xui_font_entry_t*)xrtMalloc(sizeof(*pFonts) * (size_t)iCapacity);
		if ( pFonts == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pFonts, pContext->pFonts, sizeof(*pFonts) * (size_t)pContext->iFontCount);
	} else {
		pFonts = (xui_font_entry_t*)xrtRealloc(pContext->pFonts, sizeof(*pFonts) * (size_t)iCapacity);
		if ( pFonts == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	pContext->pFonts = pFonts;
	pContext->iFontCapacity = iCapacity;
	return XUI_OK;
}

XUI_API int xuiRegisterFont(xui_context pContext, const char* sName, xui_font pFont)
{
	char* sCopy;
	int i;
	int iRet;

	if ( !__xuiContextValid(pContext) || (sName == NULL) || (sName[0] == '\0') || (pFont == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pContext->iFontCount; i++ ) {
		if ( strcmp(pContext->pFonts[i].sName, sName) == 0 ) {
			pContext->pFonts[i].pFont = pFont;
			xuiInternalContextBumpGeneration(pContext);
			return XUI_OK;
		}
	}
	iRet = __xuiCoreReserveFonts(pContext, pContext->iFontCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	sCopy = __xuiCoreStringDuplicate(sName);
	if ( sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pContext->pFonts[pContext->iFontCount].sName = sCopy;
	pContext->pFonts[pContext->iFontCount].pFont = pFont;
	pContext->iFontCount++;
	xuiInternalContextBumpGeneration(pContext);
	return XUI_OK;
}

XUI_API xui_font xuiFindFont(xui_context pContext, const char* sName)
{
	int i;

	if ( !__xuiContextValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	for ( i = 0; i < pContext->iFontCount; i++ ) {
		if ( strcmp(pContext->pFonts[i].sName, sName) == 0 ) {
			return pContext->pFonts[i].pFont;
		}
	}
	return NULL;
}

XUI_API void xuiClearFonts(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	__xuiContextDestroyFonts(pContext);
	xuiInternalContextBumpGeneration(pContext);
}

XUI_API int xuiInvalidateRect(xui_context pContext, xui_rect_i_t tRect)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiContextAddDamage(pContext, tRect);
}

XUI_API int xuiInvalidateAll(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iDamageCount = 0;
	return __xuiContextAddDamage(pContext, __xuiContextFullRect(pContext));
}

XUI_API int xuiHasDamage(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return 0;
	}
	return pContext->iDamageCount > 0;
}

XUI_API int xuiGetDamageRects(xui_context pContext, xui_rect_i_t* pRects, int iCapacity)
{
	int i;
	int iCopyCount;

	if ( !__xuiContextValid(pContext) || (iCapacity < 0) || ((iCapacity > 0) && (pRects == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCopyCount = pContext->iDamageCount;
	if ( iCopyCount > iCapacity ) {
		iCopyCount = iCapacity;
	}
	for ( i = 0; i < iCopyCount; i++ ) {
		pRects[i] = pContext->pDamage[i];
	}
	return pContext->iDamageCount;
}

XUI_API void xuiClearDamage(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) ) {
		return;
	}
	pContext->iDamageCount = 0;
}

XUI_API int xuiResourceSet(xui_context pContext, xui_resource* ppResource, const xui_resource_desc_t* pDesc)
{
	xui_resource pResource;
	xui_resource pExisting;
	xui_resource pPrev;
	char* sName;

	if ( !__xuiContextValid(pContext) || (pDesc == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->sName == NULL) || (pDesc->sName[0] == '\0') || (pDesc->iKind <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ppResource != NULL ) {
		*ppResource = NULL;
	}
	sName = __xuiCoreStringDuplicate(pDesc->sName);
	if ( sName == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pPrev = NULL;
	for ( pExisting = pContext->pResources; pExisting != NULL; pExisting = pExisting->pNext ) {
		if ( strcmp(pExisting->sName, pDesc->sName) == 0 ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pExisting->pNext;
			} else {
				pContext->pResources = pExisting->pNext;
			}
			__xuiResourceDestroyOne(pExisting);
			break;
		}
		pPrev = pExisting;
	}
	pResource = (xui_resource)xrtCalloc(1, sizeof(*pResource));
	if ( pResource == NULL ) {
		xrtFree(sName);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pResource->iMagic = XUI_RESOURCE_MAGIC;
	pResource->pContext = pContext;
	pResource->sName = sName;
	pResource->iKind = pDesc->iKind;
	pResource->pHandle = pDesc->pHandle;
	pResource->pUser = pDesc->pUser;
	pResource->onDestroy = pDesc->onDestroy;
	pResource->iGeneration = pContext->iNextResourceGeneration++;
	pResource->iRefCount = 1;
	if ( pContext->iNextResourceGeneration == 0 ) {
		pContext->iNextResourceGeneration = 1;
	}
	pResource->pNext = pContext->pResources;
	pContext->pResources = pResource;
	__xuiContextBumpGeneration(pContext);
	if ( ppResource != NULL ) {
		*ppResource = pResource;
	}
	return XUI_OK;
}

XUI_API xui_resource xuiResourceFind(xui_context pContext, const char* sName)
{
	xui_resource pResource;

	if ( !__xuiContextValid(pContext) || (sName == NULL) || (sName[0] == '\0') ) {
		return NULL;
	}
	for ( pResource = pContext->pResources; pResource != NULL; pResource = pResource->pNext ) {
		if ( strcmp(pResource->sName, sName) == 0 ) {
			return pResource;
		}
	}
	return NULL;
}

XUI_API int xuiResourceRemove(xui_resource pResource)
{
	xui_context pContext;
	xui_resource pScan;
	xui_resource pPrev;

	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = pResource->pContext;
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPrev = NULL;
	for ( pScan = pContext->pResources; pScan != NULL; pScan = pScan->pNext ) {
		if ( pScan == pResource ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pScan->pNext;
			} else {
				pContext->pResources = pScan->pNext;
			}
			__xuiResourceRemoveDependencyReferences(pContext, pScan);
			__xuiResourceDestroyOne(pScan);
			__xuiContextBumpGeneration(pContext);
			return XUI_OK;
		}
		pPrev = pScan;
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiResourceTouch(xui_resource pResource)
{
	xui_context pContext;
	xui_resource pScan;
	xui_resource_dependency_t* pDependency;

	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = pResource->pContext;
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pResource->iGeneration = pContext->iNextResourceGeneration++;
	if ( pContext->iNextResourceGeneration == 0 ) {
		pContext->iNextResourceGeneration = 1;
	}
	for ( pScan = pContext->pResources; pScan != NULL; pScan = pScan->pNext ) {
		if ( pScan == pResource ) {
			continue;
		}
		for ( pDependency = pScan->pDependencies; pDependency != NULL; pDependency = pDependency->pNext ) {
			if ( pDependency->pResource == pResource ) {
				pScan->iGeneration = pContext->iNextResourceGeneration++;
				if ( pContext->iNextResourceGeneration == 0 ) {
					pContext->iNextResourceGeneration = 1;
				}
				break;
			}
		}
	}
	__xuiContextBumpGeneration(pContext);
	return XUI_OK;
}

XUI_API const char* xuiResourceGetName(xui_resource pResource)
{
	return __xuiResourceValid(pResource) ? pResource->sName : NULL;
}

XUI_API int xuiResourceGetKind(xui_resource pResource)
{
	return __xuiResourceValid(pResource) ? pResource->iKind : 0;
}

XUI_API void* xuiResourceGetHandle(xui_resource pResource)
{
	return __xuiResourceValid(pResource) ? pResource->pHandle : NULL;
}

XUI_API uint32_t xuiResourceGetGeneration(xui_resource pResource)
{
	return __xuiResourceValid(pResource) ? pResource->iGeneration : 0;
}

XUI_API int xuiResourceAddRef(xui_resource pResource)
{
	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pResource->iRefCount++;
	if ( pResource->iRefCount <= 0 ) {
		pResource->iRefCount = 1;
		return XUI_ERROR_UNSUPPORTED;
	}
	return XUI_OK;
}

XUI_API int xuiResourceRelease(xui_resource pResource)
{
	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pResource->iRefCount--;
	if ( pResource->iRefCount > 0 ) {
		return XUI_OK;
	}
	return xuiResourceRemove(pResource);
}

XUI_API int xuiResourceGetRefCount(xui_resource pResource)
{
	return __xuiResourceValid(pResource) ? pResource->iRefCount : 0;
}

XUI_API int xuiResourceAddDependency(xui_resource pResource, xui_resource pDependency)
{
	xui_resource_dependency_t* pScan;
	xui_resource_dependency_t* pNewDependency;

	if ( !__xuiResourceValid(pResource) ||
	     !__xuiResourceValid(pDependency) ||
	     (pResource->pContext != pDependency->pContext) ||
	     (pResource == pDependency) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( pScan = pResource->pDependencies; pScan != NULL; pScan = pScan->pNext ) {
		if ( pScan->pResource == pDependency ) {
			return XUI_OK;
		}
	}
	pNewDependency = (xui_resource_dependency_t*)xrtCalloc(1, sizeof(*pNewDependency));
	if ( pNewDependency == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pNewDependency->pResource = pDependency;
	pNewDependency->pNext = pResource->pDependencies;
	pResource->pDependencies = pNewDependency;
	return xuiResourceTouch(pResource);
}

XUI_API void xuiResourceClearDependencies(xui_resource pResource)
{
	xui_resource_dependency_t* pDependency;
	xui_resource_dependency_t* pNext;

	if ( !__xuiResourceValid(pResource) ) {
		return;
	}
	pDependency = pResource->pDependencies;
	while ( pDependency != NULL ) {
		pNext = pDependency->pNext;
		xrtFree(pDependency);
		pDependency = pNext;
	}
	pResource->pDependencies = NULL;
	(void)xuiResourceTouch(pResource);
}

XUI_API int xuiResourceGetDependencyCount(xui_resource pResource)
{
	xui_resource_dependency_t* pDependency;
	int iCount;

	if ( !__xuiResourceValid(pResource) ) {
		return 0;
	}
	iCount = 0;
	for ( pDependency = pResource->pDependencies; pDependency != NULL; pDependency = pDependency->pNext ) {
		iCount++;
	}
	return iCount;
}

XUI_API xui_resource xuiResourceGetDependency(xui_resource pResource, int iIndex)
{
	xui_resource_dependency_t* pDependency;
	int i;

	if ( !__xuiResourceValid(pResource) || (iIndex < 0) ) {
		return NULL;
	}
	i = 0;
	for ( pDependency = pResource->pDependencies; pDependency != NULL; pDependency = pDependency->pNext ) {
		if ( i == iIndex ) {
			return pDependency->pResource;
		}
		i++;
	}
	return NULL;
}

XUI_API int xuiPainterBegin(xui_context pContext, xui_surface pTarget, xui_painter* ppPainter)
{
	xui_painter pPainter;
	xui_proxy pProxy;
	int iRet;

	if ( !__xuiContextValid(pContext) || (pTarget == NULL) || (ppPainter == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppPainter = NULL;
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pPainter = (xui_painter)xrtCalloc(1, sizeof(*pPainter));
	if ( pPainter == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pPainter->iMagic = XUI_PAINTER_MAGIC;
	pPainter->pContext = pContext;
	pPainter->pTarget = pTarget;
	iRet = pProxy->drawBegin(pProxy, &pPainter->pDraw, pTarget);
	if ( iRet != XUI_OK ) {
		pPainter->iMagic = 0;
		xrtFree(pPainter);
		return iRet;
	}
	*ppPainter = pPainter;
	return XUI_OK;
}

XUI_API int xuiPainterEnd(xui_painter pPainter)
{
	xui_proxy pProxy;
	int iRet;

	if ( !__xuiPainterValid(pPainter) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	if ( pProxy == NULL ) {
		iRet = XUI_ERROR_NOT_INITIALIZED;
	} else {
		iRet = pProxy->drawEnd(pProxy, pPainter->pDraw);
	}
	pPainter->iMagic = 0;
	xrtFree(pPainter);
	return iRet;
}

XUI_API xui_draw_context xuiPainterGetDrawContext(xui_painter pPainter)
{
	return __xuiPainterValid(pPainter) ? pPainter->pDraw : NULL;
}

XUI_API int xuiPainterClearRect(xui_painter pPainter, xui_rect_t tRect, uint32_t iColor)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	tRect = xuiInternalSnapRectOut(tRect);
	return (pProxy != NULL) ? pProxy->drawClearRect(pProxy, pPainter->pDraw, tRect, iColor) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterDrawSurface(xui_painter pPainter, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) || (pSurface == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((iColor & 0xffu) == 0u) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	tDst = xuiInternalSnapRect(tDst);
	return (pProxy != NULL) ? pProxy->drawSurface(pProxy, pPainter->pDraw, pSurface, tSrc, tDst, iColor, iFlags) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterDrawSurfaceQuad(xui_painter pPainter, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) || (pSurface == NULL) || (pVertices == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pVertices[0].iColor | pVertices[1].iColor | pVertices[2].iColor | pVertices[3].iColor) & 0xffu) == 0u ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	return (pProxy != NULL) ? pProxy->drawSurfaceQuad(pProxy, pPainter->pDraw, pSurface, pVertices, iFlags) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterDrawMeshTriangles(xui_painter pPainter, const xui_mesh_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iFlags)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) || (pVertices == NULL) || (iVertexCount <= 0) ||
	     (pIndices == NULL) || (iIndexCount <= 0) || ((iIndexCount % 3) != 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	{
		int i;
		uint32_t iAlphaMask;

		iAlphaMask = 0u;
		for ( i = 0; i < iVertexCount; i++ ) {
			iAlphaMask |= pVertices[i].iColor;
		}
		if ( (iAlphaMask & 0xffu) == 0u ) {
			return XUI_OK;
		}
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	if ( pProxy->drawMeshTriangles == NULL ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	return pProxy->drawMeshTriangles(pProxy, pPainter->pDraw, pVertices, iVertexCount, pIndices, iIndexCount, iFlags);
}

XUI_API int xuiPainterFillPath(xui_painter pPainter, xui_path pPath, uint32_t iColor, float fTolerance)
{
	xui_path_style_t tStyle;

	if ( !__xuiPainterValid(pPainter) || !__xuiPathValid(pPath) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iColor & 0xffu) == 0u ) {
		return XUI_OK;
	}
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = (uint32_t)sizeof(tStyle);
	tStyle.iFillColor = iColor;
	tStyle.iFillRule = XUI_PATH_FILL_NON_ZERO;
	return xuiPainterDrawPath(pPainter, pPath, &tStyle, fTolerance);
}

XUI_API int xuiPainterDrawPath(xui_painter pPainter, xui_path pPath, const xui_path_style_t* pStyle, float fTolerance)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	return (pProxy != NULL) ? xuiInternalDrawPath(pProxy, pPainter->pDraw, pPath, pStyle, fTolerance) : XUI_ERROR_NOT_INITIALIZED;
}

int xuiInternalDrawPath(xui_proxy pProxy, xui_draw_context pDraw, xui_path pPath, const xui_path_style_t* pStyle, float fTolerance)
{
	if ( (pProxy == NULL) || (pDraw == NULL) || !__xuiPathValid(pPath) || (pStyle == NULL) ||
	     (pStyle->iSize < sizeof(*pStyle)) || (fTolerance <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPath->iCommandCount == 0 ||
	     (((pStyle->iFillColor & 0xffu) == 0u) &&
	      (((pStyle->iStrokeColor & 0xffu) == 0u) || (pStyle->fStrokeWidth <= 0.0f))) ) {
		return XUI_OK;
	}
	return pProxy->drawPath(pProxy, pDraw, pPath->pCommands, pPath->iCommandCount, pStyle, fTolerance);
}

XUI_API int xuiPainterDrawSvgPath(xui_painter pPainter, const char* sPath, xui_rect_t tViewBox, xui_rect_t tTarget, const xui_path_style_t* pStyle, float fTolerance)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) || (sPath == NULL) || (sPath[0] == '\0') ||
	     (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) ||
	     (tTarget.fW <= 0.0f) || (tTarget.fH <= 0.0f) ||
	     (pStyle == NULL) || (pStyle->iSize < sizeof(*pStyle)) || (fTolerance <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pStyle->iFillColor & 0xffu) == 0u) &&
	     (((pStyle->iStrokeColor & 0xffu) == 0u) || (pStyle->fStrokeWidth <= 0.0f)) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	return (pProxy != NULL) ? pProxy->drawSvgPath(pProxy, pPainter->pDraw, sPath, tViewBox, tTarget, pStyle, fTolerance) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterFillRect(xui_painter pPainter, xui_rect_t tRect, uint32_t iColor)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || ((iColor & 0xffu) == 0u) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	tRect = xuiInternalSnapRect(tRect);
	return (pProxy != NULL) ? pProxy->drawRectFill(pProxy, pPainter->pDraw, tRect, iColor) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterStrokeRect(xui_painter pPainter, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || ((iColor & 0xffu) == 0u) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	tRect = xuiInternalSnapRect(tRect);
	fWidth = xuiInternalSnapSize(fWidth);
	return (pProxy != NULL) ? pProxy->drawRectStroke(pProxy, pPainter->pDraw, tRect, fWidth, iColor) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiVectorIconGetCount(void)
{
	return (int)(sizeof(g_arrXuiVectorIcons) / sizeof(g_arrXuiVectorIcons[0]));
}

XUI_API const char* xuiVectorIconGetName(int iIndex)
{
	if ( (iIndex < 0) || (iIndex >= xuiVectorIconGetCount()) ) {
		return NULL;
	}
	return g_arrXuiVectorIcons[iIndex].sName;
}

static const xui_vector_icon_def_t* __xuiVectorIconFind(const char* sName)
{
	int i;

	if ( sName == NULL ) {
		return NULL;
	}
	for ( i = 0; i < xuiVectorIconGetCount(); i++ ) {
		if ( strcmp(g_arrXuiVectorIcons[i].sName, sName) == 0 ) {
			return &g_arrXuiVectorIcons[i];
		}
	}
	return NULL;
}

XUI_API int xuiPainterDrawVectorIcon(xui_painter pPainter, const char* sName, xui_rect_t tRect, uint32_t iColor)
{
	const xui_vector_icon_def_t* pIcon;
	xui_path_style_t tStyle;

	if ( !__xuiPainterValid(pPainter) || (sName == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pIcon = __xuiVectorIconFind(sName);
	if ( pIcon == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	if ( (iColor & 0xffu) == 0u ) {
		return XUI_OK;
	}
	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iSize = sizeof(tStyle);
	tStyle.iStrokeColor = iColor;
	tStyle.fStrokeWidth = 2.0f;
	tStyle.iLineJoin = XUI_PATH_JOIN_ROUND;
	tStyle.iLineCap = XUI_PATH_CAP_ROUND;
	return xuiPainterDrawSvgPath(pPainter, pIcon->sPath,
		(xui_rect_t){0.0f, 0.0f, pIcon->fViewWidth, pIcon->fViewHeight}, tRect, &tStyle, 1.0f);
}

XUI_API int xuiPainterDrawText(xui_painter pPainter, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	xui_proxy pProxy;

	if ( !__xuiPainterValid(pPainter) || (pFont == NULL) || (sText == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (sText[0] == '\0') || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || ((iColor & 0xffu) == 0u) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pPainter->pContext);
	tRect = xuiInternalSnapRect(tRect);
	return (pProxy != NULL) ? pProxy->drawText(pProxy, pPainter->pDraw, pFont, sText, tRect, iColor, iFlags) : XUI_ERROR_NOT_INITIALIZED;
}

XUI_API int xuiPainterDrawNinePatch(xui_painter pPainter, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, xui_thickness_t tSlice, uint32_t iColor, uint32_t iFlags)
{
	xui_rect_t arrSrc[9];
	xui_rect_t arrDst[9];
	float arrSrcX[4];
	float arrSrcY[4];
	float arrDstX[4];
	float arrDstY[4];
	float fSrcLeft;
	float fSrcTop;
	float fSrcRight;
	float fSrcBottom;
	float fDstLeft;
	float fDstTop;
	float fDstRight;
	float fDstBottom;
	int x;
	int y;
	int iIndex;
	int iRet;

	if ( !__xuiPainterValid(pPainter) || (pSurface == NULL) ||
	     (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) || (tDst.fW < 0.0f) || (tDst.fH < 0.0f) ||
	     (tSlice.fLeft < 0.0f) || (tSlice.fTop < 0.0f) || (tSlice.fRight < 0.0f) || (tSlice.fBottom < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((iColor & 0xffu) == 0u) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fSrcLeft = tSlice.fLeft;
	fSrcTop = tSlice.fTop;
	fSrcRight = tSlice.fRight;
	fSrcBottom = tSlice.fBottom;
	if ( fSrcLeft + fSrcRight > tSrc.fW ) {
		fSrcRight = tSrc.fW - fSrcLeft;
		if ( fSrcRight < 0.0f ) {
			fSrcRight = 0.0f;
			fSrcLeft = tSrc.fW;
		}
	}
	if ( fSrcTop + fSrcBottom > tSrc.fH ) {
		fSrcBottom = tSrc.fH - fSrcTop;
		if ( fSrcBottom < 0.0f ) {
			fSrcBottom = 0.0f;
			fSrcTop = tSrc.fH;
		}
	}
	fDstLeft = fSrcLeft;
	fDstTop = fSrcTop;
	fDstRight = fSrcRight;
	fDstBottom = fSrcBottom;
	if ( fDstLeft + fDstRight > tDst.fW ) {
		fDstRight = tDst.fW - fDstLeft;
		if ( fDstRight < 0.0f ) {
			fDstRight = 0.0f;
			fDstLeft = tDst.fW;
		}
	}
	if ( fDstTop + fDstBottom > tDst.fH ) {
		fDstBottom = tDst.fH - fDstTop;
		if ( fDstBottom < 0.0f ) {
			fDstBottom = 0.0f;
			fDstTop = tDst.fH;
		}
	}
	arrSrcX[0] = tSrc.fX;
	arrSrcX[1] = tSrc.fX + fSrcLeft;
	arrSrcX[2] = tSrc.fX + tSrc.fW - fSrcRight;
	arrSrcX[3] = tSrc.fX + tSrc.fW;
	arrSrcY[0] = tSrc.fY;
	arrSrcY[1] = tSrc.fY + fSrcTop;
	arrSrcY[2] = tSrc.fY + tSrc.fH - fSrcBottom;
	arrSrcY[3] = tSrc.fY + tSrc.fH;
	arrDstX[0] = tDst.fX;
	arrDstX[1] = tDst.fX + fDstLeft;
	arrDstX[2] = tDst.fX + tDst.fW - fDstRight;
	arrDstX[3] = tDst.fX + tDst.fW;
	arrDstY[0] = tDst.fY;
	arrDstY[1] = tDst.fY + fDstTop;
	arrDstY[2] = tDst.fY + tDst.fH - fDstBottom;
	arrDstY[3] = tDst.fY + tDst.fH;
	for ( y = 0; y < 3; y++ ) {
		for ( x = 0; x < 3; x++ ) {
			iIndex = (y * 3) + x;
			arrSrc[iIndex].fX = arrSrcX[x];
			arrSrc[iIndex].fY = arrSrcY[y];
			arrSrc[iIndex].fW = arrSrcX[x + 1] - arrSrcX[x];
			arrSrc[iIndex].fH = arrSrcY[y + 1] - arrSrcY[y];
			arrDst[iIndex].fX = arrDstX[x];
			arrDst[iIndex].fY = arrDstY[y];
			arrDst[iIndex].fW = arrDstX[x + 1] - arrDstX[x];
			arrDst[iIndex].fH = arrDstY[y + 1] - arrDstY[y];
		}
	}
	for ( iIndex = 0; iIndex < 9; iIndex++ ) {
		if ( (arrSrc[iIndex].fW <= 0.0f) || (arrSrc[iIndex].fH <= 0.0f) ||
		     (arrDst[iIndex].fW <= 0.0f) || (arrDst[iIndex].fH <= 0.0f) ) {
			continue;
		}
		iRet = xuiPainterDrawSurface(pPainter, pSurface, arrSrc[iIndex], arrDst[iIndex], iColor, iFlags);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiPathCreate(xui_path* ppPath)
{
	xui_path pPath;

	if ( ppPath == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppPath = NULL;
	pPath = (xui_path)xrtCalloc(1, sizeof(*pPath));
	if ( pPath == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pPath->iMagic = XUI_PATH_MAGIC;
	*ppPath = pPath;
	return XUI_OK;
}

XUI_API void xuiPathDestroy(xui_path pPath)
{
	if ( !__xuiPathValid(pPath) ) {
		return;
	}
	if ( pPath->pCommands != NULL ) {
		xrtFree(pPath->pCommands);
	}
	pPath->iMagic = 0;
	xrtFree(pPath);
}

XUI_API int xuiPathClear(xui_path pPath)
{
	if ( !__xuiPathValid(pPath) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPath->iCommandCount = 0;
	return XUI_OK;
}

XUI_API int xuiPathMoveTo(xui_path pPath, float fX, float fY)
{
	return __xuiPathAddCommand(pPath, XUI_PATH_CMD_MOVE, (xui_vec2_t){fX, fY}, (xui_vec2_t){0.0f, 0.0f}, (xui_vec2_t){0.0f, 0.0f});
}

XUI_API int xuiPathLineTo(xui_path pPath, float fX, float fY)
{
	return __xuiPathAddCommand(pPath, XUI_PATH_CMD_LINE, (xui_vec2_t){fX, fY}, (xui_vec2_t){0.0f, 0.0f}, (xui_vec2_t){0.0f, 0.0f});
}

XUI_API int xuiPathQuadTo(xui_path pPath, float fCX, float fCY, float fX, float fY)
{
	return __xuiPathAddCommand(pPath, XUI_PATH_CMD_QUAD, (xui_vec2_t){fCX, fCY}, (xui_vec2_t){fX, fY}, (xui_vec2_t){0.0f, 0.0f});
}

XUI_API int xuiPathCubicTo(xui_path pPath, float fC1X, float fC1Y, float fC2X, float fC2Y, float fX, float fY)
{
	return __xuiPathAddCommand(pPath, XUI_PATH_CMD_CUBIC, (xui_vec2_t){fC1X, fC1Y}, (xui_vec2_t){fC2X, fC2Y}, (xui_vec2_t){fX, fY});
}

XUI_API int xuiPathClose(xui_path pPath)
{
	return __xuiPathAddCommand(pPath, XUI_PATH_CMD_CLOSE, (xui_vec2_t){0.0f, 0.0f}, (xui_vec2_t){0.0f, 0.0f}, (xui_vec2_t){0.0f, 0.0f});
}

XUI_API int xuiPathGetCommandCount(xui_path pPath)
{
	return __xuiPathValid(pPath) ? pPath->iCommandCount : 0;
}

XUI_API int xuiPathGetCommand(xui_path pPath, int iIndex, xui_path_command_t* pCommand)
{
	if ( !__xuiPathValid(pPath) || (pCommand == NULL) || (iIndex < 0) || (iIndex >= pPath->iCommandCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pCommand = pPath->pCommands[iIndex];
	return XUI_OK;
}

static int __xuiCoreEffectiveCachePolicy(xui_widget pWidget)
{
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_AUTO ) {
		return (pWidget->iChildCount > 0) ? XUI_CACHE_POLICY_SUBTREE : XUI_CACHE_POLICY_SELF;
	}
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_SUBTREE_TILED ) {
		return XUI_CACHE_POLICY_SUBTREE;
	}
	if ( pWidget->tCachePolicy.iPolicy == XUI_CACHE_POLICY_DISPLAY_LIST ) {
		return XUI_CACHE_POLICY_SELF;
	}
	return pWidget->tCachePolicy.iPolicy;
}

static xui_widget_cache_slot_t* __xuiCoreFindCacheSlot(xui_widget pWidget, uint32_t iStateId)
{
	int i;

	if ( pWidget == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pWidget->iCacheCount; i++ ) {
		if ( pWidget->pCacheSlots[i].iStateId == iStateId ) {
			return &pWidget->pCacheSlots[i];
		}
	}
	return NULL;
}

static int __xuiCoreFloatRectValid(xui_rect_t tRect)
{
	return (tRect.fW > 0.0f) && (tRect.fH > 0.0f);
}

static xui_rect_t __xuiCoreRectFromInt(xui_rect_i_t tRect)
{
	return xuiInternalRectFromInt(tRect);
}

static xui_rect_t __xuiCoreIntersectRect(xui_rect_t tA, xui_rect_t tB)
{
	xui_rect_t tRet;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	fLeft = (tA.fX > tB.fX) ? tA.fX : tB.fX;
	fTop = (tA.fY > tB.fY) ? tA.fY : tB.fY;
	fRight = ((tA.fX + tA.fW) < (tB.fX + tB.fW)) ? (tA.fX + tA.fW) : (tB.fX + tB.fW);
	fBottom = ((tA.fY + tA.fH) < (tB.fY + tB.fH)) ? (tA.fY + tA.fH) : (tB.fY + tB.fH);
	tRet.fX = fLeft;
	tRet.fY = fTop;
	tRet.fW = fRight - fLeft;
	tRet.fH = fBottom - fTop;
	if ( tRet.fW < 0.0f ) {
		tRet.fW = 0.0f;
	}
	if ( tRet.fH < 0.0f ) {
		tRet.fH = 0.0f;
	}
	return tRet;
}

static int __xuiCoreClipDamageToTarget(const xui_surface_desc_t* pDesc, xui_rect_i_t* pRect)
{
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;

	iLeft = pRect->iX;
	iTop = pRect->iY;
	iRight = pRect->iX + pRect->iW;
	iBottom = pRect->iY + pRect->iH;
	if ( iLeft < 0 ) {
		iLeft = 0;
	}
	if ( iTop < 0 ) {
		iTop = 0;
	}
	if ( iRight > pDesc->iWidth ) {
		iRight = pDesc->iWidth;
	}
	if ( iBottom > pDesc->iHeight ) {
		iBottom = pDesc->iHeight;
	}
	pRect->iX = iLeft;
	pRect->iY = iTop;
	pRect->iW = iRight - iLeft;
	pRect->iH = iBottom - iTop;
	return __xuiRectValid(*pRect);
}

static int __xuiCoreRectContains(xui_rect_t tOuter, xui_rect_t tInner)
{
	return (tInner.fX >= tOuter.fX) &&
	       (tInner.fY >= tOuter.fY) &&
	       ((tInner.fX + tInner.fW) <= (tOuter.fX + tOuter.fW)) &&
	       ((tInner.fY + tInner.fH) <= (tOuter.fY + tOuter.fH));
}

static int __xuiCoreNodeCoveredBySubtreeCache(const xui_render_node_t* pNode)
{
	xui_widget pParent;
	xui_widget_cache_slot_t* pSlot;
	xui_rect_t tParentRect;

	for ( pParent = pNode->pWidget->pParent; pParent != NULL; pParent = pParent->pParent ) {
		if ( __xuiCoreEffectiveCachePolicy(pParent) != XUI_CACHE_POLICY_SUBTREE ) {
			continue;
		}
		pSlot = __xuiCoreFindCacheSlot(pParent, pParent->iStateId);
		if ( (pSlot == NULL) || (pSlot->pSurface == NULL) ) {
			continue;
		}
		tParentRect = xuiWidgetGetWorldRect(pParent);
		if ( __xuiCoreRectContains(tParentRect, pNode->tPaintRect) ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiCoreDrawRenderNode(xui_context pContext, xui_surface pTarget, const xui_render_node_t* pNode, xui_rect_i_t tDamage)
{
	xui_widget_cache_slot_t* pSlot;
	xui_proxy pProxy;
	xui_rect_t tDamageRect;
	xui_rect_t tClipRect;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	float fScaleX;
	float fScaleY;
	int iPolicy;
	int iRet;

	if ( (pNode == NULL) || (pNode->pWidget == NULL) || !pNode->pWidget->bVisible ) {
		return XUI_OK;
	}
	iPolicy = pNode->iCachePolicy;
	if ( iPolicy == XUI_CACHE_POLICY_NONE ) {
		return XUI_OK;
	}
	pSlot = __xuiCoreFindCacheSlot(pNode->pWidget, pNode->iStateId);
	if ( (pSlot == NULL) || (pSlot->pSurface == NULL) ||
	     (pSlot->iWidth <= 0) || (pSlot->iHeight <= 0) ||
	     (pNode->tWorldRect.fW <= 0.0f) || (pNode->tWorldRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tDamageRect = __xuiCoreRectFromInt(tDamage);
	tClipRect = __xuiCoreIntersectRect(pNode->tPaintRect, tDamageRect);
	tClipRect = xuiInternalSnapRect(tClipRect);
	if ( !__xuiCoreFloatRectValid(tClipRect) ) {
		return XUI_OK;
	}
	fScaleX = (float)pSlot->iWidth / pNode->tWorldRect.fW;
	fScaleY = (float)pSlot->iHeight / pNode->tWorldRect.fH;
	tSrc.fX = (tClipRect.fX - pNode->tWorldRect.fX) * fScaleX;
	tSrc.fY = (tClipRect.fY - pNode->tWorldRect.fY) * fScaleY;
	tSrc.fW = tClipRect.fW * fScaleX;
	tSrc.fH = tClipRect.fH * fScaleY;
	tSrc = xuiInternalSnapRect(tSrc);
	tDst = xuiInternalSnapRect(tClipRect);
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = pProxy->surfaceDrawTo(pProxy, pTarget, pSlot->pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
	if ( iRet == XUI_OK ) {
		pContext->tRenderStats.iDrawnCaches++;
	}
	return iRet;
}

static void __xuiCoreClearRenderDirtyRecursive(xui_widget pWidget)
{
	xui_widget pChild;

	if ( pWidget == NULL ) {
		return;
	}
	pWidget->iDirtyFlags &= ~(XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
	pWidget->iSubtreeDirtyFlags &= ~(XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE);
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiCoreClearRenderDirtyRecursive(pChild);
	}
}

static int __xuiCoreComposeRenderTree(xui_context pContext, xui_surface pTarget, const xui_surface_desc_t* pTargetDesc, const xui_rect_i_t* pRects, int iRectCount)
{
	xui_rect_i_t tDamage;
	const xui_render_node_t* pNode;
	int i;
	int n;
	int iRet;

	for ( i = 0; i < iRectCount; i++ ) {
		tDamage = pRects[i];
		if ( !__xuiCoreClipDamageToTarget(pTargetDesc, &tDamage) ) {
			continue;
		}
		for ( n = 0; n < pContext->iRenderNodeCount; n++ ) {
			pNode = &pContext->pRenderNodes[n];
			if ( __xuiCoreNodeCoveredBySubtreeCache(pNode) ) {
				pContext->tRenderStats.iSkippedWidgets++;
				continue;
			}
			iRet = __xuiCoreDrawRenderNode(pContext, pTarget, pNode, tDamage);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	if ( pContext->pRoot != NULL ) {
		__xuiCoreClearRenderDirtyRecursive(pContext->pRoot);
	}
	if ( pContext->pOverlayRoot != NULL ) {
		__xuiCoreClearRenderDirtyRecursive(pContext->pOverlayRoot);
	}
	return XUI_OK;
}

XUI_API int xuiRender(xui_context pContext, xui_surface pTarget, const xui_rect_i_t* pRects, int iRectCount)
{
	xui_surface_desc_t tDesc;
	xui_rect_i_t tInlineRect;
	const xui_rect_i_t* pRenderRects;
	int iRenderRectCount;
	int i;
	int iRet;

	if ( !__xuiContextValid(pContext) || (pTarget == NULL) || (iRectCount < 0) || ((iRectCount > 0) && (pRects == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pContext->bHasProxy ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	for ( i = 0; i < iRectCount; i++ ) {
		if ( !__xuiRectValid(pRects[i]) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	memset(&tDesc, 0, sizeof(tDesc));
	iRet = pContext->tProxy.surfaceGetDesc(&pContext->tProxy, pTarget, &tDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (tDesc.iKind != XUI_SURFACE_KIND_TEXTURE) ||
	     (tDesc.iFormat != XUI_SURFACE_FORMAT_RGBA8) ||
	     ((tDesc.iFlags & XUI_SURFACE_USAGE_TARGET) == 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->pRoot != NULL ) {
		iRet = xuiRenderPrepare(pContext);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iRet = xuiBuildRenderTree(pContext);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( iRectCount > 0 ) {
			pRenderRects = pRects;
			iRenderRectCount = iRectCount;
		} else if ( pContext->iDamageCount > 0 ) {
			pRenderRects = pContext->pDamage;
			iRenderRectCount = pContext->iDamageCount;
		} else {
			tInlineRect.iX = 0;
			tInlineRect.iY = 0;
			tInlineRect.iW = tDesc.iWidth;
			tInlineRect.iH = tDesc.iHeight;
			pRenderRects = &tInlineRect;
			iRenderRectCount = 1;
		}
		iRet = __xuiCoreComposeRenderTree(pContext, pTarget, &tDesc, pRenderRects, iRenderRectCount);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->iDamageCount = 0;
	return XUI_OK;
}
