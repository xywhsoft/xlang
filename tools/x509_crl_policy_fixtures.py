"""Normal offline CRL-policy vectors, signed with the existing public test key."""
from datetime import datetime,timezone
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from check_key_format_wire import vectors


def fixtures(root,out):
    key=vectors(root)[3]['key'];utc=timezone.utc
    start=datetime(2024,1,1,tzinfo=utc);end=datetime(2030,1,1,tzinfo=utc)
    instant=datetime(2026,1,2,3,4,5,tzinfo=utc)
    name=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'xlang CRL policy issuer')])
    other=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary other issuer')])
    ski=x509.SubjectKeyIdentifier.from_public_key(key.public_key())
    aki=x509.AuthorityKeyIdentifier(ski.digest,None,None)
    ku=x509.KeyUsage(False,False,False,False,False,True,True,False,False)
    url=x509.UniformResourceIdentifier('https://crl.example.invalid/ordinary')
    def certificate(label,serial,issuer=name,ca=False,indirect=False):
        builder=(x509.CertificateBuilder().subject_name(name if ca else x509.Name([
            x509.NameAttribute(x509.NameOID.COMMON_NAME,label)]))
            .issuer_name(issuer).public_key(key.public_key()).serial_number(serial)
            .not_valid_before(start).not_valid_after(end)
            .add_extension(x509.BasicConstraints(ca,None),True))
        if ca:builder=builder.add_extension(ski,False).add_extension(ku,True)
        if indirect:builder=builder.add_extension(x509.CRLDistributionPoints([
            x509.DistributionPoint([url],None,None,[x509.DirectoryName(name)])]),False)
        result=builder.sign(key,None);key.public_key().verify(result.signature,result.tbs_certificate_bytes)
        return result.public_bytes(serialization.Encoding.DER)
    named=dict(issuer=certificate('issuer',1,ca=True),target=certificate('target',128),
        clear=certificate('clear',129),unrelated=certificate('unrelated',128,other),
        indirect_target=certificate('indirect target',128,other,indirect=True))
    def entry(serial,reason=None,extensions=()):
        result=x509.RevokedCertificateBuilder().serial_number(serial).revocation_date(start)
        if reason is not None:result=result.add_extension(x509.CRLReason(reason),False)
        for value,critical in extensions:result=result.add_extension(value,critical)
        return result.build()
    held=entry(128,x509.ReasonFlags.certificate_hold,[(x509.InvalidityDate(start),False)])
    removed=entry(128,x509.ReasonFlags.remove_from_crl)
    all_reasons=frozenset([x509.ReasonFlags.key_compromise,x509.ReasonFlags.ca_compromise,
        x509.ReasonFlags.affiliation_changed,x509.ReasonFlags.superseded,x509.ReasonFlags.cessation_of_operation,
        x509.ReasonFlags.certificate_hold,x509.ReasonFlags.privilege_withdrawn,x509.ReasonFlags.aa_compromise])
    left=frozenset([x509.ReasonFlags.key_compromise]);right=all_reasons-left
    def point(reasons=None,indirect=False):
        return x509.IssuingDistributionPoint([url] if indirect else None,None,False,False,reasons,indirect,False)
    critical_crl=x509.UnrecognizedExtension(x509.ObjectIdentifier('1.2.3.4.5'),b'\x04\x03abc')
    critical_entry=x509.UnrecognizedExtension(x509.ObjectIdentifier('1.2.3.4.6'),b'\x04\x03xyz')
    rows=[('base',10,[held],[]),('delta',11,[removed],[(x509.DeltaCRLIndicator(10),True)]),
        ('left',12,[],[(point(left),True)]),('right',13,[],[(point(right),True)]),
        ('complete',14,[],[]),('critical',15,[entry(128,None,[(critical_entry,True)])],[(critical_crl,True)]),
        ('indirect',20,[entry(127,None,[(x509.CertificateIssuer([x509.DirectoryName(other)]),True)]),entry(128)],[(point(indirect=True),True)])]
    for label,number,entries,extensions in rows:
        builder=(x509.CertificateRevocationListBuilder().issuer_name(name).last_update(start).next_update(end)
            .add_extension(x509.CRLNumber(number),False).add_extension(aki,False))
        for value,critical in extensions:builder=builder.add_extension(value,critical)
        for value in entries:builder=builder.add_revoked_certificate(value)
        result=builder.sign(key,None);key.public_key().verify(result.signature,result.tbs_certlist_bytes)
        named[label]=result.public_bytes(serialization.Encoding.DER)
    lines=[]
    for label,data in named.items():
        lines.append('static const uint8 policy_'+label+'[]={'+','.join(hex(b) for b in data)+'};')
        lines.append('static const xbytesview policy_'+label+'_view={policy_'+label+','+str(len(data))+'};')
        (out/(label+'.der')).write_bytes(data)
    lines.append('static const int64_t policy_time='+str(int(instant.timestamp())*1000000)+';')
    (out/'x509_crl_policy_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
    return named
