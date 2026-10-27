#include "analysis/frontend_overlay.h"
#include <xrt.h>
#include <string.h>

int xlsFrontendOverlayBorrow(XL6_SourceOverlay* overlay,const xls_input_batch* batch,
    int (*cancelled)(void*),void* user)
{
    if(!overlay || !batch || overlay->count || overlay->bytes)return 0;
    xls_semantic_input inputs[64];size_t count=xlsInputBatchInputs(batch,inputs,64);
    for(size_t i=0;i<count;++i) {
        if(cancelled && cancelled(user))return -1;
        const xls_semantic_input* in=&inputs[i];size_t bom=0;
        /* Match original absolute-path/duplicate/byte limits. Text is a
         * retained xls_text: strict UTF-8, no embedded NUL, trailing NUL.
         * Public raw-input constructors still use the original copying Set. */
        if(!in->path || !xrtPathIsAbs(in->path) || !in->text ||
           in->size>XL6_SOURCE_OVERLAY_BYTES || xl6SourceOverlayFind(overlay,in->path))return 0;
        if(in->size>=3 && !memcmp(in->text,"\xef\xbb\xbf",3))bom=3;
        size_t size=in->size-bom;
        if(size>XL6_SOURCE_OVERLAY_TOTAL-overlay->bytes)return 0;
        char* path=xrtPathAbs(in->path);if(!path)return 0;
        overlay->items[overlay->count++]=(XL6_SourceOverlayItem){path,(char*)in->text+bom,size};
        overlay->bytes+=size;
    }
    return 1;
}
void xlsFrontendOverlayClear(XL6_SourceOverlay* overlay,const xls_input_batch* owner)
{
    if(owner && overlay)for(size_t i=0;i<overlay->count;++i)overlay->items[i].text=NULL;
    xl6SourceOverlayClear(overlay);
}
