#include "xl6_io_private.inc"

typedef struct xlir_io_lines { xlinereader* stream; xlineview view; bool hasLine; unsigned origin; xrtownershipref contextOwner; } xlir_io_lines;

XL6_TEXT_API void* xl6_xrt_io_lines_create(void* handle, int64_t limit)
{ xlir_io_reader* r = (xlir_io_reader*)handle; xlir_io_lines* lines; size_t max;xrtownershipscope mutation={0};
  if (!xlir_io_size(limit, &max) || xlir_io_reader_get(handle) == NULL) return NULL;
  lines = (xlir_io_lines*)xrtCalloc(1, sizeof(*lines)); if (lines == NULL) return NULL;
  if(!xrtOwnershipMutationBegin(&mutation))abort();
  lines->stream = xrtLineReaderTake(&r->stream, max);
  if(lines->stream){lines->origin=r->origin;lines->contextOwner=r->contextOwner;r->contextOwner=(xrtownershipref){0};}
  if(!xrtOwnershipScopeEnd(&mutation))abort();
  if (lines->stream == NULL) { xrtFree(lines); return NULL; }
  return lines; }

static bool xlir_io_lines_close(void* handle)
{
    xlir_io_lines* l=handle;xlinereader* stream;xrtownershipscope mutation={0};
    if(!l)return true;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    stream=l->stream;l->stream=NULL;l->hasLine=false;l->contextOwner=(xrtownershipref){0};
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return xrtLineReaderDestroy(stream);
}

XL6_TEXT_API void xl6_xrt_io_lines_destroy(void* handle)
{ xerror* error = xrtTakeError(); (void)xlir_io_lines_close(handle); xrtFree(handle); xrtClearError();
  if (error != NULL) { xrtSetErrorTake(error); } }

XL6_TEXT_API bool xl6_xrt_io_lines_close(void* handle) { return xlir_io_lines_close(handle); }

XL6_TEXT_API bool xl6_xrt_io_lines_advance(void* handle)
{ xlir_io_lines* l = (xlir_io_lines*)handle; xlinenext status;
  if (l == NULL || l->stream == NULL) { xrtSetErrorKind(XERR_STATE); return false; }
  l->hasLine = false; status = xrtLineReaderNext(l->stream, &l->view);
  l->hasLine = status == XLINE_NEXT_LINE; return l->hasLine; }

XL6_TEXT_API xvalue* xl6_xrt_io_lines_bytes(void* handle)
{ xlir_io_lines* l = (xlir_io_lines*)handle; xbytesview view;
  if (l == NULL || !l->hasLine) { xrtSetErrorKind(XERR_STATE); return NULL; }
  view.Data = (const unsigned char*)l->view.Text.Data; view.Size = l->view.Text.Size;
  return xrtValueBytes(view); }

XL6_TEXT_API int64_t xl6_xrt_io_lines_ending(void* handle)
{ xlir_io_lines* l = (xlir_io_lines*)handle;
  if (l == NULL || !l->hasLine) { xrtSetErrorKind(XERR_STATE); return -1; } return (int64_t)l->view.End; }
static bool linesTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    if(!xrtValueGetHandle(value,&raw,NULL,NULL) || !raw)return false;
    const xlir_io_lines* lines=raw;
    return xl6IoContextTrace(lines->origin,lines->stream,lines->contextOwner,visit,context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6IoLinesHandlePolicyV1Get,xl6_xrt_io_lines_destroy,linesTrace)
