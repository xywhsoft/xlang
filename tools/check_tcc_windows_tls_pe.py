"""Explicit Windows OS-loader qualification of TinyCC PE static TLS from GCC COFF."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,subprocess,time

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ['root','implementation-root','tcc','distribution','out']:ap.add_argument('--'+name,required=True,type=Path)
 ap.add_argument('--execute-pe',action='store_true',help='Run fresh TinyCC PE CPU programs and observe process/thread TLS callbacks')
 a=ap.parse_args();assert os.name=='nt';root=a.root.resolve();impl=a.implementation_root.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(a.tcc.resolve()):sha(a.tcc)};source=out/'source';frozen=[]
 for p in sorted((impl/'lib/tcc').rglob('*')):
  if p.is_file() and '__pycache__' not in p.parts:
   q=source/p.relative_to(impl);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);h=sha(q);assert h==sha(p);frozen.append({'path':p.relative_to(impl).as_posix(),'sha256':h})
 for name in ['test/cases/tcc_windows_tls_native.c','test/cases/tcc_windows_tls_client.c','tools/check_tcc_windows_tls_pe.py']:
  p=root/name;q=source/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);h=sha(q);inputs[str(p)]=h;frozen.append({'path':name,'sha256':h})
 report={'complete':False,'passed':False,'scope':__doc__,'implementationRoot':str(impl),'sourceInputs':frozen,'inputs':inputs,'rows':[],'executePeRequested':a.execute_pe,'peOsLoaderQualified':False,'shippingDelivery':False,'variants':[]}
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,args,cwd=out):
  t=time.monotonic()
  try:p=subprocess.run([str(x) for x in args],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
  except OSError as e:
   report['rows'].append({'name':name,'command':[str(x) for x in args],'exitCode':None,'winerror':getattr(e,'winerror',None),'error':str(e)});save();raise
  log=out/(name+'.log');log.write_bytes(p.stdout);report['rows'].append({'name':name,'command':[str(x) for x in args],'exitCode':p.returncode,'rawStatusU32':p.returncode&0xffffffff,'seconds':time.monotonic()-t,'logSha256':sha(log)});save()
  print(name,p.returncode,flush=True)
  if p.returncode:raise RuntimeError(name+' failed: '+str(log))
  return p.stdout.decode(errors='replace')
 try:
  native=source/'test/cases/tcc_windows_tls_native.c';client=source/'test/cases/tcc_windows_tls_client.c'
  run('gcc-native',['gcc','-std=c11','-O2','-ffunction-sections','-fdata-sections','-c',native,'-o',out/'native.o'])
  archive=Path(run('find-real-crt',['gcc','-print-file-name=libmingw32.a']).strip()).resolve();inputs[str(archive)]=sha(archive)
  crt=out/'crt';crt.mkdir();members=['lib64_libmingw32_a-'+x+'.o' for x in ['tlssup','tlsthrd','tlsmcrt']]
  run('extract-real-crt',['ar','x',archive,*members],crt)
  report['crtScope']='Real MinGW C static-TLS descriptor/thread callbacks (tlssup, tlsthrd, tlsmcrt); C++ tls_atexit/UCRT registration is not used by this C fixture'
  report['crtInputs']=[{'member':name,'sha256':sha(crt/name)} for name in members]
  run('native-relocations',['objdump','-x','-r',out/'native.o'])
  for mode in ['synthetic','real-crt']:
   for relocate in [False,True]:
    label=mode+('-aslr' if relocate else '-fixed');d=out/label;d.mkdir();program=d/'tls.exe';marker=d/'tls-callback-report.bin'
    options=['-Wl,--gc-sections']+(['-Wl,-dynamicbase,-high-entropy-va,-image-base=140000000'] if relocate else ['-Wl,-no-dynamicbase'])
    args=[a.tcc.resolve(),'-B'+str(a.distribution.resolve()),'-ffunction-sections','-fdata-sections',*options,client,out/'native.o']
    if mode=='real-crt':args.extend(crt/name for name in members)
    args.extend(['-o',program]);run(label+'-link',args)
    description=run(label+'-inspect',['objdump','-x','-r',program]);data=program.read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0];opt=pe+24;imagebase=struct.unpack_from('<Q',data,opt+24)[0];tlsrva,tlssize=struct.unpack_from('<II',data,opt+112+9*8)
    if not tlsrva or tlssize!=40:raise RuntimeError('Missing real PE64 TLS directory')
    item={'name':label,'image':str(program),'sha256':sha(program),'bytes':program.stat().st_size,'tlsDirectoryRva':tlsrva,'tlsDirectorySize':tlssize,'preferredImageBase':imagebase,'executed':False,'crt':mode};report['variants'].append(item);save()
    if a.execute_pe:
     text=run(label+'-execute',[program,marker]);item['executed']=True
     m=re.search(r'TLS_BODY_CHECKS=(\d+) FAILURE_MASK=([0-9a-f]+) RESULT=(\d+)',text)
     if not m or int(m[2],16) or int(m[3]):raise RuntimeError('Native TLS body checks failed')
     item['bodyChecks']=int(m[1]);item['rawBodyResult']=int(m[3])
     payload=marker.read_bytes()
     if len(payload)!=40:raise RuntimeError('Process-detach report incomplete: '+str(len(payload)))
     magic,count,failed,process_attach,thread_attach,thread_detach,process_detach,complete,lo,hi=struct.unpack('<10I',payload)
     if magic!=0x544c5337 or failed or (process_attach,thread_attach,thread_detach,process_detach,complete)!=(1,2,2,1,1):raise RuntimeError('OS TLS lifecycle report failed: '+str(struct.unpack('<10I',payload)))
     item.update(callbackReportSha256=sha(marker),checksIncludingDetach=count,failureMask=failed,processAttach=process_attach,threadAttach=thread_attach,threadDetach=thread_detach,processDetach=process_detach,mainComplete=complete,actualImageBase=(hi<<32)|lo,relocated=((hi<<32)|lo)!=imagebase)
     report['rows'][-1]['checks']=count;save()
  report['frozenInputsUnchanged']=all(sha(source/x['path'])==x['sha256'] for x in frozen)
  report['externalInputsUnchanged']=all(sha(Path(p))==h for p,h in inputs.items())
  report['peOsLoaderQualified']=a.execute_pe and all(x['executed'] for x in report['variants'])
  report['aslrRelocationObserved']=any(x.get('relocated',False) for x in report['variants'] if x['name'].endswith('aslr'))
  report['complete']=True;report['passed']=report['frozenInputsUnchanged'] and report['externalInputsUnchanged'] and (not a.execute_pe or report['peOsLoaderQualified'])
 except Exception as e:report['error']=str(e)
 save();print(json.dumps({'passed':report['passed'],'report':str(out/'results.json'),'peOsLoaderQualified':report['peOsLoaderQualified'],'error':report.get('error')}));raise SystemExit(0 if report['passed'] else 1)
if __name__=='__main__':main()
