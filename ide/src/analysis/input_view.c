#include "analysis/frontend_input_view.h"
#include "analysis/frontend_input_usage.h"
#include "analysis/input_topology.h"
#include "src/compile/xl6_source_overlay.h"
#include <string.h>
static _Thread_local xls_input_usage_scope* activeUsage;
void xlsFrontendInputUsageBegin(xls_input_usage_scope* scope)
{if(scope){*scope=(xls_input_usage_scope){.previous=activeUsage};activeUsage=scope;}}
void xlsFrontendInputUsageBind(const struct XL6_SourceOverlay* overlay,size_t count)
{
    xlsInputTopologyBind(overlay,count);
    if(!activeUsage)return;
    if(activeUsage->bound || !overlay || count>64){activeUsage->failed=1;return;}
    activeUsage->overlay=overlay;activeUsage->count=count;activeUsage->bound=1;
}
void xlsFrontendInputUsageRead(const struct XL6_SourceOverlay* overlay,size_t index)
{
    if(!activeUsage || !activeUsage->bound || activeUsage->overlay!=overlay)return;
    if(index>=activeUsage->count){activeUsage->failed=1;return;}
    activeUsage->used|=UINT64_C(1)<<index;
}
int xlsFrontendInputUsageFailure(const struct XL6_SourceOverlay* overlay)
{
    xlsInputTopologyFailure(overlay);
    if(activeUsage && activeUsage->bound && (!overlay || activeUsage->overlay==overlay))activeUsage->failed=1;
    return 0;
}
void xlsFrontendInputUsageReportFailure(const struct XL6_SourceReport* report)
{
    xlsInputTopologyReportFailure(report);
    if(report && activeUsage && activeUsage->bound && activeUsage->overlay && activeUsage->overlay->report==report)activeUsage->failed=1;
}
int xlsFrontendInputUsageEnd(xls_input_usage_scope* scope,uint64_t* used)
{
    if(used)*used=0;
    if(!scope || activeUsage!=scope)return 0;
    int complete=scope->bound && !scope->failed;if(complete && used)*used=scope->used;
    activeUsage=scope->previous;return complete;
}
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
