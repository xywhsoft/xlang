#include "xui_config.h"
#if XGE_ENABLE_XUI
#include "xui_internal.h"
#include "xui_native_accessibility_internal.h"

#include <limits.h>
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
	uint32_t iCascadeMark;
	int iRefCount;
    uint32_t iOwnershipFlags;
	xui_resource_dependency_t* pDependencies;
};
enum { XUI_RESOURCE_OWNS_SURFACE=1u, XUI_RESOURCE_OWNS_FONT=2u, XUI_RESOURCE_UPDATING=4u };

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
	{XUI_TR_FILE_USER, "User", "\xE7\x94\xA8\xE6\x88\xB7", "\xD0\x9F\xD0\xBE\xD0\xBB\xD1\x8C\xD0\xB7\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x82\xD0\xB5\xD0\xBB\xD1\x8C", "\xE3\x83\xA6\xE3\x83\xBC\xE3\x82\xB6\xE3\x83\xBC", "Utilisateur", "Usuario"},
	{XUI_TR_FILE_DOCUMENTS, "Documents", "\xE6\x96\x87\xE6\xA1\xA3", "\xD0\x94\xD0\xBE\xD0\xBA\xD1\x83\xD0\xBC\xD0\xB5\xD0\xBD\xD1\x82\xD1\x8B", "\xE3\x83\x89\xE3\x82\xAD\xE3\x83\xA5\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88", "Documents", "Documentos"},
	{XUI_TR_FILE_PICTURES, "Pictures", "\xE5\x9B\xBE\xE7\x89\x87", "\xD0\x98\xD0\xB7\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB6\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F", "\xE7\x94\xBB\xE5\x83\x8F", "Images", "Imagenes"},
	{XUI_TR_FILE_MUSIC, "Music", "\xE9\x9F\xB3\xE4\xB9\x90", "\xD0\x9C\xD1\x83\xD0\xB7\xD1\x8B\xD0\xBA\xD0\xB0", "\xE3\x83\x9F\xE3\x83\xA5\xE3\x83\xBC\xE3\x82\xB8\xE3\x83\x83\xE3\x82\xAF", "Musique", "Musica"},
	{XUI_TR_FILE_VIDEOS, "Videos", "\xE8\xA7\x86\xE9\xA2\x91", "\xD0\x92\xD0\xB8\xD0\xB4\xD0\xB5\xD0\xBE", "\xE3\x83\x93\xE3\x83\x87\xE3\x82\xAA", "Videos", "Videos"},
	{XUI_TR_FILE_DOWNLOADS, "Downloads", "\xE4\xB8\x8B\xE8\xBD\xBD", "\xD0\x97\xD0\xB0\xD0\xB3\xD1\x80\xD1\x83\xD0\xB7\xD0\xBA\xD0\xB8", "\xE3\x83\x80\xE3\x82\xA6\xE3\x83\xB3\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x89", "Telechargements", "Descargas"},
	{XUI_TR_FILE_DESKTOP, "Desktop", "\xE6\xA1\x8C\xE9\x9D\xA2", "\xD0\xA0\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x87\xD0\xB8\xD0\xB9 \xD1\x81\xD1\x82\xD0\xBE\xD0\xBB", "\xE3\x83\x87\xE3\x82\xB9\xE3\x82\xAF\xE3\x83\x88\xE3\x83\x83\xE3\x83\x97", "Bureau", "Escritorio"},
	{XUI_TR_FILE_THIS_PC, "This PC", "\xE6\xAD\xA4\xE7\x94\xB5\xE8\x84\x91", "\xD0\xAD\xD1\x82\xD0\xBE\xD1\x82 \xD0\xBA\xD0\xBE\xD0\xBC\xD0\xBF\xD1\x8C\xD1\x8E\xD1\x82\xD0\xB5\xD1\x80", "\xE3\x81\x93\xE3\x81\xAEPC", "Ce PC", "Este equipo"},
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
	{XUI_TR_MESSAGE_THINKING, "Thinking", "\xE6\x80\x9D\xE8\x80\x83", "\xD0\xA0\xD0\xB0\xD0\xB7\xD0\xBC\xD1\x8B\xD1\x88\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB5", "\xE6\x80\x9D\xE8\x80\x83", "R\xC3\xA9" "flexion", "Razonamiento"},
	{XUI_TR_RICH_BOLD, "Bold", "\xE7\xB2\x97\xE4\xBD\x93", "\xD0\x96\xD0\xB8\xD1\x80\xD0\xBD\xD1\x8B\xD0\xB9", "\xE5\xA4\xAA\xE5\xAD\x97", "Gras", "Negrita"},
	{XUI_TR_RICH_ITALIC, "Italic", "\xE6\x96\x9C\xE4\xBD\x93", "\xD0\x9A\xD1\x83\xD1\x80\xD1\x81\xD0\xB8\xD0\xB2", "\xE6\x96\x9C\xE4\xBD\x93", "Italique", "Cursiva"},
	{XUI_TR_RICH_UNDERLINE, "Underline", "\xE4\xB8\x8B\xE5\x88\x92\xE7\xBA\xBF", "\xD0\x9F\xD0\xBE\xD0\xB4\xD1\x87\xD1\x91\xD1\x80\xD0\xBA\xD0\xB8\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5", "\xE4\xB8\x8B\xE7\xB7\x9A", "Souligner", "Subrayado"},
	{XUI_TR_RICH_STRIKEOUT, "Strikeout", "\xE5\x88\xA0\xE9\x99\xA4\xE7\xBA\xBF", "\xD0\x97\xD0\xB0\xD1\x87\xD1\x91\xD1\x80\xD0\xBA\xD0\xB8\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5", "\xE5\x8F\x96\xE3\x82\x8A\xE6\xB6\x88\xE3\x81\x97\xE7\xB7\x9A", "Barrer", "Tachado"},
	{XUI_TR_RICH_ALIGN_LEFT, "Align left", "\xE5\xB7\xA6\xE5\xAF\xB9\xE9\xBD\x90", "\xD0\x9F\xD0\xBE \xD0\xBB\xD0\xB5\xD0\xB2\xD0\xBE\xD0\xBC\xD1\x83 \xD0\xBA\xD1\x80\xD0\xB0\xD1\x8E", "\xE5\xB7\xA6\xE6\x8F\x83\xE3\x81\x88", "Aligner a gauche", "Alinear a la izquierda"},
	{XUI_TR_RICH_ALIGN_CENTER, "Align center", "\xE5\xB1\x85\xE4\xB8\xAD", "\xD0\x9F\xD0\xBE \xD1\x86\xD0\xB5\xD0\xBD\xD1\x82\xD1\x80\xD1\x83", "\xE4\xB8\xAD\xE5\xA4\xAE\xE6\x8F\x83\xE3\x81\x88", "Centrer", "Centrar"},
	{XUI_TR_RICH_ALIGN_RIGHT, "Align right", "\xE5\x8F\xB3\xE5\xAF\xB9\xE9\xBD\x90", "\xD0\x9F\xD0\xBE \xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBE\xD0\xBC\xD1\x83 \xD0\xBA\xD1\x80\xD0\xB0\xD1\x8E", "\xE5\x8F\xB3\xE6\x8F\x83\xE3\x81\x88", "Aligner a droite", "Alinear a la derecha"},
	{XUI_TR_RICH_ALIGN_JUSTIFY, "Justify", "\xE4\xB8\xA4\xE7\xAB\xAF\xE5\xAF\xB9\xE9\xBD\x90", "\xD0\x9F\xD0\xBE \xD1\x88\xD0\xB8\xD1\x80\xD0\xB8\xD0\xBD\xD0\xB5", "\xE4\xB8\xA1\xE7\xAB\xAF\xE6\x8F\x83\xE3\x81\x88", "Justifier", "Justificar"},
	{XUI_TR_RICH_PARAGRAPH, "Paragraph", "\xE6\xAE\xB5\xE8\x90\xBD", "\xD0\x90\xD0\xB1\xD0\xB7\xD0\xB0\xD1\x86", "\xE6\xAE\xB5\xE8\x90\xBD", "Paragraphe", "Parrafo"},
	{XUI_TR_RICH_HEADING_1, "Heading 1", "\xE4\xB8\x80\xE7\xBA\xA7\xE6\xA0\x87\xE9\xA2\x98", "\xD0\x97\xD0\xB0\xD0\xB3\xD0\xBE\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE\xD0\xBA 1", "\xE8\xA6\x8B\xE5\x87\xBA\xE3\x81\x97 1", "Titre 1", "Encabezado 1"},
	{XUI_TR_RICH_HEADING_2, "Heading 2", "\xE4\xBA\x8C\xE7\xBA\xA7\xE6\xA0\x87\xE9\xA2\x98", "\xD0\x97\xD0\xB0\xD0\xB3\xD0\xBE\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE\xD0\xBA 2", "\xE8\xA6\x8B\xE5\x87\xBA\xE3\x81\x97 2", "Titre 2", "Encabezado 2"},
	{XUI_TR_RICH_HEADING_3, "Heading 3", "\xE4\xB8\x89\xE7\xBA\xA7\xE6\xA0\x87\xE9\xA2\x98", "\xD0\x97\xD0\xB0\xD0\xB3\xD0\xBE\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE\xD0\xBA 3", "\xE8\xA6\x8B\xE5\x87\xBA\xE3\x81\x97 3", "Titre 3", "Encabezado 3"},
	{XUI_TR_RICH_BLOCK_QUOTE, "Block quote", "\xE5\xBC\x95\xE7\x94\xA8", "\xD0\xA6\xD0\xB8\xD1\x82\xD0\xB0\xD1\x82\xD0\xB0", "\xE5\xBC\x95\xE7\x94\xA8", "Citation", "Cita"},
	{XUI_TR_RICH_BULLET_LIST, "Bullet list", "\xE9\xA1\xB9\xE7\x9B\xAE\xE7\xAC\xA6\xE5\x8F\xB7\xE5\x88\x97\xE8\xA1\xA8", "\xD0\x9C\xD0\xB0\xD1\x80\xD0\xBA\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xBD\xD1\x8B\xD0\xB9 \xD1\x81\xD0\xBF\xD0\xB8\xD1\x81\xD0\xBE\xD0\xBA", "\xE7\xAE\x87\xE6\x9D\xA1\xE6\x9B\xB8\xE3\x81\x8D", "Liste a puces", "Lista con vinetas"},
	{XUI_TR_RICH_NUMBER_LIST, "Numbered list", "\xE7\xBC\x96\xE5\x8F\xB7\xE5\x88\x97\xE8\xA1\xA8", "\xD0\x9D\xD1\x83\xD0\xBC\xD0\xB5\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xBD\xD1\x8B\xD0\xB9 \xD1\x81\xD0\xBF\xD0\xB8\xD1\x81\xD0\xBE\xD0\xBA", "\xE7\x95\xAA\xE5\x8F\xB7\xE4\xBB\x98\xE3\x81\x8D\xE3\x83\xAA\xE3\x82\xB9\xE3\x83\x88", "Liste numerotee", "Lista numerada"},
	{XUI_TR_RICH_CHECK_LIST, "Check list", "\xE4\xBB\xBB\xE5\x8A\xA1\xE5\x88\x97\xE8\xA1\xA8", "\xD0\xA1\xD0\xBF\xD0\xB8\xD1\x81\xD0\xBE\xD0\xBA \xD0\xB7\xD0\xB0\xD0\xB4\xD0\xB0\xD1\x87", "\xE3\x82\xBF\xE3\x82\xB9\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xB9\xE3\x83\x88", "Liste de taches", "Lista de tareas"},
	{XUI_TR_RICH_INDENT, "Increase indent", "\xE5\xA2\x9E\xE5\x8A\xA0\xE7\xBC\xA9\xE8\xBF\x9B", "\xD0\xA3\xD0\xB2\xD0\xB5\xD0\xBB\xD0\xB8\xD1\x87\xD0\xB8\xD1\x82\xD1\x8C \xD0\xBE\xD1\x82\xD1\x81\xD1\x82\xD1\x83\xD0\xBF", "\xE3\x82\xA4\xE3\x83\xB3\xE3\x83\x87\xE3\x83\xB3\xE3\x83\x88\xE3\x82\x92\xE5\xA2\x97\xE3\x82\x84\xE3\x81\x99", "Augmenter le retrait", "Aumentar sangria"},
	{XUI_TR_RICH_OUTDENT, "Decrease indent", "\xE5\x87\x8F\xE5\xB0\x91\xE7\xBC\xA9\xE8\xBF\x9B", "\xD0\xA3\xD0\xBC\xD0\xB5\xD0\xBD\xD1\x8C\xD1\x88\xD0\xB8\xD1\x82\xD1\x8C \xD0\xBE\xD1\x82\xD1\x81\xD1\x82\xD1\x83\xD0\xBF", "\xE3\x82\xA4\xE3\x83\xB3\xE3\x83\x87\xE3\x83\xB3\xE3\x83\x88\xE3\x82\x92\xE6\xB8\x9B\xE3\x82\x89\xE3\x81\x99", "Reduire le retrait", "Reducir sangria"},
	{XUI_TR_RICH_HORIZONTAL_RULE, "Horizontal rule", "\xE5\x88\x86\xE9\x9A\x94\xE7\xBA\xBF", "\xD0\x93\xD0\xBE\xD1\x80\xD0\xB8\xD0\xB7\xD0\xBE\xD0\xBD\xD1\x82\xD0\xB0\xD0\xBB\xD1\x8C\xD0\xBD\xD0\xB0\xD1\x8F \xD0\xBB\xD0\xB8\xD0\xBD\xD0\xB8\xD1\x8F", "\xE6\xB0\xB4\xE5\xB9\xB3\xE7\xB7\x9A", "Ligne horizontale", "L\xC3\xADnea horizontal"},
	{XUI_TR_RICH_CODE_BLOCK, "Code block", "\xE4\xBB\xA3\xE7\xA0\x81\xE5\x9D\x97", "\xD0\x91\xD0\xBB\xD0\xBE\xD0\xBA \xD0\xBA\xD0\xBE\xD0\xB4\xD0\xB0", "\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89\xE3\x83\x96\xE3\x83\xAD\xE3\x83\x83\xE3\x82\xAF", "Bloc de code", "Bloque de c\xC3\xB3" "digo"},
	{XUI_TR_RICH_INSERT_ROW_BEFORE, "Insert row above", "\xE5\x9C\xA8\xE4\xB8\x8A\xE6\x96\xB9\xE6\x8F\x92\xE5\x85\xA5\xE8\xA1\x8C", "\xD0\x92\xD1\x81\xD1\x82\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD1\x83\x20\xD0\xB2\xD1\x8B\xD1\x88\xD0\xB5", "\xE4\xB8\x8A\xE3\x81\xAB\xE8\xA1\x8C\xE3\x82\x92\xE6\x8C\xBF\xE5\x85\xA5", "\x49\x6E\x73\xC3\xA9\x72\x65\x72\x20\x75\x6E\x65\x20\x6C\x69\x67\x6E\x65\x20\x61\x75\x2D\x64\x65\x73\x73\x75\x73", "Insertar fila encima"},
	{XUI_TR_RICH_INSERT_ROW_AFTER, "Insert row below", "\xE5\x9C\xA8\xE4\xB8\x8B\xE6\x96\xB9\xE6\x8F\x92\xE5\x85\xA5\xE8\xA1\x8C", "\xD0\x92\xD1\x81\xD1\x82\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD1\x83\x20\xD0\xBD\xD0\xB8\xD0\xB6\xD0\xB5", "\xE4\xB8\x8B\xE3\x81\xAB\xE8\xA1\x8C\xE3\x82\x92\xE6\x8C\xBF\xE5\x85\xA5", "\x49\x6E\x73\xC3\xA9\x72\x65\x72\x20\x75\x6E\x65\x20\x6C\x69\x67\x6E\x65\x20\x61\x75\x2D\x64\x65\x73\x73\x6F\x75\x73", "Insertar fila debajo"},
	{XUI_TR_RICH_DELETE_ROW, "Delete row", "\xE5\x88\xA0\xE9\x99\xA4\xE8\xA1\x8C", "\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD1\x83", "\xE8\xA1\x8C\xE3\x82\x92\xE5\x89\x8A\xE9\x99\xA4", "Supprimer la ligne", "Eliminar fila"},
	{XUI_TR_RICH_INSERT_COLUMN_BEFORE, "Insert column left", "\xE5\x9C\xA8\xE5\xB7\xA6\xE4\xBE\xA7\xE6\x8F\x92\xE5\x85\xA5\xE5\x88\x97", "\xD0\x92\xD1\x81\xD1\x82\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD0\xBE\xD0\xBB\xD0\xB1\xD0\xB5\xD1\x86\x20\xD1\x81\xD0\xBB\xD0\xB5\xD0\xB2\xD0\xB0", "\xE5\xB7\xA6\xE3\x81\xAB\xE5\x88\x97\xE3\x82\x92\xE6\x8C\xBF\xE5\x85\xA5", "\x49\x6E\x73\xC3\xA9\x72\x65\x72\x20\x75\x6E\x65\x20\x63\x6F\x6C\x6F\x6E\x6E\x65\x20\xC3\xA0\x20\x67\x61\x75\x63\x68\x65", "Insertar columna a la izquierda"},
	{XUI_TR_RICH_INSERT_COLUMN_AFTER, "Insert column right", "\xE5\x9C\xA8\xE5\x8F\xB3\xE4\xBE\xA7\xE6\x8F\x92\xE5\x85\xA5\xE5\x88\x97", "\xD0\x92\xD1\x81\xD1\x82\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD0\xBE\xD0\xBB\xD0\xB1\xD0\xB5\xD1\x86\x20\xD1\x81\xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB0", "\xE5\x8F\xB3\xE3\x81\xAB\xE5\x88\x97\xE3\x82\x92\xE6\x8C\xBF\xE5\x85\xA5", "\x49\x6E\x73\xC3\xA9\x72\x65\x72\x20\x75\x6E\x65\x20\x63\x6F\x6C\x6F\x6E\x6E\x65\x20\xC3\xA0\x20\x64\x72\x6F\x69\x74\x65", "Insertar columna a la derecha"},
	{XUI_TR_RICH_DELETE_COLUMN, "Delete column", "\xE5\x88\xA0\xE9\x99\xA4\xE5\x88\x97", "\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x81\xD1\x82\xD0\xBE\xD0\xBB\xD0\xB1\xD0\xB5\xD1\x86", "\xE5\x88\x97\xE3\x82\x92\xE5\x89\x8A\xE9\x99\xA4", "Supprimer la colonne", "Eliminar columna"},
	{XUI_TR_RICH_MERGE_CELLS, "Merge cells", "\xE5\x90\x88\xE5\xB9\xB6\xE5\x8D\x95\xE5\x85\x83\xE6\xA0\xBC", "\xD0\x9E\xD0\xB1\xD1\x8A\xD0\xB5\xD0\xB4\xD0\xB8\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x8F\xD1\x87\xD0\xB5\xD0\xB9\xD0\xBA\xD0\xB8", "\xE3\x82\xBB\xE3\x83\xAB\xE3\x82\x92\xE7\xB5\x90\xE5\x90\x88", "Fusionner les cellules", "Combinar celdas"},
	{XUI_TR_RICH_SPLIT_CELL, "Split cell", "\xE6\x8B\x86\xE5\x88\x86\xE5\x8D\x95\xE5\x85\x83\xE6\xA0\xBC", "\xD0\xA0\xD0\xB0\xD0\xB7\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C\x20\xD1\x8F\xD1\x87\xD0\xB5\xD0\xB9\xD0\xBA\xD1\x83", "\xE3\x82\xBB\xE3\x83\xAB\xE3\x82\x92\xE5\x88\x86\xE5\x89\xB2", "Fractionner la cellule", "Dividir celda"},
};

static int g_iXuiDefaultLanguage = XUI_LANGUAGE_EN;

static void __xuiContextDestroyLanguages(xui_context pContext);

static void __xuiContextInitIcons(xui_context pContext)
{
	xuiXrtMapInit(&pContext->mapIconCategories, sizeof(void*));
	xuiXrtArrayInit(&pContext->arrIconCategories, sizeof(xui_icon_category));
	pContext->iIconGeneration = 1;
}

static void __xuiContextDestroyIcons(xui_context pContext)
{
	if ( pContext->onDestroyIcons != NULL ) {
		pContext->onDestroyIcons(pContext);
	}
	xrtMapUnit(&pContext->mapIconCategories);
	xrtArrayUnit(&pContext->arrIconCategories);
	pContext->onDestroyIcons = NULL;
}

static int __xuiContextValid(xui_context pContext)
{
	return (pContext != NULL) && (pContext->iMagic == XUI_CONTEXT_MAGIC);
}

static void __xuiContextDestroyNow(xui_context pContext);
static int __xuiContextDestroyOwnerInMode(xui_context,bool*,xui_internal_destroy_mode_t);

static int __xuiResourceValid(xui_resource pResource)
{
	return (pResource != NULL) && (pResource->iMagic == XUI_RESOURCE_MAGIC);
}

static char* __xuiCoreStringDuplicate(const char* sText)
{
	return xuiInternalStringDuplicateNullable(sText);
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
	return (xui_language_text_t*)xuiXrtArrayGet(&pLanguage->arrTexts, (uint32_t)iTextId);
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
		pText = (xui_language_text_t*)xuiXrtArrayGet(&pLanguage->arrTexts, i);
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
		ppLanguage = (xui_language*)xuiXrtArrayGet(&pContext->arrLanguages, i);
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
		iPos = xuiXrtArrayAppendSpace(&pLanguage->arrTexts, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pText = (xui_language_text_t*)xuiXrtArrayGet(&pLanguage->arrTexts, iPos);
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
	xuiXrtArrayInit(&pLanguage->arrTexts, sizeof(xui_language_text_t));
	pLanguage->sCode = __xuiCoreStringDuplicate(sCode);
	pLanguage->sName = __xuiCoreStringDuplicate(sName);
	if ( pLanguage->sCode == NULL || pLanguage->sName == NULL || __xuiLanguageEnsureTextSlots(pLanguage) != XUI_OK ) {
		__xuiLanguageDestroyOne(pLanguage);
		return NULL;
	}
	iPos = xuiXrtArrayAppendSpace(&pContext->arrLanguages, 1u);
	if ( iPos == 0u ) {
		__xuiLanguageDestroyOne(pLanguage);
		return NULL;
	}
	ppSlot = (xui_language*)xuiXrtArrayGet(&pContext->arrLanguages, iPos);
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
	xuiXrtArrayInit(&pContext->arrLanguages, sizeof(xui_language));
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
		ppLanguage = (xui_language*)xuiXrtArrayGet(&pContext->arrLanguages, i);
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

XUI_API xarray* xuiGetLanguageTextArray(xui_language pLanguage)
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

static void __xuiResourceRegistryChanged(xui_context pContext)
{
	pContext->iResourceRegistryGeneration++;
	if ( pContext->iResourceRegistryGeneration == 0 ) {
		pContext->iResourceRegistryGeneration = 1;
	}
}

static uint32_t __xuiResourceNextGeneration(xui_context pContext)
{
	uint32_t iGeneration;

	iGeneration = pContext->iNextResourceGeneration++;
	if ( pContext->iNextResourceGeneration == 0 ) {
		pContext->iNextResourceGeneration = 1;
	}
	return iGeneration;
}

static uint32_t __xuiResourceNextCascadeMark(xui_context pContext)
{
	xui_resource pResource;

	pContext->iResourceCascadeMark++;
	if ( pContext->iResourceCascadeMark != 0 ) {
		return pContext->iResourceCascadeMark;
	}
	for ( pResource = pContext->pResources; pResource != NULL;
	      pResource = pResource->pNext ) {
		pResource->iCascadeMark = 0;
	}
	pContext->iResourceCascadeMark = 1;
	return 1;
}

static void __xuiResourceCascadeChanged(xui_context pContext,
	xui_resource pResource)
{
	xui_resource pScan;
	xui_resource_dependency_t* pDependency;
	uint32_t iMark;
	int bChanged;

	iMark = __xuiResourceNextCascadeMark(pContext);
	pResource->iCascadeMark = iMark;
	pResource->iGeneration = __xuiResourceNextGeneration(pContext);
	do {
		bChanged = 0;
		for ( pScan = pContext->pResources; pScan != NULL;
		      pScan = pScan->pNext ) {
			if ( pScan->iCascadeMark == iMark ) continue;
			for ( pDependency = pScan->pDependencies; pDependency != NULL;
			      pDependency = pDependency->pNext ) {
				if ( pDependency->pResource != NULL &&
				     pDependency->pResource->iCascadeMark == iMark ) {
					pScan->iCascadeMark = iMark;
					pScan->iGeneration =
						__xuiResourceNextGeneration(pContext);
					bChanged = 1;
					break;
				}
			}
		}
	} while ( bChanged );
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

void xuiInternalOperationEnter(xui_context pContext)
{
	if ( __xuiContextValid(pContext) ) {
		pContext->iOperationDepth++;
	}
}

/* A diagnostic restored by an independently ended outer receipt is still an
 * inherited ambient value in the remaining receipts, not a new failure. */
static bool __xuiContextHasAmbient(xui_context context,const xerror* error)
{
    if(!error)return false;
    for(xui_call_scope_t* scope=context->pCallScopes;scope;scope=scope->pNext)
        if(scope->iResult==XUI_OK && scope->_pFirstError==error)return true;
    return false;
}
static int __xuiCallScopeBegin(xui_context pContext,xui_call_scope_t* pScope,bool closing)
{
	if ( pScope == NULL || !__xuiContextValid(pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
    if(pContext->pOwnedProxy && !xuiInternalProxyCreator(pContext->pOwnedProxy))return XUI_ERROR_BUSY;
	if ( pScope->pContext != NULL || pScope->pSelf != NULL || pScope->pPrevious != NULL ||
		pScope->pNext != NULL || pScope->iResult != XUI_OK || pScope->bEnding || pScope->_pFirstError != NULL ||
        (!closing && (pContext->bDestroying || pContext->bDestroyPending || pContext->bCloseRequested)) ||
        pContext->iOperationDepth == INT_MAX ) return XUI_ERROR_INVALID_STATE;
    /* Capture a new diagnostic for its actual enclosing operations before a
     * child hides it. This also permits balanced receipts to end out of order. */
    if(!__xuiContextHasAmbient(pContext,xrtGetError())){
        xuiInternalContextRecordOutcome(pContext,XUI_OK,xrtGetError());
    }
	pScope->pContext = pContext;
	pScope->pSelf = pScope;
	pScope->pNext = pContext->pCallScopes;
	if ( pScope->pNext != NULL ) pScope->pNext->pPrevious = pScope;
	pContext->pCallScopes = pScope;
    pScope->_pFirstError=xrtTakeError();
	xuiInternalOperationEnter(pContext);
	return XUI_OK;
}

XUI_API int xuiCallScopeBegin(xui_context pContext,xui_call_scope_t* pScope)
{return __xuiCallScopeBegin(pContext,pScope,false);}
int xuiInternalCleanupCallScopeBegin(xui_context context,xui_call_scope_t* scope)
{return __xuiCallScopeBegin(context,scope,true);}

XUI_API int xuiCallScopeEnd(xui_call_scope_t* pScope)
{
	xui_context pContext;
	if ( pScope == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pScope->pContext == NULL && pScope->pSelf == NULL && pScope->pPrevious == NULL &&
		pScope->pNext == NULL && pScope->iResult == XUI_OK && !pScope->bEnding && pScope->_pFirstError == NULL ) return XUI_OK;
	if ( pScope->pSelf != pScope || pScope->pContext == NULL || (pScope->bEnding&XUI_CALL_SCOPE_ENDING) ) return XUI_ERROR_INVALID_STATE;
	pContext = pScope->pContext;
    if(pContext->pOwnedProxy && !xuiInternalProxyCreator(pContext->pOwnedProxy))return XUI_ERROR_BUSY;
	pScope->bEnding |= XUI_CALL_SCOPE_ENDING;
    xerror* inheritedAmbient=__xuiContextHasAmbient(pContext,xrtGetError()) ? xrtErrorRef(xrtGetError()):NULL;
    if(!inheritedAmbient){
        xuiInternalContextRecordOutcome(pContext,XUI_OK,xrtGetError());
    }
	xuiInternalOperationLeave(pContext);
	/* Destruction clears pContext in every still-linked receipt before free.
	 * Keep this receipt linked during Leave, including final native cleanup. */
	if ( pScope->pContext != NULL ) {
        if(pScope->pPrevious && pScope->pPrevious->iResult==XUI_OK){
            /* Its entry ambient belonged to this operation. Move the ending
             * receipt's outcome into that inherited slot, retaining exactly
             * one owner until the remaining nested operation ends. */
            xerror* inherited=xrtErrorRef(pScope->_pFirstError);
            xrtErrorFree(pScope->pPrevious->_pFirstError);
            pScope->pPrevious->_pFirstError=inherited;
        }
		if ( pScope->pPrevious != NULL ) pScope->pPrevious->pNext = pScope->pNext;
		else pContext->pCallScopes = pScope->pNext;
		if ( pScope->pNext != NULL ) pScope->pNext->pPrevious = pScope->pPrevious;
	}
    /* A legacy void destructor may leave only TLS. Capture that new tail
     * diagnostic before restoring the entry ambient value, even after the
     * Context invalidated all linked receipts during physical destruction. */
    if(xrtGetError()!=inheritedAmbient && !(pScope->iResult==XUI_OK && pScope->_pFirstError==xrtGetError())){
        xuiInternalCallScopeRecordOutcome(pScope,XUI_OK,xrtGetError());
    }
	int iResult = pScope->iResult;
    xerror* first=pScope->_pFirstError;
	xrtErrorFree(inheritedAmbient);
	memset(pScope, 0, sizeof(*pScope));
    xrtSetErrorTake(first);
	return iResult;
}

void xuiInternalOperationLeave(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) || (pContext->iOperationDepth <= 0) ) {
		return;
	}
	pContext->iOperationDepth--;
	if ( pContext->bDestroying ) return;
	if ( (pContext->iOperationDepth > 0) || (pContext->iWidgetCallbackDepth > 0) ||
	     (pContext->iDestroyFlushDepth > 0) ) {
		return;
	}
	while ( __xuiContextValid(pContext) ) {
		if ( pContext->bDestroyPending ) {
			__xuiContextDestroyNow(pContext);
			return;
		}
        if(pContext->iDestroyOwnership==1){
            /* Void native close relinquished its unique owner. The Context
             * itself keeps that exact responsibility until true preparation
             * finishes; no raw handle or code observation is discarded. */
            bool consumed=false;(void)__xuiContextDestroyOwnerInMode(pContext,&consumed,
                xuiInternalDestroyModeInherited(pContext));
            return; /* It may have freed the Context; never inspect it again. */
        }
		if ( pContext->pStateChangeHead != NULL && !pContext->bCloseRequested ) {
			/* State callbacks are part of the operation that produced the change. */
			pContext->iOperationDepth = 1;
			xuiInternalStateChangeFlush(pContext);
			pContext->iOperationDepth = 0;
			continue;
		}
        if(pContext->pDeferredDestroyHead && xuiInternalWidgetDestroyFlush(pContext))continue;
        if(pContext->pPendingManagers) {
            pContext->iOperationDepth=1;
            bool progressed=xuiInternalContextDrainManagers(pContext);
            pContext->iOperationDepth=0;
            if(!progressed)break;
            continue;
        }
		break;
	}
}

xui_internal_destroy_mode_t xuiInternalDestroyModeInherited(xui_context context)
{
    if(!__xuiContextValid(context) ||
       (context->pOwnedProxy && !xuiInternalProxyCreator(context->pOwnedProxy)))return XUI_DESTROY_NONBLOCKING;
    return context->pDestroyScopes ? context->pDestroyScopes->iMode:context->iPendingDestroyMode;
}
void xuiInternalDestroyScopeBegin(xui_context context,xui_internal_destroy_mode_t mode,
    xui_internal_destroy_scope_t* scope)
{
    memset(scope,0,sizeof(*scope));
    if(!__xuiContextValid(context) ||
       (context->pOwnedProxy && !xuiInternalProxyCreator(context->pOwnedProxy)))return;
    scope->pContext=context;scope->pPrevious=context->pDestroyScopes;scope->iMode=mode;
    context->pDestroyScopes=scope;
}
void xuiInternalDestroyScopeEnd(xui_internal_destroy_scope_t* scope)
{
    xui_context context=scope->pContext;
    if(context){
        if(context->pDestroyScopes!=scope)abort();
        context->pDestroyScopes=scope->pPrevious;
    }
    memset(scope,0,sizeof(*scope));
}
bool xuiInternalDestroyWaitAllowed(xui_context context)
{
    /* A pending void owner alone must not relax an unrelated pure Busy query.
     * Only a real destroy-call frame can authorize a synchronous worker wait. */
    return __xuiContextValid(context) &&
        (!context->pOwnedProxy || xuiInternalProxyCreator(context->pOwnedProxy)) && context->pDestroyScopes &&
        context->pDestroyScopes->iMode==XUI_DESTROY_SYNC_WORKER;
}

int xuiInternalContextDestroyPending(xui_context pContext)
{
	/* Business transactions stop at logical close. Physical consumption may
     * wait for checked preparation, but must never reopen adapter publication,
     * event dispatch or rendering in that interval. */
	return __xuiContextValid(pContext) &&
        (pContext->bCloseRequested || pContext->bDestroyPending || pContext->bDestroying);
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
	/* Owned resource factories key their capability by Proxy identity. The
	 * legacy snapshot is suitable only for a borrowed C backend. */
	return pContext->pOwnedProxy ? pContext->pOwnedProxy : &pContext->tProxy;
}

int xuiInternalSurfaceDependencySync(xui_proxy pProxy, xui_surface pSurface,
	xui_surface_dependency_t* pDependencies, int iIndex)
{
	xui_surface_dependency_t* pDependency;
	uint32_t iGeneration;
	int bGenerationValid;
	int bChanged;
	int i;

	if ( pDependencies == NULL || iIndex < 0 ) return 0;
	pDependency = &pDependencies[iIndex];
	iGeneration = 0;
	bGenerationValid = 0;
	if ( pSurface != NULL && pProxy != NULL && pProxy->surfaceGetGeneration != NULL ) {
		for ( i = 0; i < iIndex; i++ ) {
			if ( pDependencies[i].pSurface == pSurface &&
			     pDependencies[i].bGenerationValid ) {
				iGeneration = pDependencies[i].iGeneration;
				bGenerationValid = 1;
				break;
			}
		}
		if ( !bGenerationValid &&
		     pProxy->surfaceGetGeneration(pProxy, pSurface, &iGeneration) == XUI_OK ) {
			bGenerationValid = 1;
		}
	}
	bChanged = pDependency->pSurface == pSurface && bGenerationValid &&
		(!pDependency->bGenerationValid || pDependency->iGeneration != iGeneration);
	pDependency->pSurface = pSurface;
	pDependency->iGeneration = iGeneration;
	pDependency->bGenerationValid = bGenerationValid;
	return bChanged;
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

XUI_API int xuiSetErrorCallback(xui_context pContext, xui_error_proc onError, void* pUser)
{
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->onError = onError;
	pContext->pErrorUser = (onError != NULL) ? pUser : NULL;
	return XUI_OK;
}

XUI_API int xuiSetAccessibilityEventCallback(xui_context pContext,
	xui_accessibility_event_proc onEvent, void* pUser)
{
	if ( !__xuiContextValid(pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext->onAccessibilityEvent = onEvent;
	pContext->pAccessibilityEventUser = onEvent != NULL ? pUser : NULL;
	return XUI_OK;
}

XUI_API int xuiReportError(xui_context pContext, const xui_error_info_t* pError)
{
	xui_error_info_t tError;

	if ( !__xuiContextValid(pContext) || (pError == NULL) ||
	     (pError->iSize < sizeof(*pError)) || (pError->iCode == XUI_OK) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->onError == NULL) || pContext->bReportingError ) {
		return XUI_OK;
	}
	tError = *pError;
	tError.iSize = sizeof(tError);
	xuiInternalOperationEnter(pContext);
	pContext->bReportingError = 1;
	pContext->onError(pContext, &tError, pContext->pErrorUser);
	pContext->bReportingError = 0;
	xuiInternalOperationLeave(pContext);
	return XUI_OK;
}

void xuiInternalReportError(xui_context pContext, xui_widget pWidget, int iCode, int iStage,
	int bRecoverable, const char* sOperation, const char* sMessage)
{
	xui_error_info_t tError;

	if ( !__xuiContextValid(pContext) || (iCode == XUI_OK) ) {
		return;
	}
	memset(&tError, 0, sizeof(tError));
	tError.iSize = sizeof(tError);
	tError.iCode = iCode;
	tError.iStage = iStage;
	tError.bRecoverable = bRecoverable != 0;
	tError.pWidget = pWidget;
	if ( (pWidget != NULL) && xuiInternalWidgetIsValid(pWidget) ) {
		tError.tRect = xuiWidgetGetWorldRect(pWidget);
	}
	tError.sOperation = sOperation;
	tError.sMessage = sMessage;
	if ( tError.bRecoverable &&
	     ((iStage == XUI_ERROR_STAGE_CACHE) || (iStage == XUI_ERROR_STAGE_RENDER)) ) {
		pContext->tRenderStats.iRecoveredErrors++;
	}
	(void)xuiReportError(pContext, &tError);
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
	pContext->pRenderHead = NULL;
	pContext->pRenderTail = NULL;
	pContext->iRenderLinkedNodeCount = 0;
	pContext->bRenderIndexDirty = 0;
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
    pResource->iMagic=0;
	pDependency = pResource->pDependencies;
	while ( pDependency != NULL ) {
		pNextDependency = pDependency->pNext;
		xrtFree(pDependency);
		pDependency = pNextDependency;
	}
	pResource->pDependencies = NULL;
    if(pResource->iOwnershipFlags&XUI_RESOURCE_OWNS_SURFACE){
        xui_surface surface=pResource->pHandle;pResource->pHandle=NULL;pResource->iOwnershipFlags=0;
        if(surface)(void)xuiInternalSurfaceRelease(pResource->pContext,surface);
    }else if(pResource->iOwnershipFlags&XUI_RESOURCE_OWNS_FONT){
        xui_font font=pResource->pHandle;pResource->pHandle=NULL;pResource->iOwnershipFlags=0;
        if(font)(void)xuiInternalFontRelease(pResource->pContext,font);
    }else if ( pResource->onDestroy != NULL ) {
		pResource->onDestroy(pResource->pContext, pResource->pHandle, pResource->pUser);
	}
	if ( pResource->sName != NULL ) {
		xrtFree(pResource->sName);
	}
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
    pContext->pResources=NULL;
	while ( pResource != NULL ) {
		pNext = pResource->pNext;
		__xuiResourceDestroyOne(pResource);
		pResource = pNext;
	}
}

bool xuiInternalContextTraceRegistry(xui_context context,xrtownershipvisitor visit,void* user)
{
    for(xui_resource resource=context->pResources;resource;resource=resource->pNext){
        if(!resource->pHandle)continue;
        if((resource->iOwnershipFlags&XUI_RESOURCE_OWNS_SURFACE) && !visit(xuiSurfaceOwnership(resource->pHandle),user))return false;
        if((resource->iOwnershipFlags&XUI_RESOURCE_OWNS_FONT) && !visit(xuiFontOwnership(resource->pHandle),user))return false;
    }
    return true;
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
	int iRet;
	int i;

	if ( !__xuiContextValid(pContext) || (pWidget == NULL) ) {
		return;
	}
	if ( !pContext->bDestroying && xuiInternalWidgetIsValid(pWidget) ) {
		pWidget->bInteractionCancelling = 1;
		iRet = xuiInternalInputCancelSubtree(pContext, pWidget);
		if ( iRet != XUI_OK && __xuiContextValid(pContext) ) {
			xuiInternalReportError(pContext, pWidget, iRet, XUI_ERROR_STAGE_INPUT, 1,
				"input.cancel_subtree", "A subtree interaction-cancel handler failed.");
		}
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pDragAdornerOwner) ) {
		xuiInternalDragAdornerHide(pContext, pContext->pDragAdornerOwner);
	}
	xuiInternalDragTransferDetachWidget(pContext, pWidget);
	if ( pContext->pDragAdornerWidget == pWidget ) {
		pContext->pDragAdornerWidget = NULL;
		pContext->pDragAdornerOwner = NULL;
		pContext->iDragAdornerPrimitiveCount = 0;
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
	if ( __xuiContextWidgetContains(pWidget, pContext->pMenuBarFocusRestore) ) {
		pContext->pMenuBarFocusRestore = NULL;
	}
	if ( __xuiContextWidgetContains(pWidget, pContext->pMenuBarModeOwner) ) {
		pContext->pMenuBarModeOwner = NULL;
		pContext->pMenuBarFocusRestore = NULL;
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
	/* Hotkeys own command copies. Remove entries for the destroyed subtree
	 * instead of leaving inert records allocated for the context lifetime. */
	i = 0;
	while ( i < pContext->iHotkeyCount ) {
		xui_hotkey_t* pHotkey = &pContext->pHotkeys[i];
		if ( !__xuiContextWidgetContains(pWidget, pHotkey->pWidget) ) {
			i++;
			continue;
		}
		if ( pHotkey->sCommand != NULL ) {
			xrtFree(pHotkey->sCommand);
		}
		if ( i + 1 < pContext->iHotkeyCount ) {
			memmove(pHotkey, pHotkey + 1,
				sizeof(pContext->pHotkeys[0]) * (size_t)(pContext->iHotkeyCount - i - 1));
		}
		pContext->iHotkeyCount--;
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
	if ( pWidget->iMagic == XUI_WIDGET_MAGIC ) pWidget->bInteractionCancelling = 0;
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
	       (pProxy->zstdDecompress != NULL) &&
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
	/* New optional callbacks are appended. Accept the previous complete prefix
	 * and zero-fill fields which are absent from an older provider. */
	if ( (pProxy->iSize < offsetof(xui_proxy_t,fontLoadSystem)) || (pProxy->iVersion != XUI_PROXY_VERSION) ) {
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
	/* The measurement fallback cannot consume any explicit shaping input,
	 * even when the underlying drawing callbacks have richer capabilities. */
	if ( pProxy->textShape == NULL ) {
		pCaps->iCaps &= ~(XUI_PROXY_CAP_TEXT_CONTEXT | XUI_PROXY_CAP_TEXT_SCRIPT |
			XUI_PROXY_CAP_TEXT_LANGUAGE | XUI_PROXY_CAP_TEXT_RTL);
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

static void __xuiContextCopyProxy(xui_proxy_t* output,const xui_proxy_t* input)
{
	size_t size=input->iSize<sizeof(*output) ? input->iSize:sizeof(*output);
	memset(output,0,sizeof(*output));
	memcpy(output,input,size);
}

XUI_API void xuiInteractionPolicyDefault(xui_interaction_policy_t* pPolicy)
{
	if ( pPolicy == NULL ) return;
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->fDoubleClickSeconds = 0.50f;
	pPolicy->iDoubleClickWidth = 4;
	pPolicy->iDoubleClickHeight = 4;
	pPolicy->iDragWidth = 4;
	pPolicy->iDragHeight = 4;
	pPolicy->iWheelScrollLines = 3;
	pPolicy->fTooltipInitialDelay = 0.35f;
	pPolicy->fCaretBlinkSeconds = 0.53f;
}

static int __xuiInteractionPolicyValid(const xui_interaction_policy_t* pPolicy)
{
	return pPolicy != NULL &&
		((pPolicy->iSize == 0) || (pPolicy->iSize >= sizeof(*pPolicy))) &&
		pPolicy->fDoubleClickSeconds > 0.0f && pPolicy->fDoubleClickSeconds <= 10.0f &&
		pPolicy->iDoubleClickWidth > 0 && pPolicy->iDoubleClickWidth <= 1024 &&
		pPolicy->iDoubleClickHeight > 0 && pPolicy->iDoubleClickHeight <= 1024 &&
		pPolicy->iDragWidth > 0 && pPolicy->iDragWidth <= 1024 &&
		pPolicy->iDragHeight > 0 && pPolicy->iDragHeight <= 1024 &&
		pPolicy->iWheelScrollLines >= 0 && pPolicy->iWheelScrollLines <= 100 &&
		pPolicy->fTooltipInitialDelay >= 0.0f && pPolicy->fTooltipInitialDelay <= 60.0f &&
		pPolicy->fCaretBlinkSeconds >= 0.0f && pPolicy->fCaretBlinkSeconds <= 10.0f;
}

int xuiInternalSetInteractionPolicy(xui_context pContext, const xui_interaction_policy_t* pPolicy, int bUserSet)
{
	if ( !__xuiContextValid(pContext) || !__xuiInteractionPolicyValid(pPolicy) ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext->tInteractionPolicy = *pPolicy;
	pContext->tInteractionPolicy.iSize = sizeof(pContext->tInteractionPolicy);
	if ( bUserSet ) pContext->bInteractionPolicyUserSet = 1;
	return XUI_OK;
}

void xuiInternalCaretBlinkReset(xui_context pContext)
{
	xui_proxy pProxy;
	double fNow;

	if ( !__xuiContextValid(pContext) ) return;
	pContext->fCaretBlinkElapsed = 0.0f;
	pContext->bCaretBlinkVisible = 1;
	pProxy = xuiInternalContextGetProxy(pContext);
	fNow = (pProxy != NULL && pProxy->clockSeconds != NULL) ? pProxy->clockSeconds(pProxy) : 0.0;
	pContext->bCaretBlinkClockValid = isfinite(fNow) && fNow >= 0.0;
	pContext->fCaretBlinkClockSeconds = pContext->bCaretBlinkClockValid ? fNow : 0.0;
}

int xuiInternalCaretBlinkVisible(xui_widget pWidget)
{
	xui_context pContext;
	xui_proxy pProxy;
	float fRemaining;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return 1;
	pContext = xuiWidgetGetContext(pWidget);
	if ( !__xuiContextValid(pContext) ) return 1;
	if ( pContext->pFocusWidget == pWidget && pContext->tInteractionPolicy.fCaretBlinkSeconds > 0.0f ) {
		pProxy = xuiInternalContextGetProxy(pContext);
		if ( pProxy != NULL && pProxy->requestFrame != NULL ) {
			fRemaining = pContext->tInteractionPolicy.fCaretBlinkSeconds - pContext->fCaretBlinkElapsed;
			if ( fRemaining < 0.001f ) fRemaining = 0.001f;
			pProxy->requestFrame(pProxy, fRemaining);
		}
	}
	return pContext->bCaretBlinkVisible;
}

void xuiInternalCaretBlinkUpdate(xui_context pContext, float fDelta)
{
	float fInterval;
	float fStep;
	int bOldVisible;
	xui_proxy pProxy;
	double fNow;

	if ( !__xuiContextValid(pContext) ) return;
	if ( (pContext->pFocusWidget == NULL) || (pContext->tInteractionPolicy.fCaretBlinkSeconds <= 0.0f) ) {
		xuiInternalCaretBlinkReset(pContext);
		return;
	}
	fStep = fDelta;
	pProxy = xuiInternalContextGetProxy(pContext);
	if ( pProxy != NULL && pProxy->clockSeconds != NULL ) {
		fNow = pProxy->clockSeconds(pProxy);
		if ( isfinite(fNow) && fNow >= 0.0 ) {
			if ( pContext->bCaretBlinkClockValid && fNow >= pContext->fCaretBlinkClockSeconds ) {
				fStep = (float)(fNow - pContext->fCaretBlinkClockSeconds);
			}
			pContext->fCaretBlinkClockSeconds = fNow;
			pContext->bCaretBlinkClockValid = 1;
		}
	}
	if ( fStep <= 0.0f ) return;
	/* Windows reports the interval between visible-state toggles, not a full cycle. */
	fInterval = pContext->tInteractionPolicy.fCaretBlinkSeconds;
	if ( fInterval < 0.05f ) fInterval = 0.05f;
	bOldVisible = pContext->bCaretBlinkVisible;
	pContext->fCaretBlinkElapsed += fStep;
	while ( pContext->fCaretBlinkElapsed >= fInterval ) {
		pContext->fCaretBlinkElapsed -= fInterval;
		pContext->bCaretBlinkVisible = !pContext->bCaretBlinkVisible;
	}
	if ( bOldVisible != pContext->bCaretBlinkVisible ) {
		(void)xuiWidgetInvalidate(pContext->pFocusWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

XUI_API int xuiSetInteractionPolicy(xui_context pContext, const xui_interaction_policy_t* pPolicy)
{
	return xuiInternalSetInteractionPolicy(pContext, pPolicy, 1);
}

XUI_API int xuiGetInteractionPolicy(xui_context pContext, xui_interaction_policy_t* pPolicy)
{
	if ( !__xuiContextValid(pContext) || pPolicy == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pPolicy = pContext->tInteractionPolicy;
	pPolicy->iSize = sizeof(*pPolicy);
	return XUI_OK;
}

XUI_API void xuiContextDescDefault(xui_context_desc_t* pDesc)
{
	if(!pDesc)return;
	memset(pDesc,0,sizeof(*pDesc));
	pDesc->iSize=sizeof(*pDesc);
	pDesc->iSystemFontPolicy=XUI_SYSTEM_FONT_AUTO;
}

static int __xuiCreateContext(const xui_context_desc_t* pDesc,xui_context* ppContext)
{
	xui_context pContext;
	xui_context_desc_t tDesc;

	if ( ppContext == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	xuiContextDescDefault(&tDesc);
	if(pDesc!=NULL){
		if(pDesc->iSize<sizeof(*pDesc) || pDesc->iFlags!=0 ||
		   pDesc->iSystemFontPolicy<XUI_SYSTEM_FONT_AUTO ||
		   pDesc->iSystemFontPolicy>XUI_SYSTEM_FONT_EAGER ||
		   pDesc->fSystemFontSize<0.0f)return XUI_ERROR_INVALID_ARGUMENT;
		tDesc=*pDesc;
		tDesc.iSize=sizeof(tDesc);
	}
	*ppContext = NULL;
	pContext = (xui_context)xrtMalloc(sizeof(*pContext));
	if ( pContext == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pContext, 0, sizeof(*pContext));
	pContext->iMagic = XUI_CONTEXT_MAGIC;
	pContext->tCreateDesc=tDesc;
	pContext->pLayoutContext = xLayoutContextCreate(NULL);
	if ( pContext->pLayoutContext == NULL ) {
		pContext->iMagic = 0;
		xrtFree(pContext);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pContext->fDpiScale = 1.0f;
	pContext->iDpiGeneration = 1u;
	xuiInteractionPolicyDefault(&pContext->tInteractionPolicy);
	pContext->bCaretBlinkVisible = 1;
	pContext->iGeneration = 1;
	pContext->iNextStylePropertyId = 1;
	pContext->iNextResourceGeneration = 1;
	pContext->iResourceRegistryGeneration = 1;
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
		xLayoutContextDestroy(pContext->pLayoutContext);
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

XUI_API int xuiCreate(xui_context* ppContext)
{return __xuiCreateContext(NULL,ppContext);}

XUI_API int xuiCreateEx(const xui_context_desc_t* pDesc,xui_context* ppContext)
{return __xuiCreateContext(pDesc,ppContext);}

XUI_API int xuiCreateOwned(xui_proxy pProxy,xui_context* ppContext)
{return xuiCreateOwnedEx(pProxy,NULL,ppContext);}

XUI_API int xuiCreateOwnedEx(xui_proxy pProxy,const xui_context_desc_t* pDesc,xui_context* ppContext)
{
    if(!ppContext)return XUI_ERROR_INVALID_ARGUMENT;
    *ppContext=NULL;
    if(!xuiProxyIsOwned(pProxy))return XUI_ERROR_INVALID_ARGUMENT;
    /* SetProxyOwned retains the candidate backend before invoking getCaps.
     * Each real graph publication uses its own mutation gate; do not keep a
     * global mutation interval open across an arbitrary backend callback. */
    xui_context context=NULL;
    int status=xuiInternalProxyReady(pProxy) ? __xuiCreateContext(pDesc,&context):XUI_ERROR_BUSY;
    if(status==XUI_OK)status=xuiSetProxyOwned(context,pProxy);
    if(status==XUI_OK)*ppContext=context;
    else if(context){
        /* This unpublished Context has no external owners or active calls.
         * Roll back directly through the normal physical destructor, rather
         * than enqueueing an unobservable owner for a later retry. Preserve
         * the first operation outcome even when its diagnostic is NULL. */
        xerror* first=xrtTakeError();__xuiContextDestroyNow(context);
        xrtErrorFree(xrtTakeError());xrtSetErrorTake(first);
    }
    return status;
}

XUI_API xui_proxy xuiGetProxyOwned(xui_context pContext)
{return __xuiContextValid(pContext) && !pContext->bCloseRequested && !pContext->bDestroyPending && !pContext->bDestroying ? pContext->pOwnedProxy:NULL;}

XUI_API int xuiRequestClose(xui_context context)
{
    if(!__xuiContextValid(context))return XUI_ERROR_INVALID_ARGUMENT;
    if(context->bDestroying || context->bDestroyPending)return XUI_ERROR_INVALID_STATE;
    if(context->pOwnedProxy && !xuiInternalProxyCreator(context->pOwnedProxy))return XUI_ERROR_BUSY;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return XUI_ERROR_INVALID_STATE;
    context->bCloseRequested=1;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return XUI_OK;
}

static void __xuiContextDestroyFonts(xui_context pContext)
{
    if(!__xuiContextValid(pContext))return;
    /* Empty every public slot before a consuming backend hook can reenter. */
    xui_font_entry_t inlineFonts[XUI_CONTEXT_FONT_INLINE];
    xui_font_entry_t* fonts=pContext->pFonts;int count=pContext->iFontCount;
    bool dynamic=fonts!=pContext->arrInlineFonts;
    if(!dynamic){memcpy(inlineFonts,fonts,sizeof(inlineFonts));fonts=inlineFonts;}
    xui_font defaultFont=pContext->pDefaultFont;
    xui_font systemFont=pContext->pSystemFont;
    bool owned=pContext->pOwnedProxy!=NULL;
    pContext->pFonts=pContext->arrInlineFonts;pContext->iFontCount=0;
    pContext->iFontCapacity=XUI_CONTEXT_FONT_INLINE;pContext->pDefaultFont=NULL;
    pContext->pSystemFont=NULL;pContext->tTheme.pFont=NULL;
    memset(pContext->arrInlineFonts,0,sizeof(pContext->arrInlineFonts));
    xerror* first=xrtTakeError();int result=first ? XUI_ERROR_CALLBACK_FAILED:XUI_OK;
    for(int i=0;i<count;++i){
        xrtFree(fonts[i].sName);
        if(owned && fonts[i].pFont){
            int status=xuiFontReleaseChecked(fonts[i].pFont);xerror* error=xrtTakeError();
            if(result==XUI_OK && (status!=XUI_OK || error)){result=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}
            else xrtErrorFree(error);
        }
    }
    if(dynamic)xrtFree(fonts);
    if(owned && defaultFont){
        int status=xuiFontReleaseChecked(defaultFont);xerror* error=xrtTakeError();
        if(result==XUI_OK && (status!=XUI_OK || error)){result=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}
        else xrtErrorFree(error);
    }
    if(systemFont){
        if(owned){
            int status=xuiFontReleaseChecked(systemFont);xerror* error=xrtTakeError();
            if(result==XUI_OK && (status!=XUI_OK || error)){result=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;first=error;}
            else xrtErrorFree(error);
        }else if(pContext->bHasProxy && pContext->tProxy.fontDestroy){
            pContext->tProxy.fontDestroy(&pContext->tProxy,systemFont);
            xerror* error=xrtTakeError();
            if(result==XUI_OK && error){result=XUI_ERROR_CALLBACK_FAILED;first=error;}
            else xrtErrorFree(error);
        }
    }
    xuiInternalContextRecordOutcome(pContext,result,first);xrtSetErrorTake(first);
}

static void __xuiContextDestroyNow(xui_context pContext)
{
	if ( !__xuiContextValid(pContext) || pContext->bDestroying ) {
		return;
	}
    /* Physical destruction can follow an ordinary native operation with no
     * external Scope. Keep a stack receipt through every cleanup phase. */
    xui_call_scope_t receipt={0};
    receipt.pContext=pContext;receipt.pSelf=&receipt;receipt.pNext=pContext->pCallScopes;
    if(receipt.pNext)receipt.pNext->pPrevious=&receipt;
    pContext->pCallScopes=&receipt;
    xuiInternalContextRecordOutcome(pContext,XUI_OK,xrtGetError());
	pContext->bDestroying = 1;
	xuiInternalOperationEnter(pContext);
	xuiInternalContextInvalidateLifetimes(pContext);
	xuiInternalOperationLeave(pContext);
	xuiInternalNativeAccessibilityDetach(pContext);
	pContext->bDestroyPending = 0;
	pContext->pStateChangeHead = NULL;
	pContext->pStateChangeTail = NULL;
	if ( pContext->bHasProxy && pContext->tProxy.imeSetEnabled != NULL ) {
		(void)pContext->tProxy.imeSetEnabled(&pContext->tProxy, 0);
	}
	if ( pContext->onImeDetach != NULL ) {
		pContext->onImeDetach(pContext);
		pContext->onImeDetach = NULL;
	}
	xuiInternalDragTransferShutdown(pContext);
    xuiInternalContextDestroyManagers(pContext);
	if ( pContext->pRoot != NULL ) {
		(void)xuiInternalWidgetRequestDestroy(pContext->pRoot,NULL);
		pContext->pRoot = NULL;
	}
	if ( pContext->pOverlayRoot != NULL ) {
		(void)xuiInternalWidgetRequestDestroy(pContext->pOverlayRoot,NULL);
		pContext->pOverlayRoot = NULL;
	}
	xuiInternalWidgetDestroyFlush(pContext);
	xuiInternalContextDestroyWidgets(pContext);
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
	if ( pContext->pLayoutTrackScratch != NULL ) {
		xrtFree(pContext->pLayoutTrackScratch);
		pContext->pLayoutTrackScratch = NULL;
		pContext->iLayoutTrackScratchCapacity = 0u;
	}
	xLayoutContextDestroy(pContext->pLayoutContext);
	pContext->pLayoutContext = NULL;
    xui_proxy ownedProxy=pContext->pOwnedProxy;pContext->pOwnedProxy=NULL;
    if(ownedProxy)xuiInternalContextRecordFailure(pContext,xuiProxyReleaseChecked(ownedProxy));
	pContext->iMagic = 0;
	for ( xui_call_scope_t* pScope = pContext->pCallScopes; pScope != NULL; pScope = pScope->pNext )
		pScope->pContext = NULL;
    while(pContext->pDestroyScopes){
        xui_internal_destroy_scope_t* scope=pContext->pDestroyScopes;
        pContext->pDestroyScopes=scope->pPrevious;
        scope->pContext=NULL;scope->pPrevious=NULL;
    }
	xrtFree(pContext);
    if(receipt.iResult!=XUI_OK)xrtSetErrorTake(receipt._pFirstError);
}

static int __xuiContextDestroyOwner(xui_context context,bool* consumed)
{
    if(!consumed)return XUI_ERROR_INVALID_ARGUMENT;
    *consumed=false;
    if(!__xuiContextValid(context))return XUI_ERROR_INVALID_ARGUMENT;
    if(context->bDestroying || context->bDestroyPending || context->iDestroyOwnership==2)return XUI_ERROR_INVALID_STATE;
    if(context->pOwnedProxy && !xuiInternalProxyCreator(context->pOwnedProxy))return XUI_ERROR_BUSY;
    int result=xuiRequestClose(context);if(result!=XUI_OK)return result;
    /* Deep graph readiness and idle destruction are different questions.
     * Independent cache owners can become ready only after another widget is
     * actually prepared (for example a custom backend's LIFO Draw stack).
     * Active native C stacks still block every destructive preparation. */
    if(context->iOperationDepth || context->iWidgetCallbackDepth || context->iDestroyFlushDepth)
        return XUI_STATUS_DEFERRED;
    bool selfPending=context->iDestroyOwnership==1;
    if(selfPending)context->iDestroyOwnership=2;
    xui_call_scope_t scope={0};result=xuiInternalCleanupCallScopeBegin(context,&scope);
    if(result!=XUI_OK){if(selfPending)context->iDestroyOwnership=1;return result;}
    bool complete=false;result=xuiInternalContextPrepareDestroy(context,&complete);
    xerror* first=xrtTakeError();
    if(result>=0 && first)result=XUI_ERROR_CALLBACK_FAILED;
    /* Pure waiting is not a failure receipt. Actual negative cleanup results
     * stay independent of completion and of a possibly NULL diagnostic. */
    int failure=result<0 ? result:XUI_OK;
    xuiInternalCallScopeRecordOutcome(&scope,failure,first);
    if((selfPending || complete) && failure!=XUI_OK)xuiInternalContextRecordOutcome(context,failure,first);
    if(complete){context->bDestroyPending=1;*consumed=true;}
    int ended=xuiInternalCallScopeEndOutcome(&scope,failure,&first);
    if(selfPending && !complete)context->iDestroyOwnership=1;
    xrtSetErrorTake(first);
    return ended==XUI_OK && !complete ? XUI_STATUS_DEFERRED:ended;
}
static int __xuiContextDestroyOwnerInMode(xui_context context,bool* consumed,xui_internal_destroy_mode_t mode)
{
    xui_internal_destroy_scope_t scope={0};xuiInternalDestroyScopeBegin(context,mode,&scope);
    int result=__xuiContextDestroyOwner(context,consumed);
    xuiInternalDestroyScopeEnd(&scope);return result;
}
XUI_API int xuiDestroyOwnerChecked(xui_context context,bool* consumed)
{return __xuiContextDestroyOwnerInMode(context,consumed,XUI_DESTROY_NONBLOCKING);}
XUI_API int xuiDestroyChecked(xui_context pContext)
{bool consumed=false;return __xuiContextDestroyOwnerInMode(pContext,&consumed,XUI_DESTROY_NONBLOCKING);}
static void __xuiContextDestroyVoid(xui_context context)
{
    if(!__xuiContextValid(context) || context->bDestroying || context->bDestroyPending || context->iDestroyOwnership==2)return;
    int status=xuiRequestClose(context);
    if(status!=XUI_OK){
        if(!xrtGetError())xrtSetErrorInfo(XERR_STATE,"xui",status,"Native Context close was refused.");
        return;
    }
    context->iDestroyOwnership=1;
    context->iPendingDestroyMode=XUI_DESTROY_SYNC_WORKER;
    if(!context->iOperationDepth && !context->iWidgetCallbackDepth && !context->iDestroyFlushDepth){
        bool consumed=false;(void)__xuiContextDestroyOwner(context,&consumed);
    }
}
XUI_API void xuiDestroy(xui_context context)
{
    xui_internal_destroy_scope_t scope={0};xuiInternalDestroyScopeBegin(context,XUI_DESTROY_SYNC_WORKER,&scope);
    __xuiContextDestroyVoid(context);
    xuiInternalDestroyScopeEnd(&scope);
}

XUI_API int xuiInternalContextSetImeDetach(xui_context pContext, void (*onDetach)(xui_context pContext))
{
	if ( !__xuiContextValid(pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext->onImeDetach = onDetach;
	return XUI_OK;
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
	__xuiContextCopyProxy(&pContext->tProxy,pProxy);
	pContext->tProxyCaps = tCaps;
	pContext->bHasProxy = 1;
	__xuiContextBumpGeneration(pContext);
	if(pContext->tCreateDesc.iSystemFontPolicy==XUI_SYSTEM_FONT_EAGER){
		iRet=xuiEnsureDefaultFont(pContext);
		if(iRet!=XUI_OK){
			memset(&pContext->tProxy,0,sizeof(pContext->tProxy));
			memset(&pContext->tProxyCaps,0,sizeof(pContext->tProxyCaps));
			pContext->bHasProxy=0;
			pContext->iSystemFontStatus=0;
			__xuiContextBumpGeneration(pContext);
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiSetProxyOwned(xui_context pContext,xui_proxy pProxy)
{
    if(!__xuiContextValid(pContext) || !xuiProxyIsOwned(pProxy))return XUI_ERROR_INVALID_ARGUMENT;
    pProxy=xuiInternalProxyCanonical(pProxy);
    if(!xuiInternalProxyReady(pProxy))return XUI_ERROR_BUSY;
    if(pContext->bCloseRequested || pContext->bDestroyPending || pContext->bDestroying)return XUI_ERROR_INVALID_STATE;
    if(pContext->pOwnedProxy)return pContext->pOwnedProxy==pProxy ? XUI_OK:XUI_ERROR_ALREADY_INITIALIZED;
    if(pContext->bHasProxy)return XUI_ERROR_ALREADY_INITIALIZED;
    /* Legacy backend-less contexts may bind only before they acquire slots
     * whose old contract was borrowed. Safe language factories bind first. */
    if(pContext->pAllWidgets || pContext->pResources || pContext->pDefaultFont || pContext->pSystemFont || pContext->iFontCount)
        return XUI_ERROR_INVALID_STATE;
    xui_call_scope_t scope={0};int result=xuiCallScopeBegin(pContext,&scope);if(result!=XUI_OK)return result;
    result=xuiProxyAddRef(pProxy);bool retained=result==XUI_OK;
    xui_proxy_caps_t caps={0};
    if(result==XUI_OK)result=__xuiContextValidateProxy(pProxy,&caps);
    if(result==XUI_OK && (pContext->bCloseRequested || pContext->bDestroyPending || pContext->bDestroying || pContext->bHasProxy))result=XUI_ERROR_INVALID_STATE;
    if(result==XUI_OK){
        xrtownershipscope mutation={0};
        if(!xrtOwnershipMutationBegin(&mutation))result=XUI_ERROR_INVALID_STATE;
        else{
            __xuiContextCopyProxy(&pContext->tProxy,pProxy);pContext->tProxyCaps=caps;pContext->pOwnedProxy=pProxy;
            pContext->bHasProxy=1;retained=false;__xuiContextBumpGeneration(pContext);
            if(!xrtOwnershipScopeEnd(&mutation))abort();
        }
    }
    if(result==XUI_OK && pContext->tCreateDesc.iSystemFontPolicy==XUI_SYSTEM_FONT_EAGER)
        result=xuiEnsureDefaultFont(pContext);
    if(result!=XUI_OK && pContext->pOwnedProxy==pProxy && !pContext->pSystemFont && !pContext->pDefaultFont){
        xrtownershipscope mutation={0};
        if(xrtOwnershipMutationBegin(&mutation)){
            memset(&pContext->tProxy,0,sizeof(pContext->tProxy));
            memset(&pContext->tProxyCaps,0,sizeof(pContext->tProxyCaps));
            pContext->pOwnedProxy=NULL;pContext->bHasProxy=0;pContext->iSystemFontStatus=0;
            retained=true;__xuiContextBumpGeneration(pContext);
            if(!xrtOwnershipScopeEnd(&mutation))abort();
        }
    }
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    xuiInternalCallScopeRecordOutcome(&scope,result,first);
    if(retained){
        int next=xuiProxyReleaseChecked(pProxy);xerror* error=xrtTakeError();
        if(next!=XUI_OK || error)xuiInternalContextRecordOutcome(pContext,next!=XUI_OK ? next:XUI_ERROR_CALLBACK_FAILED,result!=XUI_OK ? first:error);
        if(result==XUI_OK && (next!=XUI_OK || error)){result=next!=XUI_OK ? next:XUI_ERROR_CALLBACK_FAILED;first=error;}
        else xrtErrorFree(error);
    }
    result=xuiInternalCallScopeEndOutcome(&scope,result,&first);
    xrtSetErrorTake(first);return result;
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

XUI_API int xuiSetViewportSize(xui_context pContext, int iWidth, int iHeight)
{
	if ( !__xuiContextValid(pContext) || (iWidth < 0) || (iHeight < 0) ||
	     (iWidth > XUI_CONTEXT_MAX_VIEWPORT) || (iHeight > XUI_CONTEXT_MAX_VIEWPORT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->fViewportWidth == iWidth) && (pContext->fViewportHeight == iHeight) ) {
		return XUI_OK;
	}
	pContext->fViewportWidth = iWidth;
	pContext->fViewportHeight = iHeight;
	pContext->iDamageCount = 0;
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		__xuiContextBumpGeneration(pContext);
		return XUI_OK;
	}
	return __xuiContextAddDamage(pContext, __xuiContextFullRect(pContext));
}

XUI_API xui_size_t xuiGetViewportSize(xui_context pContext)
{
	xui_size_t tSize;

	tSize.iW = 0;
	tSize.iH = 0;
	if ( __xuiContextValid(pContext) ) {
		tSize.iW = pContext->fViewportWidth;
		tSize.iH = pContext->fViewportHeight;
	}
	return tSize;
}

XUI_API xui_rect_t xuiRectFromFloatNearest(float fX, float fY, float fWidth, float fHeight)
{
	return xuiInternalRectFromFloatNearest(fX, fY, fWidth, fHeight);
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
	if ( ++pContext->iDpiGeneration == 0u ) pContext->iDpiGeneration = 1u;
	/* Pixel-valued geometry stays unchanged; dependent metrics and caches do not. */
	if ( pContext->pRoot != NULL ) {
		(void)xuiWidgetInvalidate(pContext->pRoot,
			XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( pContext->pOverlayRoot != NULL ) {
		(void)xuiWidgetInvalidate(pContext->pOverlayRoot,
			XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
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
	pTheme->iSelectionColor = XUI_COLOR_RGBA(74, 144, 239, 255);
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
    xui_font_slot_update_t fontUpdate={0};
    iRet=xuiInternalFontSlotBegin(pContext,&pContext->pDefaultFont,pTheme->pFont,true,&fontUpdate);
    if(iRet!=XUI_OK)return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
	tTheme = *pTheme;
	tTheme.iSize = sizeof(tTheme);
	pContext->tTheme = tTheme;

	iRet = xuiStyleBeginUpdate(pContext);
	if ( iRet != XUI_OK ) {
		return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
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
		return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
	}
	return xuiInternalFontSlotEnd(&fontUpdate,true,xuiStyleEndUpdate(pContext));
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
	iRet = xuiStyleEndUpdate(pContext);
	if ( iRet == XUI_OK && pContext->pTooltipPopupWidget != NULL ) {
		(void)xuiWidgetInvalidate(pContext->pTooltipPopupWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
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
    xui_font_slot_update_t update={0};
    int result=xuiInternalFontSlotBegin(pContext,&pContext->pDefaultFont,pFont,true,&update);
    if(result!=XUI_OK)return result;
	pContext->tTheme.pFont = pFont;
	xuiInternalContextBumpGeneration(pContext);
	return xuiInternalFontSlotEnd(&update,true,XUI_OK);
}

XUI_API xui_font xuiGetDefaultFont(xui_context pContext)
{
	if(!__xuiContextValid(pContext))return NULL;
	if(!pContext->pDefaultFont && pContext->tCreateDesc.iSystemFontPolicy!=XUI_SYSTEM_FONT_DISABLED)
		(void)xuiEnsureDefaultFont(pContext);
	return pContext->pDefaultFont;
}

static void __xuiSystemFontDiscard(xui_context pContext,xui_font pFont)
{
	if(!pFont)return;
	if(pContext->pOwnedProxy){
		bool consumed=false;
		int status=xuiFontReleaseOwnerChecked(pFont,&consumed);
		if(status!=XUI_OK)xuiInternalContextRecordFailure(pContext,status);
	}else if(pContext->tProxy.fontDestroy)pContext->tProxy.fontDestroy(&pContext->tProxy,pFont);
}

XUI_API int xuiEnsureDefaultFont(xui_context pContext)
{
	xui_system_font_desc_t desc={0};
	xui_font font=NULL;
	int result;
	if(!__xuiContextValid(pContext))return XUI_ERROR_INVALID_ARGUMENT;
	if(pContext->pDefaultFont)return XUI_OK;
	if(pContext->tCreateDesc.iSystemFontPolicy==XUI_SYSTEM_FONT_DISABLED)return XUI_ERROR_UNSUPPORTED;
	if(pContext->pSystemFont)return xuiSetDefaultFont(pContext,pContext->pSystemFont);
	if(!pContext->bHasProxy)return XUI_ERROR_NOT_INITIALIZED;
	if(pContext->iSystemFontStatus<0)return pContext->iSystemFontStatus;
	if(pContext->bSystemFontLoading)return XUI_ERROR_BUSY;
	if(!pContext->tProxy.fontLoadSystem){
		pContext->iSystemFontStatus=XUI_ERROR_UNSUPPORTED;
		return pContext->iSystemFontStatus;
	}
	pContext->bSystemFontLoading=1;
	desc.iSize=sizeof(desc);
	desc.fSize=pContext->tCreateDesc.fSystemFontSize;
	result=pContext->tProxy.fontLoadSystem(&pContext->tProxy,&font,&desc);
	if(result==XUI_OK && !font)result=XUI_ERROR_BACKEND_FAILED;
	if(result!=XUI_OK){
		__xuiSystemFontDiscard(pContext,font);
		pContext->bSystemFontLoading=0;
		pContext->iSystemFontStatus=result;
		return result;
	}
	if(pContext->pOwnedProxy){
		xui_font_slot_update_t systemUpdate={0};
		result=xuiInternalFontSlotBegin(pContext,&pContext->pSystemFont,font,true,&systemUpdate);
		if(result==XUI_OK)result=xuiInternalFontSlotEnd(&systemUpdate,true,xuiSetDefaultFont(pContext,font));
		if(result!=XUI_OK && pContext->pSystemFont==font){
			xui_font_slot_update_t clearUpdate={0};
			int cleanup=xuiInternalFontSlotBegin(pContext,&pContext->pSystemFont,NULL,true,&clearUpdate);
			(void)xuiInternalFontSlotEnd(&clearUpdate,true,cleanup);
		}
		__xuiSystemFontDiscard(pContext,font);
	}else{
		pContext->pSystemFont=font;
		result=xuiSetDefaultFont(pContext,font);
		if(result!=XUI_OK){
			pContext->pSystemFont=NULL;
			__xuiSystemFontDiscard(pContext,font);
		}
	}
	pContext->bSystemFontLoading=0;
	pContext->iSystemFontStatus=result==XUI_OK ? XUI_OK:result;
	return result;
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
            xui_font_slot_update_t update={0};
            iRet=xuiInternalFontSlotBegin(pContext,&pContext->pFonts[i].pFont,pFont,true,&update);
            if(iRet!=XUI_OK)return iRet;
			xuiInternalContextBumpGeneration(pContext);
			return xuiInternalFontSlotEnd(&update,true,XUI_OK);
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
    xui_font_entry_t* entry=&pContext->pFonts[pContext->iFontCount];
    entry->pFont=NULL;xui_font_slot_update_t update={0};
    iRet=xuiInternalFontSlotBegin(pContext,&entry->pFont,pFont,true,&update);
    if(iRet!=XUI_OK){xrtFree(sCopy);return iRet;}
	pContext->pFonts[pContext->iFontCount].sName = sCopy;

	pContext->iFontCount++;
	xuiInternalContextBumpGeneration(pContext);
	return xuiInternalFontSlotEnd(&update,true,XUI_OK);
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
    xui_call_scope_t scope={0};
    if(pContext->pOwnedProxy && xuiCallScopeBegin(pContext,&scope)!=XUI_OK)return;
	__xuiContextDestroyFonts(pContext);
	xuiInternalContextBumpGeneration(pContext);
    (void)xuiCallScopeEnd(&scope);
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

XUI_API int xuiResourceSet(xui_context context,xui_resource* output,const xui_resource_desc_t* desc)
{
    if(output)*output=NULL;
    if(!__xuiContextValid(context) || !desc || desc->iSize<sizeof(*desc) || !desc->sName || !*desc->sName || desc->iKind<=0)
        return XUI_ERROR_INVALID_ARGUMENT;
    if(context->bCloseRequested || context->bDestroyPending || context->bDestroying)return XUI_ERROR_INVALID_STATE;
    if(context->pOwnedProxy && !xuiInternalProxyReady(context->pOwnedProxy))return XUI_ERROR_BUSY;
    uint32_t owned=0;
    if(context->pOwnedProxy && (desc->iKind==XUI_RESOURCE_SURFACE || desc->iKind==XUI_RESOURCE_FONT)){
        if(desc->onDestroy)return XUI_ERROR_INVALID_ARGUMENT;
        owned=desc->iKind==XUI_RESOURCE_SURFACE ? XUI_RESOURCE_OWNS_SURFACE:XUI_RESOURCE_OWNS_FONT;
    }
    xui_resource existing=NULL,previous=NULL;
    for(xui_resource scan=context->pResources;scan;scan=scan->pNext){
        if(strcmp(scan->sName,desc->sName)==0){existing=scan;break;}previous=scan;
    }
    if(existing && (existing->iOwnershipFlags&XUI_RESOURCE_UPDATING))return XUI_ERROR_BUSY;
    if(existing && existing->iKind==desc->iKind && existing->pHandle==desc->pHandle &&
       existing->pUser==desc->pUser && existing->onDestroy==desc->onDestroy){if(output)*output=existing;return XUI_OK;}
    if(existing && existing->iRefCount!=1)return XUI_ERROR_INVALID_STATE;
    if(existing && existing->pHandle && existing->pHandle==desc->pHandle && existing->onDestroy)return XUI_ERROR_INVALID_STATE;
    xui_call_scope_t scope={0};int status=xuiCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;
    xui_resource candidate=NULL;bool held=false,published=false;
    char* name=__xuiCoreStringDuplicate(desc->sName);
    if(!name){status=XUI_ERROR_OUT_OF_MEMORY;goto finish;}
    candidate=xrtCalloc(1,sizeof(*candidate));
    if(!candidate){status=XUI_ERROR_OUT_OF_MEMORY;goto finish;}
    if(owned && desc->pHandle){
        status=owned==XUI_RESOURCE_OWNS_SURFACE ? xuiInternalSurfaceAdmit(context,desc->pHandle):xuiInternalFontAdmit(context,desc->pHandle);
        if(status!=XUI_OK)goto finish;
        held=true;
    }
    candidate->iMagic=XUI_RESOURCE_MAGIC;candidate->pContext=context;candidate->sName=name;
    candidate->iKind=desc->iKind;candidate->pHandle=desc->pHandle;candidate->pUser=desc->pUser;
    candidate->onDestroy=desc->onDestroy;candidate->iRefCount=1;
    candidate->iOwnershipFlags=owned|XUI_RESOURCE_UPDATING;
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation)){status=XUI_ERROR_INVALID_STATE;goto finish;}
    if(existing){
        __xuiResourceCascadeChanged(context,existing);
        if(previous)previous->pNext=existing->pNext;else context->pResources=existing->pNext;
        __xuiResourceRemoveDependencyReferences(context,existing);
    }
    candidate->iGeneration=__xuiResourceNextGeneration(context);
    candidate->pNext=context->pResources;context->pResources=candidate;published=true;
    __xuiContextBumpGeneration(context);__xuiResourceRegistryChanged(context);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* The complete new entry is visible while an old finalizer reenters.
     * Same-name replacement/removal is BUSY until that cleanup has returned. */
    if(existing)__xuiResourceDestroyOne(existing);
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    candidate->iOwnershipFlags&=~XUI_RESOURCE_UPDATING;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(context->bCloseRequested || context->bDestroyPending || context->bDestroying)status=XUI_ERROR_INVALID_STATE;
    else if(output)*output=candidate;
finish:
    if(status==XUI_OK && xrtGetError())status=XUI_ERROR_CALLBACK_FAILED;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    xuiInternalCallScopeRecordOutcome(&scope,status,first);
    if(!published){
        if(held)xuiInternalContextCleanupOutcome(context,
            owned==XUI_RESOURCE_OWNS_SURFACE ? xuiSurfaceReleaseChecked(desc->pHandle):xuiFontReleaseChecked(desc->pHandle),&status,&first);
        xrtFree(candidate);xrtFree(name);
    }
    status=xuiInternalCallScopeEndOutcome(&scope,status,&first);
    xrtSetErrorTake(first);return status;
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

XUI_API int xuiResourceRemove(xui_resource resource)
{
    if(!__xuiResourceValid(resource))return XUI_ERROR_INVALID_ARGUMENT;
    if(resource->iOwnershipFlags&XUI_RESOURCE_UPDATING)return XUI_ERROR_BUSY;
    if(resource->iRefCount!=1)return XUI_ERROR_INVALID_STATE;
    xui_context context=resource->pContext;
    if(!__xuiContextValid(context))return XUI_ERROR_INVALID_ARGUMENT;
    if(context->bCloseRequested || context->bDestroyPending || context->bDestroying)return XUI_ERROR_INVALID_STATE;
    if(context->pOwnedProxy && !xuiInternalProxyReady(context->pOwnedProxy))return XUI_ERROR_BUSY;
    xui_resource previous=NULL;
    for(xui_resource scan=context->pResources;scan;scan=scan->pNext){
        if(scan==resource){
            xui_call_scope_t scope={0};int status=xuiCallScopeBegin(context,&scope);if(status!=XUI_OK)return status;
            xrtownershipscope mutation={0};
            if(!xrtOwnershipMutationBegin(&mutation))return xuiCallScopeEnd(&scope)==XUI_OK ? XUI_ERROR_INVALID_STATE:XUI_ERROR_CALLBACK_FAILED;
            __xuiResourceCascadeChanged(context,resource);
            if(previous)previous->pNext=resource->pNext;else context->pResources=resource->pNext;
            __xuiResourceRemoveDependencyReferences(context,resource);
            __xuiContextBumpGeneration(context);__xuiResourceRegistryChanged(context);
            if(!xrtOwnershipScopeEnd(&mutation))abort();
            __xuiResourceDestroyOne(resource);return xuiCallScopeEnd(&scope);
        }
        previous=scan;
    }
    return XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiResourceTouch(xui_resource pResource)
{
	xui_context pContext;

	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = pResource->pContext;
	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiResourceCascadeChanged(pContext, pResource);
	__xuiContextBumpGeneration(pContext);
	__xuiResourceRegistryChanged(pContext);
	return XUI_OK;
}

XUI_API uint64_t xuiResourceGetRegistryGeneration(xui_context pContext)
{
	return __xuiContextValid(pContext) ? pContext->iResourceRegistryGeneration : 0;
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
	if ( pResource->iRefCount == INT_MAX ) {
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	pResource->iRefCount++;
	return XUI_OK;
}

XUI_API int xuiResourceRelease(xui_resource pResource)
{
	if ( !__xuiResourceValid(pResource) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pResource->iRefCount <= 0 ) {
		return XUI_ERROR_INVALID_STATE;
	}
	if ( pResource->iRefCount > 1 ) {
		pResource->iRefCount--;
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
	if ( pResource->pDependencies == NULL ) {
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

int xuiInternalDrawDisclosureChevron(xui_proxy pProxy, xui_draw_context pDraw,
	xui_rect_t tRect, int bExpanded, uint32_t iColor)
{
	float fCX;
	float fCY;
	float fX0;
	float fY0;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	int iRet;

	if ( (pProxy == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pProxy->drawLine == NULL) || ((iColor & 0xffu) == 0u) ||
		(tRect.fW < 8.0f) || (tRect.fH < 8.0f) ) {
		return XUI_OK;
	}
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	if ( bExpanded ) {
		fX0 = fCX - 3.5f;
		fY0 = fCY - 1.75f;
		fX1 = fCX;
		fY1 = fCY + 1.75f;
		fX2 = fCX + 3.5f;
		fY2 = fY0;
	} else {
		fX0 = fCX - 1.75f;
		fY0 = fCY - 3.5f;
		fX1 = fCX + 1.75f;
		fY1 = fCY;
		fX2 = fX0;
		fY2 = fCY + 3.5f;
	}
	iRet = pProxy->drawLine(pProxy, pDraw, fX0, fY0, fX1, fY1, 1.35f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fX1, fY1, fX2, fY2, 1.35f, iColor);
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
	return (pProxy != NULL) ? pProxy->drawText(pProxy, pPainter->pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pFont, .sText=sText, .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((iFlags) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tRect, iColor, iFlags) : XUI_ERROR_NOT_INITIALIZED;
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

static int __xuiCoreDrawRenderNodeRegion(xui_context pContext, xui_surface pTarget,
	const xui_render_node_t* pNode, xui_widget pAncestor, xui_rect_t tClipRect)
{
	xui_widget_cache_slot_t* pSlot;
	xui_widget_cache_slot_t* pAncestorSlot;
	xui_proxy pProxy;
	xui_rect_t arrParts[4];
	xui_rect_t tSrc;
	xui_rect_t tDst;
	float fScaleX;
	float fScaleY;
	int iPartCount;
	int i;
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
	if ( !__xuiCoreFloatRectValid(tClipRect) ) {
		return XUI_OK;
	}
	for ( ; pAncestor != NULL; pAncestor = pAncestor->pParent ) {
		if ( __xuiCoreEffectiveCachePolicy(pAncestor) != XUI_CACHE_POLICY_SUBTREE ) continue;
		pAncestorSlot = __xuiCoreFindCacheSlot(pAncestor, pAncestor->iStateId);
		if ( pAncestorSlot == NULL || pAncestorSlot->pSurface == NULL ) continue;
		iPartCount = xuiInternalRectSubtract(tClipRect, xuiWidgetGetWorldRect(pAncestor), arrParts);
		if ( iPartCount == 1 && arrParts[0].fW == tClipRect.fW && arrParts[0].fH == tClipRect.fH ) continue;
		/* Only compose pixels not already present in any ancestor subtree cache. */
		for ( i = 0; i < iPartCount; ++i ) {
			iRet = __xuiCoreDrawRenderNodeRegion(pContext, pTarget, pNode, pAncestor->pParent, arrParts[i]);
			if ( iRet != XUI_OK ) return iRet;
		}
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

static int __xuiCoreDrawRenderNode(xui_context pContext, xui_surface pTarget,
	const xui_render_node_t* pNode, xui_rect_i_t tDamage)
{
	xui_rect_t tClip = __xuiCoreIntersectRect(pNode->tPaintRect, __xuiCoreRectFromInt(tDamage));
	return __xuiCoreDrawRenderNodeRegion(pContext, pTarget, pNode, pNode->pWidget->pParent, tClip);
}

static void __xuiCoreClearRenderDirtyRecursive(xui_widget pWidget)
{
	xui_widget pChild;
	const uint32_t iFlags = XUI_WIDGET_DIRTY_RENDER | XUI_WIDGET_DIRTY_TREE;

	if ( pWidget == NULL || (pWidget->iSubtreeDirtyFlags & iFlags) == 0 ) {
		return;
	}
	pWidget->iDirtyFlags &= ~iFlags;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		__xuiCoreClearRenderDirtyRecursive(pChild);
	}
	pWidget->iSubtreeDirtyFlags &= ~iFlags;
}

static int __xuiCoreComposeRenderTree(xui_context pContext, xui_surface pTarget, const xui_surface_desc_t* pTargetDesc, const xui_rect_i_t* pRects, int iRectCount)
{
	xui_rect_i_t tDamage;
	const xui_render_node_t* pNode;
	xui_widget pRenderWidget;
	int i;
	int iRet;

	for ( i = 0; i < iRectCount; i++ ) {
		tDamage = pRects[i];
		if ( !__xuiCoreClipDamageToTarget(pTargetDesc, &tDamage) ) {
			continue;
		}
		for ( pRenderWidget = pContext->pRenderHead; pRenderWidget != NULL;
		      pRenderWidget = pRenderWidget->pRenderNext ) {
			pNode = &pRenderWidget->tRenderNode;
			if ( __xuiCoreNodeCoveredBySubtreeCache(pNode) ) {
				pContext->tRenderStats.iSkippedWidgets++;
				continue;
			}
			iRet = __xuiCoreDrawRenderNode(pContext, pTarget, pNode, tDamage);
			if ( iRet != XUI_OK ) {
				pContext->tRenderStats.iSkippedWidgets++;
				xuiInternalReportError(pContext, pNode->pWidget, iRet, XUI_ERROR_STAGE_RENDER, 1,
					"render.compose", "The render node failed and was skipped.");
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

static int __xuiRenderOperation(xui_context pContext, xui_surface pTarget, const xui_rect_i_t* pRects, int iRectCount)
{
	xui_surface_desc_t tDesc;
	xui_rect_i_t tInlineRect;
	const xui_rect_i_t* pRenderRects;
	int iRenderRectCount;
	int i;
	int iRet;

	if ( !__xuiContextValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pTarget == NULL) || (iRectCount < 0) || ((iRectCount > 0) && (pRects == NULL)) ) {
		xuiInternalReportError(pContext, NULL, XUI_ERROR_INVALID_ARGUMENT, XUI_ERROR_STAGE_RENDER, 0,
			"render.arguments", "The render target or damage rectangle arguments are invalid.");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pContext->bHasProxy ) {
		xuiInternalReportError(pContext, NULL, XUI_ERROR_NOT_INITIALIZED, XUI_ERROR_STAGE_RENDER, 0,
			"render.proxy", "No render proxy is attached to the XUI context.");
		return XUI_ERROR_NOT_INITIALIZED;
	}
	for ( i = 0; i < iRectCount; i++ ) {
		if ( !__xuiRectValid(pRects[i]) ) {
			xuiInternalReportError(pContext, NULL, XUI_ERROR_INVALID_ARGUMENT, XUI_ERROR_STAGE_RENDER, 0,
				"render.damage", "A damage rectangle is empty or invalid.");
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	memset(&tDesc, 0, sizeof(tDesc));
	iRet = pContext->tProxy.surfaceGetDesc(&pContext->tProxy, pTarget, &tDesc);
	if ( iRet != XUI_OK ) {
		xuiInternalReportError(pContext, NULL, iRet, XUI_ERROR_STAGE_RENDER, 0,
			"render.target", "The target surface description could not be read.");
		return iRet;
	}
	if ( (tDesc.iKind != XUI_SURFACE_KIND_TEXTURE) ||
	     (tDesc.iFormat != XUI_SURFACE_FORMAT_RGBA8) ||
	     ((tDesc.iFlags & XUI_SURFACE_USAGE_TARGET) == 0) ) {
		xuiInternalReportError(pContext, NULL, XUI_ERROR_INVALID_ARGUMENT, XUI_ERROR_STAGE_RENDER, 0,
			"render.target", "The target surface is not a supported RGBA8 render target.");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pContext->pRoot != NULL ) {
		iRet = xuiRenderPrepare(pContext);
		if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iRet = xuiBuildRenderTree(pContext);
		if ( xuiInternalContextDestroyPending(pContext) ) return XUI_OK;
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		xuiInternalStateDamageFlush(pContext);
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

XUI_API int xuiRender(xui_context pContext, xui_surface pTarget, const xui_rect_i_t* pRects, int iRectCount)
{
	int iRet;

	xuiInternalOperationEnter(pContext);
	iRet = __xuiRenderOperation(pContext, pTarget, pRects, iRectCount);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

#endif
