"""Immutable TLS metadata, using ordinary self-created local connections only."""
from pathlib import Path
from tls_io_fixtures import io_language_fixture
from tls_public_fixtures import public_text


def fixtures(root,out):
    out=Path(out)
    source=public_text(root,io_language_fixture(root,out).read_text(encoding='utf8'))
    contexts=source.split('async func exerciseIo(',1)[0]
    # Matching ALPN is part of the ordinary successful connection, not a
    # negotiation/security test. All keys and certificates are offline fixtures.
    contexts=contexts.replace('\nend\nfunc serverContext()', '.withProtocols([b"xlang-test"])\nend\nfunc serverContext()',1)
    # The server context factory occupies the remainder before exerciseIo.
    split=contexts.rfind('\nend\n')
    assert split>0
    contexts=contexts[:split]+'.withProtocols([b"xlang-test"],true)'+contexts[split:]
    main=out/'public_tls_metadata.xl'
    main.write_text(contexts+EXERCISE,encoding='utf8')
    declarations=out/'public_tls_metadata_declarations.xl'
    declarations.write_text('TlsConnectionInfo? info=null\nTlsServerStats? stats=null\nassert info==null and stats==null\n',encoding='utf8')
    return main,declarations


EXERCISE=r'''
async func exerciseMetadata(NetEngine engine) as TlsConnectionInfo
    auto server=TlsServer(engine,TcpListenOptions(NetAddress.loopback(4,0)),serverContext())
    auto initial=server.stats()
    assert initial.state == TlsListenerState.open
    assert initial.handshakes == 0 and initial.accepted == 0
    auto pending=server.acceptAsyncTimeout(5000)
    auto client=TlsStream(engine,server.local(),clientContext(),"localhost")
    assert await client.waitAsyncTimeout(0,5000)
    auto peer=await pending
    auto info=client.info(5000)
    auto remote=await peer.infoAsync()
    assert info.role == TlsRole.client and remote.role == TlsRole.server
    assert info.state == TlsStreamState.open and remote.state == TlsStreamState.open
    assert info.sessionState == TlsState.ready and remote.sessionState == TlsState.ready
    assert info.ready and remote.ready and info.hasSelection and remote.hasSelection
    assert info.version == remote.version and info.cipher == remote.cipher
    assert not info.resumed and not remote.resumed
    assert info.protocol == b"xlang-test" and remote.protocol == b"xlang-test"
    assert info.serverName == b"" and remote.serverName == b"localhost"
    assert not info.hasServerCookie
    assert not info.hasPeerAlert and not remote.hasPeerAlert
    assert info.peerAlertLevel == 0 and info.peerAlertCode == 0
    assert not info.hasError and not remote.hasError
    assert info.errorKind == 0 and info.errorCode == 0
    assert info.errorDomain == "" and info.errorOperation == "" and info.errorMessage == ""
    assert info.available == 0 and info.pending == 0
    assert info.asyncBytes >= 0 and info.asyncCount >= 0
    assert info.peerCertificateCount >= 1 and remote.peerCertificateCount == 0
    auto certificate=info.peerCertificateAt()
    assert certificate.toDer().length > 0
    int failures=0
    try
        auto bad=info.peerCertificateAt(-1)
    catch failure
        failures += 1
    end
    try
        auto bad=info.peerCertificateAt(info.peerCertificateCount)
    catch failure
        failures += 1
    end
    assert failures == 2
    auto later=await client.infoAsync()
    auto stats=server.stats()
    assert stats.accepted == 1 and stats.handshakes >= 1
    assert stats.rejected == 0 and stats.handshakeErrors == 0
    assert stats.activeHandshakes == 0 and stats.peakHandshakes >= 1
    assert stats.queuedAccepts == 0 and stats.peakQueuedAccepts >= 0 and stats.acceptWaiters == 0
    server.close()
    assert server.stats().state != TlsListenerState.open
    assert stats.state == TlsListenerState.open and initial.accepted == 0
    client.abort()
    peer.abort()
    assert info.ready and later.ready and remote.ready
    assert info.protocol == b"xlang-test" and remote.serverName == b"localhost"
    assert info.peerCertificateAt().toDer() == certificate.toDer()
    assert info.errorMessage == ""
    return info
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
engine.start()
auto retained=await exerciseMetadata(engine)
engine.close()
assert engine.wait(10000)
assert retained.ready and retained.protocol == b"xlang-test"
assert retained.peerCertificateAt().toDer().length > 0
println("TLS metadata: immutable owned snapshots, certificates, stats and Future: ok")
'''
