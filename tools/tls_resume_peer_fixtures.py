"""Normal offline full/resumed TLS with current trust and language policies."""
from pathlib import Path
from tls_identity_fixtures import fixtures as identities
from tls_revocation_fixtures import fixtures as revocation


def fixtures(root, out):
    out = Path(out)
    keys = identities(root, out)
    assert revocation(root, out) == ['rsa', 'p256', 'p384', 'ed25519']
    literal = lambda data: 'b"'+''.join('\\x%02x' % b for b in data)+'"'
    methods = dict(rsa='fromRsa', p256='fromP256', p384='fromP384', ed25519='fromEd25519')
    constructors = dict(rsa='RsaPrivateKey.fromPkcs1', p256='P256Key', p384='P384Key', ed25519='Ed25519Key')
    helpers = '''
class ResumePolicyState
 int crls = 0
 int additional = 0
 bool known = true
 bool allow = true
end
class ResumeCache
 TlsResume? ticket = null
 func find(TlsResumeRequest request) as TlsResume?
  return self.ticket
 end
end
func move(TlsSession source, TlsSession target)
 while source.pendingCipher > 0
  assert target.feed(source.takeCipher(317)) == TlsResult.ok
 end
end
func ready(TlsSession client, TlsSession server)
 int rounds = 0
 while not client.ready or not server.ready
  auto a = client.drive()
  assert a == TlsResult.ok or a == TlsResult.wouldBlock
  move(client, server)
  auto b = server.drive()
  assert b == TlsResult.ok or b == TlsResult.wouldBlock
  move(server, client)
  rounds += 1
  assert rounds < 256
 end
end
func connect(TlsClientContext context, TlsServerContext serverContext, ResumeCache cache, bool resumed)
 auto client = TlsSession(context, "localhost")
 auto server = TlsSession(serverContext.withResumeLookup(cache.find))
 try
  ready(client, server)
  assert client.resumed == resumed and server.resumed == resumed
  assert client.peerCertificateCount == 2
 finally
  client.dispose()
  server.dispose()
 end
end
'''
    cases = []
    for row in keys:
        name = row['Name']
        der = {part: literal((out/(name+'.'+part+'.der')).read_bytes())
               for part in ['root', 'mid', 'leaf', 'root.crl', 'mid.crl']}
        key = constructors[name]+'('+literal(Path(row['KeyPath']).read_bytes())+')'
        text = helpers+'''
auto state = ResumePolicyState()
auto trust = TrustStore()
assert trust.add(Certificate.fromDer('''+der['root']+'''))
auto base = TlsVerificationPolicy().withTime(time(1893456000000000))
auto crl = TlsRevocationPolicy(func(TlsRevocationRequest request) as X509RevocationCheck?
 state.crls += 1
 if not state.known then return null end
 assert request.verifiedAt == base.fixedTime
 if request.index == 0
  auto source = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+der['mid.crl']+'''), request.issuer, request.verifiedAt)
  return X509RevocationCheck(request.target).withCrl(source)
 end
 auto source = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+der['root.crl']+'''), request.issuer, request.verifiedAt)
 return X509RevocationCheck(request.target).withCrl(source)
end)
auto verifier = TlsVerifier(trust.snapshot(), base, func(TlsVerifiedPeer peer) as bool
 state.additional += 1
 return state.allow and peer.verificationName == "localhost"
end, crl)
auto context = TlsClientContext(verifier)
array<Certificate> chain = [Certificate.fromDer('''+der['leaf']+'''), Certificate.fromDer('''+der['mid']+''')]
auto identity = TlsIdentity.'''+methods[name]+'(chain, '+key+''')
auto serverContext = TlsServerContext(identity)
auto first = TlsSession(context, "localhost")
auto firstServer = TlsSession(serverContext)
ready(first, firstServer)
assert state.crls == 2 and state.additional == 1
auto issued = firstServer.issueTicket(b"normal\\0ticket", 120)
assert issued.status == TlsResult.ok
move(firstServer, first)
auto driven = first.drive()
assert driven == TlsResult.ok or driven == TlsResult.wouldBlock
auto received = first.tryTakeTicket()
assert received.status == TlsResult.ok
auto ticket = received.resume as TlsResume
assert ticket.peerCertificateCount == 2 and ticket.peerCertificateAt(0).toDer() == '''+der['leaf']+'''
first.dispose()
firstServer.dispose()
auto cache = ResumeCache()
cache.ticket = issued.resume
connect(context.withRequiredResume(ticket), serverContext, cache, true)
assert state.crls == 4 and state.additional == 2

auto raw = TlsResume.fromParts(ticket.version, ticket.cipher, ticket.ticket, ticket.exportSecret(),
 ticket.issuedAt, ticket.lifetimeSeconds, ticket.ageAdd, ticket.serverName, ticket.protocol,
 ticket.peerIdentity, ticket.maxEarlyData)
assert raw.peerCertificateCount == 0
connect(context.withResume(raw), serverContext, cache, false)
assert state.crls == 6 and state.additional == 3
int caught = 0
try
 auto invalid = TlsSession(context.withRequiredResume(raw), "localhost")
catch Error expected
 caught += 1
end
assert caught == 1
array<Certificate> savedChain = [ticket.peerCertificateAt(0), ticket.peerCertificateAt(1)]
auto imported = raw.withPeerCertificates(savedChain)
assert raw.peerCertificateCount == 0 and imported.peerCertificateCount == 2
connect(context.withRequiredResume(imported), serverContext, cache, true)
assert state.crls == 8 and state.additional == 4

state.allow = false
try
 connect(context.withRequiredResume(ticket), serverContext, cache, true)
catch Error expected
 caught += 1
end
assert caught == 2 and state.crls == 10 and state.additional == 5
state.allow = true
state.known = false
try
 connect(context.withRequiredResume(ticket), serverContext, cache, true)
catch Error expected
 caught += 1
end
assert caught == 3 and state.crls == 11 and state.additional == 5
state.known = true
connect(context.withRequiredResume(ticket), serverContext, cache, true)
assert state.crls == 13 and state.additional == 6
println("TLS resume peer: '''+name+''' current trust/CRL/predicate, owning import and missing-chain rules checked")
'''
        path = out/('tls.resume.peer.'+name+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    empty = out/'tls.resume.peer.empty.xl'
    empty.write_text('assert 6 * 7 == 42\n', encoding='utf8')
    parts = out/'tls.resume.peer.parts.xl'
    parts.write_text('''auto value = TlsResume.fromParts(TlsVersion.v13, TlsCipherSuite.aes128GcmSha256,
 b"normal", b"01234567890123456789012345678901", time.now(), 120)
assert value.ticket == b"normal"
''', encoding='utf8')
    return cases+[parts, empty]
