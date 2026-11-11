#include "foreign_pack_clip.h"
#include <string.h>
void packClipHeap(size_t* count,size_t* bytes)
{xmemdebugsnapshot info;memset(&info,0,sizeof(info));xrtMemDebugSnapshot(&info);*count=info.LiveCount;*bytes=info.LiveBytes;}
