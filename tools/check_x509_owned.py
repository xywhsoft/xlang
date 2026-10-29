"""Owned X.509 data foundation, not language/trust/TLS/shipping acceptance.
Offline normal public fixed-key certificates/CRLs, native rollback and lifetime.
No malformed-input, network, fuzz, attack, or security-campaign tests.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse,hashlib,json,os,re,shutil,subprocess,time
import cryptography
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from check_key_format_wire import vectors

SOURCES=['xl6_x509_value','xl6_x509_certificate','xl6_x509_crl','xl6_x509_pem',
         'xl6_x509_certificate_pem','xl6_x509_crl_pem']


def tlv(tag, content):
    size=len(content)
    length=bytes([size]) if size<128 else bytes([128+(size.bit_length()+7)//8])+size.to_bytes((size.bit_length()+7)//8,'big')
    return bytes([tag])+length+content


def content(data):
    offset=2
    if data[1]&128:
        count=data[1]&127;size=int.from_bytes(data[2:2+count],'big');offset+=count
    else:size=data[1]
    assert offset+size==len(data)
    return data[offset:]


def children(data):
    rest=content(data);result=[]
    while rest:
        offset=2
        if rest[1]&128:
            count=rest[1]&127;size=int.from_bytes(rest[2:2+count],'big');offset+=count
        else:size=rest[1]
        assert offset+size<=len(rest);result.append(rest[:offset+size]);rest=rest[offset+size:]
    return result


def fixtures(root, out):
    keys=vectors(root);issuer_key=keys[3]['key']
    name=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'xlang offline fixture root')])
    begin=datetime(2020,1,1,tzinfo=timezone.utc);end=datetime(2040,1,1,tzinfo=timezone.utc)
    rows=[]
    for index, vector in enumerate(keys[:4]):
        subject=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'xlang fixture '+vector['name'])])
        certificate=(x509.CertificateBuilder().subject_name(subject).issuer_name(name).public_key(vector['key'].public_key())
            .serial_number(128+index).not_valid_before(begin).not_valid_after(end)
            .add_extension(x509.BasicConstraints(ca=False,path_length=None),critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),critical=False)
            .sign(issuer_key,None))
        der=certificate.public_bytes(serialization.Encoding.DER)
        parsed=x509.load_der_x509_certificate(der)
        assert parsed.tbs_certificate_bytes==certificate.tbs_certificate_bytes
        issuer_key.public_key().verify(parsed.signature,parsed.tbs_certificate_bytes)
        tbs=children(parsed.tbs_certificate_bytes)
        rows.append(dict(name=vector['name'],crl=False,der=der,pem=parsed.public_bytes(serialization.Encoding.PEM),
            version=parsed.version.value+1,begin=begin,end=end,hasEnd=True,
            tbs=parsed.tbs_certificate_bytes,issuer=parsed.issuer.public_bytes(),subject=parsed.subject.public_bytes(),
            spki=parsed.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo),
            serial=content(tbs[1]),signature=parsed.signature,oid=bytes.fromhex('2b6570')))
    revoked=x509.RevokedCertificateBuilder().serial_number(128).revocation_date(begin).build()
    crl=(x509.CertificateRevocationListBuilder().issuer_name(name).last_update(begin).next_update(end)
         .add_revoked_certificate(revoked).add_extension(x509.CRLNumber(1),critical=False).sign(issuer_key,None))
    ordinary=crl.public_bytes(serialization.Encoding.DER)
    parts=children(ordinary);tbs=children(parts[0]);assert tbs[4][0] in (23,24)
    without_next=tlv(48,b''.join(tbs[:4]+tbs[5:]));signature=issuer_key.sign(without_next)
    no_end=tlv(48,without_next+parts[1]+tlv(3,b'\0'+signature))
    for name,der in [('crl',ordinary),('crl_no_next',no_end)]:
        parsed=x509.load_der_x509_crl(der);assert parsed.is_signature_valid(issuer_key.public_key())
        rows.append(dict(name=name,crl=True,der=der,pem=parsed.public_bytes(serialization.Encoding.PEM),version=2,
            begin=parsed.last_update_utc,end=parsed.next_update_utc,hasEnd=parsed.next_update_utc is not None,
            tbs=parsed.tbs_certlist_bytes,issuer=parsed.issuer.public_bytes(),subject=b'',spki=b'',serial=b'',
            signature=parsed.signature,oid=bytes.fromhex('2b6570')))
    lines=['typedef struct X509Vector { bool crl; xbytesview der; xstrview pem; int64_t version,begin,end; bool hasEnd;',
           'xbytesview tbs,issuer,subject,spki,serial,signature,oid; } X509Vector;']
    records=[]
    for row in rows:
        spans={}
        for field in ('der','pem','tbs','issuer','subject','spki','serial','signature','oid'):
            data=row[field];symbol='vector_'+row['name']+'_'+field
            if data:
                lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(byte) for byte in data)+'};')
                spans[field]='{'+('(const char*)' if field=='pem' else '')+symbol+',sizeof('+symbol+')}'
            else:spans[field]='{0}'
        epoch=lambda dt:int(dt.timestamp())*1000000 if dt else 0
        records.append('{'+str(row['crl']).lower()+','+spans['der']+','+spans['pem']+','+str(row['version'])+','+
            str(epoch(row['begin']))+'LL,'+str(epoch(row['end']))+'LL,'+str(row['hasEnd']).lower()+','+
            ','.join(spans[field] for field in ('tbs','issuer','subject','spki','serial','signature','oid'))+'}')
        (out/(row['name']+'.der')).write_bytes(row['der']);(out/(row['name']+'.pem')).write_bytes(row['pem'])
    lines.append('static const X509Vector x509Vectors[]={\n'+',\n'.join(records)+'\n};')
    (out/'x509_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
    return len(rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True)
    a=parser.parse_args();out=a.out.resolve();root=Path(__file__).resolve().parents[1];assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    native=root/'test/support/x509_owned.c'
    sources=[root/('src/runtime/'+name+'.c') for name in SOURCES]+[root/'src/runtime/xl6_native_handle.c',root/'src/api/xl_foreign_error.c']
    files=[Path(__file__).resolve(),native,gcc,tcc,*sources,*root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),
           *root.glob('src/compile/*.h'),*root.glob('src/compile/*.inc'),root/'src/api/xl_foreign_error.h',
           root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
           root/'test/support/x509_data_plan.c',root/'src/compile/xl6_compile_unit.c',
           root/'tools/check_key_format_wire.py',*[root/('test/support/crypto_'+s+'_runtime.c') for s in ('rsa','nist','ed25519','xdh')]]
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_symbols.inc').read_text()))
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_abi.h').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_contracts.inc').read_text(),re.M))
    assert len(exported)==35 and exported==declared==contracted
    files += [root/'src/ir/xl6_ir_x509_contracts.inc']
    report=dict(Complete=False,Rows=[],Failures=[],Inputs={str(path):sha(path) for path in files},Products=[],
        Vectors=fixtures(root,out),LanguageApiRegistered=True,ResidentEntries=35,LanguageAcceptance=False,
        Oracle=dict(Cryptography=cryptography.__version__,AllFixtureSignaturesIndependentlyChecked=True),TrustValidationAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,
        NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));invocation=command;started=time.monotonic()
        if command[0] in (str(gcc),str(tcc)):
            response=out/(name+'.rsp');response.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8')
            invocation=[command[0],'@'+str(response)]
        log=out/(name+'.log')
        with log.open('wb') as stream:r=subprocess.run(invocation,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        report['Rows'].append(dict(Name=name,Command=command,Invocation=invocation,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    planner=out/('planner'+('.exe' if os.name=='nt' else ''))
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0',
        '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
        root/'test/support/x509_data_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/name for name in run('planner.sources',[planner,'--sources']).splitlines()]
    modules=run('planner.modules',[planner,'--modules']);assert '_MODULE_ALL' not in modules
    assert not any(word in modules for word in ('MODULE_TLS','MODULE_CRYPTO','MODULE_X509_VERIFY','MODULE_X509_PATH','MODULE_X509_STORE'))
    report['NativeClosure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for path in sources:report['Inputs'][str(path)]=sha(path)
    implementation=out/'native.c'
    body='#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'
    implementation.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+body,encoding='utf8')
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=4'] if cc==gcc else []),native,implementation,*sources,*libs,'-o',binary])
            assert 'X509 owned: 6 valid vectors,' in run(name+'.run',[binary])
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in report['Inputs'].items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in out.iterdir() if path.is_file() and path.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
