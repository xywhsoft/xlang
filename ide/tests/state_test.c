#include "platform/state_file.h"
#include "platform/path.h"
#include "editor/coordinates.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"state_test:%d: %s; %s\n",__LINE__,#e,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
int main(int argc,char** argv)
{
    int failed=0; char *root=NULL,*path=NULL; xide_state_file state={0}; xide_model_error error={0}; xvalue* value=NULL;
    CHECK(argc==2); char* id=xideCreateId("state"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id);
    CHECK(root && xrtDirCreateAll(root)); path=xidePathResolve(root,"状态.json"); CHECK(path);
    CHECK(xideStateOpen(&state,path,"test-1",&error) && !state.existed);
    value=xrtJsonParse(sv("{\"format\":\"test-1\",\"future\":{\"unknown\":[1,true,\"中\"]}}")); CHECK(value);
    CHECK(xideStateWrite(&state,value,&error) && state.existed); xrtValueRelease(value); value=NULL;
    xideStateClear(&state); CHECK(xideStateOpen(&state,path,"test-1",&error));
    value=xrtValueDeepClone(state.source); CHECK(value && xrtValueObjectSetNew(value,sv("version"),xrtValueInt(2)));
    CHECK(xideStateWrite(&state,value,&error) && xrtValueObjectGet(state.source,sv("future"))); xrtValueRelease(value); value=NULL;
    CHECK(writeText(path,"{\"format\":\"test-1\",\"external\":true}"));
    value=xrtValueDeepClone(state.source); CHECK(!xideStateWrite(&state,value,&error) && error.code==XIDE_MODEL_CONFLICT);
    xideStateClear(&state); xrtValueRelease(value); value=NULL;
    CHECK(writeText(path,"{\"format\":\"unsupported\"}")); CHECK(!xideStateOpen(&state,path,"test-1",&error) && !state.path);
    const unsigned char invalid[]={0xff,'{','}'}; CHECK(xrtFileWriteAtomic(path,(xbytesview){invalid,sizeof(invalid)})); CHECK(!xideStateOpen(&state,path,"test-1",&error));
    size_t length=0; unsigned char* raw=xrtFileReadAllLimit(path,32,&length); CHECK(raw && length==sizeof(invalid) && !memcmp(raw,invalid,length)); xrtFree(raw);
    CHECK(writeText(path,"\xef\xbb\xbf{\"format\":\"test-1\",\"bom\":true}") && xideStateOpen(&state,path,"test-1",&error));
    const char* unicode="A中😀\n\t字Z\n"; int line=0,column=0;
    xidePositionFromOffset(unicode,8,&line,&column); CHECK(line==1 && column==4 && xidePositionToOffset(unicode,1,4)==8);
    xidePositionFromOffset(unicode,6,&line,&column); CHECK(line==1 && column==3 && xidePositionToOffset(unicode,1,3)==4);
    CHECK(xidePositionToOffset(unicode,2,3)==13 && xidePositionToOffset(unicode,2,999)==14);
    CHECK(xidePositionToOffset(unicode,99,99)==(int)strlen(unicode));
    puts("state_test passed: atomic JSON baseline, unknown fields, external conflict, corrupt/unsupported preservation, BOM and Unicode code-point coordinates");
cleanup:
    xrtValueRelease(value); xideStateClear(&state); free(root); free(path); return failed;
}