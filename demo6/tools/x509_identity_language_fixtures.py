"""Ordinary fixed-key names and public ownership/ABI checks only."""
from x509_identity_fixtures import fixtures
def literal(data):return 'b"'+''.join('\\x%02x'%b for b in data)+'"'
def language_fixtures(root,out):
    rows=fixtures(root,out);cases=[]
    def put(name,text):
        path=out/(name+'.xl');path.write_text(text,encoding='utf8');cases.append(path)
    put('identity.dns','''if not x509.matchesDns("localhost","LOCALHOST") then throw RuntimeError "DNS match" end
if x509.matchesDns("localhost","other.test") then throw RuntimeError "different DNS" end
if not x509.matchesDns("localhost","localhost",1000,1000) then throw RuntimeError "clamped views" end
auto caught=false
try
    x509.matchesDns("localhost","localhost",-2)
catch RangeError error
    caught=true
end
if not caught then throw RuntimeError "pure call can still raise" end
''')
    body='''func check(bytes der)
    auto certificate=Certificate.fromDer(der)
    if not certificate.matchesHost("localhost") or not certificate.matchesHost("127.0.0.1") or not certificate.matchesHost("::1") then throw RuntimeError "normal SAN identities" end
    if certificate.matchesHost("other.test") then throw RuntimeError "ordinary no-match" end
    if not certificate.matchesHost("localhost",1000) or not certificate.matchesHost("::1\\0",3) then throw RuntimeError "explicit size" end
end
'''
    for row in rows:body+='check('+literal((out/(row['Name']+'.identity.der')).read_bytes())+')\n'
    put('identity.bool',body)
    body='''func detached(bytes der) as X509GeneralName?
    auto certificate=Certificate.fromDer(der)
    if certificate.matchHost("other.test") != null then throw RuntimeError "nullable no-match" end
    return certificate.matchHost("localhost")
end
func check(bytes der)
    auto name=detached(der)
    if name == null then throw RuntimeError "owned identity missing" end
    if name.kind != X509GeneralNameKind.dns or name.text != "localhost" or name.value != b"localhost" or name.isConstraintBase then throw RuntimeError "detached name" end
    auto certificate=Certificate.fromDer(der)
    auto ip=certificate.matchHost("127.0.0.1")
    if ip == null then throw RuntimeError "owned IP missing" end
    if ip.kind != X509GeneralNameKind.ip or ip.value != b"\\x7f\\x00\\x00\\x01" then throw RuntimeError "numeric identity bytes" end
end
'''
    for row in rows:body+='check('+literal((out/(row['Name']+'.identity.der')).read_bytes())+')\n'
    put('identity.owned',body)
    der=literal((out/'p256.identity.der').read_bytes())
    put('identity.errors','''auto certificate=Certificate.fromDer('''+der+''')
auto caught=false
try
    certificate.matchesHost("::1\\0")
catch ValueError error
    caught=true
end
if not caught then throw RuntimeError "full logical host length" end
caught=false
try
    auto name=certificate.matchHost("localhost",-2)
catch RangeError error
    caught=true
end
if not caught or not certificate.matchesHost("localhost") then throw RuntimeError "error leaves certificate intact" end
''')
    contract=out/'identity-contract.xl';contract.write_text('''func contracts(Certificate certificate)
auto dns=x509.matchesDns("localhost","localhost",-1,-1)
auto host=certificate.matchesHost("localhost",-1)
auto name=certificate.matchHost("localhost",-1)
end
''')
    return cases,contract
