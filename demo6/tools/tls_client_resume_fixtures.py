"""Actual language client credential binding and ordinary fixed TLS sessions."""
from pathlib import Path
from tls_resume_lookup_fixtures import fixtures as lookup_fixtures
def fixtures(root,out):
    out=Path(out);previous=lookup_fixtures(root,out)
    common=previous[0].read_text(encoding='utf8').split('auto state=ResumeLookupState()\n',1)[0]
    helper=r'''
func resumeClient() as TlsClientContext
 auto credential=TlsResume.fromParts(TlsVersion.v13,TlsCipherSuite.aes128GcmSha256,
  b"ticket\0\xff",b"01234567890123456789012345678901",time.now(),120,
  uint64(1),"localhost",b"h2")
 return makeClient().withProtocols([b"h2",b"x\0\xff"]).withResume(credential)
end
func transfer(TlsSession source,TlsSession target)
 while source.pendingCipher > 0
  assert target.feed(source.takeCipher(317)) == TlsResult.ok
 end
end
func handshake(TlsSession client,TlsSession server,bool resumed)
 assert not client.resumed and not server.resumed
 int rounds=0
 while not client.ready or not server.ready
  auto a=client.drive()
  assert a == TlsResult.ok or a == TlsResult.wouldBlock
  transfer(client,server)
  auto b=server.drive()
  assert b == TlsResult.ok or b == TlsResult.wouldBlock
  transfer(server,client)
  rounds+=1
  assert rounds < 256
 end
 assert client.resumed == resumed and server.resumed == resumed
 client.dispose()
 server.dispose()
end
'''
    session=out/'tls.client.resume.xl';session.write_text(common+helper+r'''
auto plain=makeClient()
assert not plain.hasResume and plain.resume == null
auto bound=resumeClient()
assert bound.hasResume
auto credential=bound.resume as TlsResume
assert credential.ticket == b"ticket\0\xff"
auto secret=credential.exportSecret()
secret[0]=65
assert (bound.resume as TlsResume).exportSecret() == b"01234567890123456789012345678901"
auto copied=bound.withParameters(TlsParameters()).withProtocols([b"h2",b"x\0\xff"])
assert copied.hasResume and (copied.resume as TlsResume).ticket == b"ticket\0\xff"
auto cleared=copied.withoutResume()
assert not cleared.hasResume and cleared.resume == null and copied.hasResume
auto newTrust=copied.withVerifier(copied.verifier)
assert not newTrust.hasResume and newTrust.resume == null
handshake(TlsSession(copied,"localhost"),TlsSession(tls_resume_lookup_context(0)),true)
handshake(TlsSession(copied,"localhost"),TlsSession(tls_resume_lookup_context(1)),false)
handshake(TlsSession(cleared,"localhost"),TlsSession(makeServer().withProtocols([b"h2",b"x\0\xff"],true)),false)
println("TLS client resume: independent immutable credential, clone/clear/trust rebind, hit/miss/full verified handshakes: ok")
''',encoding='utf8')
    stream=out/'tls.client.resume.stream.xl';stream.write_text('import concurrent\n'+common+helper+r'''
async func connect(NetEngine engine,TlsServer server,TlsClientContext context,bool resumed)
 auto accepted=server.acceptAsyncTimeout(10000)
 auto client=TlsStream(engine,server.local(),context,"localhost")
 assert await client.waitAsyncTimeout(0,10000)
 auto peer=await accepted
 auto left=await client.infoAsync()
 auto right=await peer.infoAsync()
 assert left.ready and right.ready and left.resumed == resumed and right.resumed == resumed
 client.close()
 peer.close()
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto server=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),tls_resume_lookup_context(0))
await connect(engine,server,resumeClient(),true)
await connect(engine,server,resumeClient().withoutResume(),false)
server.close()
engine.close()
assert engine.wait(10000)
println("TLS client resume transport: temporary context, actual Worker metadata, resumed and verified full literal-loopback connections: ok")
''',encoding='utf8')
    static=out/'tls.client.resume.static.xl';static.write_text(common.split('class ResumeLookupState\n',1)[0]+r'''
auto client=makeClient()
assert not client.hasResume
auto cleared=client.withoutResume()
assert not cleared.hasResume
''',encoding='utf8')
    empty=previous[3]
    return [session,stream,static,empty]
