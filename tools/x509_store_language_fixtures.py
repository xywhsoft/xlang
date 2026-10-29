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
    body='''func encoded(bytes der,string pem)
    auto store=TrustStore()
    auto old=store.snapshot()
    if store.addEncoded(der,der.length+100) != 1 then throw RuntimeError "exact DER bytes" end
    if store.addEncoded(Buffer(der)) != 0 then throw RuntimeError "Buffer duplicate" end
    store.clear()
    if store.addEncoded(pem) != 1 or store.addEncoded(der) != 0 then throw RuntimeError "PEM auto format" end
    auto caught=false
    try
        store.addEncoded(der,-2)
    catch RangeError error
        caught=true
    end
    if not caught or old.count != 0 or store.count != 1 or store.at(0).toDer() != der then throw RuntimeError "encoded transaction" end
end
'''
    for family in families:
        der=(out/(family+'.root.der')).read_bytes()
        pem=x509.load_der_x509_certificate(der).public_bytes(serialization.Encoding.PEM)
        body+='encoded('+literal(der)+','+literal(pem,True)+')\n'
    put('store.encoded',body)
    der=(out/'p256.root.der').read_bytes();pem=x509.load_der_x509_certificate(der).public_bytes(serialization.Encoding.PEM)
    owned=out/'owned-trust.pem';owned.write_bytes(pem)
    name=literal(owned.as_posix().encode(),True)
    put('store.sources.file','''func checkFile(string path,int size)
    auto store=TrustStore.fromFile(path,size)
    auto old=store.snapshot()
    if store.count != 1 or store.addFile(path,size+10) != 0 then throw RuntimeError "file source" end
    auto caught=false
    try
        store.addFile(path,size-1)
    catch Error error
        caught=true
    end
    if not caught or store.count != 1 or old.count != 1 then throw RuntimeError "hard read limit" end
    caught=false
    try
        store.addFile(path,0)
    catch Error error
        caught=true
    end
    if not caught or store.count != 1 then throw RuntimeError "zero hard limit" end
    caught=false
    try
        store.addFile(path+"\\0suffix")
    catch ValueError error
        caught=true
    end
    if not caught or store.count != 1 then throw RuntimeError "path NUL boundary" end
end
checkFile('''+name+','+str(len(pem))+')\n')
    put('store.sources.vfs','''func checkVfs(bytes der)
    auto memory=MemoryFS()
    if not memory.put("trust.der",der) or not memory.seal() then throw RuntimeError "memory source" end
    auto fs=Vfs()
    auto mount=fs.mount("/",memory)
    auto store=TrustStore.fromVfs(fs,"/trust.der",der.length)
    auto old=store.snapshot()
    if store.count != 1 or store.addVfs(fs,"/trust.der") != 0 then throw RuntimeError "VFS source" end
    auto caught=false
    try
        store.addVfs(fs,"/trust.der",der.length-1)
    catch Error error
        caught=true
    end
    if not caught or store.count != 1 or old.count != 1 then throw RuntimeError "VFS hard limit" end
    caught=false
    try
        store.addVfs(fs,"/trust.der",-1)
    catch RangeError error
        caught=true
    end
    if not caught or store.count != 1 then throw RuntimeError "VFS negative limit" end
end
checkVfs('''+literal(der)+')\n')
    put('store.sources.system','''auto snapshot=TrustStoreSnapshot.fromSystem()
if snapshot.count <= 0 or snapshot.byteCount <= 0 then throw RuntimeError "OS roots" end
auto root=snapshot.at(0).toDer()
auto store=TrustStore.fromSystem()
if store.count <= 0 or store.byteCount <= 0 then throw RuntimeError "mutable OS roots" end
auto imported=TrustStore()
if imported.addSystem() <= 0 or imported.count <= 0 then throw RuntimeError "explicit system merge" end
store.clear()
if snapshot.at(0).toDer() != root then throw RuntimeError "detached OS snapshot" end
''')
    contract=out/'store-contract.xl'
    contract.write_text('''func contracts(Certificate certificate,TrustStore store,TrustStoreSnapshot version,Vfs filesystem,Buffer buffer,bytes data)
auto empty=TrustStore()
auto copy=store.clone()
auto captured=store.snapshot()
auto count=store.count
auto size=store.byteCount
auto item=store.at(0)
auto anchor=store.anchorAt(0)
auto added=store.add(certificate)
auto parsed=store.addPem("",-1)
auto encoded=store.addEncoded("",-1)
auto encodedBytes=store.addEncoded(data,-1)
auto encodedBuffer=store.addEncoded(buffer,-1)
auto fileCount=store.addFile("owned.pem",67108864)
auto vfsCount=store.addVfs(filesystem,"/owned.pem",67108864)
auto systemCount=store.addSystem()
auto systemStore=TrustStore.fromSystem()
auto systemSnapshot=TrustStoreSnapshot.fromSystem()
auto merged=store.merge(version)
store.clear()
auto frozenCount=version.count
auto frozenBytes=version.byteCount
auto frozenCertificate=version.at(0)
auto frozenAnchor=version.anchorAt(0)
end
''',encoding='utf8')
    return cases,contract
