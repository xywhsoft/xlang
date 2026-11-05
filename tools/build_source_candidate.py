"""Plan/build an honest incremental compiler over an immutable source tree.

Each actual parent object's archived source and project include closure is
compared with the new frozen tree. Changed or uncertain inputs are rebuilt;
unchanged objects retain their original source/flags/path provenance. This
does not reinterpret SourceDriver flags as a production DLL build profile.
Planning is the default. A provider/VFS is a separate, later operation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_validation_build import load_build
from compiler_response import command_with_response

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def inside(path,root):
 try:return path.relative_to(root)
 except ValueError:return None
def include_dirs(flags,root):
 result=[];index=0
 while index<len(flags):
  value=flags[index];index+=1
  if value in ('-I','-iquote','-isystem'):
   value=flags[index];index+=1
  elif value.startswith('-I'):value=value[2:]
  else:continue
  p=Path(value);result.append((root/p if not p.is_absolute()else p).resolve())
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','freeze','parent-overlay','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--header-qualification',type=Path,help='Explicit inherited additive-header qualification; exact old/new hashes plus forbidden-reference audit')
 p.add_argument('--allow-lf-equivalence',action='store_true',help='Explicitly inherit prior canonical-LF source qualification; preserve original hashes')
 p.add_argument('--execute',action='store_true');a=p.parse_args();root,out=a.root.resolve(),a.out.resolve()
 frozen=json.loads(a.freeze.read_text(encoding='utf-8-sig'));assert frozen.get('passed')and frozen.get('complete')and frozen.get('files')
 assert Path(frozen['out']).resolve()==root
 for name,digest in frozen['files'].items():assert sha(root/name)==digest,name
 actual={v.relative_to(root).as_posix()for v in root.rglob('*')if v.is_file()};assert actual==set(frozen['files']),'Unrecorded source-tree mutation'
 assert not out.exists()and not out.is_relative_to(root/'release');out.mkdir()
 selected=load_build(overlay=a.parent_overlay);parent=json.loads(a.parent_overlay.read_text(encoding='utf-8-sig'));base=selected['base']
 originalRoot=next(Path(r['Source']).parents[2]for r in base['Objects']if Path(r['Source']).name.startswith('xlir_public_source'))
 oldMapping={row['ParentObject']:row for row in parent['Objects']};cc=shutil.which('gcc');assert cc
 knownRoots={originalRoot,selected['cohort']}
 for original in base['Objects']:
  relative=Path(original['Source']).relative_to(originalRoot);candidate=Path(oldMapping.get(original['Path'],original)['Source']).resolve()
  for _ in relative.parts:candidate=candidate.parent
  knownRoots.add(candidate)
 compilerHash=sha(cc);matchingCompiler=[row for row in base.get('Inputs',[])if row['Sha256'].lower()==compilerHash]
 assert matchingCompiler,'Compiler bytes differ from the parent profile; require an explicitly qualified new profile'
 aliases={Path(row['Path']).resolve():Path(row['Source']).resolve()for row in parent.get('Headers',base.get('Headers',[]))}
 def currentSource(path):
  for ancestor in sorted(knownRoots,key=lambda v:len(v.parts),reverse=True):
   relative=inside(path,ancestor)
   if relative is not None:return root/relative
  return path
 # Match all existing SDK aliases, then admit new files in the same explicitly
 # observed source/alias directory families (e.g. new Sem includes and runtime
 # private headers). These are derived outputs, never snapshot mutations.
 sdkAliases=[];families={};parentHeaders=parent.get('Headers',base.get('Headers',[]))
 aliasRoots=[Path(parent.get('RuntimeHeaders',selected['basePath'].parent/'include')).resolve(),(selected['basePath'].parent/'include').resolve()]
 for header in parentHeaders:
  alias=Path(header['Path']).resolve();relative=next((v for ancestor in aliasRoots if (v:=inside(alias,ancestor))is not None),None)
  assert relative is not None,alias
  source=currentSource(Path(header['Source']).resolve());assert source.is_file(),source
  target=out/'include'/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
  sdkAliases.append(dict(Source=str(source),Path=str(target),Sha256=sha(source)));aliases[target.resolve()]=source
  families.setdefault((source.parent,relative.parent),set()).add(source.suffix)
 for (folder,relative),suffixes in families.items():
  for source in sorted(folder.iterdir()):
   if not source.is_file()or source.suffix not in suffixes:continue
   target=out/'include'/relative/source.name
   if target.exists():continue
   target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);aliases[target.resolve()]=source
   sdkAliases.append(dict(Source=str(source),Path=str(target),Sha256=sha(source)))
 parseCache={};fileShaCache={};fileTextCache={};allInputs={};units=[];qualifications={}
 if a.header_qualification:
  qualified=json.loads(a.header_qualification.read_text(encoding='utf-8-sig'))
  assert qualified.get('approved') and qualified.get('basis'),a.header_qualification
  qualifications={v['path']:v for v in qualified['headers']}
  for name,q in qualifications.items():
   assert name.endswith('.h') and q['forbiddenReferences'] and q['compiledSha256']!=q['qualifiedSha256']
   assert sha(Path(q['qualifiedSource']))==q['qualifiedSha256']
 dependencyCommands=[]
 def inputSha(path):
  if path not in fileShaCache:fileShaCache[path]=sha(path)
  return fileShaCache[path]
 def inputText(path):
  if path not in fileTextCache:fileTextCache[path]=path.read_text(encoding='utf-8-sig',errors='replace')
  return fileTextCache[path]
 def flagsFor(row,original):
  if row.get('Flags')is not None:return row['Flags']
  command=next(v['Command']for v in base['Rows']if '-c'in v['Command']and original['Source']in v['Command']);return command[1:command.index('-c')]
 def parsed(path):
  if path in parseCache:return parseCache[path]
  text=inputText(path);literals=[];computed=False
  for line in text.splitlines():
   match=re.match(r'^\s*#\s*include\s+(.*)',line)
   if not match:continue
   value=match[1];literal=re.match(r'(["<])([^">]+)[">]',value)
   if literal:literals.append((literal[2],literal[1]=='<'))
   else:computed=True
  parseCache[path]=(literals,computed);return parseCache[path]
 def dependencies(start,dirs):
  found={};computed=[];todo=[start];seen=set()
  while todo:
   path=todo.pop().resolve()
   if path in seen:continue
   seen.add(path);assert path.is_file(),path;found[path]=inputSha(path)
   literals,uncertain=parsed(path)
   if uncertain:computed.append(str(path))
   for name,angle in literals:
    candidates=([path.parent/name]if not angle else[])+[d/name for d in dirs]
    child=next((v.resolve()for v in candidates if v.is_file()),None)
    if child is not None:todo.append(child)
  return found,computed
 def exactDependencies(source,flags,label,workRoot):
  folder=out/'dependencies';folder.mkdir(exist_ok=True)
  dep=folder/(label+'.d');log=folder/(label+'.log')
  command=[cc,*flags,'-MM','-MT','unit','-MF',str(dep),str(source)]
  actual=command_with_response(command,cc,folder/(label+'.rsp'))
  result=subprocess.run(actual,cwd=workRoot,capture_output=True);log.write_bytes(result.stdout+result.stderr)
  dependencyCommands.append(dict(command=command,exitCode=result.returncode,log=str(log),logSha256=sha(log)))
  assert result.returncode==0,log
  body=dep.read_text(encoding='utf-8-sig').replace('\\\n',' ').split(':',1)[1]
  names=re.findall(r'(?:\\.|[^\s])+',body)
  found={}
  for name in names:
   name=re.sub(r'\\([\\ #:\t])',r'\1',name);path=Path(name)
   if not path.is_absolute():path=workRoot/path
   path=path.resolve();assert path.is_file(),path;found[path]=inputSha(path)
  assert source.resolve()in found,dep
  return found
 def inspect(row,original,relative,extra=False):
  source=Path(row['Source']).resolve();oldRoot=source
  for _ in relative.parts:oldRoot=oldRoot.parent
  assert source==oldRoot/relative and (root/relative).is_file(),relative
  flags=flagsFor(row,original);newFlags=['-I'+str(out),'-I'+str(out/'include')]+[v.replace(str(oldRoot),str(root)).replace(oldRoot.as_posix(),root.as_posix())for v in flags]
  oldDirs=include_dirs(flags,oldRoot);newDirs=include_dirs(newFlags,root)
  before,uncertain=dependencies(source,oldDirs);after,newUncertain=dependencies(root/relative,newDirs)
  reasons=[];logicalOld={};logicalNew={}
  def logical(path,ownRoot):
   target=aliases.get(path,path)
   for ancestor in sorted({ownRoot,*knownRoots,root},key=lambda v:len(v.parts),reverse=True):
    rel=inside(target,ancestor)
    if rel is not None:return 'source:'+rel.as_posix()
   return 'external:'+str(target)
  inherited=[];lfEquivalent=[]
  def logicalInputs(found,ownRoot,inherit=False):
   result={}
   for path,digest in found.items():
    name=logical(path,ownRoot);value=digest;allInputs[str(path)]=digest
    if inherit and name.startswith('source:'):
     q=qualifications.get(name[7:])
     if q and digest==q['compiledSha256']:
      pattern=re.compile(r'\b(?:'+'|'.join(re.escape(v)for v in q['forbiddenReferences'])+r')\b')
      hits=[str(p)for p in found if logical(p,ownRoot)!=name and pattern.search(inputText(p))]
      if not hits:
       value=q['qualifiedSha256'];allInputs[q['qualifiedSource']]=value
       if a.allow_lf_equivalence:value=hashlib.sha256(Path(q['qualifiedSource']).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
       inherited.append(dict(path=str(path),compiledSha256=digest,qualifiedSource=q['qualifiedSource'],qualifiedSha256=q['qualifiedSha256'],comparisonSha256=value,referenceAudit='No added identifiers or their preprocessor guards in this TU/dependency closure'))
    if a.allow_lf_equivalence and name.startswith('source:'):
     content=path.read_bytes();normalized=content.replace(b'\r\n',b'\n')
     if value==digest and normalized!=content:
      value=hashlib.sha256(normalized).hexdigest();lfEquivalent.append(dict(path=str(path),sha256=digest,canonicalLFSha256=value))
    result[name]=value
   return result
  logicalOld=logicalInputs(before,oldRoot,True);logicalNew=logicalInputs(after,root)
  for name in sorted(set(logicalOld)|set(logicalNew)):
   if logicalOld.get(name)!=logicalNew.get(name):reasons.append(dict(input=name,before=logicalOld.get(name),after=logicalNew.get(name)))
  approximateReasons=reasons;usedExact=False
  if reasons or uncertain or newUncertain:
   # Text traversal is a candidate filter, never authority for rebuilding an
   # inactive include branch. Compiler dependency output resolves macros and
   # actual conditional includes using the exact old/new profile flags.
   before=exactDependencies(source,flags,str(len(units))+'-parent',oldRoot)
   after=exactDependencies(root/relative,newFlags,str(len(units))+'-candidate',root)
   inherited=[];lfEquivalent=[]
   logicalOld=logicalInputs(before,oldRoot,True);logicalNew=logicalInputs(after,root)
   reasons=[dict(input=name,before=logicalOld.get(name),after=logicalNew.get(name))
    for name in sorted(set(logicalOld)|set(logicalNew))if logicalOld.get(name)!=logicalNew.get(name)]
   allInputs.update((str(path),digest)for found in(before,after)for path,digest in found.items());usedExact=True
  units.append(dict(relative=relative.as_posix(),baseObject=original['Path']if not extra else None,parentObject=row['Path'],parentObjectSha256=sha(row['Path']),parentSource=str(source),parentFlags=flags,
   source=str(root/relative),flags=newFlags,extra=extra,rebuild=bool(reasons),reasons=reasons,approximateReasons=approximateReasons,
   computedIncludes=sorted(set(uncertain+newUncertain)),exactCompilerDependencies=usedExact,dependencyCount=len(before),inheritedHeaderQualifications=inherited,canonicalLFQualifications=lfEquivalent,inputIdentity='Archived source/header bytes; original __FILE__/debug paths remain attached to reused objects'))
  if len(units)%100==0:(out/'dependency-progress.json').write_text(json.dumps(dict(units=units,scanned=len(units),rebuild=sum(v['rebuild']for v in units)),indent=2)+'\n')
  if len(units)%100==0:print('Dependency scan:',len(units),'TUs;',sum(v['rebuild']for v in units),'changed',flush=True)
 for old in base['Objects']:
  row=oldMapping.get(old['Path'],old);inspect(row,old,Path(old['Source']).relative_to(originalRoot))
 for row in parent.get('ExtraObjects',[]):inspect(row,row,Path(row['Source']).relative_to(selected['cohort']),True)
 plan=dict(complete=True,passed=True,mode='build'if a.execute else'plan',scope=__doc__,fullRebuild=False,sourceRoot=str(root),sourceFreeze=str(a.freeze),sourceFreezeSha256=sha(a.freeze),
  parentOverlay=str(a.parent_overlay),parentOverlaySha256=sha(a.parent_overlay),parentBuild=parent['parentBuild'],parentBuildSha256=parent['parentBuildSha256'],
  compiler=dict(path=cc,sha256=compilerHash,parentInputs=matchingCompiler),units=units,inputHashes=allInputs,rebuildCount=sum(v['rebuild']for v in units),reuseCount=sum(not v['rebuild']for v in units),
  profile='SourceDriver only; production runtime DLL requires its own matching flags/cache',providerExecuted=False,dependencyCommands=dependencyCommands,
  headerQualification=str(a.header_qualification)if a.header_qualification else None,headerQualificationSha256=sha(a.header_qualification)if a.header_qualification else None,canonicalLFQualification=a.allow_lf_equivalence,
  builderTool=dict(path=str(Path(__file__).resolve()),sha256=sha(__file__)))
 (out/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
 print('Dependency plan:',plan['rebuildCount'],'rebuild,',plan['reuseCount'],'reuse',flush=True)
 if not a.execute:return
 mapping=dict(oldMapping);extras=[];commands=[]
 for unit in units:
  row=next((v for v in parent.get('ExtraObjects',[])if v['Path']==unit['parentObject']),None)if unit['extra']else oldMapping.get(unit['baseObject'])
  if not unit['rebuild']:
   if unit['extra']:extras.append(row)
   continue
  target=out/('obj-'+str(len(commands))+'.o');command=[cc,*unit['flags'],'-c',unit['source'],'-o',str(target)]
  actual=command_with_response(command,cc,out/(target.stem+'.rsp'));start=time.monotonic();r=subprocess.run(actual,cwd=root,capture_output=True)
  log=out/(target.stem+'.log');log.write_bytes(r.stdout+r.stderr);commands.append(dict(command=command,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
  assert not r.returncode,log
  record=dict(Source=unit['source'],Path=str(target),Flags=unit['flags'],Sha256=sha(target))
  if unit['extra']:extras.append(record)
  else:record['ParentObject']=unit['baseObject'];mapping[unit['baseObject']]=record
 if 'LinkResponse'in base:args=[s.strip('"')for s in Path(base['LinkResponse']).read_text(encoding='utf-8-sig').splitlines()]
 else:args=next(v['Command'][1:]for v in base['Rows']if v['Name']=='link')
 for i,arg in enumerate(args):
  key=str(Path(arg))
  if key in mapping:args[i]=mapping[key]['Path']
 runtime=selected['runtime'];tcc=Path(parent['TccCore'])
 if os.name=='nt':
  args=[str(runtime)if Path(v).name.lower()=='xl.dll'else v for v in args]
  # The actual IAT, not presumed link order, is checked after this relink.
  args.insert(args.index('-o'),str(tcc))
 args=[v['Path']for v in extras]+args;driver=out/('public_source.exe'if os.name=='nt'else'public_source');args[args.index('-o')+1]=str(driver)
 actual=command_with_response([cc,*args],cc,out/'link.rsp');r=subprocess.run(actual,cwd=root,capture_output=True);(out/'link.log').write_bytes(r.stdout+r.stderr);assert not r.returncode,out/'link.log'
 binding=None
 if os.name=='nt':
  from stage_source_tcc_provider import pe_tcc_imports
  imports=subprocess.check_output([shutil.which('objdump'),'-p',str(driver)],text=True)
  (out/'driver-imports.log').write_text(imports)
  owners=pe_tcc_imports(imports);assert len(owners)==9 and set(owners.values())=={tcc.name.lower()},owners
  binding=dict(verified=True,tccImports=owners,scope='Stock adjacent-xlc SourceDriver only; arbitrary provider switching is not certified')
 if os.name=='nt':
  for file in (runtime,tcc):shutil.copy2(file,out/file.name)
  runtime=out/runtime.name;tcc=out/tcc.name
 for path,digest in allInputs.items():assert sha(path)==digest,path
 for name,digest in frozen['files'].items():assert sha(root/name)==digest,'Frozen source changed during compilation: '+name
 overlay=dict(complete=True,passed=True,fullRebuild=False,scope=__doc__,sourceRoot=str(root),parentBuild=parent['parentBuild'],parentBuildSha256=parent['parentBuildSha256'],
  sourceFreeze=str(a.freeze),sourceFreezeSha256=sha(a.freeze),predecessorOverlay=str(a.parent_overlay),predecessorOverlaySha256=sha(a.parent_overlay),
  Driver=str(driver),DriverSha256=sha(driver),RuntimeLibrary=str(runtime),TccCore=str(tcc),Objects=list(mapping.values()),ExtraObjects=extras,
  newlyCompiledObjects=len(commands),reusedObjects=plan['reuseCount'],dependencyPlan=str(out/'plan.json'),dependencyPlanSha256=sha(out/'plan.json'),sourceInputsUnchanged=True,commands=commands,linkResponse=str(out/'link.rsp'),providerImportBinding=binding)
 previousSdk=Path(parent.get('SdkDirectory')or base.get('SdkDirectory')or base.get('RuntimeDirectory')or originalRoot/'release/win_x64')
 overlay['SdkDirectory']=str(currentSource(previousSdk));overlay['RuntimeHeaders']=str(out/'include');overlay['includeOverlayRoot']=str(out/'include');overlay['Headers']=sdkAliases
 (out/'overlay.json').write_text(json.dumps(overlay,indent=2)+'\n');print('Source successor:',out/'overlay.json')
if __name__=='__main__':main()
