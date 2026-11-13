#ifndef XIDE_LANGUAGE_INPUT_OWNED_H
#define XIDE_LANGUAGE_INPUT_OWNED_H
#include "services/language_check.h"
/* Private worker adapter, NOT a wire admission API. The host must already own
 * independently verified, physically pinned sources for every supplied original
 * disk report, and keep them stable for the call. Seed exact reports only into
 * this one validation memo; all other files, resolution probes and environment
 * still validate normally. Same native name may differ only in path spelling.
 * No pin, source coverage, permission or write authorization is created here. */
int xideLanguageInputGuardsValidateOwned(xide_language_input_guard* const*,size_t,
 const xvalue* const* originalReports,size_t,int (*cancelled)(void*),void*,xide_model_error*);
/* Metadata only, no I/O; includes original raw length, full available mask,
 * attributes, device/identity and exact schema1/2 modification/change times. */
int xideLanguageInputFileMatchesInfo(const xvalue*,const xfileinfo*);
#endif
