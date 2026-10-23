"""Normal source-level signature results, strict errors and key interoperability."""
import json
from x509_signature_fixtures import fixtures
from check_key_format_wire import vectors
from check_stdlib_x509_data import literal
from cryptography.hazmat.primitives import serialization


def language_fixtures(root,directory):
    rows=fixtures(root,directory)
    def save(name,lines):
        path=directory/(name+'.xl');path.write_text('\n'.join(lines)+'\n',encoding='utf8');return path
    all_lines=[];message_lines=[]
    for i,r in enumerate(rows):
        state='valid' if r['supported'] else 'unsupported'
        body=[f'auto cert=Certificate.fromDer({literal(r["certificate"])})',f'auto issuer=Certificate.fromDer({literal(r["issuer"])})',
            f'auto crl=CertificateRevocationList.fromDer({literal(r["crl"])})',f'auto key=X509PublicKeyInfo.fromSpkiDer({literal(r["spki"])})',
            f'assert key.spkiDer == {literal(r["spki"])}',f'assert cert.checkSignature(issuer) == X509SignatureStatus.{state}',
            f'assert cert.checkSignatureWithKey(key) == X509SignatureStatus.{state}',f'assert crl.checkSignature(issuer) == X509SignatureStatus.{state}',
            f'assert crl.checkSignatureWithKey(key) == X509SignatureStatus.{state}']
        if r['supported']:body+=['cert.verifySignature(issuer)','cert.verifySignatureWithKey(key)','crl.verifySignature(issuer)','crl.verifySignatureWithKey(key)']
        else:
            body+=['int caught=0','try',' cert.verifySignature(issuer)','catch NotImplementedError error',' caught += 1','end',
                'try',' crl.verifySignatureWithKey(key)','catch NotImplementedError error',' caught += 1','end','assert caught == 2']
        all_lines+=['func check'+str(i)+'()']+[' '+x for x in body]+['end']
        body=[f'auto key=X509PublicKeyInfo.fromSpkiDer({literal(r["spki"])})',f'auto optional=X509Algorithm.fromDer({literal(r["algorithm"])}).signatureInfo()',
            'assert optional != null','if optional != null',' auto scheme=optional',f' auto message={literal(r["message"])}',f' auto signature={literal(r["signature"])}',
            ' auto buffer=Buffer(message)',f' assert scheme.check(message,signature,key) == X509SignatureStatus.{state}',
            f' assert scheme.check("a\\0b你",signature,key) == X509SignatureStatus.{state}',f' assert scheme.check(buffer,signature,key) == X509SignatureStatus.{state}',
            f' assert scheme.check(message,signature,key,1000) == X509SignatureStatus.{state}',
            f' assert scheme.check({literal(r["message"]+b"--")},signature,key,6) == X509SignatureStatus.{state}']
        if r['supported']:body+=[' scheme.verify(message,signature,key)',' scheme.verify("a\\0b你",signature,key)',' scheme.verify(buffer,signature,key)']
        body+=['end'];message_lines+=['func check'+str(i)+'()']+[' '+x for x in body]+['end']
    all_lines+=['check'+str(i)+'()' for i in range(len(rows))];all_case=save('verify.certificates',all_lines)
    message_lines+=['check'+str(i)+'()' for i in range(len(rows))];messages=save('verify.messages',message_lines)
    rejected=save('verify.rejected',[f'auto cert=Certificate.fromDer({literal(rows[0]["certificate"])})',
        f'auto different=Certificate.fromDer({literal(rows[3]["issuer"])})','assert cert.checkSignature(different) == X509SignatureStatus.rejected',
        'int caught=0','try',' cert.verifySignature(different)','catch IOError error',' caught += 1','end','assert caught == 1'])
    keys=vectors(root);der=[r['key'].public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo) for r in keys]
    kinds=['rsa','ec','ec','ed25519','x25519','x448'];spki=[]
    for i,encoded in enumerate(der):
        spki += [f'auto key{i}=X509PublicKeyInfo.fromSpkiDer({literal(encoded)})',f'assert key{i}.spkiDer == {literal(encoded)}',f'assert key{i}.kind == X509KeyKind.{kinds[i]}']
    spki+=['int caught=0','try',f' auto bad=X509PublicKeyInfo.fromSpkiDer({literal(der[0])},1)','catch RangeError error',' caught += 1','end',
        'try',f' auto bad=X509PublicKeyInfo.fromSpkiDer({literal(der[0])},-1)','catch ValueError error',' caught += 1','end','assert caught == 2']
    spki_case=save('verify.spki',spki)
    interop=[f'auto rsa=RsaPublicKey.fromSpki({literal(der[0])})','assert X509PublicKeyInfo.fromSpkiDer(rsa.toSpki()).kind == X509KeyKind.rsa',
        f'auto p256=P256Key({literal(keys[1]["raw"])})',f'auto p384=P384Key({literal(keys[2]["raw"])})',f'auto ed=Ed25519Key({literal(keys[3]["raw"])})',
        f'assert X509PublicKeyInfo.fromSpkiDer(p256.publicSpki()).spkiDer == {literal(der[1])}',
        f'assert X509PublicKeyInfo.fromSpkiDer(p384.publicSpki()).spkiDer == {literal(der[2])}',
        f'assert X509PublicKeyInfo.fromSpkiDer(ed.publicSpki()).spkiDer == {literal(der[3])}',
        f'assert X509PublicKeyInfo.fromSpkiDer(crypto.x25519PublicToSpki({literal(keys[4]["public"])})).spkiDer == {literal(der[4])}',
        f'assert X509PublicKeyInfo.fromSpkiDer(crypto.x448PublicToSpki({literal(keys[5]["public"])})).spkiDer == {literal(der[5])}']
    interop_case=save('verify.interop',interop)
    enums=save('verify.enums',['array<X509SignatureStatus> states=[X509SignatureStatus.valid,X509SignatureStatus.rejected,X509SignatureStatus.unsupported]',
        'assert states[0] == X509SignatureStatus.valid and states[2] == X509SignatureStatus.unsupported'])
    threaded=save('verify.threads',['func detached(bytes spki) as X509PublicKeyInfo',' return X509PublicKeyInfo.fromSpkiDer(spki)','end',
        'func reader(X509PublicKeyInfo key,Certificate cert) as func() as int',' return func() as int',
        '  if cert.checkSignatureWithKey(key) == X509SignatureStatus.valid then return 0 end','  return 1',' end','end',
        f'auto key=detached({literal(rows[3]["spki"])})',f'auto cert=Certificate.fromDer({literal(rows[3]["certificate"])})',
        'auto first=thread(reader(key,cert))','auto second=thread(reader(key,cert))','assert first.waitTimeout(30000) and second.waitTimeout(30000)',
        'assert first.exitCode() == 0 and second.exitCode() == 0'])
    contract=['int marker=1','func contracts(Certificate cert,CertificateRevocationList crl,X509SignatureInfo scheme,X509PublicKeyInfo key,Buffer buffer)',
        ' auto parsed=X509PublicKeyInfo.fromSpkiDer(b"der")',
        ' auto a=cert.checkSignature(cert)',' auto b=cert.checkSignatureWithKey(key)',' cert.verifySignature(cert)',' cert.verifySignatureWithKey(key)',
        ' auto c=crl.checkSignature(cert)',' auto d=crl.checkSignatureWithKey(key)',' crl.verifySignature(cert)',' crl.verifySignatureWithKey(key)',
        ' auto e=scheme.check(b"message",b"signature",key)',' auto f=scheme.check("message",b"signature",key)',' auto g=scheme.check(buffer,b"signature",key)',
        ' scheme.verify(b"message",b"signature",key)',' scheme.verify("message",b"signature",key)',' scheme.verify(buffer,b"signature",key)','end']
    errors=save('verify.errors',['int caught=0','try',' try',
        f'  auto limited=X509PublicKeyInfo.fromSpkiDer({literal(der[0])},1)',
        ' catch MemoryError wrong','  assert false',' end','catch RangeError expected',' caught += 1','end','assert caught == 1'])
    return [all_case,messages,rejected,spki_case,interop_case,enums,threaded,errors],save('verify.contract',contract)
