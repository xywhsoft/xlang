"""Independent ordinary X.509 record oracles shared by source acceptance."""
import json,re
from check_x509_records import fixtures,children,content,oid_text
from check_stdlib_x509_data import literal


def language_fixtures(root,directory):
    rows=fixtures(root,directory)
    combined='''func checkAttribute(X509NameAttribute item, bytes raw, bytes oidBytes, string oid, uint64 rdn, bytes value)
 assert item.toDer() == raw
 assert item.oid == oid and item.oidContent == oidBytes
 assert item.rdnIndex == rdn
 assert item.valueDer == value and item.value.raw == value
end
func checkExtension(X509Extension item, bytes raw, bytes oidBytes, string oid, bool critical, bytes value)
 assert item.toDer() == raw
 assert item.oid == oid and item.oidContent == oidBytes
 assert item.critical == critical and item.value == value
end
func checkAlgorithm(X509Algorithm item, bytes raw, bytes oidBytes, string oid, bytes parameters, bool hasParameters, X509SignatureKind kind, X509Digest digest, X509Digest maskDigest, uint64 salt, uint64 trailer)
 assert item.toDer() == raw
 assert item.oid == oid and item.oidContent == oidBytes
 assert item.hasParameters == hasParameters and item.parametersDer == parameters
 assert X509Algorithm.fromDer(item.toDer()).toDer() == raw
 auto info = item.signatureInfo()
 assert info != null
 if info != null
  assert info.kind == kind and info.digest == digest and info.maskDigest == maskDigest
  assert info.saltBytes == salt and info.trailer == trailer
  array<X509SignatureKind> kinds = [info.kind]
  assert kinds[0] == kind
 end
end
func checkKey(X509PublicKeyInfo item, bytes spki, bytes key, bytes modulus, bytes exponent, bytes algorithm, X509KeyKind kind, X509Curve curve)
 assert item.spkiDer == spki and item.keyBytes == key
 assert item.modulus == modulus and item.exponent == exponent
 assert item.algorithm.toDer() == algorithm
 assert item.kind == kind and item.curve == curve
 array<X509KeyKind> kinds = [item.kind]
 assert kinds[0] == kind
end
func detachedAttribute(bytes name) as X509NameAttribute
 return X509Name.fromDer(name).at(0)
end
func detachedValue(bytes name) as DerValue
 return X509Name.fromDer(name).at(0).value
end
'''.splitlines()
    kinds={1:'rsa',3:'ec',4:'ed25519',6:'x25519',7:'x448'}
    signatures={1:'rsaPkcs1',2:'rsaPss',3:'ecdsa',4:'ed25519'}
    hashes={0:'none',3:'sha256',4:'sha384'}
    for index,row in enumerate(rows):
        v='v'+str(index);n='n'+str(index);x='x'+str(index);a='a'+str(index);cls='CertificateRevocationList' if row['crl'] else 'Certificate'
        lines=[f'auto {v} = {cls}.fromDer({literal(row["der"])})',f'auto {n} = {v}.'+('issuerName' if row['crl'] else 'subjectName')+'()',
               f'assert {n}.count == {len(row["attrs"])}',f'assert {n}.toDer() == {literal(row["nameDer"])}',
               f'assert {n}.equals(X509Name.fromDer({n}.toDer()))',f'assert {n}.isWithin({v}.issuerName())',
               f'assert {n}.find("1.2.3.99") == null',f'assert {n}.findOid(b"\\x2a\\x03\\x63") == null']
        for j,attr in enumerate(row['attrs']):
            item=f'n{index}a{j}';oid=oid_text(attr['oid'])
            lines += [f'checkAttribute({n}.at({j}),{literal(attr["raw"])},{literal(attr["oid"])},"{oid}",{attr["rdn"]},{literal(attr["value"])})',
                      f'auto {item}Found = {n}.find("{oid}")',f'assert {item}Found != null',
                      f'if {item}Found != null then assert {item}Found.oid == "{oid}" end',
                      f'auto {item}Bytes = {n}.findOid({literal(attr["oid"])})',f'assert {item}Bytes != null']
        lines += [f'auto {x} = {v}.extensions()',f'assert {x}.count == {len(row["exts"])}',f'assert {x}.toDer() == {literal(row["extensions"])}',
                  f'assert X509Extensions.fromDer({x}.toDer()).count == {x}.count',f'assert {x}.find("1.2.3.99") == null',f'assert {x}.findOid(b"\\x2a\\x03\\x63") == null']
        for j,ext in enumerate(row['exts']):
            item=f'e{index}a{j}';oid=oid_text(ext['oid'])
            lines += [f'checkExtension({x}.at({j}),{literal(ext["raw"])},{literal(ext["oid"])},"{oid}",'+str(ext['critical']).lower()+f',{literal(ext["value"])})',
                      f'assert {x}.find("{oid}") != null',f'assert {x}.findOid({literal(ext["oid"])}) != null']
        parts=children(row['algorithm']);params=parts[1] if len(parts)>1 else b''
        lines += [f'auto {a} = {v}.signatureAlgorithm',f'assert {v}.tbsSignatureAlgorithm.toDer() == {a}.toDer()',
                  f'checkAlgorithm({a},{literal(row["algorithm"])},{literal(content(parts[0]))},"{oid_text(content(parts[0]))}",{literal(params)},'+str(bool(params)).lower()+
                  f',X509SignatureKind.{signatures[row["kind"]]},X509Digest.{hashes[row["hash"]]},X509Digest.{hashes[row["mask"]]},{row["salt"]},{row["trailer"]})']
        if not row['crl']:
            key=f'key{index}';keyalg=children(row['spki'])[0]
            lines += [f'checkKey({v}.keyInfo,{literal(row["spki"])},{literal(row["key"])},{literal(row["modulus"])},{literal(row["exponent"])},{literal(keyalg)},X509KeyKind.{kinds[row["keyKind"]]},X509Curve.'+{0:'unknown',1:'p256',2:'p384'}[row['curve']]+')',
                      f'assert {v}.hasIssuerUniqueId == '+str(row['unique']).lower(),f'assert {v}.hasSubjectUniqueId == '+str(row['unique']).lower(),
                      f'assert {v}.issuerUniqueId == {literal(bytes.fromhex("a0") if row["unique"] else b"")}',
                      f'assert {v}.subjectUniqueId == {literal(bytes.fromhex("0058") if row["unique"] else b"")}',
                      f'assert {v}.issuerUniqueIdUnusedBits == '+str(3 if row['unique'] else 0),f'assert {v}.subjectUniqueIdUnusedBits == 0']
        combined += [f'func check{index}()']+[' '+line for line in lines]+['end']
    combined += ['check'+str(index)+'()' for index in range(len(rows))]
    combined += [f'assert detachedAttribute({literal(rows[0]["nameDer"])}).toDer() == {literal(rows[0]["attrs"][0]["raw"])}',
                 f'assert detachedValue({literal(rows[0]["nameDer"])}).raw == {literal(rows[0]["attrs"][0]["value"])}']
    combined += [f'auto n0 = X509Name.fromDer({literal(rows[0]["nameDer"])})',
                 f'auto x0 = X509Extensions.fromDer({literal(rows[0]["extensions"])})',
                 f'auto a0 = X509Algorithm.fromDer({literal(rows[0]["algorithm"])})',
                 'assert X509Algorithm.fromDer(b"\\x30\\x05\\x06\\x03\\x2a\\x03\\x04").signatureInfo() == null',
                 'assert X509Extensions.fromDer(b"").count == 0','int limits = 0',
                 'try',' auto bad = n0.at(-1)','catch RangeError',' limits += 1','end',
                 'try',' auto bad = X509Name.fromDer(n0.toDer(),0,1)','catch RangeError',' limits += 1','end',
                 'try',' auto bad = X509Extensions.fromDer(x0.toDer(),0,1)','catch RangeError',' limits += 1','end',
                 'try',' auto bad = X509Algorithm.fromDer(a0.toDer(),-1)','catch ValueError',' limits += 1','end','assert limits == 4']
    def save(name,lines):
        path=directory/(name+'.xl');path.write_text('\n'.join(lines)+'\n',encoding='utf8');return path
    all_source=save('records.all',combined)
    row=rows[3];name=row['nameDer'];rawext=row['extensions']
    reads=save('records.reads',[f'auto name = X509Name.fromDer({literal(name)})','assert name.count == 3',
        'assert name.at(0).value.raw == '+literal(row['attrs'][0]['value']),f'auto exts = X509Extensions.fromDer({literal(rawext)})',
        'assert exts.count == 2','assert exts.at(1).value == b"\\x04\\x04a\\0bc"'])
    compare=save('records.compare',[f'auto name = X509Name.fromDer({literal(name)})','assert name.equals(X509Name.fromDer(name.toDer()))','assert name.isWithin(name)'])
    metadata=save('records.metadata',[f'auto algorithm = X509Algorithm.fromDer({literal(rows[4]["algorithm"])})','auto info = algorithm.signatureInfo()',
        'assert info != null','if info != null',' assert info.kind == X509SignatureKind.rsaPss',' assert info.digest == X509Digest.sha256',' assert info.maskDigest == X509Digest.sha384',' assert info.saltBytes == 20','end'])
    time=save('records.time',['assert DerReader(b"\\x17\\x0d200101000000Z").read().asTime() == time(1577836800000000)',
                              'assert DerReader(b"\\x18\\x0f20200101000000Z").read().asTime() == time(1577836800000000)'])
    enums=save('records.enums',['array<X509Curve> curves = [X509Curve.p256, X509Curve.p384]','assert curves[0] == X509Curve.p256',
        'array<X509Digest> digests = [X509Digest.none, X509Digest.sha512]','assert digests[1] == X509Digest.sha512'])
    threaded=save('records.threads',['func reader(X509Name name, X509Extensions exts) as func() as int',' return func() as int',
        '  if name.at(0).oid == "2.5.4.6" and exts.at(1).value == b"\\x04\\x04a\\0bc" then return 0 end','  return 1',' end','end',
        f'auto name = X509Name.fromDer({literal(name)})',f'auto exts = X509Extensions.fromDer({literal(rawext)})',
        'auto first = thread(reader(name,exts))','auto second = thread(reader(name,exts))',
        'assert first.waitTimeout(30000) and second.waitTimeout(30000)','assert first.exitCode() == 0 and second.exitCode() == 0'])
    owners={'xl6X509Name':'n','xl6X509Attribute':'attr','xl6X509Extensions':'exts','xl6X509Extension':'ext',
            'xl6X509Algorithm':'alg','xl6X509SignatureInfo':'sig','xl6X509PublicKeyInfo':'key','xl6X509Certificate':'cert','xl6X509Crl':'crl','xl6X509DerValue':'der'}
    types={'n':'X509Name','attr':'X509NameAttribute','exts':'X509Extensions','ext':'X509Extension','alg':'X509Algorithm',
           'sig':'X509SignatureInfo','key':'X509PublicKeyInfo','cert':'Certificate','crl':'CertificateRevocationList','der':'DerValue'}
    code=['int marker = 1','func contracts('+','.join(kind+' '+var for var,kind in types.items())+')']
    for file in ['xl6_stdlib_x509_records.inc','xl6_stdlib_x509_certificate_records.inc','xl6_stdlib_x509_crl_records.inc','xl6_stdlib_x509_der_time.inc']:
        for line in (root/'src/stdlib'/file).read_text().splitlines():
            if 'extern ' not in line or 'destroy()' in line:continue
            declaration=json.loads(line.strip().rstrip(';')).strip();native=re.search(r'extern "(\w+)"',declaration)[1]
            var=next(value for prefix,value in owners.items() if native.startswith(prefix));match=re.search(r'(func|get) (\w+)\(([^)]*)\)',declaration);kind,method,parameters=match.groups()
            args=[]
            for parameter in filter(None,parameters.split(',')):
                if '=' in parameter:continue
                typ=parameter.strip().split()[0];args.append({'bytes':'b"der"','string':'"1.2.3"','int':'0','X509Name':'n'}[typ])
            expr=(types[var] if 'static extern' in declaration else var)+'.'+method+('('+','.join(args)+')' if kind=='func' else '')
            code.append(' auto result'+str(len(code))+' = '+expr)
    code.append('end');contract=save('records.contract',code)
    return [all_source,reads,compare,metadata,time,enums,threaded],contract
