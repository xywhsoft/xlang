"""Ordinary offline fixed-key X.509 profiles; no network or attack samples."""
from datetime import datetime,timezone
import ipaddress
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from check_key_format_wire import vectors
from check_x509_owned import children,content,tlv


def fixtures(root,out):
    key=vectors(root)[3]['key'];oid=x509.ExtensionOID
    name=x509.Name([x509.NameAttribute(x509.NameOID.COUNTRY_NAME,'CN'),x509.NameAttribute(x509.NameOID.COMMON_NAME,'profile fixture')])
    directory=x509.DirectoryName(name)
    san=x509.SubjectAlternativeName([x509.OtherName(x509.ObjectIdentifier('1.2.3.4'),tlv(12,b'other')),
        x509.RFC822Name('person@sub.example.invalid'),x509.DNSName('service.example.invalid'),
        x509.UniformResourceIdentifier('https://service.example.invalid/item'),x509.IPAddress(ipaddress.ip_address('192.0.2.9')),
        x509.IPAddress(ipaddress.ip_address('2001:db8::9')),directory,x509.RegisteredID(x509.ObjectIdentifier('1.2.3.4'))])
    ski=b'\0'+bytes(range(1,20))
    aki=x509.AuthorityKeyIdentifier(ski,[directory],128)
    relative=x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COMMON_NAME,'relative CRL')])
    points=x509.CRLDistributionPoints([
        x509.DistributionPoint([x509.UniformResourceIdentifier('https://crl.example.invalid/full')],None,
            frozenset([x509.ReasonFlags.key_compromise,x509.ReasonFlags.ca_compromise]),[directory]),
        x509.DistributionPoint(None,relative,None,None)])
    freshest=x509.FreshestCRL([x509.DistributionPoint([x509.UniformResourceIdentifier('https://crl.example.invalid/delta')],None,None,None)])
    permitted=[x509.DNSName('example.invalid'),x509.IPAddress(ipaddress.ip_network('192.0.2.0/24')),
               x509.DirectoryName(x509.Name([x509.NameAttribute(x509.NameOID.COUNTRY_NAME,'CN')]))]
    constraints=x509.NameConstraints(permitted,[x509.DNSName('blocked.example.invalid')])
    extensions=[(san,False),(x509.IssuerAlternativeName([directory]),False),
        (x509.KeyUsage(True,False,False,False,True,True,True,True,True),True),
        (x509.BasicConstraints(True,0),True),
        (x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.SERVER_AUTH,x509.ExtendedKeyUsageOID.CLIENT_AUTH,x509.ObjectIdentifier('1.2.3.4')]),False),
        (x509.SubjectKeyIdentifier(ski),False),(aki,False),(points,False),(freshest,False),(constraints,True)]
    rows=[]
    for index,(label,exts) in enumerate([('profile',extensions),('leaf',[(x509.BasicConstraints(False,None),True),
        (x509.SubjectAlternativeName([x509.DNSName('service.example.invalid'),x509.IPAddress(ipaddress.ip_address('192.0.2.9'))]),False)]),('sparse',[])]):
        builder=(x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(128+index)
                 .not_valid_before(datetime(2020,1,1,tzinfo=timezone.utc)).not_valid_after(datetime(2040,1,1,tzinfo=timezone.utc)))
        for value,critical in exts:builder=builder.add_extension(value,critical)
        cert=builder.sign(key,None);der=cert.public_bytes(serialization.Encoding.DER)
        oracle=x509.load_der_x509_certificate(der);key.public_key().verify(oracle.signature,oracle.tbs_certificate_bytes)
        (out/(label+'.der')).write_bytes(der);rows.append(dict(name=label,der=der,oracle=oracle))
    full=rows[0]['oracle']
    named={key:full.extensions.get_extension_for_oid(value).value.public_bytes() for key,value in [
        ('san',oid.SUBJECT_ALTERNATIVE_NAME),('ian',oid.ISSUER_ALTERNATIVE_NAME),('eku',oid.EXTENDED_KEY_USAGE),
        ('aki',oid.AUTHORITY_KEY_IDENTIFIER),('points',oid.CRL_DISTRIBUTION_POINTS),('freshest',oid.FRESHEST_CRL),('constraints',oid.NAME_CONSTRAINTS)]}
    named['ski']=ski;named['directory']=name.public_bytes()
    fields=children(named['constraints']);named['permitted']=tlv(48,content(fields[0]));named['excluded']=tlv(48,content(fields[1]))
    named['distances']=tlv(48,tlv(130,b'example.invalid')+tlv(128,b'\0\x80')+tlv(129,b'\x01\0'))
    aki_fields=children(named['aki']);named['aki_issuer']=tlv(48,content(aki_fields[1]));named['aki_serial']=content(aki_fields[2])
    dp=children(named['points']);named['point0']=dp[0];named['point1']=dp[1]
    named['name0']=content(children(dp[0])[0]);named['name1']=content(children(dp[1])[0])
    named['fullnames']=tlv(48,content(named['name0']));named['relative']=tlv(49,content(named['name1']))
    named['point_issuer']=tlv(48,content(children(dp[0])[-1]))
    lines=[]
    def span(symbol,data):
        lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
        lines.append('static const xbytesview '+symbol+'_view={'+symbol+',sizeof('+symbol+')};')
    for row in rows:span('fixture_'+row['name'],row['der'])
    for label,data in named.items():span('expected_'+label,data)
    lines+=['typedef struct GeneralNameOracle {xbytesview raw,value;int64_t kind;} GeneralNameOracle;']
    records=[]
    for i,raw in enumerate(children(named['san'])):
        span('general_'+str(i),raw);span('general_value_'+str(i),content(raw))
        records.append('{'+f'{{general_{i},sizeof(general_{i})}},{{general_value_{i},sizeof(general_value_{i})}},'+str(raw[0]&31)+'}')
    lines+=['static const GeneralNameOracle generalNames[]={'+','.join(records)+'};']
    (out/'x509_profile_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
    for label,data in named.items():(out/(label+'.der')).write_bytes(data)
    return rows,named
