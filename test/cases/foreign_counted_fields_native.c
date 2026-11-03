#include "foreign_native_fields_native.c"
#include "foreign_counted_fields.h"
int counted_fire(ManagedRoot* root,int mode)
{
    ManagedCallback fn=root->callback;void* data=root->userdata;
    ManagedCallbackOwner owner=root->callbackOwner;
    if(!fn)return -99;
    if(owner.Hold && !owner.Hold(owner.reference.Data))return -98;
    static const unsigned char bytes[]={0,255,0,3};
    ManagedChild event={0};event.root=root;event.value=mode;
    event.countedText[0]='a';event.countedText[1]=0;event.countedText[2]='b';
    event.countedTextSize=mode==1 ? -1 : mode==2 ? 9 : mode==5 ? 0 : 3;
    event.countedData=mode==3 || mode==4 ? NULL : bytes;
    event.countedDataSize=mode==3 ? 0 : mode==4 ? 1 : mode==6 ? SIZE_MAX : 4;
    int result=fn(&event,mode,data);
    if(owner.Drop && !owner.Drop(owner.reference.Data))return -97;
    return result;
}
