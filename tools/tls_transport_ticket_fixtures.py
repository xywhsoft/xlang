"""Normal verified literal-loopback tickets; no external or malformed traffic."""
from pathlib import Path
from tls_client_resume_fixtures import fixtures as client_fixtures

def fixtures(root,out):
    out=Path(out);previous=client_fixtures(root,out)
    helpers=previous[2].read_text(encoding='utf8').split('auto client=makeClient()\n',1)[0]
    common='import concurrent\n'+helpers+r'''
class TransportTicketCache
 TlsResume? saved
 func find(TlsResumeRequest request) as TlsResume?
  return self.saved
 end
end
async func ticketPause(NetEngine engine)
 await engine.submitAfter(1,func()
 end)
end
async func ticketWait(NetEngine engine,TlsStream client,int target)
 int rounds=0
 while (await client.ticketCountAsync()) < target
  await ticketPause(engine)
  rounds+=1
  assert rounds < 10000
 end
end
async func ticketConnect(NetEngine engine,TlsServer server,TlsClientContext context,bool dial) as array<TlsStream>
 auto accepted=server.acceptAsyncTimeout(10000)
 if dial
  auto resolver=NetResolver()
  auto options=TlsDialOptions().withServerNameFromHost(false).withServerName("localhost").withVerifyName("localhost")
  auto client=await TlsStream.dialAsync(engine,resolver,"127.0.0.1",server.local().port,context,options)
  return [client,await accepted]
 end
 auto client=TlsStream(engine,server.local(),context,"localhost")
 assert await client.waitAsyncTimeout(0,10000)
 return [client,await accepted]
end
async func transportTicketFixture(int workers) as array<var>
 auto engine=NetEngine(NetEngineOptions().withWorkers(workers).withBackend(NetBackend.select))
 engine.start()
 auto cache=TransportTicketCache()
 auto server=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),makeServer().withResumeLookup(cache.find))
 auto pair=await ticketConnect(engine,server,makeClient().withTicketCapacity(1),false)
 return [pair[0],pair[1],engine,server,cache]
end
export func tls_transport_ticket_fixture(int workers) as future<array<var>>
 return transportTicketFixture(workers)
end
async func transportTicketSeed(array<var> state) as TlsTicketResult
 auto client=state[0] as TlsStream
 auto peer=state[1] as TlsStream
 auto engine=state[2] as NetEngine
 auto cache=state[4] as TransportTicketCache
 auto result=await peer.issueTicketAsync(b"ticket\0\xff",90,999)
 assert result.status == TlsResult.ok
 cache.saved=result.resume
 await ticketWait(engine,client,1)
 return result
end
export func tls_transport_ticket_seed(array<var> state) as future<TlsTicketResult>
 return transportTicketSeed(state)
end
export func tls_transport_ticket_pop(TlsStream client) as future<TlsTicketResult>
 return client.tryTakeTicketAsync()
end
export func tls_transport_ticket_pop_dynamic(var client) as var
 return client.tryTakeTicketAsync()
end
async func exerciseTickets()
 auto state=await tls_transport_ticket_fixture(2)
 auto client=state[0] as TlsStream
 auto peer=state[1] as TlsStream
 auto engine=state[2] as NetEngine
 auto server=state[3] as TlsServer
 auto cache=state[4] as TransportTicketCache
 auto empty=await client.tryTakeTicketAsync()
 assert empty.status == TlsResult.wouldBlock and empty.resume == null
 assert client.tryTakeTicket().resume == null
 assert client.ticketCount(10000) == 0 and client.droppedTickets(10000) == uint64(0)
 auto issued=await tls_transport_ticket_seed(state)
 var dynamicClient=client
 var dynamicFuture=dynamicClient.tryTakeTicketAsync()
 var dynamicResult=await (dynamicFuture as future<TlsTicketResult>)
 auto first=dynamicResult as TlsTicketResult
 auto credential=first.resume as TlsResume
 assert first.status == TlsResult.ok and credential.ticket == b"ticket\0\xff"
 assert credential.exportSecret() == (issued.resume as TlsResume).exportSecret()
 auto detached=credential.ticket
 detached[0]=65
 assert credential.ticket == b"ticket\0\xff"
 auto newer=peer.issueTicket(b"prefix\0tail",60,6)
 await ticketWait(engine,client,1)
 auto random=await peer.issueTicketAsync()
 assert await peer.waitAsyncTimeout(3,10000)
 // A following application record provides an explicit ordinary wire barrier,
 // not a fake control-record flush. First receipt above needed no app write.
 assert await peer.sendAsync(b"marker") == 6
 assert await client.recvAsyncTimeout(6,10000) == b"marker"
 assert await client.droppedTicketsAsync() == uint64(1)
 auto newest=client.tryTakeTicket()
 assert (newest.resume as TlsResume).ticket == (random.resume as TlsResume).ticket
 assert (newer.resume as TlsResume).ticket == b"prefix"
 auto fallback=peer.issueTicket()
 assert fallback.status == TlsResult.ok
 await ticketWait(engine,client,1)
 array<TlsTicketResult> popped=await task.all([client.tryTakeTicketAsync(),client.tryTakeTicketAsync()])
 assert popped[0].status == TlsResult.ok and popped[1].status == TlsResult.wouldBlock
 int refused=0
 try
  auto ignored=await client.issueTicketAsync()
 catch failure
  refused+=1
 end
 try
  auto ignored=await peer.tryTakeTicketAsync()
 catch failure
  refused+=1
 end
 try
  auto ignored=client.ticketCount(-2)
 catch failure
  refused+=1
 end
 try
  auto ignored=peer.issueTicket(b"x",-1)
 catch failure
  refused+=1
 end
 try
  auto ignored=await peer.issueTicketAsync(b"x",10,-2)
 catch failure
  refused+=1
 end
 assert refused == 5
 // Both direct and Dial apply the actual RequiredResume and cache settings.
 int mode=0
 while mode<2
  auto resumed=await ticketConnect(engine,server,makeClient().withRequiredResume(credential).withTicketCapacity(0),mode==1)
  assert (await resumed[0].infoAsync()).resumed and (await resumed[1].infoAsync()).resumed
  auto ignored=await resumed[1].issueTicketAsync()
  assert await resumed[1].sendAsync(b"x") == 1
  assert await resumed[0].recvAsyncTimeout(1,10000) == b"x"
  assert await resumed[0].ticketCountAsync() == 0 and await resumed[0].droppedTicketsAsync() == uint64(1)
  resumed[0].close()
  resumed[1].close()
  mode+=1
 end
 client.close()
 try
  auto ignored=await client.tryTakeTicketAsync()
 catch failure
  refused+=1
 end
 assert refused == 6
 peer.close()
 server.close()
 engine.close()
 assert engine.wait(10000)
 assert credential.ticket == b"ticket\0\xff"
end
'''
    case=out/'tls.transport.tickets.xl';case.write_text(common+r'''
await exerciseTickets()
println("TLS transport tickets: real Workers, owned sync/async results, dynamic/all, NUL/prefix, flush, bounded cache, direct/Dial strict resume and checked close: ok")
''',encoding='utf8')
    # Compile-only leaf consumers prove each operation family prunes separately.
    samples={
        'receive':'export func inspect(TlsStream stream) as future<TlsTicketResult>\n return stream.tryTakeTicketAsync()\nend\n',
        'issue':'export func inspect(TlsStream stream) as future<TlsTicketResult>\n return stream.issueTicketAsync()\nend\n',
        'stats':'export func inspect(TlsStream stream) as future<uint64>\n return stream.droppedTicketsAsync()\nend\n'}
    leaves=[]
    for name,text in samples.items():
        path=out/('tls.transport.tickets.'+name+'.xl');path.write_text(text+'println("ticket leaf: ok")\n',encoding='utf8');leaves.append(path)
    return [case,*leaves,previous[3]]
