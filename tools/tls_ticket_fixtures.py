"""Normal fixed-material in-memory ticket issue/receive, no external traffic."""
from pathlib import Path
from tls_client_resume_fixtures import fixtures as client_fixtures

def fixtures(root,out):
    out=Path(out);previous=client_fixtures(root,out)
    common=previous[2].read_text(encoding='utf8').split('auto client=makeClient()\n',1)[0]
    helpers=r'''
func transferTicketCipher(TlsSession source,TlsSession target)
 while source.pendingCipher > 0
  assert target.feed(source.takeCipher(317)) == TlsResult.ok
 end
end
func readyTicketPair(TlsSession client,TlsSession server)
 int rounds=0
 while not client.ready or not server.ready
  auto a=client.drive()
  assert a == TlsResult.ok or a == TlsResult.wouldBlock
  transferTicketCipher(client,server)
  auto b=server.drive()
  assert b == TlsResult.ok or b == TlsResult.wouldBlock
  transferTicketCipher(server,client)
  rounds+=1
  assert rounds < 256
 end
end
export func tls_ticket_pair() as array<TlsSession>
 auto client=TlsSession(makeClient(),"localhost")
 auto server=TlsSession(makeServer())
 readyTicketPair(client,server)
 return [client,server]
end
export func tls_ticket_seed(array<TlsSession> pair) as TlsTicketResult
 auto result=pair[1].issueTicket(b"ticket\0\xff",90,999)
 assert result.status == TlsResult.ok
 transferTicketCipher(pair[1],pair[0])
 auto status=pair[0].drive()
 assert status == TlsResult.ok or status == TlsResult.wouldBlock
 assert pair[0].ticketCount == 1
 return result
end
export func tls_ticket_pop(TlsSession client) as TlsTicketResult
 return client.tryTakeTicket()
end
export func tls_ticket_pop_dynamic(var client) as var
 return client.tryTakeTicket()
end
class TicketCache
 TlsResume? saved
 func find(TlsResumeRequest request) as TlsResume?
  return self.saved
 end
end
'''
    case=out/'tls.ticket.session.xl'
    case.write_text(common+helpers+r'''
auto pair=tls_ticket_pair()
auto client=pair[0]
auto server=pair[1]
assert client.ticketCount == 0 and client.droppedTickets == uint64(0)
auto empty=client.tryTakeTicket()
assert empty.status == TlsResult.wouldBlock and empty.resume == null
auto issued=tls_ticket_seed(pair)
auto taken=tls_ticket_pop(client)
assert taken.status == TlsResult.ok and client.ticketCount == 0
auto credential=taken.resume as TlsResume
assert credential.ticket == b"ticket\0\xff"
assert credential.exportSecret() == (issued.resume as TlsResume).exportSecret()
auto detached=credential.ticket
detached[0]=65
assert (taken.resume as TlsResume).ticket == b"ticket\0\xff"
auto cache=TicketCache()
cache.saved=issued.resume
auto required=makeClient().withTicketCapacity(1).withRequiredResume(credential)
assert required.resumeRequired and required.ticketCapacity == 1
auto cloned=required.withParameters(TlsParameters()).withProtocols([])
assert cloned.resumeRequired and cloned.ticketCapacity == 1
assert not cloned.withoutResume().resumeRequired and cloned.withoutResume().ticketCapacity == 1
assert not cloned.withVerifier(cloned.verifier).resumeRequired and cloned.withVerifier(cloned.verifier).ticketCapacity == 1
assert not cloned.withResume(credential).resumeRequired
auto c2=TlsSession(cloned,"localhost")
auto s2=TlsSession(makeServer().withResumeLookup(cache.find))
readyTicketPair(c2,s2)
assert c2.resumed and s2.resumed
auto first=s2.issueTicket(b"first\0x",60,5)
assert (first.resume as TlsResume).ticket == b"first"
transferTicketCipher(s2,c2)
auto ignored=c2.drive()
auto second=s2.issueTicket()
assert second.status == TlsResult.ok and (second.resume as TlsResume).ticket.length > 0
transferTicketCipher(s2,c2)
ignored=c2.drive()
assert c2.ticketCount == 1 and c2.droppedTickets == uint64(1)
auto newest=c2.tryTakeTicket()
assert (newest.resume as TlsResume).ticket == (second.resume as TlsResume).ticket
auto c3=TlsSession(makeClient().withTicketCapacity(0),"localhost")
auto s3=TlsSession(makeServer())
readyTicketPair(c3,s3)
auto unused=s3.issueTicket()
transferTicketCipher(s3,c3)
ignored=c3.drive()
assert c3.ticketCount == 0 and c3.droppedTickets == uint64(1)
int refused=0
try
 auto bad=client.issueTicket()
catch failure
 refused+=1
end
try
 auto bad=server.tryTakeTicket()
catch failure
 refused+=1
end
try
 auto bad=server.issueTicket(b"x",-1)
catch failure
 refused+=1
end
try
 auto bad=server.issueTicket(b"x",30,-2)
catch failure
 refused+=1
end
try
 auto bad=makeClient().withTicketCapacity(65)
catch failure
 refused+=1
end
assert refused == 5
client.dispose()
server.dispose()
c2.dispose()
s2.dispose()
c3.dispose()
s3.dispose()
assert credential.ticket == b"ticket\0\xff" and (newest.resume as TlsResume).ticket.length > 0
println("TLS tickets: owned issue/pop, empty status, NUL/prefix, required resume, clone/trust rules, bounded/disabled cache and independent lifetime: ok")
''',encoding='utf8')
    leaf=out/'tls.ticket.leaf.xl';leaf.write_text('''func inspect(TlsTicketResult result) as TlsResult
 return result.status
end
println("TLS ticket result declaration: ok")
''',encoding='utf8')
    return [case,leaf,previous[3]]
