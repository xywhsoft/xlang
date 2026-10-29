"""Actual owned TLS I/O and class Future results, normal local traffic only."""
from pathlib import Path
from tls_execution_fixtures import language_fixture

STREAM_MEMBERS='''    extern "xl6TlsStreamRecvAsyncV1" @nonblocking func recvAsync(int maximum) as future<bytes>
    extern "xl6TlsStreamRecvAsyncCancelV1" @nonblocking func recvAsyncCancel(int maximum, CancellationToken parent) as future<bytes>
    extern "xl6TlsStreamRecvAsyncUntilV1" @nonblocking func recvAsyncUntil(int maximum, uint64 due) as future<bytes>
    extern "xl6TlsStreamRecvAsyncUntilCancelV1" @nonblocking func recvAsyncUntilCancel(int maximum, uint64 due, CancellationToken parent) as future<bytes>
    extern "xl6TlsStreamRecvAsyncTimeoutV1" @nonblocking func recvAsyncTimeout(int maximum, int millis) as future<bytes>
    extern "xl6TlsStreamRecvAsyncTimeoutCancelV1" @nonblocking func recvAsyncTimeoutCancel(int maximum, CancellationToken parent, int millis) as future<bytes>
    extern "xl6TlsStreamRecvV1" func recv(int maximum, int millis) as bytes
    extern "xl6TlsStreamRecvCancelV1" func recvCancel(int maximum, CancellationToken parent, int millis) as bytes
    extern "xl6TlsStreamRecvUntilV1" func recvUntil(int maximum, uint64 due) as bytes
    extern "xl6TlsStreamRecvUntilCancelV1" func recvUntilCancel(int maximum, uint64 due, CancellationToken parent) as bytes
    extern "xl6TlsStreamWaitAsyncV1" @nonblocking func waitAsync(int condition) as future<bool>
    extern "xl6TlsStreamWaitAsyncCancelV1" @nonblocking func waitAsyncCancel(int condition, CancellationToken parent) as future<bool>
    extern "xl6TlsStreamWaitAsyncUntilV1" @nonblocking func waitAsyncUntil(int condition, uint64 due) as future<bool>
    extern "xl6TlsStreamWaitAsyncUntilCancelV1" @nonblocking func waitAsyncUntilCancel(int condition, uint64 due, CancellationToken parent) as future<bool>
    extern "xl6TlsStreamWaitAsyncTimeoutV1" @nonblocking func waitAsyncTimeout(int condition, int millis) as future<bool>
    extern "xl6TlsStreamWaitAsyncTimeoutCancelV1" @nonblocking func waitAsyncTimeoutCancel(int condition, CancellationToken parent, int millis) as future<bool>
    extern "xl6TlsStreamWaitV1" func wait(int condition, int millis) as bool
    extern "xl6TlsStreamWaitCancelV1" func waitCancel(int condition, CancellationToken parent, int millis) as bool
    extern "xl6TlsStreamWaitUntilV1" func waitUntil(int condition, uint64 due) as bool
    extern "xl6TlsStreamWaitUntilCancelV1" func waitUntilCancel(int condition, uint64 due, CancellationToken parent) as bool
    extern "xl6TlsStreamSendTextAsyncV1" @nonblocking func sendTextAsync(string input, int size) as future<int>
    extern "xl6TlsStreamSendTextAsyncCancelV1" @nonblocking func sendTextAsyncCancel(string input, int size, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendTextAsyncUntilV1" @nonblocking func sendTextAsyncUntil(string input, int size, uint64 due) as future<int>
    extern "xl6TlsStreamSendTextAsyncUntilCancelV1" @nonblocking func sendTextAsyncUntilCancel(string input, int size, uint64 due, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendTextAsyncTimeoutV1" @nonblocking func sendTextAsyncTimeout(string input, int size, int millis) as future<int>
    extern "xl6TlsStreamSendTextAsyncTimeoutCancelV1" @nonblocking func sendTextAsyncTimeoutCancel(string input, int size, CancellationToken parent, int millis) as future<int>
    extern "xl6TlsStreamSendTextV1" func sendText(string input, int size, int millis) as int
    extern "xl6TlsStreamSendTextCancelV1" func sendTextCancel(string input, int size, CancellationToken parent, int millis) as int
    extern "xl6TlsStreamSendTextUntilV1" func sendTextUntil(string input, int size, uint64 due) as int
    extern "xl6TlsStreamSendTextUntilCancelV1" func sendTextUntilCancel(string input, int size, uint64 due, CancellationToken parent) as int
    extern "xl6TlsStreamSendBytesAsyncV1" @nonblocking func sendBytesAsync(bytes input, int size) as future<int>
    extern "xl6TlsStreamSendBytesAsyncCancelV1" @nonblocking func sendBytesAsyncCancel(bytes input, int size, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendBytesAsyncUntilV1" @nonblocking func sendBytesAsyncUntil(bytes input, int size, uint64 due) as future<int>
    extern "xl6TlsStreamSendBytesAsyncUntilCancelV1" @nonblocking func sendBytesAsyncUntilCancel(bytes input, int size, uint64 due, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendBytesAsyncTimeoutV1" @nonblocking func sendBytesAsyncTimeout(bytes input, int size, int millis) as future<int>
    extern "xl6TlsStreamSendBytesAsyncTimeoutCancelV1" @nonblocking func sendBytesAsyncTimeoutCancel(bytes input, int size, CancellationToken parent, int millis) as future<int>
    extern "xl6TlsStreamSendBytesV1" func sendBytes(bytes input, int size, int millis) as int
    extern "xl6TlsStreamSendBytesCancelV1" func sendBytesCancel(bytes input, int size, CancellationToken parent, int millis) as int
    extern "xl6TlsStreamSendBytesUntilV1" func sendBytesUntil(bytes input, int size, uint64 due) as int
    extern "xl6TlsStreamSendBytesUntilCancelV1" func sendBytesUntilCancel(bytes input, int size, uint64 due, CancellationToken parent) as int
    extern "xl6TlsStreamSendBufferAsyncV1" @nonblocking func sendBufferAsync(Buffer input, int size) as future<int>
    extern "xl6TlsStreamSendBufferAsyncCancelV1" @nonblocking func sendBufferAsyncCancel(Buffer input, int size, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendBufferAsyncUntilV1" @nonblocking func sendBufferAsyncUntil(Buffer input, int size, uint64 due) as future<int>
    extern "xl6TlsStreamSendBufferAsyncUntilCancelV1" @nonblocking func sendBufferAsyncUntilCancel(Buffer input, int size, uint64 due, CancellationToken parent) as future<int>
    extern "xl6TlsStreamSendBufferAsyncTimeoutV1" @nonblocking func sendBufferAsyncTimeout(Buffer input, int size, int millis) as future<int>
    extern "xl6TlsStreamSendBufferAsyncTimeoutCancelV1" @nonblocking func sendBufferAsyncTimeoutCancel(Buffer input, int size, CancellationToken parent, int millis) as future<int>
    extern "xl6TlsStreamSendBufferV1" func sendBuffer(Buffer input, int size, int millis) as int
    extern "xl6TlsStreamSendBufferCancelV1" func sendBufferCancel(Buffer input, int size, CancellationToken parent, int millis) as int
    extern "xl6TlsStreamSendBufferUntilV1" func sendBufferUntil(Buffer input, int size, uint64 due) as int
    extern "xl6TlsStreamSendBufferUntilCancelV1" func sendBufferUntilCancel(Buffer input, int size, uint64 due, CancellationToken parent) as int
'''
LISTENER_MEMBERS='''    extern "xl6TlsListenerAcceptAsyncV1" @nonblocking func acceptAsync() as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptAsyncCancelV1" @nonblocking func acceptAsyncCancel(CancellationToken parent) as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptAsyncUntilV1" @nonblocking func acceptAsyncUntil(uint64 due) as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptAsyncUntilCancelV1" @nonblocking func acceptAsyncUntilCancel(uint64 due, CancellationToken parent) as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptAsyncTimeoutV1" @nonblocking func acceptAsyncTimeout(int millis) as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptAsyncTimeoutCancelV1" @nonblocking func acceptAsyncTimeoutCancel(CancellationToken parent, int millis) as future<ResidentTlsClient>
    extern "xl6TlsListenerAcceptV1" func accept(int millis) as ResidentTlsClient
    extern "xl6TlsListenerAcceptCancelV1" func acceptCancel(CancellationToken parent, int millis) as ResidentTlsClient
    extern "xl6TlsListenerAcceptUntilV1" func acceptUntil(uint64 due) as ResidentTlsClient
    extern "xl6TlsListenerAcceptUntilCancelV1" func acceptUntilCancel(uint64 due, CancellationToken parent) as ResidentTlsClient
'''

def io_language_fixture(root,out):
    original=language_fixture(root,out)
    source=original.read_text(encoding="utf8")
    source=source.replace("end\nclass ResidentTlsListener",STREAM_MEMBERS+"end\nclass ResidentTlsListener",1)
    source=source.replace("end\nfunc clientContext",LISTENER_MEMBERS+"end\nfunc clientContext",1)
    source=source.split("func exercise(",1)[0]
    source="import concurrent\n"+source+EXERCISE
    result=Path(out)/"resident_tls_io.xl";result.write_text(source,encoding="utf8");return result

EXERCISE=r'''
async func exerciseIo(NetEngine engine)
    auto listen=TcpListenOptions(NetAddress.loopback(4,0))
    auto server=ResidentTlsListener(engine,listen,serverContext())
    auto pending=server.acceptAsyncTimeout(5000)
    auto client=ResidentTlsClient(engine,server.local(),clientContext(),"localhost")
    assert await client.waitAsyncTimeout(0,5000)
    auto peer=await pending
    assert peer.state == 2
    auto incoming=peer.recvAsyncTimeout(3,5000)
    assert await client.sendTextAsync("a\0b",100) == 3
    assert await incoming == b"a\0b"
    assert client.sendBytes(b"x\0y",100,5000) == 3
    assert peer.recv(3,5000) == b"x\0y"
    auto returning=client.recvAsync(4)
    assert await peer.sendBufferAsync(Buffer(b"buf\0"),-1) == 4
    assert await returning == b"buf\0"
    assert client.wait(0,5000)
    auto empty=client.sendBytesAsync(b"",-1)
    assert await empty == 0
    auto cancelled=peer.recvAsync(1)
    assert cancelled.cancel()
    int failures=0
    try
        auto ignored=await cancelled
    catch failure
        failures += 1
    end
    auto stop=CancellationSource()
    assert stop.cancel()
    try
        auto ignored=await peer.recvAsyncCancel(1,stop.token)
    catch failure
        failures += 1
    end
    try
        auto ignored=await client.sendTextAsyncCancel("not-sent",-1,stop.token)
    catch failure
        failures += 1
    end
    try
        auto ignored=await peer.recvAsyncUntil(1,uint64(0))
    catch failure
        failures += 1
    end
    try
        auto ignored=await server.acceptAsyncTimeout(0)
    catch failure
        failures += 1
    end
    try
        auto ignored=await server.acceptAsyncCancel(stop.token)
    catch failure
        failures += 1
    end
    try
        auto ignored=peer.recvCancel(1,stop.token,5000)
    catch failure
        failures += 1
    end
    assert failures == 7
    assert peer.available == 0
    auto resumed=peer.recvAsyncTimeout(3,5000)
    assert client.sendText("ok\0",-1,5000) == 3
    assert await resumed == b"ok\0"
    auto second=ResidentTlsClient(engine,server.local(),clientContext(),"localhost")
    auto accepted=server.accept(5000)
    assert second.wait(0,5000)
    assert second.sendBuffer(Buffer(b"sync"),2,5000) == 2
    assert accepted.recv(2,5000) == b"sy"
    server.close()
    assert peer.state == 2 and accepted.state == 2
    client.abort()
    peer.abort()
    second.abort()
    accepted.abort()
end

auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
await exerciseIo(engine)
engine.close()
assert engine.wait(10000)
println("resident TLS I/O: owned bytes, sync/async, class accept, cancellation and deadlines: ok")
'''
