#ifndef XLANG_IDE_LEGACY_IMPORT_H
#define XLANG_IDE_LEGACY_IMPORT_H
#include "model/workspace.h"
typedef struct xide_legacy_import xide_legacy_import;
enum { XIDE_IMPORT_WORKSPACE=1, XIDE_IMPORT_PROJECT=2 };
/* Read-only preparation. New bundle references original assets. */
xide_legacy_import* xideLegacyImportPrepare(const char* source,const char* destination,int kind,xide_model_error* error);
void xideLegacyImportDestroy(xide_legacy_import* plan);
const char* xideLegacyImportDirectory(const xide_legacy_import* plan);
const char* xideLegacyImportWorkspacePath(const xide_legacy_import* plan);
size_t xideLegacyImportProjectCount(const xide_legacy_import* plan);
const char* xideLegacyImportProjectId(const xide_legacy_import* plan,size_t index);
const char* xideLegacyImportProjectSource(const xide_legacy_import* plan,size_t index);
int xideLegacyImportProjectSupported(const xide_legacy_import* plan,size_t index);
size_t xideLegacyImportEntryCount(const xide_legacy_import* plan,size_t index);
/* Candidate absolute paths belong to the plan. Choices resolve against original
   project directory. No implicit first/Open entry or startup. Failure is atomic. */
const char* xideLegacyImportEntryAt(const xide_legacy_import* plan,size_t index,size_t candidate);
/* An explicit external .xl choice adds a new reference without changing the old manifest. */
int xideLegacyImportSetEntry(xide_legacy_import* plan,size_t index,const char* path,xide_model_error* error);
int xideLegacyImportSetKind(xide_legacy_import* plan,size_t index,const char* kind,xide_model_error* error);
int xideLegacyImportSetStartup(xide_legacy_import* plan,const char* projectId,xide_model_error* error);
int xideLegacyImportReady(const xide_legacy_import* plan);
/* Preview strings belong to plan, invalidated by successful setters. Pending
   project previews are intentionally incomplete; unavailable projects are NULL. */
const char* xideLegacyImportWorkspaceJson(const xide_legacy_import* plan);
const char* xideLegacyImportProjectJson(const xide_legacy_import* plan,size_t index);
const char* xideLegacyImportReportJson(const xide_legacy_import* plan);
const char* xideLegacyImportSessionHintsJson(const xide_legacy_import* plan);
/* Review acknowledgement; source byte/identity recheck under read-only locks;
   one no-replace directory publication. No original/model/session writes. */
int xideLegacyImportCommit(xide_legacy_import* plan,int reviewed,xide_model_error* error);
#endif
