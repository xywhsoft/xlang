"""Normal offline trust-store language fixtures, no network/security tests."""
from pathlib import Path
from cryptography.hazmat.primitives import serialization
from x509_path_fixtures import fixtures

def literal(data,text=False):
    return ('"' if text else 'b"')+''.join('\\x%02x'%b for b in data)+'"'

def language_fixtures(root,out):
    fixtures(root,out)
    families=('rsa','p256','p384','ed25519')
    cases=[]
    def put(name,body):
        path=out/(name+'.xl');path.write_text(body,encoding='utf8');cases.append(path)
    put('store.empty','''auto store=TrustStore()
if store.count != 0 or store.byteCount != 0 then throw RuntimeError "empty store" end
auto snapshot=store.snapshot()
if snapshot.count != 0 or snapshot.byteCount != 0 then throw RuntimeError "empty snapshot" end
auto copy=store.clone()
if copy.count != 0 then throw RuntimeError "empty clone" end
''')
    body='''func exercise(bytes rootDer,bytes issuerDer)
    auto root=Certificate.fromDer(rootDer)
    auto issuer=Certificate.fromDer(issuerDer)
    auto store=TrustStore()
    if not store.add(root) then throw RuntimeError "add root" end
    if store.add(root) then throw RuntimeError "duplicate root" end
    auto old=store.snapshot()
    auto copy=store.clone()
    if not copy.add(issuer) then throw RuntimeError "copy add" end
    if copy.count != 2 or store.count != 1 or old.count != 1 then throw RuntimeError "copy isolation" end
    if old.byteCount != store.byteCount or copy.byteCount != rootDer.length + issuerDer.length then throw RuntimeError "byte accounting" end
    if store.merge(copy) != 1 or store.merge(copy.snapshot()) != 0 then throw RuntimeError "merge counts" end
    if store.count != 2 or old.count != 1 then throw RuntimeError "snapshot stable" end
    auto child=old.at(0)
    auto anchor=old.anchorAt(0)
    store.clear()
    copy.clear()
    if store.count != 0 or store.byteCount != 0 or copy.count != 0 then throw RuntimeError "clear" end
    if old.count != 1 or child.toDer() != rootDer or anchor.certificate.toDer() != rootDer then throw RuntimeError "detached views" end
end
'''
    for family in families:
        body+='exercise('+literal((out/(family+'.root.der')).read_bytes())+','+literal((out/(family+'.mid.der')).read_bytes())+')\n'
    put('store.versions',body)
    body='''func checkPem(string text,int firstSize,int fullSize)
    auto store=TrustStore()
    if store.addPem(text,firstSize) != 1 or store.count != 1 then throw RuntimeError "prefix" end
    if store.addPem(text,fullSize+100) != 1 or store.count != 2 then throw RuntimeError "clamped size" end
    if store.addPem(text) != 0 then throw RuntimeError "PEM duplicate" end
    auto snapshot=store.snapshot()
    auto retained=store.at(0)
    auto anchor=store.anchorAt(0)
    if retained.toDer() != anchor.certificate.toDer() then throw RuntimeError "mutable projection" end
    auto caught=false
    try
        store.addPem(text,-2)
    catch RangeError error
        caught=true
    end
    if not caught or store.count != 2 or snapshot.count != 2 then throw RuntimeError "invalid size transaction" end
    caught=false
    try
        auto missing=snapshot.at(-1)
    catch RangeError error
        caught=true
    end
    if not caught then throw RuntimeError "negative snapshot index" end
    caught=false
    try
        auto missing=store.anchorAt(2)
    catch RangeError error
        caught=true
    end
    if not caught then throw RuntimeError "anchor index" end
end
'''
    from cryptography import x509
    for family in families:
        pem=[x509.load_der_x509_certificate((out/(family+'.'+kind+'.der')).read_bytes()).public_bytes(serialization.Encoding.PEM) for kind in ('root','mid')]
        body+='checkPem('+literal(b''.join(pem),True)+','+str(len(pem[0]))+','+str(sum(map(len,pem)))+')\n'
    put('store.pem',body)
    body='''func detached(bytes der) as Certificate
    auto store=TrustStore()
    store.add(Certificate.fromDer(der))
    return store.snapshot().at(0)
end
func exercise(bytes der)
    auto copy=detached(der)
    if copy.toDer() != der then throw RuntimeError "child outlived store" end
end
'''
    for family in families:body+='exercise('+literal((out/(family+'.root.der')).read_bytes())+')\n'
    put('store.lifetime',body)
    put('store.snapshot.only','''func countSnapshot(TrustStoreSnapshot? value) as int
    if value != null then
        return value.count + value.byteCount
    end
    return 0
end
if countSnapshot(null) != 0 then throw RuntimeError "nullable snapshot" end
''')
    contract=out/'store-contract.xl'
    contract.write_text('''func contracts(Certificate certificate,TrustStore store,TrustStoreSnapshot version)
auto empty=TrustStore()
auto copy=store.clone()
auto captured=store.snapshot()
auto count=store.count
auto size=store.byteCount
auto item=store.at(0)
auto anchor=store.anchorAt(0)
auto added=store.add(certificate)
auto parsed=store.addPem("",-1)
auto merged=store.merge(version)
store.clear()
auto frozenCount=version.count
auto frozenBytes=version.byteCount
auto frozenCertificate=version.at(0)
auto frozenAnchor=version.anchorAt(0)
end
''',encoding='utf8')
    return cases,contract
