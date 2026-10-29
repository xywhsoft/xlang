"""Ordinary offline TLS chain/CRL source fixtures. No network operations."""
from datetime import datetime, timezone
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, padding
from check_key_format_wire import vectors
from tls_verifier_fixtures import fixtures as peers


def fixtures(root, out):
    out = Path(out)
    names = peers(root, out)
    lines = ['#include "tls_verifier_vectors.h"']
    records = []
    for row in vectors(root)[:4]:
        name, key = row['name'], row['key']
        crls = []
        for role in ['mid', 'root']:
            issuer = x509.load_der_x509_certificate((out/(name+'.'+role+'.der')).read_bytes())
            builder = (x509.CertificateRevocationListBuilder().issuer_name(issuer.subject)
                .last_update(datetime(2029, 1, 1, tzinfo=timezone.utc))
                .next_update(datetime(2031, 1, 1, tzinfo=timezone.utc))
                .add_extension(x509.CRLNumber(1), False)
                .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(key.public_key()), False))
            crl = builder.sign(key, None if name == 'ed25519' else hashes.SHA256())
            key.public_key().verify(crl.signature, crl.tbs_certlist_bytes,
                *([padding.PKCS1v15(), crl.signature_hash_algorithm] if name == 'rsa' else
                  [ec.ECDSA(crl.signature_hash_algorithm)] if name in ['p256', 'p384'] else []))
            data = crl.public_bytes(serialization.Encoding.DER)
            (out/(name+'.'+role+'.crl.der')).write_bytes(data)
            symbol = 'revocation_'+name+'_'+role
            lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
            crls.append('{'+symbol+',sizeof('+symbol+')}')
        records.append('{'+','.join(crls)+'}')
    lines.append('static const xbytesview revocationCrls[][2]={'+','.join(records)+'};')
    (out/'tls_revocation_vectors.h').write_text('\n'.join(lines)+'\n', encoding='utf8')
    return names


def language_fixtures(root, out):
    out = Path(out)
    names = fixtures(root, out)
    literal = lambda data: 'b"'+''.join('\\x%02x' % b for b in data)+'"'
    cases = []
    for name in names:
        data = {part: literal((out/(name+'.'+part+'.der')).read_bytes())
                for part in ['root', 'leaf', 'mid', 'mid.crl', 'root.crl']}
        text = '''class RevocationState
 int calls = 0
 int additional = 0
 TlsRevocationRequest? first = null
 TlsRevocationRequest? second = null
end
func inspect(RevocationState state) as TlsRevocationRequest
 auto store = TrustStore()
 assert store.add(Certificate.fromDer('''+data['root']+'''))
 auto base = TlsVerificationPolicy().withTime(time(1893456000000000))
 auto revocation = TlsRevocationPolicy(func(TlsRevocationRequest request) as X509RevocationCheck?
  state.calls += 1
  assert request.verifiedAt == base.fixedTime and request.skew == 0
  assert request.flags == X509CrlPolicyFlags.strict
  assert request.peer.verificationName == "localhost"
  bytes der = '''+data['root.crl']+'''
  if request.index == 0
   state.first = request
   der = '''+data['mid.crl']+'''
  else
   assert request.index == 1
   state.second = request
  end
  auto source = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(der), request.issuer,
      request.verifiedAt, request.skew, request.flags)
  return X509RevocationCheck(request.target).withCrl(source)
 end)
 assert not revocation.allowsUnknown and revocation.skew == 0
 assert revocation.flags == X509CrlPolicyFlags.strict
 assert revocation.maxSources == 64 and revocation.maxSignerCertificates == 64
 auto verifier = TlsVerifier(store.snapshot(), base, func(TlsVerifiedPeer peer) as bool
  assert state.calls == 2
  state.additional += 1
  return peer.verificationName == "localhost"
 end, revocation)
 array<Certificate> chain = [Certificate.fromDer('''+data['leaf']+'''), Certificate.fromDer('''+data['mid']+''')]
 verifier.verifyServer(chain, "localhost")
 return state.first as TlsRevocationRequest
end
auto state = RevocationState()
auto retained = inspect(state)
assert state.calls == 2 and state.additional == 1
assert retained.index == 0 and retained.target.toDer() == '''+data['leaf']+'''
assert retained.issuer.toDer() == '''+data['mid']+'''
assert (state.second as TlsRevocationRequest).issuer.toDer() == '''+data['root']+'''
assert retained.peer.anchor.toDer() == '''+data['root']+'''
println("TLS revocation: '''+name+''' checked")
'''
        path = out/('tls.revocation.'+name+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    data = {part: literal((out/('p256.'+part+'.der')).read_bytes()) for part in ['root', 'leaf', 'mid']}
    for name, body, lenient, error in [
        ('unknown', 'return null', 'false', 'Error'),
        ('lenient', 'return null', 'true', None),
        ('throw', 'throw RangeError("crl-provider-first-error")', 'false', 'RangeError')]:
        text = '''auto store = TrustStore()
assert store.add(Certificate.fromDer('''+data['root']+'''))
auto base = TlsVerificationPolicy().withTime(time(1893456000000000))
int calls = 0
auto revocation = TlsRevocationPolicy(func(TlsRevocationRequest request) as X509RevocationCheck?
 calls += 1
 assert request.index == calls - 1
 '''+body+'''
end, '''+lenient+''')
auto verifier = TlsVerifier(store.snapshot(), base, revocation)
array<Certificate> chain = [Certificate.fromDer('''+data['leaf']+'''), Certificate.fromDer('''+data['mid']+''')]
'''
        if error:
            text += '''int caught = 0
try
 verifier.verifyServer(chain, "localhost")
catch '''+error+''' expected
 caught += 1
end
assert caught == 1 and calls == 1
'''
        else:
            text += 'verifier.verifyServer(chain, "localhost")\nassert calls == 2 and revocation.allowsUnknown\n'
        text += 'println("TLS revocation: '+name+' checked")\n'
        path = out/('tls.revocation.'+name+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    source = cases[1].read_text(encoding='utf8')
    assert source.count(' int additional = 0') == source.count(' array<Certificate> chain =') == 1
    source = source.replace(' int additional = 0', ' int additional = 0\n TlsVerifier? owner = null', 1)
    source = source.replace(' array<Certificate> chain =', ' state.owner = verifier\n array<Certificate> chain =', 1)
    cycle = out/'tls.revocation.cycle.xl'
    cycle.write_text(source, encoding='utf8')
    cases.append(cycle)
    empty = out/'tls.revocation.empty.xl'
    empty.write_text('assert 6 * 7 == 42\n', encoding='utf8')
    cases.extend(extra_language_fixtures(root, out, literal))
    return cases, empty


def extra_language_fixtures(root, out, literal):
    """Valid partitioned/delta and delegated-signer CRLs, not malformed inputs."""
    key = vectors(root)[1]['key']
    issuer = x509.load_der_x509_certificate((out/'p256.mid.der').read_bytes())
    anchor = x509.load_der_x509_certificate((out/'p256.root.der').read_bytes())
    before = datetime(2020, 1, 1, tzinfo=timezone.utc)
    after = datetime(2040, 1, 1, tzinfo=timezone.utc)
    all_reasons = frozenset([x509.ReasonFlags.key_compromise, x509.ReasonFlags.ca_compromise,
        x509.ReasonFlags.affiliation_changed, x509.ReasonFlags.superseded,
        x509.ReasonFlags.cessation_of_operation, x509.ReasonFlags.certificate_hold,
        x509.ReasonFlags.privilege_withdrawn, x509.ReasonFlags.aa_compromise])
    left = frozenset([x509.ReasonFlags.key_compromise])
    def crl(name, issuer_name, number, extensions):
        builder = (x509.CertificateRevocationListBuilder().issuer_name(issuer_name)
            .last_update(datetime(2029, 1, 1, tzinfo=timezone.utc))
            .next_update(datetime(2031, 1, 1, tzinfo=timezone.utc))
            .add_extension(x509.CRLNumber(number), False)
            .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(key.public_key()), False))
        for value in extensions:
            builder = builder.add_extension(value, True)
        value = builder.sign(key, hashes.SHA256())
        key.public_key().verify(value.signature, value.tbs_certlist_bytes, ec.ECDSA(value.signature_hash_algorithm))
        path = out/('p256.'+name+'.crl.der')
        path.write_bytes(value.public_bytes(serialization.Encoding.DER))
        return literal(path.read_bytes())
    left_der = crl('left', issuer.subject, 2, [x509.IssuingDistributionPoint(None, None, False, False, left, False, False)])
    right_der = crl('right', issuer.subject, 3, [x509.IssuingDistributionPoint(None, None, False, False, all_reasons-left, False, False)])
    delta_der = crl('delta', issuer.subject, 2, [x509.DeltaCRLIndicator(1)])
    signer_name = x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME, 'ordinary delegated CRL signer')])
    uri = x509.UniformResourceIdentifier('https://crl.example.invalid/offline-only')
    def certificate(name, serial, signer):
        builder = (x509.CertificateBuilder().subject_name(name).issuer_name(issuer.subject)
            .public_key(key.public_key()).serial_number(serial).not_valid_before(before).not_valid_after(after)
            .add_extension(x509.BasicConstraints(False, None), True)
            .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()), False)
            .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(key.public_key()), False)
            .add_extension(x509.KeyUsage(not signer, False, False, False, False, False, signer, False, False), True))
        if not signer:
            builder = (builder.add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]), False)
                .add_extension(x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.SERVER_AUTH]), False)
                .add_extension(x509.CRLDistributionPoints([
                    x509.DistributionPoint([uri], None, None, [x509.DirectoryName(signer_name)])]), False))
        cert = builder.sign(key, hashes.SHA256())
        cert.verify_directly_issued_by(issuer)
        return cert.public_bytes(serialization.Encoding.DER)
    signer_der = certificate(signer_name, 20, True)
    leaf_der = certificate(x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME, 'ordinary indirect target')]), 21, False)
    (out/'p256.signer.der').write_bytes(signer_der)
    (out/'p256.indirect-leaf.der').write_bytes(leaf_der)
    indirect_der = crl('indirect', signer_name, 1, [x509.IssuingDistributionPoint([uri], None, False, False, None, True, False)])
    data = {part: literal((out/('p256.'+part+'.der')).read_bytes())
            for part in ['root', 'leaf', 'mid', 'mid.crl', 'root.crl']}
    setup = '''auto trust = TrustStore()
auto anchor = Certificate.fromDer('''+data['root']+''')
assert trust.add(anchor)
auto intermediate = Certificate.fromDer('''+data['mid']+''')
auto base = TlsVerificationPolicy().withTime(time(1893456000000000))
'''
    cases = []
    for name in ['partial', 'partitions', 'delta', 'indirect']:
        text = setup
        if name == 'indirect':
            text += '''auto signer = Certificate.fromDer('''+literal(signer_der)+''')
array<Certificate> signerChain = [signer, intermediate]
auto signerPolicy = X509PathPolicy(base.fixedTime, X509PathFlags.requireKeyUsage, X509KeyUsageFlags.crlSign)
auto outcome = X509Path.check(signerChain, X509TrustAnchor(anchor), signerPolicy)
outcome.requireValid()
auto signerPath = outcome.path as X509Path
'''
        elif name == 'delta':
            text += '''array<Certificate> signerChain = [intermediate]
auto signerPolicy = X509PathPolicy(base.fixedTime, X509PathFlags.requireKeyUsage, X509KeyUsageFlags.crlSign)
auto outcome = X509Path.check(signerChain, X509TrustAnchor(anchor), signerPolicy)
outcome.requireValid()
auto signerPath = outcome.path as X509Path
'''
        text += '''int calls = 0
auto policy = TlsRevocationPolicy(func(TlsRevocationRequest request) as X509RevocationCheck?
 calls += 1
 assert request.index == calls - 1
 if request.index == 1
  auto source = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+data['root.crl']+'''), request.issuer, request.verifiedAt)
  return X509RevocationCheck(request.target).withCrl(source)
 end
'''
        if name in ['partial', 'partitions']:
            text += ''' auto first = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+left_der+'''), request.issuer, request.verifiedAt)
 auto result = X509RevocationCheck(request.target).withCrl(first)
 assert not result.determined
'''
            if name == 'partitions':
                text += ''' auto second = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+right_der+'''), request.issuer, request.verifiedAt)
 result = result.withCrl(second)
 assert result.determined and result.result.status == X509RevocationStatus.good
'''
            text += ' return result\n'
        elif name == 'delta':
            text += ''' auto first = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+data['mid.crl']+'''), request.issuer, request.verifiedAt)
 auto second = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+delta_der+'''), request.issuer, request.verifiedAt)
 return X509RevocationCheck(request.target).withSet(X509CrlSet(first, second), signerPath)
'''
        else:
            text += ''' auto source = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer('''+indirect_der+'''), signer, request.verifiedAt)
 return X509RevocationCheck(request.target).withCrl(source, signerPath)
'''
        text += '''end)
auto verifier = TlsVerifier(trust.snapshot(), base, policy)
array<Certificate> chain = [Certificate.fromDer('''+(literal(leaf_der) if name == 'indirect' else data['leaf'])+'''), intermediate]
'''
        if name == 'partial':
            text += '''int caught = 0
try
 verifier.verifyServer(chain, "localhost")
catch Error expected
 caught += 1
end
assert caught == 1 and calls == 1
'''
        else:
            text += 'verifier.verifyServer(chain, "localhost")\nassert calls == 2\n'
        text += 'println("TLS revocation: '+name+' checked")\n'
        path = out/('tls.revocation.'+name+'.xl')
        path.write_text(text, encoding='utf8')
        cases.append(path)
    return cases
