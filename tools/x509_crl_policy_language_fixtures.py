"""Source-language normal policy fixtures. No network or security campaigns."""
from pathlib import Path
from x509_crl_policy_fixtures import fixtures


def language_fixtures(root,out):
    named=fixtures(root,out);cases=[]
    literal=lambda b:'b"'+''.join('\\x%02x'%v for v in b)+'"'
    header='\n'.join('bytes '+n+'Der = '+literal(named[n]) for n in named)+'\n'
    header+='time at = time(1767323045000000)\n'
    def emit(name,body,data=True):
        path=out/(name+'.xl');path.write_text((header if data else '')+body+'\nprintln("ok")\n',encoding='utf8');cases.append(path)
    emit('policy.enums','''
auto flags = X509CrlPolicyFlags.requireNextUpdate | X509CrlPolicyFlags.requireNumber
if (flags & X509CrlPolicyFlags.requireNumber) != X509CrlPolicyFlags.requireNumber then throw RuntimeError "flags" end
auto status = X509RevocationStatus.unknown
if status == X509RevocationStatus.good then throw RuntimeError "unknown" end
''',False)
    emit('policy.empty','''
auto certificate = Certificate.fromDer(targetDer)
auto check = X509RevocationCheck(certificate)
if check.determined or check.result.status != X509RevocationStatus.unknown then throw RuntimeError "empty" end
if check.result.coveredReasons != X509ReasonFlags.none or check.result.entry != null then throw RuntimeError "empty result" end
''')
    emit('policy.normal','''
auto issuer = Certificate.fromDer(issuerDer)
auto target = Certificate.fromDer(targetDer)
auto base = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(baseDer), issuer, at)
auto delta = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(deltaDer), issuer, at)
if base.isDelta or not delta.isDelta or base.at != at or base.skew != 0 then throw RuntimeError "metadata" end
if base.flags != X509CrlPolicyFlags.strict or base.crl.toDer() != baseDer or base.issuer.toDer() != issuerDer then throw RuntimeError "copies" end
auto revoked = base.query(target)
if revoked.status != X509RevocationStatus.revoked or not revoked.hasObservation then throw RuntimeError "revoked" end
auto entry = revoked.entry
if entry == null then throw RuntimeError "entry" end
if entry.reason != X509CrlReason.certificateHold then throw RuntimeError "reason" end
auto removed = delta.query(target)
if removed.status != X509RevocationStatus.removed then throw RuntimeError "removed" end
auto source = X509CrlSet(base, delta)
if source.base.crl.toDer() != baseDer or not source.delta.isDelta then throw RuntimeError "set owners" end
auto good = source.query(target)
if good.status != X509RevocationStatus.good then throw RuntimeError "complete delta" end
auto removal = good.entry
if removal == null then throw RuntimeError "removal evidence" end
if removal.reason != X509CrlReason.removeFromCrl then throw RuntimeError "removal reason" end
auto combined = X509RevocationCheck(target).withSet(source)
if not combined.determined or combined.result.status != X509RevocationStatus.good then throw RuntimeError "with set" end
auto confirmed = X509RevocationCheck(target).withCrl(base)
if not confirmed.determined or confirmed.result.status != X509RevocationStatus.revoked then throw RuntimeError "with revoked" end
''')
    emit('policy.partial','''
auto issuer = Certificate.fromDer(issuerDer)
auto target = Certificate.fromDer(clearDer)
auto left = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(leftDer), issuer, at)
auto right = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(rightDer), issuer, at)
auto observed = left.query(target)
if observed.status != X509RevocationStatus.unknown or not observed.hasObservation then throw RuntimeError "partial status" end
if observed.coveredReasons != X509ReasonFlags.keyCompromise then throw RuntimeError "partial reasons" end
auto empty = X509RevocationCheck(target)
auto partial = empty.withCrl(left)
auto complete = partial.withCrl(right)
if empty.determined or partial.determined or not complete.determined then throw RuntimeError "snapshots" end
if empty.result.hasObservation or not partial.result.hasObservation then throw RuntimeError "partial observation" end
if complete.result.status != X509RevocationStatus.good or complete.result.coveredReasons != X509ReasonFlags.all then throw RuntimeError "coverage" end
bool caught = false
try
    auto unused = complete.withCrl(left)
catch RuntimeError error
    caught = true
end
if not caught then throw RuntimeError "final state" end
''')
    emit('policy.callback','''
auto issuer = Certificate.fromDer(issuerDer)
X509CrlEntry? savedEntry = null
X509Extension? savedExtension = null
int count = 0
auto handler = func(X509CrlEntry? entry, X509Extension extension) as bool
    count += 1
    savedExtension = extension
    if entry != null then savedEntry = entry end
    return extension.critical
end
auto validated = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(criticalDer), issuer, at, handler)
if count != 2 then throw RuntimeError "callback count" end
if savedEntry == null or savedExtension == null then throw RuntimeError "owned callback data" end
if savedEntry.version != X509CrlVersion.v2 or not savedExtension.critical then throw RuntimeError "saved fields" end
if validated.query(Certificate.fromDer(targetDer)).status != X509RevocationStatus.revoked then throw RuntimeError "accepted" end
''')
    emit('policy.errors','''
auto issuer = Certificate.fromDer(issuerDer)
auto crl = CertificateRevocationList.fromDer(baseDer)
bool caught = false
try
    auto unused = X509ValidatedCrl.fromCrl(crl, issuer, at, -1)
catch RangeError error
    caught = true
end
if not caught then throw RuntimeError "negative skew" end
auto handler = func(X509CrlEntry? entry, X509Extension extension) as bool
    throw RangeError "original callback error"
    return true
end
caught = false
try
    auto unused = X509ValidatedCrl.fromCrl(CertificateRevocationList.fromDer(criticalDer), issuer, at, handler)
catch RangeError error
    caught = true
end
if not caught then throw RuntimeError "callback error type" end
auto context = __X509CrlCriticalContext()
caught = false
try
    auto unused = context.extension
catch RuntimeError error
    caught = true
end
if not caught then throw RuntimeError "inactive context" end
''')
    emit('policy.locals','''
class HeldPolicyRecords
    X509CrlEntry? entry = null
    X509Extension? extension = null
    int count = 0
end
func collect(CertificateRevocationList crl, Certificate issuer, time at) as HeldPolicyRecords
    auto saved = HeldPolicyRecords()
    auto handler = func(X509CrlEntry? entry, X509Extension extension) as bool
        saved.count += 1
        saved.extension = extension
        if entry != null then saved.entry = entry end
        return true
    end
    auto validated = X509ValidatedCrl.fromCrl(crl, issuer, at, handler)
    return saved
end
auto saved = collect(CertificateRevocationList.fromDer(criticalDer), Certificate.fromDer(issuerDer), at)
auto retainedEntry = saved.entry
auto retainedExtension = saved.extension
if saved.count != 2 then throw RuntimeError "local callback count" end
if retainedEntry == null then throw RuntimeError "local entry owner" end
if retainedExtension == null then throw RuntimeError "local extension owner" end
if not retainedExtension.critical then throw RuntimeError "retained extension" end
''')
    contract=out/'policy.contract.xl';contract.write_text('''
func contracts(CertificateRevocationList crl, Certificate cert, X509ValidatedCrl validated, X509CrlSet source, X509RevocationResult result, X509RevocationCheck check)
    auto a = X509ValidatedCrl.fromCrl(crl, cert, time(0))
    auto b = X509ValidatedCrl.fromCrl(crl, cert, time(0), func(X509CrlEntry? e, X509Extension x) as bool
        return true
    end)
    auto c = validated.crl
    auto d = validated.issuer
    auto e = validated.at
    auto f = validated.skew
    auto g = validated.flags
    auto h = validated.isDelta
    auto i = validated.query(cert)
    auto j = X509CrlSet(validated, validated)
    auto k = source.base
    auto l = source.delta
    auto m = source.query(cert)
    auto n = result.status
    auto o = result.coveredReasons
    auto p = result.hasObservation
    auto q = result.entry
    auto r = X509RevocationCheck(cert)
    auto s = check.withCrl(validated)
    auto t = check.withSet(source)
    auto u = check.result
    auto v = check.determined
    auto context = __X509CrlCriticalContext()
    auto w = context.entry
    auto x = context.extension
end
''',encoding='utf8')
    return cases,contract
