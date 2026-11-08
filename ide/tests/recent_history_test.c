#include "platform/settings.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"recent_history_test:%d: %s (%s)\n",__LINE__,#e,error.message);failed=1;goto cleanup;}}while(0)
static int writeValue(const char* path,xvalue* value) {
    size_t size=0;char* text=xrtJsonStringify(value,0,&size);
    int okay=text && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,size});xrtFree(text);return okay;
}
int main(int argc,char** argv) {
    if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;
    int failed=0;xide_settings settings={0},loaded={0};xide_model_error error={0};xvalue* legacy=NULL;char* path=NULL;char* last=NULL;
    char* id=xideCreateId("history");char* root=id?xidePathResolve(argv[1],id):NULL;free(id);
    CHECK(root && xrtDirCreateAll(root));path=xidePathResolve(root,"settings.json");CHECK(path && xideSettingsLoad(&settings,path,&error));
    for(int i=0;i<105;++i) {
        char name[128];snprintf(name,sizeof(name),"中文 文件-%03d.%s",i,i%3==0?"xlw":i%3==1?"xlp":"xl");
        free(last);last=xidePathResolve(root,name);CHECK(last && xideSettingsRememberOpen(&settings,last,&error));
        CHECK(xideSettingsRecentCount(&settings)==(size_t)(i<100?i+1:100));
    }
    CHECK(xideSettingsLoad(&loaded,path,&error) && xideSettingsRecentCount(&loaded)==100);
    CHECK(!strcmp(xideSettingsRecentPath(&loaded,0),last) && xideSettingsRecentTime(&loaded,0)>0 && !xideSettingsRecentPath(&loaded,100));
    const char* tail=xideSettingsRecentPath(&settings,99);char* promoted=xidePathResolve(NULL,tail);CHECK(promoted);
    int promotedOkay=xideSettingsRememberOpen(&settings,promoted,&error) && xideSettingsRecentCount(&settings)==100 && xidePathEqual(promoted,xideSettingsRecentPath(&settings,0));free(promoted);CHECK(promotedOkay);
    CHECK(xideSettingsRememberOpen(&settings,last,&error) && xideSettingsRecentCount(&settings)==100);
    /* Old releases stored strings and only 12 entries; import both shapes,
       bound oversized files on the next write, and keep unrelated settings. */
    legacy=xrtValueObject();xvalue* list=xrtValueArray();CHECK(legacy && list);
    CHECK(xrtValueObjectSetNew(legacy,(xstrview){"format",6},xrtValueString((xstrview){"xlang-ide-settings-1",20})) && xrtValueObjectSetNew(legacy,(xstrview){"future",6},xrtValueInt(7)));
    for(int i=0;i<110;++i) {char item[128];snprintf(item,sizeof(item),"D:/history/legacy-%d.xl",i);CHECK(xrtValueArrayAppendNew(list,xrtValueString((xstrview){item,strlen(item)})));}
    CHECK(xrtValueObjectSetNew(legacy,(xstrview){"recent",6},list) && writeValue(path,legacy));
    CHECK(xideSettingsLoad(&settings,path,&error) && xideSettingsRecentCount(&settings)==100 && xideSettingsRecentTime(&settings,0)==0);
    CHECK(xideSettingsRememberOpen(&settings,last,&error) && xideSettingsLoad(&loaded,path,&error));
    CHECK(xrtValueCount(xrtValueObjectGet(loaded.source,(xstrview){"recent",6}))==100 && xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
    xvalue* prior=xrtValueRetain(settings.source);
    int changed=writeValue(path,legacy) && !xideSettingsClearRecent(&settings,&error) && error.code==XIDE_MODEL_CONFLICT && settings.source==prior;
    xrtValueRelease(prior);CHECK(changed);
    CHECK(xideSettingsLoad(&settings,path,&error) && xideSettingsClearRecent(&settings,&error) && xideSettingsLoad(&loaded,path,&error));
    CHECK(xideSettingsRecentCount(&loaded)==0 && xrtValueObjectGet(loaded.source,(xstrview){"future",6}));
    printf("recent_history_test passed: %d checks; MRU 100, dedupe, timestamps, legacy import, bounded disk, conflict preservation and clear/reload\n",checks);
cleanup:
    xrtValueRelease(legacy);xideSettingsClear(&settings);xideSettingsClear(&loaded);free(root);free(path);free(last);return failed;
}
