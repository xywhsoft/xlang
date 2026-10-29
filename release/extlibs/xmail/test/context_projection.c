#include <xrt.h>
#include <xl_foreign_net.h>
#include <string.h>
static bool inspect(xnetengine* engine,const xtlsclientconfig* config,void* ignored)
{
    (void)ignored;
    return engine&&config&&config->Context&&config->Verifier&&config->ProtocolCount==1&&
        config->Protocols[0].Size==5&&memcmp(config->Protocols[0].Data,"xlang",5)==0;
}
bool mail_context_probe(xvalue* engine,xvalue* context)
{return xl6ForeignNetClientVisitV1(engine,context,inspect,NULL);}
