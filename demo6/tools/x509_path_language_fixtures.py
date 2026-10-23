"""Ordinary offline ordered-path API fixtures; no network/security campaigns."""
from x509_path_fixtures import fixtures


def language_fixtures(root,out):
    fixtures(root,out);cases=[];literal=lambda b:'b"'+''.join('\\x%02x'%v for v in b)+'"'
    header='time at = time(1893456000000000)\n'
    for name in ('rsa','p256','p384','ed25519'):
        for field in ('root','mid','leaf','custom'):
            header+='bytes '+name+'_'+field+' = '+literal((out/(name+'.'+field+'.der')).read_bytes())+'\n'
    def emit(name,body,data=True):
        path=out/(name+'.xl');path.write_text((header if data else '')+body+'\nprintln("ok")\n');cases.append(path)
    emit('path.enums','''
auto flags=X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose
if (flags & X509PathFlags.requirePurpose) != X509PathFlags.requirePurpose then throw RuntimeError "flags" end
if X509ValidationStatus.valid == X509ValidationStatus.rejected then throw RuntimeError "status" end
if X509PathReason.none == X509PathReason.path then throw RuntimeError "reason" end
''',False)
    emit('path.policy','''
auto policy=X509PathPolicy(at,X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose,X509KeyUsageFlags.digitalSignature,"1.3.6.1.5.5.7.3.1",12,4096)
if policy.at != at or policy.flags != (X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose) then throw RuntimeError "time flags" end
if policy.keyUsage != X509KeyUsageFlags.digitalSignature or policy.purpose != "1.3.6.1.5.5.7.3.1" then throw RuntimeError "usage purpose" end
if policy.maxCertificates != 12 or policy.maxBytes != 4096 then throw RuntimeError "budgets" end
auto plain=X509PathPolicy(at)
if plain.purpose != "" or plain.flags != X509PathFlags.none then throw RuntimeError "defaults" end
''')
    normal=''
    for name in ('rsa','p256','p384','ed25519'):
        normal+='''
func test_NAME() as X509Path
    auto anchor=X509TrustAnchor(Certificate.fromDer(NAME_root))
    array<Certificate> chain=[Certificate.fromDer(NAME_leaf),Certificate.fromDer(NAME_mid)]
    auto policy=X509PathPolicy(at,X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose,X509KeyUsageFlags.digitalSignature,"1.3.6.1.5.5.7.3.1")
    auto outcome=X509Path.check(chain,anchor,policy)
    if outcome.status != X509ValidationStatus.valid or outcome.reason != X509PathReason.none then throw RuntimeError "accepted" end
    outcome.requireValid()
    auto path=outcome.path
    if path == null then throw RuntimeError "path" end
    return path
end
auto NAME_saved=test_NAME()
if NAME_saved.count != 2 or NAME_saved.at(0).toDer() != NAME_leaf or NAME_saved.at(1).toDer() != NAME_mid then throw RuntimeError "owned chain" end
if NAME_saved.anchor.certificate.toDer() != NAME_root then throw RuntimeError "owned anchor" end
if NAME_saved.checkedAt != at or NAME_saved.flags != (X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose) then throw RuntimeError "snapshot" end
if NAME_saved.keyUsage != X509KeyUsageFlags.digitalSignature or NAME_saved.purpose != "1.3.6.1.5.5.7.3.1" then throw RuntimeError "snapshot purpose" end
'''.replace('NAME',name)
    emit('path.normal',normal)
    emit('path.callback','''
class Saved
    Certificate? certificate=null
    X509Extension? extension=null
    int depth=-1
end
func custom() as Saved
    auto saved=Saved()
    array<Certificate> chain=[Certificate.fromDer(ed25519_custom),Certificate.fromDer(ed25519_mid)]
    auto result=X509Path.check(chain,X509TrustAnchor(Certificate.fromDer(ed25519_root)),X509PathPolicy(at),func(Certificate c,X509Extension e,int depth) as bool
        if chain.length != 2 then throw RuntimeError "callback input snapshot" end
        chain.clear()
        saved.certificate=c
        saved.extension=e
        saved.depth=depth
        return true
    end)
    if result.status != X509ValidationStatus.valid then throw RuntimeError "callback" end
    return saved
end
auto saved=custom()
auto certificate=saved.certificate
auto extension=saved.extension
if certificate == null or extension == null then throw RuntimeError "saved" end
if certificate.toDer() != ed25519_custom or not extension.critical or saved.depth != 0 then throw RuntimeError "callback projection" end
array<Certificate> chain=[Certificate.fromDer(ed25519_custom),Certificate.fromDer(ed25519_mid)]
auto anchor=X509TrustAnchor(Certificate.fromDer(ed25519_root))
auto policy=X509PathPolicy(at)
auto rejected=X509Path.check(chain,anchor,policy,func(Certificate c,X509Extension e,int depth) as bool
    return false
end)
if rejected.status != X509ValidationStatus.rejected or rejected.reason != X509PathReason.criticalExtension or rejected.path != null then throw RuntimeError "not handled" end
bool caught=false
try
    auto failed=X509Path.check(chain,anchor,policy,func(Certificate c,X509Extension e,int depth) as bool
        throw RangeError "original path callback"
        return true
    end)
catch RangeError error
    caught=true
end
if not caught then throw RuntimeError "original callback error" end
''')
    emit('path.errors','''
array<Certificate> chain=[Certificate.fromDer(ed25519_leaf),Certificate.fromDer(ed25519_mid)]
auto anchor=X509TrustAnchor(Certificate.fromDer(ed25519_root))
auto rejected=X509Path.check(chain,anchor,X509PathPolicy(time(0)))
if rejected.status != X509ValidationStatus.rejected or rejected.reason != X509PathReason.path or rejected.path != null then throw RuntimeError "time conclusion" end
bool caught=false
try
    rejected.requireValid()
catch error
    caught=true
end
if not caught then throw RuntimeError "requireValid" end
caught=false
try
    auto failed=X509Path.check(chain,anchor,X509PathPolicy(at,X509PathFlags.none,X509KeyUsageFlags.none,"",1))
catch RangeError error
    caught=true
end
if not caught then throw RuntimeError "count budget" end
caught=false
try
    array<Certificate> empty=[]
    auto failed=X509Path.check(empty,anchor,X509PathPolicy(at))
catch ValueError error
    caught=true
end
if not caught then throw RuntimeError "empty" end
''')
    contract=out/'path-contract.xl'
    contract.write_text('''func contracts(array<Certificate> chain,Certificate certificate,time at)
auto anchor=X509TrustAnchor(certificate)
auto copy=anchor.certificate
auto policy=X509PathPolicy(at,X509PathFlags.requireKeyUsage,X509KeyUsageFlags.digitalSignature,"",5,8192)
auto t=policy.at
auto f=policy.flags
auto u=policy.keyUsage
auto p=policy.purpose
auto n=policy.maxCertificates
auto b=policy.maxBytes
auto result=X509Path.check(chain,anchor,policy)
auto status=result.status
auto reason=result.reason
result.requireValid()
auto path=result.path
if path != null then
 auto count=path.count
 auto child=path.at(0)
 auto root=path.anchor
 auto checkedAt=path.checkedAt
 auto flags=path.flags
 auto usage=path.keyUsage
 auto purpose=path.purpose
end
auto handled=X509Path.check(chain,anchor,policy,func(Certificate c,X509Extension e,int d) as bool
 return true
end)
end
''')
    return cases,contract
