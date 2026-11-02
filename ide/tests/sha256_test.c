#include "services/catalog_sources.h"
#include "platform/file_rename.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixed SHA-256 oracles generated independently with Python hashlib.
   Cover padding and the production file reader's 64 KiB block boundaries. */
static const struct { size_t size; const char* digest; } vectors[]={
 {0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
 {55,"2900465fcb533e05a158fd2b3be0e5e3b03740d83060aa3580e0d98a96bf2384"},
 {56,"31454ff48ef36af2f08fd511bdc37d9d5855ac23e992e5ff5445cb6b7674a674"},
 {63,"5f6401b96532c36de4e65beec0409b69b1d181864c8009b7a04f43e5d56350d1"},
 {64,"94eb5de4943613fd048dc93393ab06877405faa39c11f53e9386083339833e7e"},
 {65,"fc518669b6eb4b4dd91827ecacef86689c725bd5bab888fd3b26dbb196eec954"},
 {65535,"11cad04d25d7e2ae2bde1131397c1ee623dcf0731e3e123f278fa192a9d72976"},
 {65536,"6fc179cfd193754e6109ad043f56d146c7e7d7c3623ffceae318266286f58388"},
 {65537,"3123afca912e156e19bcd2055eb92e944cf6e6fdb0eef16d65464342ad1f5a1a"},
 {131089,"9bbbaa6605336d891b545768586e36ca9f13952910fdedd07a5216c38d8a847e"}
};
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"sha256_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
int main(int argc,char** argv)
{
 int failed=0,checks=0; xide_model_error error={0}; char *path=NULL,*id=NULL;
 unsigned char* data=NULL; char digest[65],streamed[65];
 CHECK(argc==2);
 id=xideCreateId("sha256"); CHECK(id); path=xidePathResolve(argv[1],id); CHECK(path); free(id); id=NULL;
 data=malloc(131089); CHECK(data);
 for(size_t i=0;i<131089;++i) data[i]=(unsigned char)(i*37+11);
 CHECK(xideCatalogDigest(NULL,0,digest) && !strcmp(digest,vectors[0].digest));
 CHECK(!xideCatalogDigest(NULL,1,digest) && !xideCatalogDigest(data,1,NULL));
 CHECK(xideCatalogDigest("abc",3,digest) && !strcmp(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 for(size_t i=0;i<sizeof(vectors)/sizeof(vectors[0]);++i) {
  CHECK(xideCatalogDigest(data,vectors[i].size,digest) && !strcmp(digest,vectors[i].digest));
  CHECK(xrtFileWriteAtomic(path,(xbytesview){data,vectors[i].size}));
  xide_file_snapshot snapshot={0}; CHECK(xideFileSnapshotInspect(path,&snapshot,&error) && snapshot.length==vectors[i].size);
  for(size_t j=0;j<32;++j) snprintf(streamed+j*2,3,"%02x",snapshot.sha256[j]);
  CHECK(!strcmp(streamed,vectors[i].digest));
 }
cleanup:
 if(path && !xrtFileDelete(path) && !failed) { fputs("sha256_test: fixture cleanup failed\n",stderr); failed=1; }
 free(id); free(path); free(data); printf("IDE SHA-256: %d checks %s\n",checks,failed ? "failed" : "passed"); return failed;
}
