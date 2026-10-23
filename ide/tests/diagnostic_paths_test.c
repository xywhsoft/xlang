#include "protocol/diagnostic_paths.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"diagnostic_paths_test:%d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(void)
{
 char digest[65];xvalue* paths=xrtValueArray();CHECK(paths);
 /* Fixed vectors are independently encoded with Python hashlib/struct. */
 CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,"e3d55c9066bce93a2936028873706e9f77d091b1c2bc26898cd070bfb0a55408"));
 xvalue* first=xrtValueString(xrtStrView("D:/dep.xl"));xvalue* second=xrtValueString(xrtStrView("D:/工程😀/main.xl"));
 CHECK(first && second && xrtValueArrayAppend(paths,first) && xrtValueArrayAppend(paths,second));
 CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,"1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee"));
 CHECK(xrtValueArraySet(paths,0,second) && xrtValueArraySet(paths,1,first));
 CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,"b4d59ba6f41c494064c622772a2fc2eda5e7d2deb299f43ca1d78486160b8a77"));
 xrtValueRelease(paths);paths=xrtValueArray();CHECK(paths);
 char* large=xrtMalloc(65537);CHECK(large);memset(large,'x',65536);large[65536]=0;
 xvalue* value=xrtValueString((xstrview){large,65535});CHECK(value && xrtValueArrayAppend(paths,value));xrtValueRelease(value);
 CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,"22431be162ae0d2ee4ba9ee13966248155d8c3d8ccbad127e94a64f2aa02d2a0"));
 value=xrtValueString((xstrview){large,65536});CHECK(value && xrtValueArraySet(paths,0,value));xrtValueRelease(value);
 CHECK(!xlsDiagnosticPathsDigest(paths,digest) && !*digest);xrtFree(large);
 const char* invalid[]={"","bad\xff","a\0b"};size_t sizes[]={0,4,3};
 for(size_t i=0;i<3;++i) {value=xrtValueString((xstrview){invalid[i],sizes[i]});CHECK(value && xrtValueArraySet(paths,0,value));xrtValueRelease(value);CHECK(!xlsDiagnosticPathsDigest(paths,digest) && !*digest);}
 value=xrtValueInt(3);CHECK(value && xrtValueArraySet(paths,0,value));xrtValueRelease(value);
 CHECK(!xlsDiagnosticPathsDigest(paths,digest) && !*digest && !xlsDiagnosticPathsDigest(paths,NULL));
 xrtValueRelease(paths);paths=xrtValueArray();value=xrtValueString(xrtStrView("x"));CHECK(paths && value);
 for(size_t i=0;i<163840;++i) CHECK(xrtValueArrayAppend(paths,value));
 CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,"ff6741046448fcfe12d58c78ea79f27d8525ea34cb8162ce69ee0e42185a1654"));
 CHECK(xrtValueArrayAppend(paths,value) && !xlsDiagnosticPathsDigest(paths,digest) && !*digest);
 CHECK(!xlsDiagnosticPathsDigest(value,digest) && !*digest && !xlsDiagnosticPathsDigest(NULL,digest) && !*digest);
 xrtValueRelease(paths);xrtValueRelease(value);xrtValueRelease(first);xrtValueRelease(second);
 printf("diagnostic_paths_test passed: %u checks; independent binary digest vectors, Unicode/order, empty/length/count/type/encoding rejection and failure output.\n",checks);return 0;
}
