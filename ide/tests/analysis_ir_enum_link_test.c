#define XLS_IR_CONTROL_LINK_LIBRARY 1
#include "analysis_ir_control_link_test.c"
#include "analysis/ir_enum_link.h"

_Static_assert(XLS_RECORD_NODE==8 && XLS_RECORD_CODEC==15 && XLS_RECORD_CLEANUP==16 && XLS_RECORD_REGION==20,"legacy record domains");
_Static_assert(XLS_RECORD_ENUM_LAYOUT==21 && XLS_RECORD_ENUM_VARIANT==22 && XLS_RECORD_ENUM_PAYLOAD==23,"append enum domains");

typedef struct enum_map {int shift,bad,mode;size_t calls[XLS_RECORD_DOMAIN_COUNT];}enum_map;
static int enumMapped(void* user,xls_ir_record_domain domain,uint64_t from,uint64_t* to)
{
 enum_map* m=user;++m->calls[domain];
 if((int)domain==m->bad){if(m->mode==2){*to=from+1+(from==1);return 1;}if(!m->mode)return 0;*to=UINT64_MAX;return 1;}
 uint64_t shift=domain>=XLS_RECORD_ENUM_LAYOUT?1:domain==XLS_RECORD_NODE?10:domain==XLS_RECORD_TYPE?20:domain==XLS_RECORD_SYMBOL?30:0;
 *to=from+(m->shift?shift:0);return 1;
}
static int enumOriginal(size_t table,void* value)
{
 if(table==0)*(XL6_IrEnumLayout*)value=(XL6_IrEnumLayout){.type=1,.declaration=2,.firstVariant=0,.variantCount=2};
 else if(table==1){XL6_IrEnumVariant* p=value;*p=(XL6_IrEnumVariant){.ownerType=1,.declaration=2,.symbol=3,.tag=73,.valueExpression=4,.firstPayload=0,.payloadCount=2,.name=xrtStrDup("变体")};CHECK(p->name);}
 else{XL6_IrEnumPayload* p=value;*p=(XL6_IrEnumPayload){.ownerType=1,.type=2,.declaration=3,.abi=XL6_IR_ENUM_PAYLOAD_STRING_COPY,.name=xrtStrDup("成员")};CHECK(p->name);}return 1;
}
/* Deliberately handwritten expectations, not generated reference roles. */
static void enumExpected(size_t table,void* value)
{
 if(table==0){XL6_IrEnumLayout* p=value;p->type=21;p->declaration=12;p->firstVariant=1;}
 else if(table==1){XL6_IrEnumVariant* p=value;p->ownerType=21;p->declaration=12;p->symbol=33;p->valueExpression=14;p->firstPayload=1;}
 else{XL6_IrEnumPayload* p=value;p->ownerType=21;p->type=22;p->declaration=13;}
}
typedef struct enum_fixture {XL6_IrModule current;void* arrays[3];xls_ir_record_binding bindings[9];enum_map old,fresh;}enum_fixture;
static int enumFixture(enum_fixture* f)
{
 memset(f,0,sizeof(*f));f->old=(enum_map){.shift=1,.bad=-1};f->fresh=(enum_map){.bad=-1};f->current.nodeCount=f->current.types.count=128;
 for(size_t table=0;table<3;++table){f->arrays[table]=xrtCalloc(3,enumRecordWidths[table]);CHECK(f->arrays[table]);enumRecordSet(&f->current,table,f->arrays[table],3);
  for(size_t row=0;row<3;++row){void* value=(char*)f->arrays[table]+row*enumRecordWidths[table];
   if(row)CHECK(enumOriginal(table,value));else if(!table)((XL6_IrEnumLayout*)value)->firstVariant=SIZE_MAX;else if(table==1)((XL6_IrEnumVariant*)value)->firstPayload=SIZE_MAX;
   xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(enumRecordNames[table]),.first=row?row-1:0,.count=1,.rows=value};xls_ir_graph_status status;
   xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);
   f->bindings[table*3+row]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),enumMapped,row?&f->old:&f->fresh};
   if(row)enumExpected(table,value);else if(!table)((XL6_IrEnumLayout*)value)->firstVariant=3;else if(table==1)((XL6_IrEnumVariant*)value)->firstPayload=3;
  }
 }return 1;
}
static void enumFixtureFree(enum_fixture* f)
{
 for(size_t i=0;i<9;++i)xlsIrTableUnitRelease(f->bindings[i].unit);
 for(size_t table=0;table<3;++table){size_t n;void* rows=enumRecordRows(&f->current,table,&n);if(table)for(size_t i=0;i<n;++i)xrtFree(table==1?((XL6_IrEnumVariant*)rows)[i].name:((XL6_IrEnumPayload*)rows)[i].name);xrtFree(rows);}
}
static int enumBook(xstrbuf* book,const XL6_IrModule* m)
{for(size_t table=0;table<3;++table){size_t count;const char* rows=enumRecordRows((XL6_IrModule*)m,table,&count);for(size_t i=0;i<count;++i){const void* p=rows+i*enumRecordWidths[table];int okay=table==0?book_XL6_IrEnumLayout(book,p):table==1?book_XL6_IrEnumVariant(book,p):book_XL6_IrEnumPayload(book,p);if(!okay)return 0;}}return 1;}

typedef struct enum_field {size_t table,offset;int text;}enum_field;
#define ENUM_FIELD(t,ty,f) {t,offsetof(ty,f),0}
#define ENUM_TEXT(t,ty,f) {t,offsetof(ty,f),1}
static const enum_field enumFields[]={
 ENUM_FIELD(0,XL6_IrEnumLayout,type),ENUM_FIELD(0,XL6_IrEnumLayout,declaration),ENUM_FIELD(0,XL6_IrEnumLayout,firstVariant),ENUM_FIELD(0,XL6_IrEnumLayout,variantCount),
 ENUM_FIELD(1,XL6_IrEnumVariant,ownerType),ENUM_FIELD(1,XL6_IrEnumVariant,declaration),ENUM_FIELD(1,XL6_IrEnumVariant,symbol),ENUM_FIELD(1,XL6_IrEnumVariant,tag),ENUM_FIELD(1,XL6_IrEnumVariant,valueExpression),ENUM_FIELD(1,XL6_IrEnumVariant,firstPayload),ENUM_FIELD(1,XL6_IrEnumVariant,payloadCount),ENUM_TEXT(1,XL6_IrEnumVariant,name),
 ENUM_FIELD(2,XL6_IrEnumPayload,ownerType),ENUM_FIELD(2,XL6_IrEnumPayload,type),ENUM_FIELD(2,XL6_IrEnumPayload,declaration),ENUM_FIELD(2,XL6_IrEnumPayload,abi),ENUM_TEXT(2,XL6_IrEnumPayload,name)
};
#undef ENUM_FIELD
#undef ENUM_TEXT
static int enumFailures(void)
{
 enum_fixture f;CHECK(enumFixture(&f));xls_ir_graph_status status;xls_ir_record_error error;
 faultActive=1;faultAt=faultCalls=denied=peakBytes=0;xls_ir_enum_link* image=xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;
 CHECK(image&&status==XLS_IR_GRAPH_OK&&faultCalls==2&&live==1&&!overflow);size_t peak=xlsIrRecordLinkPeakBytes(image);CHECK(peak==peakBytes&&xlsIrRecordLinkBytes(image)==liveBytes);xlsIrEnumLinkDestroy(image);CHECK(!live);
 for(size_t at=1;at<=2;++at){faultActive=1;faultAt=at;faultCalls=denied=0;image=xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error);faultActive=0;CHECK(!image&&status==XLS_IR_GRAPH_MEMORY&&denied==1&&!live);}
 image=xlsIrEnumLink(&f.current,f.bindings,9,peak,NULL,NULL,&status,&error);CHECK(image);xlsIrEnumLinkDestroy(image);CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,peak-1,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_BUDGET);
 for(size_t at=1;;++at){call_cancel cancel={.at=at};image=xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,callCancelled,&cancel,&status,&error);if(image){xlsIrEnumLinkDestroy(image);break;}CHECK(status==XLS_IR_GRAPH_CANCELLED);}
 size_t domains=0;for(size_t d=0;d<XLS_RECORD_DOMAIN_COUNT;++d)if(f.old.calls[d]){++domains;for(int mode=0;mode<2;++mode){f.old.bad=(int)d;f.old.mode=mode;CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==(mode?XLS_RECORD_RANGE:XLS_RECORD_UNMAPPED));}}CHECK(domains==6);f.old.bad=-1;
 for(size_t i=0;i<sizeof(enumFields)/sizeof(*enumFields);++i){const enum_field* field=enumFields+i;unsigned char* target=(unsigned char*)f.arrays[field->table]+enumRecordWidths[field->table]+field->offset;
  if(field->text){char* old;memcpy(&old,target,sizeof(old));char* changed="different";memcpy(target,&changed,sizeof(changed));CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error));memcpy(target,&old,sizeof(old));}
  else{unsigned char old=*target;*target^=1;CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error));*target=old;}
 }
 const void* producer=f.bindings[0].producer;f.bindings[0].producer=&f;CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_NAMESPACE);f.bindings[0].producer=producer;
 xls_ir_record_binding saved=f.bindings[2];f.bindings[2]=f.bindings[1];CHECK(!xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error)&&error.reason==XLS_RECORD_DUPLICATE);f.bindings[2]=saved;
 CHECK(!xlsIrEnumLink(&f.current,f.bindings,8,SIZE_MAX,NULL,NULL,&status,&error));CHECK(!xlsIrEnumLink(NULL,NULL,0,SIZE_MAX,NULL,NULL,&status,&error));
 xstrbuf expected={0},actual={0};CHECK(enumBook(&expected,&f.current));image=xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,&error);CHECK(image);XL6_IrModule view={0};view.callSites=(void*)&view;view.callSiteCount=27;
 CHECK(!xlsIrCallLinkApply(image,&view)&&!xlsIrControlLinkApply(image,&view)&&xlsIrEnumLinkApply(image,&view)&&view.callSites==(void*)&view&&view.callSiteCount==27);enumFixtureFree(&f);CHECK(enumBook(&actual,&view)&&tableBooksEqual(&actual,&expected));xrtStrBufFree(&actual);xrtStrBufFree(&expected);xlsIrEnumLinkDestroy(image);
 XL6_IrModule empty={0};image=xlsIrEnumLink(&empty,NULL,0,SIZE_MAX,NULL,NULL,&status,&error);CHECK(image&&xlsIrEnumLinkApply(image,&empty));xlsIrEnumLinkDestroy(image);CHECK(!xlsIrEnumLinkApply(NULL,&empty));return 1;
}
static int enumGaps(void)
{
 for(size_t table=1;table<3;++table){enum_fixture f;CHECK(enumFixture(&f));size_t width=enumRecordWidths[table];void* rows=xrtCalloc(4,width);CHECK(rows);memcpy(rows,f.arrays[table],2*width);memcpy((char*)rows+3*width,(char*)f.arrays[table]+2*width,width);xrtFree(f.arrays[table]);f.arrays[table]=rows;enumRecordSet(&f.current,table,rows,4);
  xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(enumRecordNames[table]),.first=2,.count=1,.rows=(char*)rows+2*width};xls_ir_graph_status status;xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);xls_ir_record_binding bindings[10];memcpy(bindings,f.bindings,sizeof(f.bindings));bindings[9]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),callIdentity,NULL};f.old.bad=table==1?XLS_RECORD_ENUM_VARIANT:XLS_RECORD_ENUM_PAYLOAD;f.old.mode=2;xls_ir_record_error error;
  CHECK(!xlsIrEnumLink(&f.current,bindings,10,SIZE_MAX,NULL,NULL,&status,&error)&&status==XLS_IR_GRAPH_INVALID&&error.reason==XLS_RECORD_RANGE);xlsIrTableUnitRelease(unit);enumFixtureFree(&f);
 }return 1;
}
typedef struct enum_reader {const enum_fixture* fixture;const xstrbuf* expected;int okay;}enum_reader;
static int32 enumRead(void* user)
{
 enum_reader* r=user;r->okay=1;for(size_t i=0;i<16;++i){xls_ir_record_binding bindings[9];memcpy(bindings,r->fixture->bindings,sizeof(bindings));enum_map old={.shift=1,.bad=-1},fresh={.bad=-1};for(size_t j=0;j<9;++j)bindings[j].user=j%3?&old:&fresh;xls_ir_graph_status status;xls_ir_enum_link* p=xlsIrEnumLink(&r->fixture->current,bindings,9,SIZE_MAX,NULL,NULL,&status,NULL);XL6_IrModule view={0};xstrbuf book={0};int okay=p&&xlsIrEnumLinkApply(p,&view)&&enumBook(&book,&view)&&tableBooksEqual(&book,r->expected);xrtStrBufFree(&book);xlsIrEnumLinkDestroy(p);if(!okay){r->okay=0;break;}}return r->okay?0:1;
}
static int enumLifetimes(void)
{
 enum_fixture f;CHECK(enumFixture(&f));xstrbuf expected={0};CHECK(enumBook(&expected,&f.current));enum_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(enum_reader){&f,&expected,0};threads[i]=xrtThreadCreate(enumRead,readers+i,0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].okay);xrtThreadDestroy(threads[i]);}enumFixtureFree(&f);
 for(size_t i=0;i<32;++i){faultActive=1;faultAt=faultCalls=denied=0;CHECK(enumFixture(&f));xls_ir_graph_status status;xls_ir_enum_link* image=xlsIrEnumLink(&f.current,f.bindings,9,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(image);XL6_IrModule view={0};CHECK(xlsIrEnumLinkApply(image,&view));enumFixtureFree(&f);CHECK(live==19);faultActive=0;xstrbuf book={0};CHECK(enumBook(&book,&view)&&tableBooksEqual(&book,&expected));xrtStrBufFree(&book);xlsIrEnumLinkDestroy(image);CHECK(!live&&!liveBytes&&!overflow);}xrtStrBufFree(&expected);return 1;
}

typedef struct native_enum_map {native_call_map common;uint64_t* rows[3];}native_enum_map;
static int enumNativeMapped(void* user,xls_ir_record_domain domain,uint64_t value,uint64_t* to)
{native_enum_map* m=user;if(domain>=XLS_RECORD_ENUM_LAYOUT&&domain<=XLS_RECORD_ENUM_PAYLOAD){size_t table=domain-XLS_RECORD_ENUM_LAYOUT,n;enumRecordRows((XL6_IrModule*)m->common.old,table,&n);if(value>=n||m->rows[table][value]==UINT64_MAX)return 0;*to=m->rows[table][value];return 1;}return nativeMapped(&m->common,domain,value,to);}
static int mixedNativeEnums(void)
{
 const char* text="func before() as int\n return 1\nend\nenum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\nfunc main(...) as int\n return before()\nend\n";
 const char* prefix="enum Extra\n Other(string label)\nend\nfunc added(Extra item) as int\n return 2\nend\n";
 xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,xrtStrView(prefix))&&xrtStrBufAppend(&changed,xrtStrView(text)));char* path=xrtPathAbs("ir-enum-mixed-中文.xl");CHECK(path);xls_semantic_check* old=checked(path,text);xls_semantic_check* current=checked(path,changed.Data);CHECK(old&&current&&xlsSemanticCheckInfo(old).accepted&&xlsSemanticCheckInfo(current).accepted);
 const XL6_IrModule* a=xlsSemanticCheckIr(old);const XL6_IrModule* b=xlsSemanticCheckIr(current);CHECK(a&&b&&a->enumLayoutCount&&a->enumVariantCount&&a->enumPayloadCount);native_enum_map map={.common={.old=a,.current=b}};native_call_map* common=&map.common;common->nodes=xrtCalloc(a->nodeCount+1,sizeof(uint64_t));common->types=xrtCalloc(a->types.count+1,sizeof(uint64_t));common->symbols=xrtCalloc(a->symbolNodeCapacity,sizeof(uint64_t));CHECK(common->nodes&&common->types&&common->symbols);
 for(size_t i=0;i<a->types.count;++i){const XL6_IrType* x=a->types.items+i;size_t matches=0;for(size_t j=0;j<b->types.count;++j){const XL6_IrType* y=b->types.items+j;if(x->kind==y->kind&&x->paramCount==y->paramCount&&x->returnCount==y->returnCount&&((!x->name&&!y->name)||(x->name&&y->name&&!strcmp(x->name,y->name)))&&((!x->displayName&&!y->displayName)||(x->displayName&&y->displayName&&!strcmp(x->displayName,y->displayName)))){common->types[i+1]=j+1;++matches;}}CHECK(matches==1);}
 common->nodes[a->root]=b->root;const XL6_IrNode* ar=a->nodes+a->root-1;const XL6_IrNode* br=b->nodes+b->root-1;CHECK(br->childCount==ar->childCount+2);for(size_t i=0;i<ar->childCount;++i)CHECK(nativeTreeMap(common,a->children[ar->firstChild+i],b->children[br->firstChild+i+2],0));
 for(size_t i=1;i<a->symbolNodeCapacity;++i){if(!a->symbolNodes[i]){common->symbols[i]=i;continue;}XL6_IrNodeId node=(XL6_IrNodeId)common->nodes[a->symbolNodes[i]];for(size_t j=1;j<b->symbolNodeCapacity;++j)if(b->symbolNodes[j]==node){CHECK(!common->symbols[i]);common->symbols[i]=j;}CHECK(common->symbols[i]);}
 size_t total=0;for(size_t table=0;table<3;++table){size_t n,m;enumRecordRows((XL6_IrModule*)a,table,&n);enumRecordRows((XL6_IrModule*)b,table,&m);total+=m;map.rows[table]=xrtMalloc((n?n:1)*sizeof(uint64_t));CHECK(map.rows[table]);for(size_t i=0;i<n;++i)map.rows[table][i]=UINT64_MAX;}
 for(size_t i=0;i<a->enumLayoutCount;++i)for(size_t j=0;j<b->enumLayoutCount;++j)if(common->types[a->enumLayouts[i].type]==b->enumLayouts[j].type&&common->nodes[a->enumLayouts[i].declaration]==b->enumLayouts[j].declaration){CHECK(map.rows[0][i]==UINT64_MAX);map.rows[0][i]=j;}
 for(size_t i=0;i<a->enumVariantCount;++i)for(size_t j=0;j<b->enumVariantCount;++j)if(common->types[a->enumVariants[i].ownerType]==b->enumVariants[j].ownerType&&common->nodes[a->enumVariants[i].declaration]==b->enumVariants[j].declaration&&a->enumVariants[i].tag==b->enumVariants[j].tag){CHECK(map.rows[1][i]==UINT64_MAX);map.rows[1][i]=j;}
 for(size_t i=0;i<a->enumPayloadCount;++i)for(size_t j=0;j<b->enumPayloadCount;++j)if(common->types[a->enumPayloads[i].ownerType]==b->enumPayloads[j].ownerType&&common->nodes[a->enumPayloads[i].declaration]==b->enumPayloads[j].declaration){CHECK(map.rows[2][i]==UINT64_MAX);map.rows[2][i]=j;}
 xls_ir_record_binding* bindings=xrtCalloc(total,sizeof(*bindings));CHECK(bindings);xls_ir_graph_status status;size_t used=0,reused=0,fresh=0,movedRows=0;
 for(size_t table=0;table<3;++table){size_t n,m;char* from=enumRecordRows((XL6_IrModule*)a,table,&n);char* to=enumRecordRows((XL6_IrModule*)b,table,&m);for(size_t i=0;i<n;++i){CHECK(map.rows[table][i]!=UINT64_MAX);movedRows+=map.rows[table][i]!=i;}
  for(size_t j=0;j<m;++j){size_t selected=SIZE_MAX;for(size_t i=0;i<n;++i)if(map.rows[table][i]==j){CHECK(selected==SIZE_MAX);selected=i;}xls_ir_table_capture_input input={.table=xlsIrTableSchemaFind(enumRecordNames[table]),.first=selected==SIZE_MAX?j:selected,.count=1,.rows=selected==SIZE_MAX?to+j*enumRecordWidths[table]:from+selected*enumRecordWidths[table]};xls_ir_table_unit* unit=xlsIrTableUnitCapture(&input,NULL,SIZE_MAX,NULL,NULL,&status,NULL);CHECK(unit);bindings[used++]=(xls_ir_record_binding){unit,xlsIrTableUnitProducer(unit),selected==SIZE_MAX?callIdentity:enumNativeMapped,selected==SIZE_MAX?NULL:&map};if(selected==SIZE_MAX)++fresh;else ++reused;}
 }
 xls_ir_record_error error;xls_ir_enum_link* image=xlsIrEnumLink(b,bindings,total,SIZE_MAX,NULL,NULL,&status,&error);if(!image)fprintf(stderr,"mixed enum reason=%d domain=%d binding=%zu field=%s value=%llu\n",error.reason,error.domain,error.binding,error.field?error.field:"",(unsigned long long)error.value);fprintf(stderr,"mixed enum premise reused=%zu fresh=%zu movedRows=%zu\n",reused,fresh,movedRows);CHECK(image&&reused>0&&fresh>0&&movedRows>0);
 size_t movedNodes=0,movedTypes=0;for(size_t i=1;i<=a->nodeCount;++i)movedNodes+=common->nodes[i]!=i;for(size_t i=1;i<=a->types.count;++i)movedTypes+=common->types[i]!=i;CHECK(movedNodes>0&&movedTypes>0);capture_view base;CHECK(captureView(b,&base)&&xlsIrEnumLinkApply(image,&base.module));char* expected=captureDump(b);CHECK(expected);
 for(size_t i=0;i<total;++i){xlsIrTableUnitRelease(bindings[i].unit);}xrtFree(bindings);for(size_t i=0;i<3;++i){xrtFree(map.rows[i]);}xrtFree(common->nodes);xrtFree(common->types);xrtFree(common->symbols);xlsSemanticCheckDestroy(old);xlsSemanticCheckDestroy(current);xrtStrBufFree(&changed);xrtFree(path);
 CHECK(verifyCapture(&base.module,expected));xrtFree(expected);xlsIrEnumLinkDestroy(image);captureViewFree(&base);printf("mixed native enums reused=%zu rebuilt=%zu movedRows=%zu movedNodes=%zu movedTypes=%zu;source models and maps retired\n",reused,fresh,movedRows,movedNodes,movedTypes);return 1;
}
static int enumTests(void)
{
 xrtAtomic32Init(&checks,0);xrtAtomic32Init(&oracleRuns,0);xrtAtomic32Init(&oracleErrors,0);xrtAtomic32Init(&oracleActive,0);CHECK(enumFailures()&&enumGaps()&&enumLifetimes()&&mixedNativeEnums());
 char* path=xrtPathAbs("ir-enum-link.xl");CHECK(path);xrtAtomic32Store(&oracleActive,1,XMEMORY_RELEASE);int okay=corpus(path)&&regionCorpus(path);xrtAtomic32Store(&oracleActive,0,XMEMORY_RELEASE);CHECK(okay&&!xrtAtomic32Load(&oracleErrors,XMEMORY_RELAXED)&&xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED)==134);xrtFree(path);
 printf("analysis_ir_enum_link_test passed: %u checks;%u native modules,six family combinations,retired mixed enum models,17fields,OOM/cancel\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&oracleRuns,XMEMORY_RELAXED));return 1;
}
int main(void){return enumTests()?0:1;}
