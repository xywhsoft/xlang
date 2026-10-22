"""Current-source X.509 owned data APIs, ABI, ordinary lifetime and pruning.
Does not certify trust validation, TLS, shipping, or network/security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from check_x509_owned import fixtures,children,content


def literal(data,text=False):
    return ('"' if text else 'b"')+''.join('\\x%02x'%b for b in data)+'"'


def certificate_checks(variable,data):
    cert=x509.load_der_x509_certificate(data);fields=children(cert.tbs_certificate_bytes)
    extension=next((content(field) for field in fields if field[0]==163),b'')
    expected=dict(tbsDer=cert.tbs_certificate_bytes,issuerDer=cert.issuer.public_bytes(),subjectDer=cert.subject.public_bytes(),
        serialContent=content(fields[1]),publicSpki=cert.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo),
        signature=cert.signature,signatureOid=bytes.fromhex('2b6570'),signatureParametersDer=b'',extensionsDer=extension)
    lines=['assert '+variable+'.'+key+' == '+literal(value) for key,value in expected.items()]
    begin=int(cert.not_valid_before_utc.timestamp())*1000000;end=int(cert.not_valid_after_utc.timestamp())*1000000
    lines += [f'assert {variable}.version == 3',f'assert {variable}.notBefore == time({begin})',f'assert {variable}.notAfter == time({end})',
              f'assert {variable}.validAt(time({begin})) and {variable}.validAt(time({end}))',f'assert not {variable}.validAt(time({begin-1}))']
    return lines


def crl_checks(variable,data):
    crl=x509.load_der_x509_crl(data);fields=children(crl.tbs_certlist_bytes)
    offset=1 if fields[0][0]==2 else 0;cursor=offset+3
    if fields[cursor][0] in (23,24):cursor+=1
    revoked=fields[cursor] if cursor<len(fields) and fields[cursor][0]==48 else b''
    extension=next((content(field) for field in fields if field[0]==160),b'')
    expected=dict(tbsDer=crl.tbs_certlist_bytes,issuerDer=crl.issuer.public_bytes(),revokedDer=revoked,extensionsDer=extension,
                  signature=crl.signature,signatureOid=bytes.fromhex('2b6570'),signatureParametersDer=b'')
    lines=['assert '+variable+'.'+key+' == '+literal(value) for key,value in expected.items()]
    begin=int(crl.last_update_utc.timestamp())*1000000
    lines += [f'assert {variable}.version == 2',f'assert {variable}.thisUpdate == time({begin})',f'assert {variable}.validAt(time({begin}))']
    if crl.next_update_utc:
        end=int(crl.next_update_utc.timestamp())*1000000
        lines += [f'assert {variable}.hasNextUpdate',f'assert {variable}.nextUpdate == time({end})',f'assert not {variable}.validAt(time({end+1}))']
    else:
        lines += [f'assert not {variable}.hasNextUpdate','bool missing = false','try',f' auto absent = {variable}.nextUpdate',
                  'catch StateError',' missing = true','end','assert missing']
    return lines


def language_fixtures(root,directory):
    fixtures(root,directory);combined=[]
    for index,path in enumerate(sorted(directory.glob('*.der'))):
        data=path.read_bytes();is_crl=path.stem.startswith('crl');cls='CertificateRevocationList' if is_crl else 'Certificate'
        name='v'+str(index);pem=path.with_suffix('.pem').read_bytes()
        lines=[f'auto {name} = {cls}.fromDer({literal(data)})',f'assert {name}.toDer() == {literal(data)}',
               f'assert {name}.toPem() == {literal(pem,True)}',f'assert {cls}.fromPem({literal(pem,True)}).toDer() == {literal(data)}',
               f'assert {cls}.fromDer({literal(data)},0).toDer() == {literal(data)}',
               f'assert {cls}.fromPem({literal(pem,True)},0,0).toDer() == {literal(data)}']
        lines += [f'assert {cls}.fromPem({literal(pem,True)},{len(pem)},{len(data)}).toDer() == {literal(data)}',
                  f'assert {name}.toPem({len(pem)}) == {literal(pem,True)}',f'int limits{index} = 0']
        operations=[(f'{cls}.fromDer({literal(data)},{len(data)-1})','RangeError'),
                    (f'{cls}.fromPem({literal(pem,True)},{len(pem)-1},0)','RangeError'),
                    (f'{cls}.fromPem({literal(pem,True)},0,{len(data)-1})','RangeError'),
                    (f'{name}.toPem({len(pem)-1})','RangeError'),(f'{cls}.fromDer({literal(data)},-1)','ValueError')]
        for operation,error in operations:
            lines += ['try',' auto refused = '+operation,'catch '+error,f' limits{index} += 1','end']
        lines += [f'assert limits{index} == 5']
        lines += crl_checks(name,data) if is_crl else certificate_checks(name,data)
        lines += [f'array<{cls}> holders{index} = [{name}]',f'assert holders{index}[0].toDer() == {literal(data)}']
        combined += lines
    all_source=directory/'x509.all.xl';all_source.write_text('\n'.join(combined)+'\n',encoding='utf8')
    cert=(directory/'ed25519.der').read_bytes();crl=(directory/'crl_no_next.der').read_bytes()
    certificate=directory/'x509.certificate.der.xl';certificate.write_text('auto cert = Certificate.fromDer('+literal(cert)+')\n'+'\n'.join(certificate_checks('cert',cert))+'\n',encoding='utf8')
    revocations=directory/'x509.crl.der.xl';revocations.write_text('auto crl = CertificateRevocationList.fromDer('+literal(crl)+')\n'+'\n'.join(crl_checks('crl',crl))+'\n',encoding='utf8')
    threaded=directory/'x509.threads.xl'
    threaded.write_text('func reader(Certificate cert,CertificateRevocationList crl) as func() as int\n return func() as int\n'+
        '  if cert.toDer() == '+literal(cert)+' and crl.toDer() == '+literal(crl)+' then return 0 end\n  return 1\n end\nend\n'+
        'auto cert = Certificate.fromDer('+literal(cert)+')\nauto crl = CertificateRevocationList.fromDer('+literal(crl)+')\n'+
        'auto first = thread(reader(cert,crl))\nauto second = thread(reader(cert,crl))\nassert first.waitTimeout(30000) and second.waitTimeout(30000)\nassert first.exitCode() == 0 and second.exitCode() == 0\n',encoding='utf8')
    contract=directory/'x509.contract.xl'
    text=['int marker = 1','func contracts(Certificate cert,CertificateRevocationList crl)']
    declarations=(root/'src/stdlib/xl6_stdlib_x509.inc').read_text()
    for index,line in enumerate(declarations.splitlines()):
        if 'extern ' not in line or 'destroy()' in line:continue
        decoded=json.loads(line.strip().rstrip(';')).strip();owner='CertificateRevocationList' if 'xl6X509Crl' in decoded else 'Certificate'
        if 'static extern' in decoded:
            method=re.search(r'func (\w+)\(',decoded)[1];argument='"pem"' if method.endswith('Pem') else 'b"der"';expression=owner+'.'+method+'('+argument+')'
        elif ' get ' in decoded:expression=('crl' if owner.endswith('List') else 'cert')+'.'+re.search(r'get (\w+)\(',decoded)[1]
        else:
            method=re.search(r'func (\w+)\(',decoded)[1];expression=('crl' if owner.endswith('List') else 'cert')+'.'+method+'('+('time(0)' if method=='validAt' else '')+')'
        text.append(' auto r'+str(index)+' = '+expression)
    text.append('end');contract.write_text('\n'.join(text)+'\n',encoding='utf8')
    return [all_source,certificate,revocations,threaded],contract


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    build=json.loads(a.source_build.read_text(encoding='utf-8-sig'));assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixture_dir=out/'fixtures';fixture_dir.mkdir();cases,contract=language_fixtures(root,fixture_dir)
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_symbols.inc').read_text()))
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_abi.h').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_contracts.inc').read_text(),re.M));assert len(exported)==35 and exported==declared==contracted
    report=dict(Complete=False,Rows=[],Failures=[],Products=[],Plans=[],ResidentEntries=35,SourceBuild=str(a.source_build),
        Inputs={str(path):sha(path) for path in [Path(__file__),gcc,root/'tools/check_x509_owned.py',root/'test/cases/xlir_x509_data_contract.c',root/'test/support/container_projection_artifact.c']},
        TrustValidationAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
    env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,reject=False):
        command=list(map(str,command));invocation=command
        if command[0]==str(gcc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==(1 if reject else 0),(name,str(log));return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    suffix='.exe' if os.name=='nt' else ''
    for source in cases:
        name=source.stem
        try:
            generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated])
            text=generated.read_text();assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6X509\w+\([^;]*\)\s*\{',text,re.M)
            run(name+'.jit',[driver,source,'run',out/(name+'.jit')])
            for stage in ('typed','lowered'):run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ('O0','O2'):run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            run(name+'.module',[driver,source,'module',out/(name+'.module')])
            binary=out/(name+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary]);assert not any(v in imports.lower() for v in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
            assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')];assert len(objects)==len(build['Objects'])-1
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[gcc,*flags,*strict,'-'+opt,'-c',root/'test/cases/xlir_x509_data_contract.c','-o',obj])
            run('contract.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary]);assert 'X509 data XLIR:' in run('contract.'+opt+'.run',[binary,contract])
        except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        obj=out/'plan.o';binary=out/('plan'+suffix)
        run('plan.compile',[gcc,*flags,*strict,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
        for source in cases:
            prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
            assert not any(any(word in module for word in ('_MODULE_ALL','TLS','CRYPTO','X509_VERIFY','X509_PATH','X509_STORE')) for module in plan['NativeModules'])
            if source.stem.endswith('.der'):
                assert not any('_pem' in s for s in plan['Sources']) and not any('PEM' in m for m in plan['NativeModules'])
            if source.stem=='x509.certificate.der':assert not any('x509_crl' in s for s in plan['Sources'])
            if source.stem=='x509.crl.der':assert not any('x509_certificate' in s for s in plan['Sources'])
            report['Plans'].append(dict(Name=source.stem,**plan));save()
    except Exception as error:report['Failures'].append(dict(Name='pruning',Error=repr(error)));save()
    negatives={'default-certificate':'auto value=Certificate()\n','default-crl':'auto value=CertificateRevocationList()\n',
        'der-string':'auto value=Certificate.fromDer("text")\n','pem-bytes':'auto value=CertificateRevocationList.fromPem(b"text")\n',
        'private':'func bad(Certificate value)\n auto raw=value.handle\nend\n',
        'time-integer':'func bad(Certificate value)\n auto valid=value.validAt(0)\nend\n',
        'readonly':'func bad(Certificate value)\n value.version=1\nend\n'}
    for name,body in negatives.items():
        try:
            source=fixture_dir/('bad.'+name+'.xl');source.write_text(body);target=out/('bad.'+name+'.c');message=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert not target.exists() and any(tag in message for tag in ('[sem]','[ir]','[checked]','[stdlib]'))
        except Exception as error:report['Failures'].append(dict(Name='bad.'+name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in report['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in out.rglob('*') if path.is_file() and path.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
