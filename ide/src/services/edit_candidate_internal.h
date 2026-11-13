#ifndef XIDE_EDIT_CANDIDATE_INTERNAL_H
#define XIDE_EDIT_CANDIDATE_INTERNAL_H
#include "services/edit_candidate.h"
#include "services/language_input_owned.h"
size_t xideEditCandidateSourceCount(const xide_edit_candidate*);
size_t xideEditCandidateStorage(const xide_edit_candidate*);
const xvalue* xideEditCandidateSourceAt(const xide_edit_candidate*,size_t);
const xvalue* xideEditCandidateSource(const xide_edit_candidate*,const char*);
/* Private native transaction adapter: original reports may only be substituted
 * after that host has independently pinned/verified the complete source set. */
int xideEditCandidateValidateOwned(xide_edit_candidate*,const xvalue* const*,size_t,int (*cancelled)(void*),void*,xide_model_error*);
int xideRenameEvidenceValidateOwned(xide_rename_evidence*,const xvalue* const*,size_t,int (*cancelled)(void*),void*,xide_model_error*);
#endif
