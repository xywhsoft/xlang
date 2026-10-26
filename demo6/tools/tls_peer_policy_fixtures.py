"""Ordinary fixed TLS trust with an additional language predicate; no network."""
from pathlib import Path
from tls_verifier_fixtures import fixtures as trust_fixtures


def fixtures(root, out):
    out = Path(out)
    families = trust_fixtures(root, out)
    literal = lambda data: 'b"'+''.join('\\x%02x'%b for b in data)+'"'
    cases = []
    for family in families:
        der = {part: literal((out/(family+'.'+part+'.der')).read_bytes()) for part in ('root', 'leaf', 'mid')}
        text = '''class PeerState
 int calls = 0
 TlsVerifiedPeer? saved = null
end
func inspect(PeerState state) as TlsVerifiedPeer
 auto store = TrustStore()
 assert store.add(Certificate.fromDer('''+der['root']+'''))
 auto policy = TlsVerificationPolicy().withTime(time(1893456000000000))
 auto verifier = TlsVerifier(store.snapshot(), policy, func(TlsVerifiedPeer peer) as bool
  state.calls += 1
  state.saved = peer
  assert peer.role == TlsRole.server
  assert peer.verifiedAt == time(1893456000000000)
  assert peer.verificationName == "localhost"
  assert peer.certificateCount == 2 and peer.pathCount == 2
  return true
 end)
 array<Certificate> chain = [Certificate.fromDer('''+der['leaf']+'''), Certificate.fromDer('''+der['mid']+''')]
 verifier.verifyServer(chain, "localhost")
 return state.saved as TlsVerifiedPeer
end
auto state = PeerState()
auto saved = inspect(state)
assert state.calls == 1
assert saved.certificateAt(0).toDer() == '''+der['leaf']+'''
assert saved.pathAt(1).toDer() == '''+der['mid']+'''
assert saved.anchor.toDer() == '''+der['root']+'''
int caught = 0
try
 auto invalid = saved.pathAt(-1)
catch RangeError expected
 caught += 1
end
try
 auto invalid = saved.certificateAt(2)
catch RangeError expected
 caught += 1
end
assert caught == 2 and saved.verificationName == "localhost"
println("TLS owned peer predicate: '''+family+''' passed")
'''
        path = out/('tls.peer-policy.'+family+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    der = {part: literal((out/('p256.'+part+'.der')).read_bytes()) for part in ('root', 'leaf', 'mid')}
    for kind, statement, error in [('reject', 'return false', 'Error'),
                                    ('throw', 'throw RangeError("peer-policy-first-error")', 'RangeError')]:
        text = '''auto store = TrustStore()
assert store.add(Certificate.fromDer('''+der['root']+'''))
auto policy = TlsVerificationPolicy().withTime(time(1893456000000000))
int calls = 0
auto verifier = TlsVerifier(store.snapshot(), policy, func(TlsVerifiedPeer peer) as bool
 calls += 1
 assert peer.verificationName == "localhost"
 '''+statement+'''
end)
array<Certificate> chain = [Certificate.fromDer('''+der['leaf']+'''), Certificate.fromDer('''+der['mid']+''')]
int caught = 0
try
 verifier.verifyServer(chain, "localhost")
catch '''+error+''' expected
 caught += 1
end
assert calls == 1 and caught == 1
println("TLS owned peer predicate: '''+kind+''' passed")
'''
        path = out/('tls.peer-policy.'+kind+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    # A real language closure cycle, not a stand-in C ownership capability.
    # Checked context/module retirement must collect the captured state's
    # Verifier -> native Context -> driver -> target -> state -> Verifier loop.
    cycle = out/'tls.peer-policy.cycle.xl'
    source = (out/'tls.peer-policy.p256.xl').read_text(encoding='utf8')
    assert source.count(' TlsVerifiedPeer? saved = null') == 1
    assert source.count(' array<Certificate> chain =') == 1
    source = source.replace(' TlsVerifiedPeer? saved = null',
                            ' TlsVerifiedPeer? saved = null\n TlsVerifier? owner = null', 1)
    source = source.replace(' array<Certificate> chain =',
                            ' state.owner = verifier\n array<Certificate> chain =', 1)
    cycle.write_text(source, encoding='utf8')
    cases.append(cycle)
    empty = out/'tls.peer-policy.empty.xl'
    empty.write_text('assert 6 * 7 == 42\n', encoding='utf8')
    return cases, empty
