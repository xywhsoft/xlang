"""Ordinary offline, in-memory TLS sessions with existing fixed test keys.
No probes, malformed crypto materials or native security executables.
"""
from pathlib import Path
from tls_role_context_fixtures import language_fixtures as role_fixtures

def language_fixtures(root,out):
    out=Path(out);roles,_=role_fixtures(root,out);cases=[]
    common='''
func deliver(TlsSession source, TlsSession target)
 while source.pendingCipher > 0
  auto chunk=source.takeCipher(317)
  assert chunk.length > 0 and chunk.length <= 317
  assert target.feed(chunk,chunk.length+20) == TlsResult.ok
 end
end
func receive(TlsSession source) as (TlsResult, bytes)
 return source.read(2)
end
array<bytes> protocols=[b"x\\0\\xff",b"h2"]
auto client=TlsSession(makeClient().withProtocols(protocols),"localhost")
auto server=TlsSession(makeServer().withProtocols(protocols,true))
assert client.role == TlsRole.client and server.role == TlsRole.server
assert not client.ready and not server.ready and not client.disposed
assert client.protocol == b"" and server.protocol == b""
auto waiting,empty=client.read()
assert waiting == TlsResult.wouldBlock and empty == b""
int caught=0
try
 auto invalidStatus,invalidBytes=client.read(0)
catch RangeError expected
 caught+=1
end
try
 auto unavailable=client.version
catch RuntimeError expected
 caught+=1
end
assert caught == 2
int rounds=0
while not client.ready or not server.ready
 auto clientStatus=client.drive()
 assert clientStatus == TlsResult.ok or clientStatus == TlsResult.wouldBlock
 deliver(client,server)
 auto serverStatus=server.drive()
 assert serverStatus == TlsResult.ok or serverStatus == TlsResult.wouldBlock
 deliver(server,client)
 rounds+=1
 assert rounds < 256
end
assert client.version == TlsVersion.v13 and server.version == TlsVersion.v13
assert client.protocol == b"x\\0\\xff" and server.protocol == b"x\\0\\xff"
bytes payload=b"a\\0b\\xff"
auto status,written=client.write(payload,20)
assert status == TlsResult.ok and written == uint64(4)
deliver(client,server)
auto serverProgress=server.drive()
assert serverProgress == TlsResult.ok or serverProgress == TlsResult.wouldBlock
auto firstStatus,first=receive(server)
auto secondStatus,second=server.read(2)
assert firstStatus == TlsResult.ok and first == b"a\\0"
assert secondStatus == TlsResult.ok and second == b"b\\xff"
auto detached=first
detached[0]=90
assert first == b"a\\0" and detached == b"Z\\0"
auto idleStatus,idle=server.read()
assert idleStatus == TlsResult.wouldBlock and idle == b""
auto closing=client.close()
assert closing == TlsResult.ok or closing == TlsResult.wouldBlock
deliver(client,server)
auto closeProgress=server.drive()
auto reply=server.close()
deliver(server,client)
auto closedProgress=client.drive()
assert client.state == TlsState.closed and server.state == TlsState.closed
auto endStatus,endBytes=client.read()
assert endStatus == TlsResult.closed and endBytes == b""
array<TlsSession> owners=[client,server]
assert owners[0].state == TlsState.closed and owners[1].role == TlsRole.server
client.dispose()
client.dispose()
assert client.disposed and not server.disposed
caught=0
try
 auto unavailable=client.state
catch RuntimeError expected
 caught+=1
end
assert caught == 1
server.dispose()
assert server.disposed
'''
    for family in ['rsa','p256','p384','ed25519']:
        source=next(p for p in roles if p.stem=='tls.context.'+family)
        original=source.read_text();marker='array<bytes> protocols='
        assert original.count(marker)==1
        text=original.split(marker,1)[0]+common
        p=out/('tls.session.'+family+'.xl');p.write_text(text);cases.append(p)
    for suffix,text in [('client-only','func make(TlsClientContext context) as TlsSession\n return TlsSession(context,"localhost")\nend\n'),
                        ('server-only','func make(TlsServerContext context) as TlsSession\n return TlsSession(context)\nend\n')]:
        p=out/('tls.session.'+suffix+'.xl');p.write_text(text);cases.append(p)
    # Uncalled declarations prove dependency completeness, not positive native
    # reachability. Exercise each real constructor separately for role pruning.
    original=next(p for p in roles if p.stem=='tls.context.p256').read_text()
    prefix=original.split('array<bytes> protocols=',1)[0]
    for role,creation in [('client','TlsSession(makeClient(),"localhost")'),('server','TlsSession(makeServer())')]:
        p=out/('tls.session.'+role+'-live.xl')
        p.write_text(prefix+'auto session='+creation+'\nassert session.role == TlsRole.'+role+'\nassert not session.disposed\nsession.dispose()\nassert session.disposed\n')
        cases.append(p)
    return cases,cases[1]
