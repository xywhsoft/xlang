"""Typed selectors over normal fixed-certificate, in-memory TLS sessions."""
from pathlib import Path
from tls_role_context_fixtures import language_fixtures as roles


def fixtures(root,out):
    out=Path(out);roleCases,_=roles(root,out);cases=[]
    common='''
class RoutingState
 int calls = 0
 TlsServerRequest? saved = null
end
func deliver(TlsSession source, TlsSession target)
 while source.pendingCipher > 0
  auto chunk=source.takeCipher(317)
  assert target.feed(chunk) == TlsResult.ok
 end
end
func handshake(TlsSession client,TlsSession server)
 int rounds=0
 while not client.ready or not server.ready
  auto left=client.drive()
  assert left == TlsResult.ok or left == TlsResult.wouldBlock
  deliver(client,server)
  auto right=server.drive()
  assert right == TlsResult.ok or right == TlsResult.wouldBlock
  deliver(server,client)
  rounds+=1
  assert rounds < 256
 end
end
func makeSelector(TlsIdentity identity,RoutingState state) as func(TlsServerRequest) as TlsServerChoice?
 return func(TlsServerRequest request) as TlsServerChoice?
  state.calls+=1
  state.saved=request
  assert request.serverName == b"localhost" and request.protocolCount == 2
  assert request.protocolAt(0) == b"x\\0\\xff" and request.protocolAt(1) == b"h2"
  auto detached=request.protocolAt(0)
  detached[0]=65
  assert request.protocolAt(0) == b"x\\0\\xff"
  auto automatic=TlsServerChoice(identity,uint64(1234567))
  assert not automatic.hasProtocolIndex and automatic.cookie == uint64(1234567)
  int caught=0
  try
   auto unavailable=automatic.protocolIndex
  catch RuntimeError expected
   caught+=1
  end
  try
   auto invalid=automatic.withProtocolIndex(-2)
  catch RangeError expected
   caught+=1
  end
  assert caught == 2
  auto explicitChoice=automatic.withProtocolIndex(1)
  assert explicitChoice.hasProtocolIndex and explicitChoice.protocolIndex == 1
  assert not automatic.hasProtocolIndex and explicitChoice.identity.count == 2
  return explicitChoice
 end
end
func makeDynamic(RoutingState state) as TlsServerContext
 auto original=makeServer()
 auto callback=makeSelector(original.identity,state)
 auto dynamic=TlsServerContext(original.parameters,callback)
 assert dynamic.hasSelector
 int caught=0
 try
  auto absent=dynamic.identity
 catch RuntimeError expected
  caught+=1
 end
 assert caught == 1
 array<bytes> protocols=[b"x\\0\\xff",b"h2"]
 auto rebound=original.withSelector(callback).withParameters(dynamic.parameters).withIdentity(original.identity).withProtocols(protocols,true)
 assert rebound.hasSelector and rebound.requiresProtocol and not original.hasSelector
 return dynamic.withProtocols(protocols,true).withParameters(rebound.parameters)
end
array<bytes> protocols=[b"x\\0\\xff",b"h2"]
auto state=RoutingState()
auto client=TlsSession(makeClient().withProtocols(protocols),"localhost")
auto server=TlsSession(makeDynamic(state))
handshake(client,server)
assert state.calls == 1 and client.protocol == b"h2" and server.protocol == b"h2"
assert server.serverCookie == uint64(1234567)
auto saved=state.saved as TlsServerRequest
auto status,written=client.write(b"a\\0b\\xff")
assert status == TlsResult.ok and written == uint64(4)
deliver(client,server)
auto progress=server.drive()
auto readStatus,data=server.read()
assert readStatus == TlsResult.ok and data == b"a\\0b\\xff"
server.dispose()
client.dispose()
assert saved.serverName == b"localhost" and saved.protocolAt(0) == b"x\\0\\xff"
assert state.calls == 1
println("TLS typed selector: captured identity, detached request, choice, caught errors and handshake: ok")
'''
    for family in ['rsa','p256','p384','ed25519']:
        prefix=next(p for p in roleCases if p.stem=='tls.context.'+family).read_text().split('array<bytes> protocols=',1)[0]
        path=out/('tls.selector.'+family+'.xl');path.write_text(prefix+common,encoding='utf8');cases.append(path)
    prefix=next(p for p in roleCases if p.stem=='tls.context.p256').read_text().split('array<bytes> protocols=',1)[0]
    for kind,body,expected in [('reject','return null','Error'),('throw','throw RangeError("selector-first-error")','RangeError')]:
        text=prefix+'''
int calls=0
auto callback=func(TlsServerRequest request) as TlsServerChoice?
 calls+=1
 assert request.serverName == b"localhost"
 '''+body+'''
end
auto client=TlsSession(makeClient(),"localhost")
auto server=TlsSession(TlsServerContext(makeServer().parameters,callback))
int caught=0
try
 int rounds=0
 while calls == 0
  auto sent=client.drive()
  while client.pendingCipher > 0
   assert server.feed(client.takeCipher()) == TlsResult.ok
  end
  auto selected=server.drive()
  rounds+=1
  assert rounds < 16
 end
catch '''+expected+''' expected
 caught+=1
end
assert calls == 1 and caught == 1 and server.state == TlsState.failed
server.dispose()
client.dispose()
println("TLS typed selector: '''+kind+''' and cleanup: ok")
'''
        path=out/('tls.selector.'+kind+'.xl');path.write_text(text,encoding='utf8');cases.append(path)
    empty=out/'tls.selector.empty.xl';empty.write_text('int result=0\nassert result == 0\n',encoding='utf8')
    return cases,empty
