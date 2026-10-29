"""Ordinary static-role TLS transport ABI integration; fixed local material."""
from pathlib import Path
from tls_role_context_fixtures import fixtures
from x509_identity_language_fixtures import literal

def language_fixture(root,out):
    out=Path(out);rows=fixtures(root,out);row=next(q for q in rows if q['Name']=='rsa')
    rootDer=literal((out/'rsa.root.der').read_bytes());leaf=literal((out/'rsa.leaf.der').read_bytes());mid=literal((out/'rsa.mid.der').read_bytes())
    key=literal(Path(row['KeyPath']).read_bytes())
    # Deliberately use user-class names, not built-in TlsStream/TlsServer.
    # The verified owner/effect/service contracts must work independently of
    # spelling, before the old default facade is replaced in the next batch.
    text='''class ResidentTlsClient
    private native point handle
    extern "xl6TlsStreamCreateDefaultV1" create(NetEngine engine, NetAddress address, TlsClientContext context, string name, int size = -1)
    extern "xl6TlsStreamCreateV1" create(NetEngine engine, NetAddress address, TlsClientContext context, string name, int size, string verifyName, int verifySize, TlsStreamOptions stream, TcpStreamOptions tcp, int worker)
    extern "xl6TlsStreamDestroyV1" @no_raise @nonblocking destroy()
    extern "xl6TlsStreamCloseV1" @nonblocking func close()
    extern "xl6TlsStreamAbortV1" @nonblocking func abort()
    extern "xl6TlsStreamStateV1" @nonblocking get state() as int
    extern "xl6TlsStreamAvailableV1" @nonblocking get available() as uint64
    extern "xl6TlsStreamPendingV1" @nonblocking get pending() as uint64
    extern "xl6TlsStreamLocalV1" @nonblocking func local() as NetAddress
    extern "xl6TlsStreamRemoteV1" @nonblocking func remote() as NetAddress
end
class ResidentTlsListener
    private native point handle
    extern "xl6TlsListenerCreateDefaultV1" create(NetEngine engine, TcpListenOptions options, TlsServerContext context)
    extern "xl6TlsListenerCreateV1" create(NetEngine engine, TcpListenOptions options, TlsServerContext context, TlsStreamOptions stream, int accepts, int handshakes)
    extern "xl6TlsListenerDestroyV1" @no_raise @nonblocking destroy()
    extern "xl6TlsListenerCloseV1" @nonblocking func close()
    extern "xl6TlsListenerStateV1" @nonblocking get state() as int
    extern "xl6TlsListenerLocalV1" @nonblocking func local() as NetAddress
end
func clientContext() as TlsClientContext
    auto store=TrustStore()
    assert store.add(Certificate.fromDer('''+rootDer+'''))
    return TlsClientContext(TlsVerifier(store.snapshot(),TlsVerificationPolicy().withTime(time(1893456000000000))))
end
func serverContext() as TlsServerContext
    array<Certificate> chain=[Certificate.fromDer('''+leaf+'''),Certificate.fromDer('''+mid+''')]
    return TlsServerContext(TlsIdentity.fromRsa(chain,RsaPrivateKey.fromPkcs1('''+key+''')))
end
func exercise(NetEngine engine, bool advanced)
    auto listen=TcpListenOptions(NetAddress.loopback(4,0))
    auto context=serverContext()
    auto stream=TlsStreamOptions()
    auto server = advanced ? ResidentTlsListener(engine,listen,context,stream,16,4) : ResidentTlsListener(engine,listen,context)
    assert server.state == 0
    auto remote=server.local()
    assert remote.port > 0
    auto verified=clientContext()
    auto tcp=TcpStreamOptions()
    auto client = advanced ? ResidentTlsClient(engine,remote,verified,"localhost\\0ignored",9,"",-1,stream,tcp,0) : ResidentTlsClient(engine,remote,verified,"localhost")
    int remaining=10000
    while client.state < 2 and remaining > 0
        time.sleep(1)
        remaining -= 1
    end
    assert client.state == 2
    assert client.available == 0 and client.pending >= 0
    assert client.local().port > 0 and client.remote().port == remote.port
    client.close()
    client.abort()
    server.close()
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
exercise(engine,false)
exercise(engine,true)
engine.close()
assert engine.wait(10000)
println("resident TLS transport contracts: ok")
'''
    case=out/'resident_tls_transport.xl';case.write_text(text,encoding='utf8');return case
