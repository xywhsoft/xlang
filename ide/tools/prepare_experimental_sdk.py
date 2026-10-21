"""Assemble a private matched consumer SDK from a verified source build.

Generated XRT declarations may be retained only when the implementation header,
configuration and generation provenance are unchanged. Does not build/install.
"""
import argparse,hashlib,json,re,shutil,sys
from pathlib import Path,PurePosixPath

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def relative(s):
 p=PurePosixPath(s)
 if p.is_absolute() or any(part in ('','..','.') for part in p.parts) or ':' in s or '\\' in s:raise ValueError('Invalid material path: '+s)
 return Path(*p.parts)
def plain(p):
 for parent in (p,*p.parents):
  if parent.exists() and (parent.is_symlink() or parent.stat().st_file_attributes&0x400):raise ValueError('Reparse input/output path: '+str(parent))
 if not p.is_file():raise ValueError('Missing material: '+str(p))

def run(a):
 parent=a.parent_sdk.absolute();report=a.build_report.absolute();out=a.output.absolute()
 if out.exists() or out.is_relative_to(parent):raise ValueError('Use a new output outside the parent SDK')
 build=read(report)
 if build['status']!='sdk-build-finished' or build['exitCode']!=0 or not build['wholeSdkWerror'] or not build['sourceUnchanged']:raise ValueError('Completed strict source build required')
 if sha(Path(build['sourceManifest']))!=build['sourceManifestSha256']:raise ValueError('Source build manifest digest differs')
 info=read(Path(build['sourceManifest']));source=Path(info['source'])
 if out.is_relative_to(source):raise ValueError('Output must be outside the source SDK')
 manifest=read(parent/'sdk_manifest.json');upstream=read(parent/'lib/xrt/upstream.json')
 def verify():
  for row in manifest['files']:
   p=parent/relative(row['path']);plain(p)
   if sha(p)!=row['sha256']:raise ValueError('Parent SDK changed: '+str(p))
  for row in info['files']:
   p=source/relative(row['path']);plain(p)
   if sha(p)!=row['sha256']:raise ValueError('Source SDK changed: '+str(p))
  for row in build['outputs']:
   p=Path(row['path']);plain(p)
   if sha(p)!=row['sha256']:raise ValueError('Source build changed: '+str(p))
 verify()
 if manifest['schemaVersion']!=2 or manifest['policy']!={'runtimeOwner':'xge.dll','xrtLinkage':'shared','replaceAsMatchedSetOnly':True}:raise ValueError('Unexpected shared runtime policy')
 if manifest['buildDefines']!=['XGE_XRT_PROFILE_IDE']:raise ValueError('Unexpected parent consumer profile')
 if sha(source/'lib/xrt/xrt.h')!=upstream['header_sha256'] or sha(parent/'lib/xrt/xrt.h')!=upstream['declaration_header_sha256']:raise ValueError('XRT source/declaration generation differs')
 for name in ('lib/xrt/xrt_config.h','lib/xrt/upstream.json'):
  if sha(source/name)!=sha(parent/name):raise ValueError('XRT profile/generation provenance differs: '+name)
 allowed={'xge.h','xui.h','xui_document.h','xui_document_ui.h'}
 for name in allowed:
  if not (source/name).is_file():raise ValueError('Missing public header: '+name)
 outputs={Path(r['path']).name:Path(r['path']) for r in build['outputs']}
 if set(outputs)!={'xge.dll','xge.lib','xge_res.o'}:raise ValueError('Unexpected matched build outputs')
 out.mkdir(parents=True)
 files=[]
 def copy(p,name):
  target=out/relative(name);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
  if sha(target)!=sha(p):raise ValueError('Input changed while copying: '+str(p))
 for row in manifest['files']:
  name=row['path']
  if name in ('README.md','xge-source-files.json'):continue
  p=outputs[name] if name in outputs else source/name if name in allowed else parent/name
  copy(p,name)
 copy(outputs['xge_res.o'],'xge_res.o')
 copy(report,'provenance/source-build-report.json');copy(Path(build['sourceManifest']),'provenance/source-build-manifest.json');copy(parent/'sdk_manifest.json','provenance/parent-sdk-manifest.json')
 descriptor=dict(schemaVersion=1,root=str(source),files=info['files'],sourceFileCount=len(info['files']),sourceBuildReportSha256=sha(report),parentSourceDescriptorSha256=sha(parent/'xge-source-files.json'),privateCandidate=True,fullSourceTreeNotIncludedInConsumerSdk=True)
 write(out/'xge-source-files.json',descriptor)
 (out/'README.md').write_text(f'''# xlang IDE 私有成套 SDK 候选

DLL、导入库及资源对象来自 provenance/source-build-report.json 绑定的完整严格构建，公开 XGE/XUI 头来自其同一源树。XRT 实现完整头保持原生成快照，配置与生成信息保持相同，因此消费者继续使用该快照生成的声明头；它的摘要与源码头分别由 upstream.json 记录。不要以源码完整头直接替代消费者声明头。

source-build-manifest.json 记录全部 {len(info['files'])} 份实际源码材料；xge-source-files.json 指向同一来源。完整源码仍保留在独立构建树，消费者 SDK 不复制源码实现。

只写新的私有候选目录，未替换固定 SDK 或应用，未声明整体 ABI、性能、许可或正式发行通过。仍需原生公开接口比较、完整 IDE 严格构建、标准回归及独立便携验收。XRT 由 xge.dll 单独拥有。
''',encoding='utf8')
 for p in sorted(out.rglob('*')):
  if p.is_file():files.append(dict(path=p.relative_to(out).as_posix(),sha256=sha(p)))
 candidate=dict(manifest)
 candidate.update(profile='xlang-ide-private-experimental-matched-set',files=files,privateCandidate=True,formalRelease=False,sdkInstalled=False,completeAbiAcceptance=False,fullSdkLicenseAcceptance=False)
 candidate['requiredExports']=list(dict.fromkeys(manifest['requiredExports']+['xuiCodeEditSetTextShapeVisitor','xuiCodeEditSetTextShapeCursorVisitor','xuiProxyXgeVisitTextShape','xuiProxyXgeTextShapeCursorFree','xuiProxyXgeVisitTextShapeCached']))
 candidate['provenance']=dict(builtAtUtc=build['finishedUtc'],compiler=build['compiler'],command=build['command'],parentSdkManifest='provenance/parent-sdk-manifest.json',parentSdkManifestSha256=sha(parent/'sdk_manifest.json'),sourceBuildReport='provenance/source-build-report.json',sourceBuildReportSha256=sha(report),sourceBuildManifest='provenance/source-build-manifest.json',sourceBuildManifestSha256=sha(Path(build['sourceManifest'])),xrt=upstream,generatedDeclarationsReusedFromIdenticalImplementationSource=True)
 write(out/'sdk_manifest.json',candidate);verify()
 result=dict(status='private-matched-consumer-sdk-prepared',output=str(out),source=str(source),sourceFileCount=len(info['files']),files=files,manifestSha256=sha(out/'sdk_manifest.json'),xrtImplementationHeaderSha256=upstream['header_sha256'],xrtDeclarationHeaderSha256=upstream['declaration_header_sha256'],inputsUnchanged=True,productionChanged=False,sdkInstalled=False,formalRelease=False)
 write(out/'preparation-report.json',result)
 print(json.dumps(dict(status=result['status'],files=len(files),sourceFiles=len(info['files']),output=str(out))),flush=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--parent-sdk',type=Path,required=True);p.add_argument('--build-report',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 try:run(p.parse_args())
 except Exception as error:print(type(error).__name__+': '+str(error),file=sys.stderr);raise SystemExit(1)
