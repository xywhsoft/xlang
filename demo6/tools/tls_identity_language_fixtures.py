"""Normal typed identity factories, independent ownership and thread reads."""
from pathlib import Path
import json
from tls_identity_fixtures import fixtures
from x509_identity_language_fixtures import literal

def language_fixtures(root,out):
    out=Path(out);rows=fixtures(root,out);cases=[]
    def put(name,text):
        path=out/(name+'.xl');path.write_text(text,encoding='utf8');cases.append(path);return path
    kinds={'rsa':'rsa','p256':'p256','p384':'p384','ed25519':'ed25519'}
    methods={'rsa':'fromRsa','p256':'fromP256','p384':'fromP384','ed25519':'fromEd25519'}
    def key(row):
        data=literal(Path(row['KeyPath']).read_bytes())
        return {'rsa':'RsaPrivateKey.fromPkcs1','p256':'P256Key','p384':'P384Key','ed25519':'Ed25519Key'}[row['Name']]+'('+data+')'
    for row in rows:
        name=row['Name'];der=literal(Path(row['Path']).read_bytes())
        put('tls.identity.'+name,'func make() as TlsIdentity\n'+
            ' array<Certificate> chain=[Certificate.fromDer('+der+')]\n auto key='+key(row)+'\n'+
            ' return TlsIdentity.'+methods[name]+'(chain,key)\nend\nauto identity=make()\n'+
            'assert identity.kind == TlsIdentityKind.'+kinds[name]+' and identity.count == 1\n'+
            'auto certificate=identity.at(0)\nassert certificate.toDer() == '+der+'\n')
    row=rows[1];der=literal(Path(row['Path']).read_bytes());private=literal(Path(row['KeyPath']).read_bytes())
    put('tls.identity.owned','''func make(bytes der,bytes privateKey) as TlsIdentity
 array<Certificate> chain=[Certificate.fromDer(der)]
 auto key=P256Key(privateKey)
 return TlsIdentity.fromP256(chain,key,0,0)
end
func reader(TlsIdentity identity) as func() as int
 return func() as int
  if identity.count == 1 and identity.kind == TlsIdentityKind.p256 and identity.at(0).version == 3 then return 0 end
  return 1
 end
end
auto identity=make('''+der+','+private+''')
array<TlsIdentity> identities=[identity]
auto first=thread(reader(identities[0]))
auto second=thread(reader(identities[0]))
assert first.waitTimeout(30000) and second.waitTimeout(30000)
assert first.exitCode() == 0 and second.exitCode() == 0
''')
    put('tls.identity.errors','array<Certificate> chain=[Certificate.fromDer('+der+')]\nauto key=P256Key('+private+')\n'+'''
int caught=0
try
 auto bad=TlsIdentity.fromP256(chain,key,-1)
catch RangeError expected
 caught+=1
end
try
 auto bad=TlsIdentity.fromP256(chain,key,1,1)
catch RangeError expected
 caught+=1
end
auto identity=TlsIdentity.fromP256(chain,key)
try
 auto bad=identity.at(-1)
catch RangeError expected
 caught+=1
end
try
 auto bad=identity.at(1)
catch RangeError expected
 caught+=1
end
assert caught == 4
''')
    fixture=json.loads((out/'tls-fixtures.json').read_text());longChain=','.join('Certificate.fromDer('+literal(Path(r['Path']).read_bytes())+')' for r in fixture['LongChain'])
    put('tls.identity.chain','array<Certificate> chain=['+longChain+']\nauto identity=TlsIdentity.fromP256(chain,P256Key('+private+'),9)\nassert identity.count == 9\nassert identity.at(8).toDer() == '+literal(Path(fixture['LongChain'][-1]['Path']).read_bytes())+'\n')
    put('tls.identity.enums','assert TlsIdentityKind.p256 == TlsIdentityKind.p256\n')
    contract=out/'tls-identity-contract.xl'
    contract.write_text('''func contracts(array<Certificate> chain,RsaPrivateKey rsa,P256Key p256,P384Key p384,Ed25519Key ed)
 auto a=TlsIdentity.fromRsa(chain,rsa)
 auto b=TlsIdentity.fromP256(chain,p256)
 auto c=TlsIdentity.fromP384(chain,p384)
 auto d=TlsIdentity.fromEd25519(chain,ed)
 auto kind=a.kind
 auto count=b.count
 auto certificate=c.at(0)
end
''')
    return cases,contract
