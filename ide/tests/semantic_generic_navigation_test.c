#define main fieldBindingOriginalMain
#include "semantic_field_binding_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_generic_navigation_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int sameValue(xvalue* a,xvalue* b)
{int okay=a && b && xrtValueEqual(a,b);xrtValueRelease(a);xrtValueRelease(b);return okay;}
static int genericReference(owner* o,const char* path,const char* raw,const char* occurrence,
 const char* declarationPath,size_t declarationOffset,const char* name,size_t count)
{
 const char* word=strstr(occurrence,name);CHECK(word);size_t inside=(size_t)(word-occurrence);
 CHECK(bound(o,path,raw,occurrence,inside,declarationPath,declarationOffset,name,count));
 xide_rename_target* t=target(o,path,raw,occurrence,inside);CHECK(t);xide_reference_key* key=xideRenameTargetRetainKey(t);xideRenameTargetRelease(t);CHECK(key);
 xide_reference_identifier_view view;CHECK(xideSemanticReferenceShardIdentifiers(o->shard,key,1,&view)==XIDE_REFERENCE_MATCH && view.references.count==count);
 size_t declarations=0;for(size_t i=0;i<count;++i){xide_semantic_reference at;CHECK(xideSemanticReferenceShardIdentifierAt(&view,i,&at));
  const char* source=NULL;for(size_t j=0;j<o->count;++j)if(!strcmp(o->sources[j].path,at.path))source=o->sources[j].text;
  CHECK(source && at.end.offset-at.start.offset==(int)strlen(name) && !memcmp(source+at.start.offset,name,strlen(name)));declarations+=at.declaration;
 }CHECK(declarations==1);xideSemanticReferenceKeyRelease(key);return 1;
}
static int parity(owner* o)
{
 const char* json=xlsSemanticCheckIndex(o->check);CHECK(json);xvalue* value=xrtJsonParse(xrtStrView(json));CHECK(value);
 xide_semantic_definitions* compat=xideSemanticDefinitionsCreate(value,o->sources,o->count,NULL);xrtValueRelease(value);
 xide_semantic_definitions* typed=xideSemanticDefinitionsCreateTyped(xlsSemanticCheckTypedIndex(o->check),o->sources,o->count,NULL);
 xide_navigation_snapshot* points=xideSemanticNavigationCapture(o->check,o->sources,o->count,NULL);CHECK(compat && typed && points && xideSemanticNavigationPreparePoints(points,NULL)==1);
 for(size_t s=0;s<o->count;++s){const xide_semantic_source* source=o->sources+s;size_t bytes=strlen(source->text);
  for(size_t offset=0;offset<bytes;++offset){if(((unsigned char)source->text[offset]&0xc0)==0x80)continue;int line,column;
   if(!xideLspOffsetToPosition(source->text,(int)offset,&line,&column))continue;
   if(!line)column+=source->utf16Prefix;
   CHECK(sameValue(xideSemanticDefinition(typed,source->path,line,column),xideSemanticDefinition(compat,source->path,line,column)));
   CHECK(sameValue(xideSemanticDefinition(typed,source->path,line,column),xideSemanticNavigationPointDefinition(points,source->path,line,column)));
   CHECK(sameValue(xideSemanticHover(typed,source->path,line,column),xideSemanticNavigationPointHover(points,source->path,line,column)));
  }
 }
 xideSemanticDefinitionsDestroy(typed);xideSemanticDefinitionsDestroy(compat);xideSemanticNavigationDestroy(points);return 1;
}
static int genericRun(void)
{
 char* directory=xrtPathAbs("build/generic-navigation");CHECK(directory && xrtDirCreateAll(directory));char *path=xrtPathJoin(directory,"入口😀.xl"),*dep=xrtPathJoin(directory,"dep.xl");CHECK(path && dep);
 const char* raw="class Box<T>\n T value\nend\nfunc identity<T>(T value) as T\n return value\nend\nfunc main(...) as int\n auto box=Box::<int>()\n auto copy=Box::<string>()\n auto callback=identity::<int>\n return identity::<int>(box.value)+callback(3)\nend\n";
 xls_semantic_input input={path,raw,strlen(raw)};owner o={0};CHECK(capture(&o,path,&input,1));
 CHECK(genericReference(&o,path,raw,"Box::<int>",path,(size_t)(strstr(raw,"Box<T>")-raw),"Box",3));
 CHECK(genericReference(&o,path,raw,"identity::<int>(",path,(size_t)(strstr(raw,"identity<T>")-raw),"identity",3));
 CHECK(genericReference(&o,path,raw,"identity::<int>\n",path,(size_t)(strstr(raw,"identity<T>")-raw),"identity",3));CHECK(parity(&o));release(&o);
 const char* lib="\xef\xbb\xbf" "export class 😀盒<T>\r\n T value\r\n create(T value)\r\n self.value=value\r\n end\r\nend\r\nexport class Marker\r\nend\r\n";
 const char* imported="\xef\xbb\xbf" "import dep\r\nfunc transform<T>(T value) as T\r\n return value\r\nend\r\nfunc main(...) as int\r\n auto box=dep.😀盒::<int>(3)\r\n auto marker=dep.😀盒::<dep.Marker>(dep.Marker())\r\n return transform::<int>(box.value)\r\nend\r\n";
 xls_semantic_input inputs[]={{path,imported,strlen(imported)},{dep,lib,strlen(lib)}};o=(owner){0};CHECK(capture(&o,path,inputs,2));
 CHECK(genericReference(&o,path,imported,"dep.😀盒::<int>",dep,(size_t)(strstr(lib,"😀盒<T>")-lib)-3,"😀盒",3));
 CHECK(genericReference(&o,path,imported,"transform::<int>",path,(size_t)(strstr(imported,"transform<T>")-imported)-3,"transform",2));
 /* Native reduction ranges now retain the whole qualified type argument.
    Every Marker use stays separate from the outer generic class binding. */
 CHECK(genericReference(&o,path,imported,"dep.Marker>",dep,(size_t)(strstr(lib,"Marker\r")-lib)-3,"Marker",3));
 CHECK(genericReference(&o,path,imported,"dep.Marker()",dep,(size_t)(strstr(lib,"Marker\r")-lib)-3,"Marker",3));CHECK(parity(&o));release(&o);
 const char* overloaded="func choose<T>(T value) as int\n return 1\nend\nfunc choose<T>(T first,T second) as int\n return 2\nend\nfunc main(...) as int\n return choose::<int>(1)+choose::<int>(2,3)\nend\n";
 input=(xls_semantic_input){path,overloaded,strlen(overloaded)};o=(owner){0};CHECK(capture(&o,path,&input,1));
 CHECK(genericReference(&o,path,overloaded,"choose::<int>(1)",path,(size_t)(strstr(overloaded,"choose<T>(T value)")-overloaded),"choose",2));
 CHECK(genericReference(&o,path,overloaded,"choose::<int>(2,3)",path,(size_t)(strstr(overloaded,"choose<T>(T first")-overloaded),"choose",2));CHECK(parity(&o));release(&o);
 xrtFree(path);xrtFree(dep);xrtFree(directory);return 1;
}
int main(void){if(!genericRun())return 1;printf("semantic_generic_navigation_test passed: %u checks; exact native generic class/callable origins, constructor names, selected overloads, qualified imports, type arguments, Unicode/BOM/CRLF and JSON/typed/resident parity\n",checks);return 0;}
