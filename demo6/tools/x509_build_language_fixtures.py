"""Normal offline issuer/candidate-building fixtures; no network/security tests."""
from pathlib import Path

def add_build_cases(root,out):
    cases=[];literal=lambda data:'b"'+''.join('\\x%02x'%b for b in data)+'"'
    def put(name,body):
        path=out/(name+'.xl');path.write_text(body+'\nprintln("ok")\n',encoding='utf8');cases.append(path)
    def data(family,kind):return literal((out/(family+'.'+kind+'.der')).read_bytes())
    issuer='''func exercise(bytes rootDer,bytes midDer,bytes leafDer)
    auto root=Certificate.fromDer(rootDer)
    auto mid=Certificate.fromDer(midDer)
    auto leaf=Certificate.fromDer(leafDer)
    if not leaf.matchesIssuer(mid) or not mid.matchesIssuer(root) or leaf.matchesIssuer(root) then throw RuntimeError "issuer candidates" end
end
'''
    for family in ('rsa','p256','p384','ed25519'):issuer+='exercise('+','.join(data(family,k) for k in ('root','mid','leaf'))+')\n'
    put('build.issuer',issuer)
    normal='''func detached(bytes rootDer,bytes midDer,bytes leafDer) as X509Path
    auto store=TrustStore()
    auto root=Certificate.fromDer(rootDer)
    store.add(root)
    auto version=store.snapshot()
    auto target=Certificate.fromDer(leafDer)
    auto mid=Certificate.fromDer(midDer)
    array<Certificate> candidates=[root,target,mid,mid]
    auto policy=X509PathPolicy(time(1893456000000000),X509PathFlags.requireKeyUsage | X509PathFlags.requirePurpose,X509KeyUsageFlags.digitalSignature,"1.3.6.1.5.5.7.3.1")
    auto mutableResult=X509Path.build(target,candidates,store,policy)
    if mutableResult.status != X509ValidationStatus.valid then throw RuntimeError "mutable store entry" end
    auto result=X509Path.build(target,candidates,version,policy)
    result.requireValid()
    if result.status != X509ValidationStatus.valid or result.reason != X509PathReason.none or result.depth != -1 then throw RuntimeError "built outcome" end
    auto path=result.path
    if path == null then throw RuntimeError "missing built path" end
    store.clear()
    candidates.clear()
    if path.count != 2 or path.at(0).toDer() != leafDer or path.at(1).toDer() != midDer or path.anchor.certificate.toDer() != rootDer then throw RuntimeError "selected path only" end
    if path.checkedAt != policy.at or path.flags != policy.flags or path.keyUsage != policy.keyUsage or path.purpose != policy.purpose then throw RuntimeError "built policy" end
    return path
end
func exercise(bytes rootDer,bytes midDer,bytes leafDer)
    auto path=detached(rootDer,midDer,leafDer)
    if path.at(0).toDer() != leafDer or path.anchor.certificate.toDer() != rootDer then throw RuntimeError "detached built path" end
end
'''
    for family in ('rsa','p256','p384','ed25519'):normal+='exercise('+','.join(data(family,k) for k in ('root','mid','leaf'))+')\n'
    put('build.normal',normal)
    header='time at=time(1893456000000000)\n'
    for kind in ('root','mid','leaf','custom'):header+='bytes '+kind+'Der='+data('ed25519',kind)+'\n'
    header+='''auto store=TrustStore()
store.add(Certificate.fromDer(rootDer))
auto version=store.snapshot()
auto target=Certificate.fromDer(leafDer)
array<Certificate> candidates=[Certificate.fromDer(midDer)]
auto policy=X509PathPolicy(at)
'''
    errors=header+'''array<Certificate> none=[]
auto direct=X509Path.build(Certificate.fromDer(midDer),none,version,policy)
if direct.status != X509ValidationStatus.valid then throw RuntimeError "direct anchor without issuers" end
auto absent=X509Path.build(target,none,version,policy)
if absent.status != X509ValidationStatus.rejected or absent.reason != X509PathReason.build or absent.depth != -1 or absent.path != null then throw RuntimeError "missing issuer" end
auto empty=X509Path.build(target,candidates,TrustStore().snapshot(),policy)
if empty.status != X509ValidationStatus.rejected or empty.reason != X509PathReason.build or empty.depth != -1 or empty.path != null then throw RuntimeError "empty trust conclusion" end
auto expired=X509Path.build(target,candidates,version,X509PathPolicy(time(0)))
if expired.status != X509ValidationStatus.rejected or expired.reason != X509PathReason.validityTime or expired.depth != 0 or expired.path != null then throw RuntimeError "built time conclusion" end
bool caught=false
try
    empty.requireValid()
catch error
    caught=true
end
if not caught then throw RuntimeError "negative requireValid" end
'''
    attempts=[('X509Path.build(target,candidates,version,X509PathPolicy(at,X509PathFlags.none,X509KeyUsageFlags.none,"",1))','RangeError'),
        ('X509Path.build(target,candidates,version,X509PathPolicy(at,X509PathFlags.none,X509KeyUsageFlags.none,"",64,1))','RangeError'),
        ('X509Path.build(target,candidates,version,policy,1)','RangeError'),
        ('X509Path.build(target,candidates,version,policy,4096,1)','RangeError'),
        ('X509Path.build(target,candidates,version,policy,-1)','ValueError'),
        ('X509Path.build(target,candidates,version,policy,4096,-1)','ValueError')]
    for expression,kind in attempts:errors+='caught=false\ntry\n    auto failed='+expression+'\ncatch '+kind+' error\n    caught=true\nend\nif not caught then throw RuntimeError "build budget" end\n'
    put('build.errors',errors)
    callback=header+'''class BuildSaved
    Certificate? certificate=null
    X509Extension? extension=null
    int calls=0
    int depth=-1
end
target=Certificate.fromDer(customDer)
candidates=[target,Certificate.fromDer(midDer)]
auto unhandled=X509Path.build(target,candidates,version,policy)
if unhandled.status != X509ValidationStatus.rejected or unhandled.reason != X509PathReason.criticalExtension or unhandled.path != null then throw RuntimeError "critical conclusion" end
auto saved=BuildSaved()
auto accepted=X509Path.build(target,candidates,store,policy,func(Certificate certificate,X509Extension extension,int depth) as bool
    if candidates.length != 2 then throw RuntimeError "candidate capture" end
    candidates.clear()
    store.clear()
    saved.certificate=certificate
    saved.extension=extension
    saved.depth=depth
    saved.calls+=1
    return true
end)
if accepted.status != X509ValidationStatus.valid or saved.calls != 1 or store.count != 0 or version.count != 1 then throw RuntimeError "build callback" end
auto captured=saved.certificate
auto extension=saved.extension
if captured == null or extension == null then throw RuntimeError "owned callback projection" end
if captured.toDer() != customDer or not extension.critical or saved.depth != 0 then throw RuntimeError "built callback fields" end
candidates=[target,Certificate.fromDer(midDer)]
bool caught=false
try
    auto failed=X509Path.build(target,candidates,version,policy,func(Certificate c,X509Extension e,int depth) as bool
        throw RangeError "original builder callback"
        return true
    end)
catch RangeError error
    caught=true
end
if not caught then throw RuntimeError "builder callback failure" end
'''
    put('build.callback',callback)
    unsupported='''auto store=TrustStore()
store.add(Certificate.fromDer(ROOT))
array<Certificate> none=[]
auto result=X509Path.build(Certificate.fromDer(TARGET),none,store.snapshot(),X509PathPolicy(time(1893456000000000)))
if result.status != X509ValidationStatus.unsupported or result.reason != X509PathReason.signature or result.depth != 0 or result.path != null then throw RuntimeError "unsupported build backend" end
'''.replace('ROOT',literal((out/'unsupported.root.der').read_bytes())).replace('TARGET',literal((out/'unsupported.target.der').read_bytes()))
    put('build.unsupported',unsupported)
    return cases
