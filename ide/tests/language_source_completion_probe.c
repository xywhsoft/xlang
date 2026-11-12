/* The GUI fixture owns the exact controller TU for observation and one
 * explicit test-owned connection stop. No shipped test hooks. */
#include "ui/completion_view.c"
const char* xideTestCompletionSourceState(xide_app* app)
{
 static char text[512];xide_completion_view* v=app->completionView;
 snprintf(text,sizeof(text),"source=%llu drain=%d initialized=%d supported=%d wanted=%d pending=%d ready=%d decoding=%d count=%d request=%d result=%d state=%d",
  (unsigned long long)v->sourceToken,v->sourceDrain,v->initialized,v->supported,v->wanted,v->pending,v->ready,v->decoding,v->count,v->request,v->result!=NULL,xideLanguageClientState(v->client,NULL,0));return text;
}
void xideTestCompletionSourceStop(xide_app* app)
{xideLanguageClientRequestStop(app->completionView->client);}
