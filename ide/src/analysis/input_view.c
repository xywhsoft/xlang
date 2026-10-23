#include "analysis/frontend_input_view.h"
#include <string.h>
int xlsInputReportInputs(const XL6_SourceReport* report,xls_inputs* out)
{
    if(out) memset(out,0,sizeof(*out));
    if(!report || !out) return 0;
    *out=(xls_inputs){report->count,report->probeCount,report->packagePath,report->packagePathObserved};return 1;
}
int xlsInputReportSource(const XL6_SourceReport* report,size_t index,xls_input_source* out)
{
    if(out) memset(out,0,sizeof(*out));
    if(!report || !out || index>=report->count) return 0;
    const XL6_SourceReportItem* source=&report->items[index];
    out->path=source->path;out->text=source->text;out->origin=source->origin;
    out->size=source->size;out->rawSize=source->rawSize;out->kind=source->kind;
    memcpy(out->textSha256,source->textSha256,32);memcpy(out->rawSha256,source->rawSha256,32);return 1;
}
int xlsInputReportProbe(const XL6_SourceReport* report,size_t index,xls_input_probe* out)
{
    if(out) memset(out,0,sizeof(*out));
    if(!report || !out || index>=report->probeCount) return 0;
    *out=(xls_input_probe){report->probes[index].path,report->probes[index].exists};return 1;
}
