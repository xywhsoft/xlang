#ifndef XLANG_ANALYSIS_FRONTEND_OVERLAY_H
#define XLANG_ANALYSIS_FRONTEND_OVERLAY_H
#include "analysis/input_batch.h"
#include "src/compile/xl6_source_overlay.h"
#include "analysis/frontend_overlay_lookup.h"
/* Private session path. batch must stay retained until Clear. Only opaque,
 * already validated immutable text is borrowed; normalized paths stay owned.
 * The original loader/report/parser still observes every actual source and
 * candidate. This is storage ownership, not an input-freshness proof.
 * Returns 1 on success, 0 on invalid input/allocation failure, -1 on cancel. */
int xlsFrontendOverlayBorrow(XL6_SourceOverlay* overlay,const xls_input_batch* batch,
    int (*cancelled)(void*),void* user);
/* Every item in a borrowed overlay has borrowed text, even on partial failure.
 * Detach those pointers before the original destructor frees owned paths. */
void xlsFrontendOverlayClear(XL6_SourceOverlay* overlay,const xls_input_batch* owner);
#endif
