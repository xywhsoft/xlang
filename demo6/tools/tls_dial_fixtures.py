"""Managed Dial declarations and ordinary literal-loopback fixtures.

Only deterministic, publicly known test key material is used. The proxy
authority is wire data; no connection to that authority is made by the peer.
"""
from pathlib import Path
import re
from tls_io_fixtures import io_language_fixture
from x509_identity_language_fixtures import literal


def api_rows(root):
    types = dict(engine='NetEngine', resolver='NetResolver', proxy='NetProxy',
                 host='string', port='int', role='TlsClientContext',
                 options='TlsDialOptions', size='int', parent='CancellationToken',
                 due='uint64', millis='int')
    header = (Path(root)/'src/runtime/xl6_tls_dial_abi.h').read_text()
    rows = []
    for symbol, params in re.findall(r' (xl6Tls\w+)\(([^\n]+)\);', header):
        names = [re.search(r'(\w+)$', p).group(1) for p in params.split(',')]
        names = [n for n in names if n not in ('services', 'execution', 'context')]
        asynchronous = 'Async' in symbol
        alias = 'native' + symbol.removeprefix('xl6Tls').removesuffix('V1')
        result = 'future<ResidentTlsClient>' if asynchronous else 'ResidentTlsClient'
        declaration = ('extern "'+symbol+'" '+('@nonblocking ' if asynchronous else '') +
                       'func '+alias+'('+', '.join(types[n]+' '+n for n in names)+') as '+result+'\n')
        rows.append(dict(symbol=symbol, alias=alias, names=names,
                         asynchronous=asynchronous, declaration=declaration))
    assert len(rows) == 40
    return rows


def ip_contexts(out):
    """Valid IP SAN avoids even local hostname resolution in default calls."""
    from datetime import datetime, timezone
    from ipaddress import ip_address
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    key = serialization.load_der_private_key((out/'rsa.tls-key.der').read_bytes(), None)
    name = lambda s: x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME, s)])
    issuer = name('offline managed dial root')
    def cert(ca):
        builder = (x509.CertificateBuilder().subject_name(issuer if ca else name('offline managed dial'))
                   .issuer_name(issuer).public_key(key.public_key()).serial_number(51 if ca else 52)
                   .not_valid_before(datetime(2020, 1, 1, tzinfo=timezone.utc))
                   .not_valid_after(datetime(2040, 1, 1, tzinfo=timezone.utc))
                   .add_extension(x509.BasicConstraints(ca=ca, path_length=2 if ca else None), True)
                   .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()), False)
                   .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(key.public_key()), False)
                   .add_extension(x509.KeyUsage(digital_signature=True, content_commitment=False,
                       key_encipherment=False, data_encipherment=False, key_agreement=False,
                       key_cert_sign=ca, crl_sign=ca, encipher_only=False, decipher_only=False), True))
        if not ca:
            builder = builder.add_extension(x509.SubjectAlternativeName([
                x509.DNSName('localhost'), x509.IPAddress(ip_address('127.0.0.1'))]), False)
            builder = builder.add_extension(x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.SERVER_AUTH]), False)
        return builder.sign(key, hashes.SHA256())
    root, leaf = cert(True), cert(False)
    root.verify_directly_issued_by(root); leaf.verify_directly_issued_by(root)
    for name_, value in [('root', root), ('leaf', leaf)]:
        (out/('dial.ip.'+name_+'.der')).write_bytes(value.public_bytes(serialization.Encoding.DER))
    return '''func clientContext() as TlsClientContext
 auto store=TrustStore()
 assert store.add(Certificate.fromDer('''+literal(root.public_bytes(serialization.Encoding.DER))+'''))
 return TlsClientContext(TlsVerifier(store.snapshot(),TlsVerificationPolicy().withTime(time(1893456000000000))))
end
func serverContext() as TlsServerContext
 array<Certificate> chain=[Certificate.fromDer('''+literal(leaf.public_bytes(serialization.Encoding.DER))+''')]
 return TlsServerContext(TlsIdentity.fromRsa(chain,RsaPrivateKey.fromPkcs1('''+literal(key.private_bytes(
        serialization.Encoding.DER, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption()))+''')))
end
'''


def language_fixtures(root, out):
    out = Path(out)
    original = io_language_fixture(root, out).read_text(encoding='utf8')
    declarations = ''.join(r['declaration'] for r in api_rows(root))
    classes = original.split('func clientContext()', 1)[0] + declarations
    old_contexts = 'func clientContext()' + original.split('func clientContext()', 1)[1].split('async func exerciseIo(', 1)[0]
    dispatch = '''async func connectDial(int mode,NetEngine engine,NetResolver resolver,TlsClientContext role,TlsDialOptions options,int port,CancellationToken parent) as ResidentTlsClient
'''
    index = 0
    for row in api_rows(root):
        if 'Proxy' in row['symbol']:
            continue
        values = dict(engine='engine', resolver='resolver', host='"127.0.0.1\\0ignored"',
                      port='port', role='role', options='options', size='9',
                      parent='parent', due='uint64(9223372036854775807)', millis='5000')
        call = row['alias']+'('+','.join(values[n] for n in row['names'])+')'
        dispatch += ' if mode == '+str(index)+' then return '+('await ' if row['asynchronous'] else '')+call+' end\n'
        index += 1
    dispatch += ' throw "invalid test mode"\nend\n'
    direct = classes + ip_contexts(out) + dispatch + '''async func exerciseDial(NetEngine engine, NetResolver resolver)
 auto base=TlsDialOptions()
 assert base.timeoutMillis == -1 and base.serverNameFromHost
 assert base.serverName == "" and base.verifyName == ""
 auto options=base.withServerName("localhost\\0ignored",9).withVerifyName("localhost",99).withTimeoutMillis(5000)
 assert base.serverName == "" and options.serverName == "localhost"
 assert options.verifyName == "localhost" and options.timeoutMillis == 5000
 assert options.transport().streamOptions().readSize > 0 and options.stream().asyncBatch > 0
 auto cancellation=CancellationSource()
 auto parent=cancellation.token
 auto role=clientContext()
 auto server=ResidentTlsListener(engine,TcpListenOptions(NetAddress.loopback(4,0)),serverContext())
 auto port=server.local().port
 int mode=0
 while mode < 20
  auto accepted=server.acceptAsyncTimeout(5000)
  auto client=await connectDial(mode,engine,resolver,clientContext(),options,port,parent)
  auto peer=await accepted
  assert client.state == 2 and peer.state == 2
  assert client.sendBytes(b"a\\0b",100,5000) == 3
  assert peer.recv(3,5000) == b"a\\0b"
  client.abort()
  peer.abort()
  mode += 1
 end
 assert cancellation.cancel()
 int failures=0
 try
  auto ignored=await nativeDialAsyncCancel(engine,resolver,"127.0.0.1",port,role,options,-1,parent)
 catch failure
  failures += 1
 end
 try
  auto ignored=await nativeDialAsyncUntil(engine,resolver,"127.0.0.1",port,role,options,-1,uint64(0))
 catch failure
  failures += 1
 end
 assert failures == 2
 server.close()
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
auto resolver=NetResolver()
engine.start()
await exerciseDial(engine,resolver)
resolver.close()
assert resolver.wait(10000)
engine.close()
assert engine.wait(10000)
println("resident TLS dial: 20 direct signatures, names, owned results and cancellation: ok")
'''
    proxy = classes + old_contexts + '''async func exerciseProxy(NetEngine engine,NetResolver resolver)
 int __tls_proxy_port = 1
 auto proxy=NetProxy(NetProxyType.httpConnect,"127.0.0.1",__tls_proxy_port)
 auto client=await nativeProxyDialAsyncTimeout(engine,resolver,proxy,"127.0.0.1",443,clientContext(),TlsDialOptions().withServerName("localhost"),-1,5000)
 assert client.state == 2
 client.abort()
end
auto engine=NetEngine(NetEngineOptions().withWorkers(2).withBackend(NetBackend.select))
auto resolver=NetResolver()
engine.start()
await exerciseProxy(engine,resolver)
resolver.close()
assert resolver.wait(10000)
engine.close()
assert engine.wait(10000)
println("resident TLS proxy dial: local CONNECT and owned TLS: ok")
'''
    paths=[]
    for name_, source in [('direct', direct), ('proxy', proxy), ('contracts', classes+'println("contracts")\n')]:
        target=out/('resident_tls_dial_'+name_+'.xl'); target.write_text(source, encoding='utf8'); paths.append(target)
    return paths
