#include "binding_perf.h"
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <assert.h>
#include <stdio.h>

/* Handwritten equivalent checked operations deliberately retain the same
 * error, view, native scope and retained-result admission contracts. There is
 * no reflection, opcode dispatcher, per-element boxing or Native C adapter. */
static xui_proxy_caps_t checkedCaps(XL6_ForeignView* proxy)
{
 xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
 XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(proxy,&call,NULL,0));
 xui_proxy native=call.raw;assert(native && native->getCaps);
 xui_proxy_caps_t caps={0};assert(native->getCaps(native,&caps)==XUI_OK);
 assert(xl6ForeignViewCallEndV1(&call));
 xl6_error_result result=xl6ErrorScopeEndAtSafePointV1(&errors);assert(result.ok && !result.error);
 return caps;
}
static void checkedRect(XL6_ForeignView* proxy,XL6_ForeignView* draw,xui_rect_t rect,uint32_t color)
{
 xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
 XL6_ForeignViewCallV1 p={0},d={0};xui_draw_scope_t scope={0};
 assert(xl6ForeignViewCallBeginV1(proxy,&p,NULL,0));
 assert(xl6ForeignViewCallBeginV1(draw,&d,&scope,sizeof(scope)));
 xui_proxy native=p.raw;assert(native && native->drawRectFill);
 assert(native->drawRectFill(native,d.raw,rect,color)==XUI_OK);
 assert(xl6ForeignViewCallEndV1(&d));assert(xl6ForeignViewCallEndV1(&p));
 xl6_error_result result=xl6ErrorScopeEndAtSafePointV1(&errors);assert(result.ok && !result.error);
}
static void checkedTarget(XL6_ForeignView* draw)
{
 xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
 XL6_ForeignViewCallV1 call={0};xui_draw_scope_t scope={0};
 assert(xl6ForeignViewCallBeginV1(draw,&call,&scope,sizeof(scope)));
 XL6_ForeignResultProbeV1 probe={0};
 assert(xl6ForeignResultProbeReserveRootV1(&probe,&xl6Xui3SurfacePolicy,&xl6Xui3OwnershipDomainV1,NULL,0,1,0));
 xui_surface target=xuiDrawGetTarget(call.raw);assert(target && xuiSurfaceAddRef(target)==XUI_OK);
 assert(xl6ForeignResultProbeObserveV1(&probe,target));
 XL6_ForeignView* result=xl6ForeignResultProbeMaterializeRootV1(&probe);assert(result);
 assert(xl6ForeignResultProbeEndV1(&probe));assert(xl6ForeignViewCallEndV1(&call));
 xl6_error_result boundary=xl6ErrorScopeEndAtSafePointV1(&errors);assert(boundary.ok && !boundary.error);
 assert(xl6ErrorScopeBegin(&errors));assert(xl6ForeignViewReleaseV1(result));
 boundary=xl6ErrorScopeEndAtSafePointV1(&errors);assert(boundary.ok && !boundary.error);
}

int main(void)
{
 binding_perf_counter_begin();assert(xl6Xge3RuntimeBindV1()==0);binding_perf_counter_end(-1);
 binding_perf_counter_begin();
 xui_proxy proxy=NULL;xui_surface surface=NULL;xui_draw_context draw=NULL;
 assert(binding_perf_proxy(&proxy)==XUI_OK);
 assert(binding_perf_surface(proxy,&surface)==XUI_OK);
 assert(binding_perf_draw(proxy,surface,&draw)==XUI_OK);
#ifdef BINDING_PERF_SAFE
 /* Cold setup is intentionally outside the timed regions. It is not claimed
  * equivalent to generated factory pre-reservation and gets no cold score. */
 XL6_ForeignView* pv=xl6ForeignViewCreateRootWithDomainV1(proxy,&xl6Xui3ProxyPolicy,&xl6Xui3OwnershipDomainV1,NULL);
 XL6_ForeignView* sv=xl6ForeignViewCreateRootWithDomainV1(surface,&xl6Xui3SurfacePolicy,&xl6Xui3OwnershipDomainV1,NULL);
 XL6_ForeignView* dv=xl6ForeignViewCreateRootWithDomainV1(draw,&xl6Xui3DrawPolicy,&xl6Xui3OwnershipDomainV1,NULL);
 assert(pv && sv && dv);
#endif
 binding_perf_counter_end(0);binding_perf_counter_begin();
 uint64_t sum=0,start=binding_perf_clock();
 for(int64_t i=0;i<500000;++i){
#ifdef BINDING_PERF_SAFE
  xui_proxy_caps_t caps=checkedCaps(pv);
#else
  xui_proxy_caps_t caps={0};assert(proxy->getCaps(proxy,&caps)==XUI_OK);
#endif
  sum+=(uint64_t)caps.iSurfaceFormat;
 }
 unsigned long long elapsed=binding_perf_clock()-start;binding_perf_counter_end(1);
 printf("caps %llu %llu %llu\n",elapsed,binding_perf_frequency(),(unsigned long long)sum);
 binding_perf_counter_begin();
 xui_rect_t rect={3,0,10,10};start=binding_perf_clock();
 for(int64_t i=0;i<500000;++i){
#ifdef BINDING_PERF_SAFE
  checkedRect(pv,dv,rect,7);
#else
  assert(proxy->drawRectFill(proxy,draw,rect,7)==XUI_OK);
#endif
 }
 elapsed=binding_perf_clock()-start;binding_perf_counter_end(2);
 printf("draw %llu %llu %llu\n",elapsed,binding_perf_frequency(),binding_perf_checksum());
 binding_perf_counter_begin();
 start=binding_perf_clock();
 for(int64_t i=0;i<50000;++i){
#ifdef BINDING_PERF_SAFE
  checkedTarget(dv);
#else
  xui_surface retained=xuiDrawGetTarget(draw);assert(retained && xuiSurfaceAddRef(retained)==XUI_OK);
  assert(xuiSurfaceReleaseChecked(retained)==XUI_OK);
#endif
 }
 elapsed=binding_perf_clock()-start;binding_perf_counter_end(3);
 printf("target %llu %llu\n",elapsed,binding_perf_frequency());
 binding_perf_counter_begin();
#ifdef BINDING_PERF_SAFE
 assert(xl6ForeignViewCloseV1(dv));assert(xl6ForeignViewReleaseV1(dv));
 assert(xl6ForeignViewCloseV1(sv));assert(xl6ForeignViewReleaseV1(sv));
 assert(xl6ForeignViewCloseV1(pv));assert(xl6ForeignViewReleaseV1(pv));
 assert(xl6ForeignViewDrainV1());
#else
 assert(xuiDrawEndChecked(draw)==XUI_OK && xuiDrawReleaseChecked(draw)==XUI_OK);
 assert(xuiSurfaceReleaseChecked(surface)==XUI_OK && xuiProxyReleaseChecked(proxy)==XUI_OK);
#endif
 binding_perf_counter_end(4);
 assert(binding_perf_verify());binding_perf_counter_end(5);return 0;
}
