"""Normal source-runtime identity retention with independent signature reads."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec,padding

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','controls','source-build','fixtures','out']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();controls=a.controls.resolve();out=a.out.resolve();fixtures=a.fixtures.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();build=json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    source=controls/'test/support/tls_role_context_key_retention.c';gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    paths=[Path(__file__),source,gcc,*[q for q in fixtures.iterdir() if q.is_file()]]
    r=dict(Complete=False,Rows=[],Failures=[],NormalSignatureVerifications=[],Inputs={str(q):sha(q) for q in paths},SourceBuild=str(a.source_build),NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']];assert len(objects)==len(build['Objects'])-1
    flags=['-std=c11','-DXL_STATIC_EMBED','-Wall','-Wextra','-Werror','-Wno-unused-const-variable','-I'+str(root),'-I'+str(root/'release'),'-I'+str(fixtures)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(errors='replace')
    for opt in ['O0','O2']:
        try:
            obj=out/(opt+'.o');binary=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'.compile',[gcc,*flags,'-'+opt,'-c',source,'-o',obj]);run(opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);text=run(opt+'.run',[binary])
            assert 'TLS role retained keys:' in text
            signatures=re.findall(r'^TLS role retained signature ([0-3]) ([0-9a-f]+)$',text,re.M);assert len(signatures)==4 and {int(i) for i,_ in signatures}==set(range(4))
            for index,encoded in signatures:
                index=int(index);family=('rsa','p256','p384','ed25519')[index];signature=bytes.fromhex(encoded)
                public=x509.load_der_x509_certificate((fixtures/(family+'.leaf.der')).read_bytes()).public_key();message=b'ordinary TLS context retention'
                if index==0:public.verify(signature,message,padding.PKCS1v15(),hashes.SHA256())
                elif index in [1,2]:public.verify(signature,message,ec.ECDSA(hashes.SHA256() if index==1 else hashes.SHA384()))
                else:public.verify(signature,message)
                r['NormalSignatureVerifications'].append(dict(Mode=opt,Family=family,SignatureSha256=hashlib.sha256(signature).hexdigest()))
        except Exception as e:r['Failures'].append(dict(Name=opt,Error=repr(e)));save()
    r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.iterdir() if q.is_file() and q.name!='results.json'];r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
