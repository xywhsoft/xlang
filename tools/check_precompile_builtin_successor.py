"""Focused mixed serialized-manifest validation; temporary data only, no target compiler execution."""
from pathlib import Path
import argparse,copy,hashlib,json,shutil,subprocess,sys,tempfile
from precompile_builtin_successor import plan,sha,source_fingerprint,sdk_hash,flags_for,validate_mixed_manifest

def signed(m):
 m.pop('signature',None);m['signature']=hashlib.sha256(json.dumps(m,sort_keys=True).encode()).hexdigest();return m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();out.mkdir(exist_ok=False,parents=True);rows=[]
 with tempfile.TemporaryDirectory(prefix='xl-mixed-schema-')as tmp:
  b=Path(tmp);old=b/'old';new=b/'new';dist=b/'sdk';(dist/'include').mkdir(parents=True)
  for name in ('src/api','src/compile','lib/tcc','lib/lzma','lib/xrt','lib/xruntime','release/win_x64/include','release/win_x64/extlibs/xacme','release/linux_x64/include','release/linux_x64/extlibs/xacme'):(old/name).mkdir(parents=True,exist_ok=True)
  (old/'src/api/xl_api.c').write_text('static const char* runtimeSources[] = {"src/a.c"};\nstatic const char* compilerSources[] = {"src/b.c"};\n')
  (old/'src/compile/xl6_compile_unit.c').write_text('int catalog;\n')
  (old/'src/a.c').write_text('int a=1;\n');(old/'src/b.c').write_text('int b=2;\n');shutil.copytree(old,new);(new/'src/a.c').write_text('int a=3;\n')
  binaries=[]
  for n in range(3):f=b/('compiler'+str(n));f.write_bytes(('compiler'+str(n)).encode());binaries.append(f)
  parent=dict(schema=1,target='fixture',sources=source_fingerprint(old),sdk=sdk_hash(dist),nativeCompiler=sha(binaries[0]),tcc=sha(binaries[0]),nativeSystemHeaders=hashlib.sha256(b'').hexdigest(),flags=['-std=c11'],components=1,nativeTranslationUnitSources=[],objects=[])
  for n in ('a','b'):
   f=b/(n+'.o');f.write_bytes(('old-'+n).encode());parent['objects'].append(dict(virtual='__xl_objects/static-compiler/src/'+n+'.c.o',source=str(f),sha256=sha(f),bytes=f.stat().st_size))
  pp=b/'parent.json';pp.write_text(json.dumps(parent));_,proof=plan(pp,old,new,dist);assert(proof['recompiled'],proof['inherited'])==(1,1)
  m=dict(schema=2,target='fixture',sdk=parent['sdk'],sources=source_fingerprint(new),sourceRoot=str(new),distribution=str(dist),flags=parent['flags'],currentProducer=dict(path=str(binaries[1]),sha256=sha(binaries[1]),kind='fixture'),catalogPreprocessor=dict(sha256=parent['tcc']),producers={sha(f):dict(path=str(f),sha256=sha(f))for f in binaries[:2]},parentManifest=dict(path=str(pp),sha256=sha(pp),sourceRoot=str(old)),inheritanceProof=proof,producerSourceChanges=[],counts=dict(recompiled=1,inherited=1),fullRecompile=False,nativeSystemHeaders=parent['nativeSystemHeaders'],components=1,nativeTranslationUnitSources=[],objects=[])
  for prior,decision in zip(parent['objects'],proof['rows']):
   r=copy.deepcopy(prior);kind='recompiled'if decision['recompile']else'inherited'
   if decision['recompile']:
    f=b/'new-a.o';f.write_bytes(b'new-a');r.update(source=str(f),sha256=sha(f),bytes=f.stat().st_size)
   r.update(producerSha256=sha(binaries[1 if decision['recompile']else 0]),compileFlags=flags_for(parent,prior),provenance=dict(kind=kind,parentVirtual=r['virtual'],parentManifestSha256=sha(pp),sourceRelative=decision['sourceRelative'],sourceClosureSha256=decision['closureSha256']));m['objects'].append(r)
  target=b/'mixed.json';target.write_text(json.dumps(signed(m)));validate_mixed_manifest(target,new,binaries[1]);rows.append(dict(name='mixed-two-producers',passed=True))
  variants=[('full-recompile',lambda x:x.update(fullRecompile=True)),('parent-sha',lambda x:x['parentManifest'].update(sha256='0'*64)),('duplicate',lambda x:x['objects'].append(copy.deepcopy(x['objects'][0]))),('missing',lambda x:x['objects'].pop()),('counts',lambda x:x['counts'].update(inherited=0)),('object-hash',lambda x:x['objects'][0].update(sha256='0'*64)),('object-size',lambda x:x['objects'][0].update(bytes=0)),('flags',lambda x:x['objects'][0]['compileFlags'].append('-DOTHER')),('unknown-producer',lambda x:x['objects'][0].update(producerSha256='0'*64)),('inherited-producer-relabeled',lambda x:x['objects'][1].update(producerSha256=sha(binaries[1]))),('inherited-path-relabeled',lambda x:x['objects'][1].update(source=str(b/'new-a.o'))),('source-closure',lambda x:x['objects'][0]['provenance'].update(sourceClosureSha256='0'*64)),('source-identity',lambda x:x['objects'][0]['provenance'].update(sourceRelative='src/b.c')),('provenance-kind',lambda x:x['objects'][0]['provenance'].update(kind='inherited')),('producer-binary',lambda x:x['producers'][sha(binaries[1])].update(path=str(binaries[2]))),('signed-false-proof',lambda x:x['inheritanceProof']['rows'][0].update(literalSourceSha256='0'*64))]
  for name,mutate in variants:
   bad=copy.deepcopy(m);mutate(bad);target.write_text(json.dumps(signed(bad)))
   try:validate_mixed_manifest(target,new,binaries[1])
   except (ValueError,AssertionError,KeyError)as e:rows.append(dict(name=name,passed=True,diagnostic=str(e)))
   else:raise AssertionError('Accepted invalid '+name)
  target.write_text(json.dumps(signed(m)))
  _,chainproof=plan(target,new,new,dist);chain=copy.deepcopy(m);chain.update(parentManifest=dict(path=str(target),sha256=sha(target),sourceRoot=str(new)),inheritanceProof=chainproof,counts=dict(recompiled=0,inherited=2),currentProducer=dict(path=str(binaries[2]),sha256=sha(binaries[2]),kind='fixture'))
  chain['producers'][sha(binaries[2])]=dict(path=str(binaries[2]),sha256=sha(binaries[2]))
  for r in chain['objects']:r['provenance'].update(kind='inherited',parentManifestSha256=sha(target))
  cp=b/'chain.json';cp.write_text(json.dumps(signed(chain)));validate_mixed_manifest(cp,new,binaries[2]);rows.append(dict(name='three-producer-transitive-chain',passed=True))
  command=[sys.executable,'-B',str(Path(__file__).with_name('precompile_builtin_successor.py')),'--root',str(new),'--parent-root',str(new),'--parent-manifest',str(target),'--distribution',str(dist),'--producer',str(binaries[2]),'--parent-producer',str(binaries[1]),'--expect-recompiled','0','--plan-only','--output',str(b/'cli-chain')]
  r=subprocess.run(command,capture_output=True);(out/'chain-cli.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,(r.stdout,r.stderr);rows.append(dict(name='cli-mixed-parent-plan',passed=True))
  from gen_tcc_vfs_resources import validate_precompiled_manifest
  parent['nativeSystemHeaders']='1'*64;pp.write_text(json.dumps(parent))
  gcc=copy.deepcopy(m);gcc['nativeSystemHeaders']=parent['nativeSystemHeaders'];gcc['parentManifest']['sha256']=sha(pp);gcc['inheritanceProof']['parentManifestSha256']=sha(pp)
  for row in gcc['objects']:row['provenance']['parentManifestSha256']=sha(pp)
  gcc_path=b/'native-compiler-mixed.json';gcc_path.write_text(json.dumps(signed(gcc)))
  _,qualification=validate_precompiled_manifest(gcc_path,new)
  assert qualification['readOnlyPackaging']and qualification['projectSourceFingerprintVerified']and qualification['externalNativeAbiQualificationRequired']and not qualification['nativeSystemHeadersRequalified']
  rows.append(dict(name='read-only-native-compiler-consumer-plan',passed=True,qualification=qualification))
  for name,mutate in [('consumer-wrong-project-source',lambda x:x.update(sources='0'*64)),('consumer-wrong-object',lambda x:x['objects'][0].update(sha256='0'*64))]:
   bad=copy.deepcopy(gcc);mutate(bad);gcc_path.write_text(json.dumps(signed(bad)))
   try:validate_precompiled_manifest(gcc_path,new)
   except (ValueError,RuntimeError)as e:rows.append(dict(name=name,passed=True,diagnostic=str(e)))
   else:raise AssertionError('Read-only consumer accepted '+name)

  try:plan(pp,old,new,dist)
  except ValueError as e:assert 'system-header'in str(e);rows.append(dict(name='unrecomputed-native-system-headers-rejected',passed=True,diagnostic=str(e)))
  else:raise AssertionError('Accepted unverified native system headers')
  parent['nativeSystemHeaders']=hashlib.sha256(b'').hexdigest()
  for r in(old,new):(r/'src/literal.h').write_text('#include "a.c"\n')
  parent['sources']=source_fingerprint(old);pp.write_text(json.dumps(parent))
  try:plan(pp,old,new,dist)
  except ValueError as e:assert'header'in str(e);rows.append(dict(name='changed-c-through-unchanged-header-rejected',passed=True,diagnostic=str(e)))
  else:raise AssertionError('Accepted hidden changed C closure')
 report=dict(scope=__doc__,complete=True,passed=all(r['passed']for r in rows),rows=rows,inputs={str(Path(__file__).resolve()):sha(__file__),str(Path(__file__).with_name('precompile_builtin_successor.py').resolve()):sha(Path(__file__).with_name('precompile_builtin_successor.py'))});(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(len(rows),'PASS')
if __name__=='__main__':main()
