"""Offline immutable X.509 records: ordinary fixed-key data, lifetime and OOM.
Not trust/TLS/shipping acceptance; no network, malformed-input or security tests.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse,hashlib,json,os,re,shutil,subprocess,time
import cryptography
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding
from check_key_format_wire import vectors
from check_x509_owned import children,content,tlv


def oid_text(data):
    values=[];value=0
    for byte in data:
        value=value*128+(byte&127)
        if not byte&128:values.append(value);value=0
    first=values.pop(0);return '.'.join(map(str,[min(first//40,2),first-min(first//40,2)*40,*values]))


def name_attributes(data):
    return [dict(raw=item,oid=content(children(item)[0]),value=children(item)[1],rdn=index)
            for index,rdn in enumerate(children(data)) for item in children(rdn)]


def extension_records(data):
    return [dict(raw=item,oid=content(children(item)[0]),value=content(children(item)[-1]),
                 critical=len(children(item))==3 and content(children(item)[1])!=b'\0') for item in children(data)] if data else []


def fixtures(root,out):
    keys=vectors(root);ed=keys[3]['key']
    name=x509.Name([x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COUNTRY_NAME,'CN')]),
        x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COMMON_NAME,'xlang 离线 fixture'),
                                      x509.NameAttribute(x509.NameOID.ORGANIZATIONAL_UNIT_NAME,'records')])])
    begin=datetime(2020,1,1,tzinfo=timezone.utc);end=datetime(2040,1,1,tzinfo=timezone.utc)
    rows=[]
    for index,vector in enumerate(keys[:4]+[keys[0]]):
        key=vector['key'];pss=index==4;algorithm=None if index==3 else hashes.SHA256()
        options={'rsa_padding':padding.PSS(mgf=padding.MGF1(hashes.SHA384()),salt_length=20)} if pss else {}
        cert=(x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
            .serial_number(128+index).not_valid_before(begin).not_valid_after(end)
            .add_extension(x509.BasicConstraints(ca=False,path_length=None),critical=True)
            .add_extension(x509.UnrecognizedExtension(x509.ObjectIdentifier('1.2.3.4'),b'\x04\x04a\0bc'),critical=False)
            .sign(key,algorithm,**options))
        der=cert.public_bytes(serialization.Encoding.DER)
        if index==3:
            parts=children(der);fields=children(parts[0])
            tbs=tlv(48,b''.join(fields[:7])+tlv(129,b'\x03\xa0')+tlv(130,b'\x00\x00X')+b''.join(fields[7:]))
            der=tlv(48,tbs+parts[1]+tlv(3,b'\0'+ed.sign(tbs)))
        parsed=x509.load_der_x509_certificate(der)
        if index in (0,4):key.public_key().verify(parsed.signature,parsed.tbs_certificate_bytes,parsed.signature_algorithm_parameters,parsed.signature_hash_algorithm)
        elif index in (1,2):key.public_key().verify(parsed.signature,parsed.tbs_certificate_bytes,parsed.signature_algorithm_parameters)
        else:ed.public_key().verify(parsed.signature,parsed.tbs_certificate_bytes)
        rows.append(dict(name='pss' if pss else vector['name'],der=der,crl=False,kind=2 if pss else (4 if index==3 else (1 if index==0 else 3)),
                         hash=0 if index==3 else 3,mask=4 if pss else 0,salt=20 if pss else 0,trailer=1 if pss else 0))
    # Ordinary certificates with XDH subject keys; the fixed Ed25519 issuer signs
    # the exact rewritten TBS and the independent oracle verifies every signature.
    base=rows[3]['der'];parts=children(base);fields=children(parts[0])
    for vector in keys[4:6]:
        spki=vector['key'].public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
        tbs=tlv(48,b''.join(fields[:6])+spki+b''.join(fields[7:]));der=tlv(48,tbs+parts[1]+tlv(3,b'\0'+ed.sign(tbs)))
        parsed=x509.load_der_x509_certificate(der);ed.public_key().verify(parsed.signature,parsed.tbs_certificate_bytes)
        assert parsed.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)==spki
        rows.append(dict(name=vector['name'],der=der,crl=False,kind=4,hash=0,mask=0,salt=0,trailer=0))
    crl=(x509.CertificateRevocationListBuilder().issuer_name(name).last_update(begin).next_update(end)
         .add_extension(x509.CRLNumber(1),critical=False).sign(ed,None))
    assert crl.is_signature_valid(ed.public_key())
    rows.append(dict(name='crl',der=crl.public_bytes(serialization.Encoding.DER),crl=True,kind=4,hash=0,mask=0,salt=0,trailer=0))
    lines=['typedef struct AttrOracle {xbytesview raw,oid,value;uint64_t rdn;const char* text;} AttrOracle;',
           'typedef struct ExtOracle {xbytesview raw,oid,value;bool critical;const char* text;} ExtOracle;',
           'typedef struct RecordVector {bool crl;xbytesview der,name,extensions,algorithm,spki,key,modulus,exponent;',
           'const AttrOracle* attrs;size_t attrCount;const ExtOracle* exts;size_t extCount;int64_t kind,hash,mask,keyKind,curve;uint64_t salt,trailer;bool unique;} RecordVector;']
    def span(symbol,data):
        if not data:return '{0}'
        lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};');return '{'+symbol+',sizeof('+symbol+')}'
    normalized=x509.Name([x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COUNTRY_NAME,'cn')]),
        x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COMMON_NAME,'XLANG 离线 FIXTURE'),
                                      x509.NameAttribute(x509.NameOID.ORGANIZATIONAL_UNIT_NAME,'RECORDS')])]).public_bytes()
    normalized_span=span('normalizedNameData',normalized)
    lines.append('static const xbytesview normalizedName='+normalized_span+';')
    records=[]
    for i,row in enumerate(rows):
        der=row['der'];parts=children(der);fields=children(parts[0]);crl=row['crl']
        raw_name=fields[2] if crl else fields[5]
        raw_ext=next((content(f) for f in fields if f[0]==(160 if crl else 163)),b'')
        spki=b'' if crl else fields[6];spki_fields=children(spki) if spki else []
        key=content(spki_fields[1])[1:] if spki else b'';modulus=exponent=b''
        keyKind=0 if crl else ([1,3,3,4,1,6,7][i]);curve=([0,1,2,0,0,0,0][i] if not crl else 0)
        if keyKind==1:modulus,exponent=[content(f).lstrip(b'\0') for f in children(key)]
        row.update(nameDer=raw_name,extensions=raw_ext,algorithm=parts[1],spki=spki,key=key,modulus=modulus,exponent=exponent,
                   keyKind=keyKind,curve=curve,unique=not crl and i>=3 and i!=4,attrs=name_attributes(raw_name),exts=extension_records(raw_ext))
        spans={k:span(f'v{i}_{k}',data) for k,data in [('der',der),('name',raw_name),('extensions',raw_ext),('algorithm',parts[1]),('spki',spki),('key',key),('modulus',modulus),('exponent',exponent)]}
        attrs=[];exts=[]
        for j,a in enumerate(row['attrs']):attrs.append('{'+','.join(span(f'a{i}_{j}_{k}',a[k]) for k in ('raw','oid','value'))+','+str(a['rdn'])+','+json.dumps(oid_text(a['oid']))+'}')
        for j,e in enumerate(row['exts']):exts.append('{'+','.join(span(f'e{i}_{j}_{k}',e[k]) for k in ('raw','oid','value'))+','+str(e['critical']).lower()+','+json.dumps(oid_text(e['oid']))+'}')
        lines.append(f'static const AttrOracle attrs{i}[]={{'+','.join(attrs)+'};')
        lines.append(f'static const ExtOracle exts{i}[]={{'+','.join(exts)+'};')
        records.append('{'+str(crl).lower()+','+','.join(spans.values())+f',attrs{i},'+str(len(attrs))+f',exts{i},'+str(len(exts))+','+
            ','.join(str(row[k]) for k in ('kind','hash','mask','keyKind','curve','salt','trailer'))+','+str(row['unique']).lower()+'}')
        (out/(row['name']+'.der')).write_bytes(der)
    lines.append('static const RecordVector recordVectors[]={'+',\n'.join(records)+'};')
    (out/'x509_record_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8');return rows


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_records_abi.h').read_text()))
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_records_symbols.inc').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_records_contracts.inc').read_text(),re.M))
    assert len(declared)==68 and declared==exported==contracted
    files=[Path(__file__),root/'tools/check_key_format_wire.py',root/'tools/check_x509_owned.py',gcc,tcc,
        root/'test/support/x509_records.c',root/'test/support/x509_records_plan.c',root/'src/compile/xl6_compile_unit.c',
        *root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),*root.glob('src/compile/*.inc'),*root.glob('src/compile/*.h'),
        root/'src/ir/xl6_ir_x509_records_contracts.inc',root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',
        root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h']
    report=dict(Complete=False,Rows=[],Failures=[],Inputs={str(f):sha(f) for f in files},Vectors=len(fixtures(root,out)),
        ResidentEntries=68,Oracle=cryptography.__version__,EveryFixtureSignatureIndependentlyVerified=True,
        TrustAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,NetworkTraffic=False,SecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in (str(gcc),str(tcc)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/x509_records_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert not any(s in modules for s in ('_MODULE_ALL','MODULE_TLS','MODULE_CRYPTO','MODULE_X509_VERIFY','MODULE_X509_PATH','MODULE_X509_STORE'))
    report['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for f in sources:report['Inputs'][str(f)]=sha(f)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=4'] if cc==gcc else []),root/'test/support/x509_records.c',impl,*sources,*libs,'-o',binary])
            assert 'X509 records: 8 valid vectors,' in run(name+'.run',[binary])
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==s for f,s in report['Inputs'].items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in report['Rows'])
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
