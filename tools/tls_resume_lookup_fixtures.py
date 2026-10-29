"""Typed resume lookup with ordinary fixed identities and owned binary views."""
from pathlib import Path
from tls_role_context_fixtures import language_fixtures

def fixtures(root,out):
    out=Path(out);roles,_=language_fixtures(root,out)
    prefix=next(p for p in roles if p.stem=='tls.context.p256').read_text().split('array<bytes> protocols=',1)[0]
    common=prefix+r'''
class ResumeLookupState
 TlsServerContext? context=null
 TlsResumeRequest? saved=null
 int calls=0
end
func makeLookup(int mode,ResumeLookupState state) as TlsServerContext
 auto original=makeServer()
 auto issued=time.now()
 auto callback=func(TlsResumeRequest request) as TlsResume?
  state.calls+=1
  state.saved=request
  assert request.serverName == b"localhost" and request.protocolCount == 2
  assert request.protocolAt(0) == b"h2" and request.protocolAt(1) == b"x\0\xff"
  assert request.ticket == b"ticket\0\xff" and request.age <= uint64(4294967295)
  auto detached=request.ticket
  detached[0]=65
  assert request.ticket == b"ticket\0\xff"
  if mode == 1
   return null
  end
  if mode == 2
   throw RangeError("ordinary resume lookup failure")
  end
  return TlsResume.fromParts(TlsVersion.v13,TlsCipherSuite.aes128GcmSha256,
   b"ticket\0\xff",b"01234567890123456789012345678901",issued,120,
   uint64(1),"localhost",b"h2")
 end
 auto context=original.withResumeLookup(callback)
 assert context.hasResumeLookup and context.resumeAgeToleranceMillis == uint64(10000)
 assert not original.hasResumeLookup and not context.withoutResumeLookup().hasResumeLookup
 auto selector=func(TlsServerRequest request) as TlsServerChoice?
  return TlsServerChoice(original.identity)
 end
 context=context.withParameters(original.parameters).withIdentity(original.identity).withProtocols([b"h2",b"x\0\xff"],true).withSelector(selector)
 assert context.hasResumeLookup and context.hasSelector and context.requiresProtocol
 assert context.resumeAgeToleranceMillis == uint64(10000)
 auto strict=original.withResumeLookup(callback,uint64(0))
 assert strict.resumeAgeToleranceMillis == uint64(0)
 int caught=0
 try
  auto invalid=original.withResumeLookup(callback,uint64(4294967296))
 catch RangeError expected
  caught+=1
 end
 assert caught == 1
 state.context=context
 return context
end
export func tls_resume_lookup_context(int mode) as TlsServerContext
 return makeLookup(mode,ResumeLookupState())
end
export func tls_resume_lookup_client() as TlsClientContext
 return makeClient().withProtocols([b"h2",b"x\0\xff"])
end
'''
    context=out/'tls.resume.lookup.xl'
    context.write_text(common+r'''
auto state=ResumeLookupState()
auto context=makeLookup(0,state)
auto server=TlsSession(context)
server.dispose()
assert server.disposed
println("TLS typed resume lookup: immutable contexts, exact age tolerance, owned callback cycle: ok")
''',encoding='utf8')
    listener=out/'tls.resume.lookup.listener.xl'
    listener.write_text('import concurrent\n'+common.replace('class ResumeLookupState\n','class ResumeLookupState\n TlsServer? server=null\n')+r'''
auto state=ResumeLookupState()
auto engine=NetEngine(NetEngineOptions().withWorkers(1).withBackend(NetBackend.select))
engine.start()
auto listener=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),makeLookup(0,state))
state.server=listener
listener.close()
engine.close()
assert engine.wait(10000)
println("TLS typed resume listener: two callback owners and context/listener retirement cycle: ok")
''',encoding='utf8')
    empty=out/'tls.resume.lookup.empty.xl';empty.write_text('assert 2 + 2 == 4\n',encoding='utf8')
    return [context,listener,next(p for p in roles if p.stem=='tls.context.p256'),empty]
