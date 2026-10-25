"""Ordinary self-created loopback TLS with the default typed selector API."""
from pathlib import Path
from tls_role_context_fixtures import language_fixtures
from tls_metadata_fixtures import fixtures as metadata

def fixtures(root,out):
    out=Path(out);roles,_=language_fixtures(root,out)
    prefix=next(p for p in roles if p.stem=='tls.context.p256').read_text().split('array<bytes> protocols=',1)[0]
    source='import concurrent\n'+prefix+r'''
class RoutingState
 int calls=0
 TlsServerRequest? saved=null
 TlsServer? server=null
end
func createServer(NetEngine engine,RoutingState state) as TlsServer
 auto original=makeServer()
 auto identity=original.identity
 auto selectIdentity=func(TlsServerRequest request) as TlsServerChoice?
  state.calls+=1
  state.saved=request
  assert request.serverName == b"localhost" and request.protocolCount == 2
  assert request.protocolAt(0) == b"x\0\xff" and request.protocolAt(1) == b"h2"
  auto copy=request.protocolAt(0)
  copy[0]=65
  assert request.protocolAt(0) == b"x\0\xff"
  return TlsServerChoice(identity,uint64(77123)).withProtocolIndex(0)
 end
 auto context=TlsServerContext(original.parameters,selectIdentity).withProtocols([b"h2",b"x\0\xff"],true)
 auto server=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),context)
 // A real language/native cycle is deliberately left to module retirement.
 state.server=server
 return server
end
async func exercise(NetEngine engine) as RoutingState
 auto state=RoutingState()
 auto server=createServer(engine,state)
 int round=0
 while round < 3
  auto accepted=server.acceptAsyncTimeout(5000)
  auto client=TlsStream(engine,server.local(),makeClient().withProtocols([b"x\0\xff",b"h2"]),"localhost")
  assert await client.waitAsyncTimeout(0,5000)
  auto peer=await accepted
  auto info=await peer.infoAsync()
  assert info.ready and info.protocol == b"h2" and info.hasServerCookie and info.serverCookie == uint64(77123)
  assert state.calls == round+1
  assert client.send(b"a\0b\xff",100,5000) == 4
  assert peer.recv(4,5000) == b"a\0b\xff"
  client.abort()
  peer.abort()
  round+=1
 end
 assert server.stats().accepted == 3 and server.stats().handshakeErrors == 0
 server.close()
 return state
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto state=await exercise(engine)
engine.close()
assert engine.wait(10000)
auto saved=state.saved as TlsServerRequest
assert saved.serverName == b"localhost" and saved.protocolAt(0) == b"x\0\xff"
assert state.calls == 3 and state.server != null
println("TLS listener typed selector: temporary context, ALPN/cookie, three connections, owned request and retirement cycle: ok")
'''
    dynamic=out/'tls.listener.selector.xl';dynamic.write_text(source,encoding='utf8')
    namespace=out/'tls.listener.selector.namespace.xl'
    namespace.write_text(source.replace('TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),context)',
        'tls.listen(engine,TcpListenOptions(NetAddress.loopback(4,0)),context)'),encoding='utf8')
    assert 'tls.listen(' in namespace.read_text()
    static,empty=metadata(root,out)
    return [dynamic,namespace,static,empty]
