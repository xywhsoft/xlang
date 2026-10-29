"""Ordinary fixed-key CRLs and standalone DER values; offline data oracle only."""
from datetime import datetime,timezone
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from check_key_format_wire import vectors
from check_x509_owned import children,content,tlv


def fixtures(root,out):
    key=vectors(root)[3]['key'];utc=timezone.utc
    instant=datetime(2024,1,2,3,4,5,tzinfo=utc);invalidity=datetime(2023,12,31,tzinfo=utc)
    name=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary CRL fixture')])
    directory=x509.DirectoryName(name);ski=b'\0'+bytes(range(1,20))
    relative=x509.RelativeDistinguishedName([x509.NameAttribute(x509.NameOID.COMMON_NAME,'relative CRL')])
    idp=x509.IssuingDistributionPoint([x509.UniformResourceIdentifier('https://crl.example.invalid/full')],None,
        True,False,frozenset([x509.ReasonFlags.key_compromise]),True,False)
    relative_idp=x509.IssuingDistributionPoint(None,relative,False,True,None,False,False)
    aki=x509.AuthorityKeyIdentifier(ski,[directory],128)
    fresh=x509.FreshestCRL([x509.DistributionPoint([x509.UniformResourceIdentifier('https://crl.example.invalid/delta')],None,None,None)])
    plain=x509.RevokedCertificateBuilder().serial_number(129).revocation_date(instant).build()
    rich=(x509.RevokedCertificateBuilder().serial_number(128).revocation_date(instant)
        .add_extension(x509.CRLReason(x509.ReasonFlags.key_compromise),False)
        .add_extension(x509.InvalidityDate(invalidity),False)
        .add_extension(x509.CertificateIssuer([directory]),True).build())
    rows=[]
    for label,entries,extensions in [('full',[rich,plain],[(x509.CRLNumber(128),False),(aki,False),(idp,True),(fresh,False)]),
        ('delta',[plain],[(x509.CRLNumber(256),False),(x509.DeltaCRLIndicator(128),True),(relative_idp,True)]),
        ('sparse',[],[]),('v1',[plain],[])]:
        builder=x509.CertificateRevocationListBuilder().issuer_name(name).last_update(instant).next_update(datetime(2030,1,1,tzinfo=utc))
        for entry in entries:builder=builder.add_revoked_certificate(entry)
        for value,critical in extensions:builder=builder.add_extension(value,critical)
        der=builder.sign(key,None).public_bytes(serialization.Encoding.DER)
        if label=='v1':
            outer=children(der);tbs=tlv(48,b''.join(children(outer[0])[1:]));der=tlv(48,tbs+outer[1]+tlv(3,b'\0'+key.sign(tbs)))
        oracle=x509.load_der_x509_crl(der);key.public_key().verify(oracle.signature,oracle.tbs_certlist_bytes)
        fields=children(children(der)[0]);offset=1 if fields[0][0]==2 else 0
        # Algorithm, issuer, thisUpdate, nextUpdate precede revokedCertificates.
        lists=fields[offset+4:];encoded=next((item for item in lists if item[0]==48),tlv(48,b''))
        rows.append(dict(name=label,der=der,entries=encoded,oracle=oracle,version=1 if label=='v1' else 2))
    certificate=(x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(128)
        .not_valid_before(instant).not_valid_after(datetime(2030,1,1,tzinfo=utc)).sign(key,None))
    key.public_key().verify(certificate.signature,certificate.tbs_certificate_bytes)
    rich_raw,plain_raw=children(rows[0]['entries'])
    named=dict(certificate=certificate.public_bytes(serialization.Encoding.DER),entry=rich_raw,plain=plain_raw,
        extensions=children(rich_raw)[2],issuer=x509.CertificateIssuer([directory]).public_bytes(),
        idp=idp.public_bytes(),relative_idp=relative_idp.public_bytes(),
        indirect_idp=x509.IssuingDistributionPoint(None,None,False,False,None,True,False).public_bytes(),
        attribute_idp=x509.IssuingDistributionPoint(None,None,False,False,None,False,True).public_bytes(),
        aki=aki.public_bytes(),fresh=fresh.public_bytes(),ski=tlv(4,ski),ski_value=ski,
        number=tlv(2,b'\0\x80'),number_value=b'\x80',zero=tlv(2,b'\0'),
        big=tlv(2,b'\x01'+bytes(range(20))),big_value=b'\x01'+bytes(range(20)),
        reason=tlv(10,b'\x01'),invalidity=tlv(24,b'20231231000000Z'))
    name_choice=content(children(named['idp'])[0]);relative_choice=content(children(named['relative_idp'])[0])
    named.update(name=name_choice,fullnames=tlv(48,content(name_choice)),relative=tlv(49,content(relative_choice)),
        serial=b'\0\x80',plain_serial=b'\0\x81')
    lines=[]
    def span(label,data):
        # No zero-length C arrays (GCC/TCC portability).
        lines.append('static const uint8 '+label+'[]={'+(','.join(hex(b) for b in data) or '0')+'};')
        lines.append('static const xbytesview '+label+'_view={'+label+','+str(len(data))+'};')
    for row in rows:
        span('fixture_'+row['name'],row['der']);span('entries_'+row['name'],row['entries'])
        (out/(row['name']+'.der')).write_bytes(row['der'])
    for label,data in named.items():span('expected_'+label,data);(out/(label+'.der')).write_bytes(data)
    lines+=['static const int64_t expected_revoked_at='+str(int(instant.timestamp())*1000000)+';',
        'static const int64_t expected_invalidity_time='+str(int(invalidity.timestamp())*1000000)+';']
    (out/'x509_crl_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
    return rows,named
