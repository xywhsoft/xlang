#include "test/cases/foreign_xge3_document_fragment_binary.h"
#include <assert.h>
#include <string.h>
static char* wire;
static uint64_t wireSize;
xbytesview document_fragment_binary_buffer(xui_document_fragment fragment)
{
    assert(!wire);
    assert(xuiDocumentFragmentSerialize(fragment,&wire,&wireSize)==XUI_OK);
    assert(wire && wireSize>16 && wireSize<=SIZE_MAX);
    assert(!memcmp(wire,"XUIFRAG1",8) && !memcmp(wire+9,"\0\0\0\0\0\0\0",7));
    return (xbytesview){(const unsigned char*)wire,(size_t)wireSize};
}
void document_fragment_binary_free(void)
{
    assert(wire);memset(wire,0xdd,(size_t)wireSize);
    xuiDocumentFreeBuffer(wire);wire=NULL;wireSize=0;
}
int document_fragment_binary_verify(xui_document_fragment fragment)
{
    char* html=NULL;uint64_t count=0;
    assert(xuiDocumentFragmentExportHtml(fragment,&html,&count)==XUI_OK);
    int result=count && html && strstr(html,"binary")!=NULL;
    xuiDocumentFreeBuffer(html);return result;
}
