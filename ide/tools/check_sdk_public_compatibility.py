"""Compare public C declarations, native type layouts and actual XGE imports.

This is a development compatibility check, not complete binary ABI or licence
acceptance. Both SDK trees and all supplied runtime files are read-only inputs.
"""
import argparse,collections,ctypes,hashlib,json,os,re,struct,subprocess,sys,time,uuid
from pathlib import Path
from ctypes import wintypes
from native_desktop import NativeDesktopProcess

ADDED={
 'xui_text_cluster_visit_proc','xui_text_shape_visit_proc','xui_text_shape_cursor',
 'xui_text_shape_resume_t','xui_text_shape_cursor_visit_proc','xui_text_shape_cursor_free_proc',
 'xuiCodeEditSetTextShapeVisitor','xuiCodeEditSetTextShapeCursorVisitor',
 'xuiProxyXgeVisitTextShape','xuiProxyXgeTextShapeCursorFree','xuiProxyXgeVisitTextShapeCached',
}
TOKEN=re.compile(r'''(?:u8|[LuU])?(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(?:[^\W\d]|[_$])(?:\w|\$)*|(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?)(?:[a-zA-Z0-9_+.-]*)(?:)|\.\.\.|<<=|>>=|->|\+\+|--|&&|\|\||<=|>=|==|!=|<<|>>|[^\s]''',re.UNICODE)

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def plain(p):
 if not p.is_file() or p.is_symlink() or p.stat().st_file_attributes & 0x400:raise ValueError('Expected ordinary input file: '+str(p))

def declarations(text):
 pragmas=[re.sub(r'\s+',' ',line).strip() for line in text.splitlines() if line.lstrip().startswith('#pragma')]
 text='\n'.join(line for line in text.splitlines() if not line.lstrip().startswith('#pragma'))
 if any(line.lstrip().startswith('#') for line in text.splitlines()):raise ValueError('Unprocessed preprocessor directive')
 tokens=TOKEN.findall(text);out=[];item=[];depth={'(':0,'[':0,'{':0};function=False
 for token in tokens:
  if token=='{' and not any(depth.values()):
   function=bool(item and item[-1]==')' and '=' not in item and 'typedef' not in item)
  item.append(token)
  if token in depth:depth[token]+=1
  elif token in (')',']','}'):
   key={')':'(',']':'[','}':'{'}[token];depth[key]-=1
   if depth[key]<0:raise ValueError('Unbalanced declaration')
  if (token==';' or (token=='}' and function)) and not any(depth.values()):
   out.append(item);item=[];function=False
 if item or any(depth.values()):raise ValueError('Incomplete public declaration')
 return out,pragmas

def pe(path):
 data=path.read_bytes()
 def unpack(fmt,offset):return struct.unpack_from(fmt,data,offset)
 if data[:2]!=b'MZ':raise ValueError('Expected PE input: '+str(path))
 start=unpack('<I',0x3c)[0]
 if data[start:start+4]!=b'PE\0\0':raise ValueError('Bad PE signature')
 machine,sections=unpack('<HH',start+4);optionalSize=unpack('<H',start+20)[0];optional=start+24
 if machine!=0x8664 or unpack('<H',optional)[0]!=0x20b:raise ValueError('Expected native PE32+ AMD64')
 table=optional+optionalSize;headers=unpack('<I',optional+60)[0]
 spans=[]
 for i in range(sections):
  offset=table+i*40;virtualSize,rva,rawSize,raw=unpack('<IIII',offset+8)
  spans.append((rva,max(virtualSize,rawSize),raw,rawSize))
 def fileoff(rva,size=1):
  if rva<headers and rva+size<=len(data):return rva
  for base,extent,raw,rawSize in spans:
   relative=rva-base
   if 0<=relative and relative+size<=rawSize and relative+size<=extent and raw+relative+size<=len(data):return raw+relative
  raise ValueError('PE RVA has no file-backed bytes: '+hex(rva))
 def string(rva):
  offset=fileoff(rva);end=data.find(b'\0',offset,min(len(data),offset+65536))
  if end<0:raise ValueError('Unterminated PE string')
  return data[offset:end].decode('ascii')
 def directory(index):return unpack('<II',optional+112+index*8)
 exports={};ordinalNames={};exportRva,exportSize=directory(0)
 if exportRva:
  e=fileoff(exportRva,40);base,count,nameCount,functions,names,ordinals=unpack('<IIIIII',e+16)
  if count>100000 or nameCount>count:raise ValueError('Unexpected export inventory')
  for i in range(nameCount):
   name=string(unpack('<I',fileoff(names+i*4,4))[0]);index=unpack('<H',fileoff(ordinals+i*2,2))[0]
   if index>=count or unpack('<I',fileoff(functions+index*4,4))[0]==0:raise ValueError('Invalid named export')
   exports[name]=base+index;ordinalNames.setdefault(base+index,[]).append(name)
 imports=[];importRva,importSize=directory(1)
 if importRva:
  for index in range(4096):
   descriptor=unpack('<IIIII',fileoff(importRva+index*20,20))
   if not any(descriptor):break
   original,stamp,chain,name,first=descriptor;dll=string(name);thunk=original or first
   symbols=[]
   for i in range(100000):
    value=unpack('<Q',fileoff(thunk+i*8,8))[0]
    if not value:break
    if value&(1<<63):symbols.append(dict(ordinal=value&65535))
    else:symbols.append(dict(name=string(value+2)))
   else:raise ValueError('Unterminated PE thunk table')
   imports.append(dict(dll=dll,symbols=symbols))
  else:raise ValueError('Unterminated PE import descriptors')
 delay=directory(13)
 return dict(exports=exports,ordinalNames=ordinalNames,imports=imports,delayImportDirectory=list(delay))

def checked(command,root,label):
 log=root/(label+'.log');started=time.monotonic()
 with log.open('xb') as f:r=subprocess.run(command,stdout=f,stderr=f,timeout=90)
 row=dict(arguments=list(map(str,command)),exitCode=r.returncode,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log))
 if r.returncode:raise RuntimeError('Command failed: '+str(log))
 return row

def run(a):
 original=a.original_sdk_root.resolve();candidateReport=a.candidate_build_report.resolve();build=read(candidateReport);candidate=Path(build['source'])
 root=a.output.absolute();baseline=read(a.runtime_baseline)
 if root.exists():raise ValueError('Use a new output directory')
 if root.is_relative_to(original) or root.is_relative_to(candidate):raise ValueError('Output must be outside both SDK input trees')
 if build['status']!='sdk-build-finished' or build['exitCode']!=0 or not build['wholeSdkWerror']:raise ValueError('Expected completed strict candidate build')
 if sha(Path(build['sourceManifest']))!=build['sourceManifestSha256']:raise ValueError('Source build manifest digest differs')
 info=read(Path(build['sourceManifest']));manifest=read(original/'sdk_manifest.json')
 implementation=candidate
 consumerManifest=None
 if a.candidate_sdk_root is not None:
  candidate=a.candidate_sdk_root.resolve();consumerManifest=read(candidate/'sdk_manifest.json')
  if not consumerManifest.get('privateCandidate') or consumerManifest['provenance']['sourceBuildReportSha256']!=sha(candidateReport):raise ValueError('Consumer SDK is not bound to this strict build')
  if consumerManifest['provenance']['sourceBuildManifestSha256']!=sha(Path(build['sourceManifest'])):raise ValueError('Consumer SDK source manifest differs')
 def protected():
  for row in baseline['saved']:
   if sha(Path(row['original']))!=row['sha256'] or sha(Path(row['copy']))!=row['sha256']:raise ValueError('Frozen material changed: '+row['original'])
  for group in ('coreObjects','outputHashes','compilerOutputs'):
   for path,digest in baseline[group].items():
    if sha(Path(path))!=digest:raise ValueError('Protected output changed: '+path)
  for row in manifest['files']:
   p=original/row['path'];plain(p)
   if sha(p)!=row['sha256']:raise ValueError('Original SDK differs: '+row['path'])
  for row in info['files']:
   p=implementation/row['path'];plain(p)
   if sha(p)!=row['sha256']:raise ValueError('Candidate source differs: '+row['path'])
  for row in build['outputs']:
   if sha(Path(row['path']))!=row['sha256']:raise ValueError('Candidate output differs')
  if consumerManifest:
   for row in consumerManifest['files']:
    p=candidate/row['path'];plain(p)
    if sha(p)!=row['sha256']:raise ValueError('Matched consumer SDK changed: '+row['path'])
   for row in build['outputs']:
    p=candidate/Path(row['path']).name
    if sha(p)!=row['sha256']:raise ValueError('Consumer SDK artifacts differ from source build')
   for name in ('xge.h','xui.h','xui_document.h','xui_document_ui.h'):
    if sha(candidate/name)!=sha(implementation/name):raise ValueError('Consumer public header differs from source build: '+name)
   upstream=read(implementation/'lib/xrt/upstream.json')
   if sha(implementation/'lib/xrt/xrt.h')!=upstream['header_sha256'] or sha(candidate/'lib/xrt/xrt.h')!=upstream['declaration_header_sha256']:raise ValueError('XRT source/declaration generation differs')
   for name in ('lib/xrt/xrt_config.h','lib/xrt/upstream.json'):
    if sha(candidate/name)!=sha(implementation/name):raise ValueError('Consumer XRT generation/profile differs')
 protected();root.mkdir(parents=True)
 report=dict(status='running',root=str(root),candidateBuildReport=str(candidateReport),candidateBuildReportSha256=sha(candidateReport),originalManifestSha256=sha(original/'sdk_manifest.json'),runtimeBaselineSha256=sha(a.runtime_baseline),productionChanged=False,sdkInstalled=False,completeAbiAcceptance=False,formalRelease=False)
 pointer=root/'result.json';write(pointer,report)
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DXGE_DLL','-DXUI_DLL','-DXGE_DEBUGMODE=0','-DXGE_XRT_PROFILE_IDE','-D_WIN32_WINNT=0x0601']
 source=root/'public_headers.c';source.write_text('#include "xge.h"\n#include "xui.h"\n',encoding='utf8')
 parsed={};commands=[]
 for variant,sdk in [('original',original),('candidate',candidate)]:
  args=[str(a.gcc),*flags,'-I'+str(sdk),'-I'+str(sdk/'lib/xrt'),'-fmacro-prefix-map='+str(sdk)+'=SDK','-E','-P',str(source)]
  row=checked(args,root,variant+'-public-preprocessor');commands.append(row);parsed[variant]=declarations(Path(row['log']).read_text(encoding='utf8'))
 old,oldPragmas=parsed['original'];new,newPragmas=parsed['candidate'];oldCounts=collections.Counter(tuple(d) for d in old);newCounts=collections.Counter(tuple(d) for d in new)
 missing=list((oldCounts-newCounts).elements());extras=list((newCounts-oldCounts).elements())
 report.update(preprocessorCommands=commands,originalDeclarationCount=len(old),candidateDeclarationCount=len(new),missingExistingDeclarations=[list(d) for d in missing],newDeclarations=[list(d) for d in extras],pragmaStreamsIdentical=oldPragmas==newPragmas)
 write(pointer,report)
 if missing or oldPragmas!=newPragmas:raise ValueError('Existing public declarations or packing pragmas differ')
 def own_name(d):
  if d[0]=='typedef':
   for i in range(len(d)-3):
    if d[i]=='*' and d[i+1] in ADDED and d[i+2:i+4]==(')','('):return d[i+1]
   return d[-2]
  declared=[d[i] for i in range(len(d)-1) if d[i] in ADDED and d[i+1]=='(']
  return declared[0] if len(declared)==1 else None
 additionNames=[own_name(d) for d in extras];report['addedDeclarationNames']=additionNames;write(pointer,report)
 report['publicApiPolicy']='identical' if a.identical_public_api else 'known-cursor-additions';write(pointer,report)
 if a.identical_public_api:
  if extras:raise ValueError('Public declarations were added under the identical API policy')
 elif len(extras)!=len(ADDED) or set(additionNames)!=ADDED:raise ValueError('Unexpected added public declarations')
 types=set()
 for d in old:
  if d and d[0]=='typedef' and len(d)>=4 and d[-3]=='}' and re.fullmatch(r'(?:xge|xui|xrt)_\w+',d[-2]):types.add(d[-2])
 completeTags={d[1] for d in old if len(d)>3 and d[0] in ('struct','union') and d[2]=='{' and d[-2]=='}'}
 for d in old:
  if len(d)==5 and d[0]=='typedef' and d[1] in ('struct','union') and d[2] in completeTags and re.fullmatch(r'(?:xge|xui|xrt)_\w+',d[3]):types.add(d[3])
 for name in ('xge_font_t','xge_text_shape_desc_t','xui_proxy_t','xui_text_shape_t'):
  if name not in types:raise ValueError('Expected public record missing: '+name)
 layout=root/'native_layout.c';layout.write_text('#include "xge.h"\n#include "xui.h"\n#include <stdio.h>\nint main(void){\n'+''.join(f'printf("{name} %zu %zu\\n",sizeof({name}),_Alignof({name}));\n' for name in sorted(types))+'return 0;}\n',encoding='utf8')
 layouts={};compiles=[];runs=[]
 for variant,sdk in [('original',original),('candidate',candidate)]:
  exe=root/(variant+'-native-layout.exe');args=[str(a.gcc),*flags,'-I'+str(sdk),'-I'+str(sdk/'lib/xrt'),str(layout),'-o',str(exe)]
  compiles.append(checked(args,root,variant+'-layout-compile'));row=checked([str(exe)],root,variant+'-layout-native');row['executableSha256']=sha(exe);runs.append(row)
  data={}
  for line in Path(row['log']).read_text(encoding='ascii').splitlines():
   name,size,alignment=line.split();data[name]=[int(size),int(alignment)]
  if set(data)!=types:raise ValueError('Incomplete native layout output')
  layouts[variant]=data
 report.update(nativeRecordCount=len(types),nativeLayouts=layouts,layoutCompileCommands=compiles,layoutNativeCommands=runs,nativeLayoutsIdentical=layouts['original']==layouts['candidate']);write(pointer,report)
 if layouts['original']!=layouts['candidate']:raise ValueError('Native public record layout differs')
 originalDll=original/'xge.dll';candidateDll=candidate/'xge.dll' if consumerManifest else implementation/'build/ide/xge.dll';before=pe(originalDll);after=pe(candidateDll)
 removed=sorted(set(before['exports'])-set(after['exports']));added=sorted(set(after['exports'])-set(before['exports']));changed={name:[ordinal,after['exports'][name]] for name,ordinal in before['exports'].items() if name in after['exports'] and after['exports'][name]!=ordinal}
 report.update(originalDllSha256=sha(originalDll),candidateDllSha256=sha(candidateDll),originalExportCount=len(before['exports']),candidateExportCount=len(after['exports']),removedExports=removed,addedExports=added,changedExistingOrdinals=changed)
 if removed:write(pointer,report);raise ValueError('Existing exports removed')
 if a.identical_public_api and (added or changed):write(pointer,report);raise ValueError('Export names or ordinals differ under the identical API policy')
 runtimes=[];nameImports=0;ordinalImports=0
 paths=dict(baseline['outputHashes']);paths.update(baseline['compilerOutputs'])
 for path,digest in paths.items():
  p=Path(path)
  if p.suffix.lower() not in ('.exe','.dll'):continue
  image=pe(p);requested=[]
  for imported in image['imports']:
   if imported['dll'].lower()!='xge.dll':continue
   for symbol in imported['symbols']:
    if 'name' in symbol:
     if symbol['name'] not in after['exports']:raise ValueError('Unresolved name import: '+symbol['name'])
     nameImports+=1
    else:
     ordinal=symbol['ordinal'];names=before['ordinalNames'].get(ordinal,[])
     if not names or any(after['exports'].get(name)!=ordinal for name in names):raise ValueError('Ordinal import changes meaning: '+str(p))
     ordinalImports+=1
    requested.append(symbol)
  if any(image['delayImportDirectory']):raise ValueError('Delay-import audit is not implemented: '+str(p))
  runtimes.append(dict(path=str(p),sha256=digest,xgeImports=requested,delayImportDirectory=image['delayImportDirectory']))
 report.update(runtimeImports=runtimes,runtimePeCount=len(runtimes),xgeNameImportOccurrences=nameImports,xgeOrdinalImportOccurrences=ordinalImports)
 names=root/'original-exports.txt';names.write_text('\n'.join(sorted(before['exports']))+'\n',encoding='ascii')
 loader=root/'native_loader.c';loader.write_text('''#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 2;
 FILE* f=_wfopen(argv[2],L"rb");if(!f)return 3;
 HMODULE module=LoadLibraryW(argv[1]);if(!module){printf("LoadLibrary failed %lu\\n",GetLastError());fclose(f);return 4;}
 char name[65536];int count=0,code=0;
 while(fgets(name,sizeof(name),f)){size_t n=strcspn(name,"\\r\\n");name[n]=0;if(!n)continue;if(!GetProcAddress(module,name)){printf("Missing %s %lu\\n",name,GetLastError());code=5;break;}++count;}
 fclose(f);if(!FreeLibrary(module))code=6;printf("resolved %d original exports\\n",count);return code;
}
'''.replace('#include <wchar.h>','#include <wchar.h>\n#include <string.h>'),encoding='utf8')
 exe=root/'native_loader.exe';link=checked([str(a.gcc),'-std=c11','-O2','-Wall','-Wextra','-Werror','-municode',str(loader),'-o',str(exe)],root,'loader-compile')
 user=ctypes.WinDLL('user32',use_last_error=True);user.CreateDesktopW.restype=wintypes.HANDLE;user.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p];user.CloseDesktop.argtypes=[wintypes.HANDLE]
 desktopName='XlangSdkCompatibility-'+uuid.uuid4().hex;desktop=user.CreateDesktopW(desktopName,None,None,0,0x10000000,None)
 if not desktop:raise ctypes.WinError(ctypes.get_last_error())
 log=root/'loader-native.log';process=None;started=time.monotonic()
 try:
  with log.open('xb') as f:
   process=NativeDesktopProcess([str(exe),str(candidateDll),str(names)],f,f,desktopName,cwd=str(root));code=process.wait(timeout=45)
 finally:
  if process:process.close()
  if not user.CloseDesktop(desktop):raise ctypes.WinError(ctypes.get_last_error())
 report.update(loaderCompile=link,loaderNative=dict(exitCode=code,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log),executableSha256=sha(exe),neverSwitchedDesktop=True),allExistingPublicDeclarationsIdentical=True,allRuntimeXgeImportsResolved=True)
 write(pointer,report)
 if code or log.read_text(encoding='ascii').strip()!=f"resolved {len(before['exports'])} original exports":raise ValueError('Incomplete native export resolution')
 protected();report.update(status='existing-public-declarations-layouts-and-runtime-imports-compatible',inputsUnchanged=True);write(pointer,report)
 print(json.dumps(dict(status=report['status'],declarations=len(old),publicRecords=len(types),originalExports=len(before['exports']),candidateExports=len(after['exports']),changedOrdinals=len(changed),runtimePeFiles=len(runtimes),nameImports=nameImports,ordinalImports=ordinalImports,report=str(pointer))),flush=True)

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--original-sdk-root',type=Path,required=True);parser.add_argument('--candidate-build-report',type=Path,required=True)
 parser.add_argument('--candidate-sdk-root',type=Path)
 parser.add_argument('--identical-public-api',action='store_true',help='Require no public declarations, exports or ordinal changes')
 parser.add_argument('--runtime-baseline',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
 parser.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
 args=parser.parse_args()
 try:run(args)
 except Exception as error:print(type(error).__name__+': '+str(error),file=sys.stderr);raise SystemExit(1)
