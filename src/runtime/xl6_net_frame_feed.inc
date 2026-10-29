/* Each family supplies FRAME_NAME and FRAME_INPUT(raw). No Cgen expansion.
 * Only append/move-in are exposed while an incremental cursor is retained. */
#define FRAME_JOIN_(A,B) A##B
#define FRAME_JOIN(A,B) FRAME_JOIN_(A,B)
#define FRAME_FN(Suffix) FRAME_JOIN(FRAME_NAME,Suffix)
#if defined(FRAME_FEED_VALUE)
void FRAME_FN(AppendTextV1)(void* raw,xstrview input,int64_t size)
{xl6NetBufferAppendTextV1(FRAME_INPUT(raw),input,size);}
void FRAME_FN(AppendBytesV1)(void* raw,const xvalue* input,int64_t size)
{xl6NetBufferAppendBytesV1(FRAME_INPUT(raw),input,size);}
#elif defined(FRAME_FEED_BUFFER)
void FRAME_FN(AppendBufferV1)(void* raw,void* input,int64_t size)
{xl6NetBufferAppendBufferV1(FRAME_INPUT(raw),input,size);}
#else
void FRAME_FN(AppendMoveV1)(void* raw,void* input)
{xl6NetBufferMoveInputV1(FRAME_INPUT(raw),input);}
int64_t FRAME_FN(BufferedV1)(void* raw){return xl6NetBufferLengthV1(FRAME_INPUT(raw));}
int64_t FRAME_FN(LimitV1)(const void* raw){return xl6NetBufferLimitV1(FRAME_INPUT(raw));}
bool FRAME_FN(FailedV1)(void* raw)
{XL6_NetBufferV1* b=FRAME_INPUT(raw);XL6_NetBufferGuardV1 g={0};if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return false;}bool failed=b->faulted;xl6NetBufferLeaveV1(&g);return failed;}
#endif
#undef FRAME_FN
#undef FRAME_JOIN
#undef FRAME_JOIN_
#undef FRAME_NAME
#undef FRAME_INPUT
#undef FRAME_FEED_VALUE
#undef FRAME_FEED_BUFFER
