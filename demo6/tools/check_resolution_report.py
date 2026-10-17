"""Native import-candidate and environment observation contract gate."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,uuid
from pathlib import Path
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler',type=Path,required=True);p.add_argument('--gcc',default='gcc');a=p.parse_args();root=Path(__file__).resolve().parents[1];compiler=a.compiler.resolve();work=root/'build/test'/('resolution-report-'+str(uuid.uuid4()));src=work/'来源 Ω';src.mkdir(parents=True);results=[];status='failed';started=datetime.datetime.now(datetime.timezone.utc).isoformat();env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''));protected=[compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll')];before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
 def run(name,cmd,expected=0,environment=env):
  cp=subprocess.run([str(x) for x in cmd],cwd=src,env=environment,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90);(work/(name+'.stdout')).write_bytes(cp.stdout);(work/(name+'.stderr')).write_bytes(cp.stderr);results.append({'name':name,'exitCode':cp.returncode});assert cp.returncode==expected,(name,cp.returncode,cp.stderr);return cp
 try:
  run('compile-native-host',[a.gcc,'-std=c11','-Wall','-Wextra','-Werror','-municode','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I'+str(root),'-I'+str(root/'release'),root/'test/cases/resolution_report_host.c',root/'src/compile/xl6_source_report.c',compiler.with_name('xl.dll'),'-o',work/'resolution_report_host.exe'])
  run('native-resolution-contract',[work/'resolution_report_host.exe',src]);entry=src/'main.xl';dep=src/'dep.xl';entry.write_bytes(b'import dep\nfunc main(...) as int\n    return dep.value()\nend\n');dep.write_bytes(b'export func value() as int\n    return 7\nend\n')
  absent=dict(env);absent.pop('XLANG_PACKAGE_PATH',None)
  cp=run('cli-missing-environment',[compiler,'check',entry,'--json','--source-report'],environment=absent);r=json.loads(cp.stdout);v=r['resolutionReport'];assert r['ok'] and r['sourceReport']['complete'] and v['schema']==1 and v['scope']=='xlang-import-resolution' and v['packagePathObserved'] and v['XLANG_PACKAGE_PATH'] is None
  assert any(Path(x['path']).resolve()==dep.resolve() and x['exists'] for x in v['probes']);assert any(not x['exists'] and Path(x['path']).name=='xlang.package.json' for x in v['probes']);assert all(Path(x['path']).is_absolute() for x in v['probes']);assert len({str(Path(x['path']).resolve()).lower() for x in v['probes']})==len(v['probes'])
  cp=run('cli-empty-environment',[compiler,'check',entry,'--json','--source-report'],environment=dict(env,XLANG_PACKAGE_PATH=''));assert json.loads(cp.stdout)['resolutionReport']['XLANG_PACKAGE_PATH']==''
  cp=run('cli-present-environment',[compiler,'check',entry,'--json','--source-report'],environment=dict(env,XLANG_PACKAGE_PATH=str(src/'package-root')));assert json.loads(cp.stdout)['resolutionReport']['XLANG_PACKAGE_PATH']==str(src/'package-root')
  snap=work/'virtual.utf8';snap.write_bytes(dep.read_bytes());command=[compiler,'check',entry,'--json','--source-report','--source-overlay',dep,snap];cp=run('cli-overlay-skips-disk-predicate',command,environment=absent);r=json.loads(cp.stdout);assert any(Path(x['path']).resolve()==dep.resolve() and x['origin']=='overlay' for x in r['sourceReport']['files']);assert not any(Path(x['path']).resolve()==dep.resolve() for x in r['resolutionReport']['probes'])
  entry.write_bytes(b'func main(...) as int\n    return 0\nend\n');cp=run('cli-unused-environment',[compiler,'check',entry,'--json','--source-report']);r=json.loads(cp.stdout)['resolutionReport'];assert not r['packagePathObserved'] and r['XLANG_PACKAGE_PATH'] is None and not r['probes']
  cp=run('legacy-json-contract',[compiler,'check',entry,'--json']);assert set(json.loads(cp.stdout))=={'ok','phase','diagnostics'}
  assert before=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected};status='passed'
 finally:
  evidence={'status':status,'startedUtc':started,'completedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(compiler),'results':results,'protected':before};(work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'status':status,'checks':len(results),'evidence':str(work/'evidence.json')}))
if __name__=='__main__':main()
