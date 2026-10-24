"""Ordinary role configurations; fixed public keys, no network/security tests."""
from pathlib import Path
from tls_identity_fixtures import fixtures as identities
from tls_verifier_fixtures import fixtures as verifiers
from x509_identity_language_fixtures import literal

def fixtures(root,out):
    out=Path(out);rows=identities(root,out);assert verifiers(root,out)==['rsa','p256','p384','ed25519']
    (out/'tls_role_context_vectors.h').write_text('#include "tls_verifier_vectors.h"\n#include "tls_identity_keys.h"\n')
    return rows

def language_fixtures(root,out):
    out=Path(out);rows=fixtures(root,out);cases=[]
    def put(name,text):
        p=out/(name+'.xl');p.write_text(text);cases.append(p);return p
    methods={'rsa':'fromRsa','p256':'fromP256','p384':'fromP384','ed25519':'fromEd25519'}
    constructors={'rsa':'RsaPrivateKey.fromPkcs1','p256':'P256Key','p384':'P384Key','ed25519':'Ed25519Key'}
    for row in rows:
        name=row['Name'];key=constructors[name]+'('+literal(Path(row['KeyPath']).read_bytes())+')'
        rootDer=literal((out/(name+'.root.der')).read_bytes());leaf=literal((out/(name+'.leaf.der')).read_bytes());mid=literal((out/(name+'.mid.der')).read_bytes())
        put('tls.context.'+name,'''func makeClient() as TlsClientContext
 auto store=TrustStore()
 assert store.add(Certificate.fromDer('''+rootDer+'''))
 auto verifier=TlsVerifier(store.snapshot(),TlsVerificationPolicy().withTime(time(1893456000000000)))
 return TlsClientContext(verifier)
end
func makeServer() as TlsServerContext
 array<Certificate> chain=[Certificate.fromDer('''+leaf+'''),Certificate.fromDer('''+mid+''')]
 auto identity=TlsIdentity.'''+methods[name]+'(chain,'+key+''')
 return TlsServerContext(identity)
end
array<bytes> protocols=[b"h2",b"http/1.1",b"x\\0\\xff"]
auto original=makeClient()
auto client=original.withProtocols(protocols)
auto server=makeServer().withProtocols(protocols,true)
protocols[0]=b"changed"
assert original.protocolCount == 0 and client.protocolCount == 3 and server.protocolCount == 3
assert client.protocolAt(0) == b"h2" and server.protocolAt(1) == b"http/1.1"
auto detached=client.protocolAt(2)
detached[0]=65
assert server.protocolAt(2) == b"x\\0\\xff" and client.protocolAt(2) == b"x\\0\\xff"
assert server.requiresProtocol and server.identity.count == 2
auto parameters=TlsParameters(TlsPolicy(),TlsLimits(524288,524288,524288))
auto changed=client.withParameters(parameters).withVerifier(client.verifier)
auto rebound=server.withParameters(parameters).withIdentity(server.identity)
assert changed.parameters.limits.feedLimit == 524288 and rebound.parameters.limits.feedLimit == 524288
assert changed.protocolAt(2) == b"x\\0\\xff" and rebound.requiresProtocol
auto explicitClient=TlsClientContext(parameters,changed.verifier)
auto explicitServer=TlsServerContext(parameters,rebound.identity)
assert explicitClient.protocolCount == 0 and not explicitServer.requiresProtocol
array<Certificate> chain=[Certificate.fromDer('''+leaf+'''),Certificate.fromDer('''+mid+''')]
changed.verifier.verifyServer(chain,"localhost")
assert changed.verifier.policy.fixedTime == time(1893456000000000)
''')
    put('tls.context.owned',cases[1].read_text()+'''
func reader(TlsClientContext client,TlsServerContext server) as func() as int
 return func() as int
  if client.protocolAt(2) == b"x\\0\\xff" and server.identity.count == 2 and server.requiresProtocol then return 0 end
  return 1
 end
end
array<TlsClientContext> clients=[client]
array<TlsServerContext> servers=[server]
auto first=thread(reader(clients[0],servers[0]))
auto second=thread(reader(clients[0],servers[0]))
assert first.waitTimeout(30000) and second.waitTimeout(30000)
assert first.exitCode() == 0 and second.exitCode() == 0
int caught=0
try
 auto bad=client.protocolAt(-1)
catch RangeError expected
 caught+=1
end
try
 array<bytes> invalid=[b""]
 auto bad=client.withProtocols(invalid)
catch RangeError expected
 caught+=1
end
assert caught == 2
array<bytes> empty=[]
assert client.withProtocols(empty).protocolCount == 0
assert not server.withProtocols(empty).requiresProtocol
''')
    # Declaration-only dependency probes must not inject the other role or an
    # incomplete transitive X.509/identity/key declaration island.
    put('tls.context.client-only','func read(TlsClientContext client) as int\n return client.protocolCount\nend\n')
    put('tls.context.server-only','func read(TlsServerContext server) as bool\n return server.requiresProtocol\nend\n')
    contract=out/'tls.context.contract.xl';contract.write_text(cases[0].read_text())
    return cases,contract
