"""Valid verified TLS and ordinary Worker-dispatched KeyUpdate on loopback."""
from pathlib import Path
from tls_client_resume_fixtures import fixtures as client_fixtures

def fixtures(root,out):
    out=Path(out);previous=client_fixtures(root,out)
    helpers=previous[2].read_text(encoding='utf8').split('auto client=makeClient()\n',1)[0]
    case=out/'tls.key.update.xl'
    case.write_text('import concurrent\n'+helpers+r'''
async func exercise(NetEngine engine,TlsServer server)
 auto accepted=server.acceptAsyncTimeout(10000)
 auto client=TlsStream(engine,server.local(),makeClient(),"localhost")
 assert await client.waitAsyncTimeout(0,10000)
 auto peer=await accepted
 array<TlsResult> results=await task.all([client.keyUpdateAsync(),peer.keyUpdateAsync(true)])
 assert results[0] == TlsResult.ok and results[1] == TlsResult.ok
 var boxed=results[0]
 assert (boxed as TlsResult) == TlsResult.ok
 assert client.keyUpdate(true,10000) == TlsResult.ok
 assert peer.keyUpdate(false,10000) == TlsResult.ok
 assert await client.sendBoundAsync(3) > 3
 assert await peer.protocolAsync() == b""
 assert await client.sendAsync(b"a\0b") == 3
 assert await peer.waitAsyncTimeout(1,10000)
 assert await peer.peekAsync(3) == b"a\0b"
 assert await peer.consumeAsync(3) == 3
 assert await peer.sendAsync(b"x\0y") == 3
 assert await client.recvAsync(3) == b"x\0y"
 assert (await client.infoAsync()).ready and (await peer.infoAsync()).ready
 int refused=0
 try
  auto ignored=client.keyUpdate(false,-2)
 catch failure
  refused+=1
 end
 assert refused == 1
 client.close()
 try
  auto ignored=await client.keyUpdateAsync()
 catch failure
  refused+=1
 end
 assert refused == 2
 peer.close()
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto server=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),makeServer())
await exercise(engine,server)
server.close()
engine.close()
assert engine.wait(10000)
println("TLS KeyUpdate: actual Workers, synchronous/asynchronous roles, exact enum Future/all/boxing, post-update data and checked close: ok")
''',encoding='utf8')
    return [case,previous[3]]
