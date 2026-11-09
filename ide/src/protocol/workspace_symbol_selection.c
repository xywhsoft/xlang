#include "protocol/workspace_symbol_selection.h"
#include <string.h>
#include <limits.h>
static const xvalue* field(const xvalue* o,const char* key)
{return xrtValueType(o)==XVALUE_OBJECT?xrtValueObjectGet(o,xrtStrView(key)):NULL;}
static int text(const xvalue* v)
{xstrview s={0};return xrtValueType(v)==XVALUE_STRING && xrtValueGetString(v,&s) && s.Size && s.Size<=262144 && !memchr(s.Data,0,s.Size) && xrtUtf8Valid(s,NULL);}
static int integer(const xvalue* v,int* out)
{int64 n=0;if(xrtValueType(v)!=XVALUE_INT || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX)return 0;*out=(int)n;return 1;}
static int position(const xvalue* v,int* line,int* character)
{return xrtValueType(v)==XVALUE_OBJECT && xrtValueCount(v)==2 && integer(field(v,"line"),line) && integer(field(v,"character"),character);}
int xideWorkspaceSymbolSelectionValid(const xvalue* v)
{
 int kind=0,sl=0,sc=0,el=0,ec=0;const xvalue *container=field(v,"containerName"),*location=field(v,"location"),*range=field(location,"range");
 return xrtValueType(v)==XVALUE_OBJECT && xrtValueCount(v)==(container?4u:3u) && text(field(v,"name")) && integer(field(v,"kind"),&kind) && kind>=1 && kind<=26 && (!container || text(container)) && xrtValueType(location)==XVALUE_OBJECT && xrtValueCount(location)==2 && text(field(location,"uri")) && xrtValueType(range)==XVALUE_OBJECT && xrtValueCount(range)==2 && position(field(range,"start"),&sl,&sc) && position(field(range,"end"),&el,&ec) && (el>sl || (el==sl && ec>sc));
}
