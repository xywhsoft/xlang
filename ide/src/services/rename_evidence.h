#ifndef XIDE_RENAME_EVIDENCE_H
#define XIDE_RENAME_EVIDENCE_H
#include "services/rename_scope.h"
#include "services/language_check.h"
typedef struct xide_rename_evidence xide_rename_evidence;
/* Worker-only complete source/configuration COVERAGE admission, not semantic
 * recomputation or permission to edit. Bind every provider to independent local
 * scope, import all guards against exact request overlays, prove every program
 * entry and every explicit .xl member under ALL of its scope roles. Reject
 * incompatible same-file reports even from separate accepted providers.
 * Max65536 guards/source rows and7MiB proof wire. Default/max256MiB known owned
 * scope/report/key/row charge; DOM/map/allocator/codec and shared input text are
 * additional. Batch source validation reads/hashes identical reports once per
 * operation; each later Validate starts fresh. Keep inputs stable throughout.
 * Result owns detached guards/local scope/keys, with no borrowed input DOM. */
xide_rename_evidence* xideRenameEvidenceCreate(xide_rename_scope*,const xvalue* guards,
 const xide_language_proof_overlay*,size_t,size_t budget,int (*cancelled)(void*),void*,xide_model_error*);
void xideRenameEvidenceRelease(xide_rename_evidence*);
size_t xideRenameEvidenceBytes(const xide_rename_evidence*);
size_t xideRenameEvidenceFiles(const xide_rename_evidence*);
const xvalue* xideRenameEvidenceFileAt(const xide_rename_evidence*,size_t);
/* Borrow exact imported report, plus union of compatible checked provider
 * roles for that file. This may include readonly/unconfigured dependencies;
 * caller must separately require explicit target membership and source origin.
 * Path conversion may allocate, never reads disk. */
const xvalue* xideRenameEvidenceFile(const xide_rename_evidence*,const char* path,uint64_t* providers);
/* 1 touched, 0 unrelated, -1 invalid input/path allocation failure. Treat every
 * nonzero result as requiring rejection for an unrepresented editor model. */
int xideRenameEvidenceTouches(const xide_rename_evidence*,const char* path);
/* Worker-only full fresh validation. Owner uses Current atomic flags only,
 * together with complete original request/model/configuration publication gates.
 * Still not target pinning, physical alias policy or transaction approval. */
int xideRenameEvidenceValidate(xide_rename_evidence*,int (*cancelled)(void*),void*,xide_model_error*);
int xideRenameEvidenceCurrent(const xide_rename_evidence*);
#endif
