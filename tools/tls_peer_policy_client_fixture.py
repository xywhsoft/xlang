"""Normal client-auth peer: empty reference name, owning snapshots, no I/O."""
from pathlib import Path


def fixture(out, der_directory):
    out, der_directory = Path(out), Path(der_directory)
    literal = lambda data: 'b"'+''.join('\\x%02x'%b for b in data)+'"'
    data = {name: literal((der_directory/('p256.'+name+'.der')).read_bytes())
            for name in ('root', 'mid', 'client')}
    path = out/'tls.peer-policy.client.xl'
    path.write_text('''class ClientPeerState
 TlsVerifiedPeer? saved = null
end
func inspectClient() as TlsVerifiedPeer
 auto trust = TrustStore()
 assert trust.add(Certificate.fromDer('''+data['root']+'''))
 auto policy = TlsVerificationPolicy().withTime(time(1893456000000000))
 auto state = ClientPeerState()
 auto verifier = TlsVerifier(trust.snapshot(), policy, func(TlsVerifiedPeer peer) as bool
  assert peer.role == TlsRole.client
  assert peer.verificationName == ""
  assert peer.verifiedAt == time(1893456000000000)
  assert peer.certificateCount == 2 and peer.pathCount == 2
  state.saved = peer
  return true
 end)
 array<Certificate> chain = [Certificate.fromDer('''+data['client']+'''), Certificate.fromDer('''+data['mid']+''')]
 verifier.verifyClient(chain)
 return state.saved as TlsVerifiedPeer
end
auto peer = inspectClient()
assert peer.role == TlsRole.client and peer.verificationName == ""
assert peer.certificateAt(0).toDer() == '''+data['client']+'''
assert peer.pathAt(1).toDer() == '''+data['mid']+'''
assert peer.anchor.toDer() == '''+data['root']+'''
println("TLS client peer predicate: empty owned name and detached certificates passed")
''', encoding='utf8')
    return path
