#ifndef XIDE_RENAME_CANDIDATE_H
#define XIDE_RENAME_CANDIDATE_H
#include "services/project_references.h"
#ifdef XLS_COMPILER_XRT
#define XIDE_NATIVE_RENAME_METHOD "xlang/renameCandidate"
typedef struct xide_rename_candidates xide_rename_candidates;
typedef struct xide_rename_candidate_query xide_rename_candidate_query;
/* Borrow the existing reference coordinator. It outlives this service. One
 * bounded orchestration worker, four queued tasks. Metadata preparation,
 * complete native checks, proof export and JSON framing stay off the owner.
 * Quiescence includes physical task/finalizer completion, not future state. */
xide_rename_candidates* xideRenameCandidatesCreate(xide_project_references*);
void xideRenameCandidatesDestroy(xide_rename_candidates*);
int xideRenameCandidatesIdle(xide_rename_candidates*);
/* Immutable decoder/catalog nodes and text references are retained. Labels
 * alone are copied (at most2MiB); no source-body copy/hash, disk or compiler
 * work on submit. expectedCatalog is the independently acknowledged packet,
 * never the candidate's own claim. Positive int request IDs only.
 * Caller excludes mutation of retained DOM and keeps the service alive.
 * Owner must gate document identity, input serial/config epoch and queued
 * input before publishing, and cancel on lease/shutdown/transport changes. */
xide_rename_candidate_query* xideRenameCandidateSubmit(xide_rename_candidates*,int id,
 const xvalue* params,const xvalue* expectedCatalog,xide_analysis_catalog*,
 const xls_analysis_document*,size_t);
/* Standard read-only rename proposal. Typed int/string ID, caller-captured
 * catalog (NULL only for an explicitly standalone session), and roster. No
 * receipt/proof extension on the wire; same complete native plan/guard gates.
 * Caller rejects unknown workspace roots without explicit configuration. */
xide_rename_candidate_query* xideRenameStandardSubmit(xide_rename_candidates*,
 const xvalue* id,int documentChanges,const xvalue* params,xide_analysis_catalog*,
 const xls_analysis_document*,size_t);
int xideRenameCandidateSetWake(xide_rename_candidate_query*,void (*wake)(void*),void*);
void xideRenameCandidateCancel(xide_rename_candidate_query*);
void xideRenameCandidateDestroy(xide_rename_candidate_query*);
/* 0 pending,1 complete,-1 failed/cancelled. code is a standard LSP error.
 * A success is an already framed immutable owned response, not edit approval.
 * Take transfers its single malloc buffer; free() after writing. Query owner
 * excludes concurrent Take/Destroy. DOM/native input teardown is on worker. */
int xideRenameCandidateResult(xide_rename_candidate_query*,int* code,char* error,size_t);
char* xideRenameCandidateTakeFrame(xide_rename_candidate_query*,size_t*);
#endif
#endif
