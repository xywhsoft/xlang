"""Normal offline fixed public keys and certificates; no native security tests."""
from datetime import datetime,timezone
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from check_key_format_wire import vectors


def fixtures(root,out):
    rows=[]
    before=datetime(2020,1,1,tzinfo=timezone.utc);after=datetime(2040,1,1,tzinfo=timezone.utc)
    keys=vectors(root)
    def certificate(key,subject,issuer,issuer_key,serial,ca,critical=False):
        name=lambda text:x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,text)])
        builder=(x509.CertificateBuilder().subject_name(name(subject)).issuer_name(name(issuer))
            .public_key(key.public_key()).serial_number(serial).not_valid_before(before).not_valid_after(after)
            .add_extension(x509.BasicConstraints(ca=ca,path_length=2 if ca else None),True)
            .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()),False)
            .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(issuer_key.public_key()),False)
            .add_extension(x509.KeyUsage(digital_signature=True,content_commitment=False,key_encipherment=False,
                data_encipherment=False,key_agreement=False,key_cert_sign=ca,crl_sign=ca,encipher_only=False,decipher_only=False),True))
        if not ca:
            builder=builder.add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),False)
            builder=builder.add_extension(x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.SERVER_AUTH]),False)
        if critical:builder=builder.add_extension(x509.UnrecognizedExtension(x509.ObjectIdentifier('1.2.3.4'),b'\x04\x03abc'),True)
        algorithm=None if issuer_key.__class__.__name__.lower().find('ed25519')>=0 else hashes.SHA256()
        result=builder.sign(issuer_key,algorithm)
        return result
    for vector in keys[:4]:
        key=vector['key'];name=vector['name'];root_name='offline '+name+' root';mid_name='offline '+name+' intermediate'
        root_cert=certificate(key,root_name,root_name,key,1,True)
        mid=certificate(key,mid_name,root_name,key,2,True)
        leaf=certificate(key,'offline target',mid_name,key,3,False)
        custom=certificate(key,'offline custom',mid_name,key,4,False,True)
        for cert,issuer in ((root_cert,root_cert),(mid,root_cert),(leaf,mid),(custom,mid)):
            cert.verify_directly_issued_by(issuer)
        rows.append(dict(name=name,root=root_cert,mid=mid,leaf=leaf,custom=custom))
    lines=['/* Public deterministic-key normal certificates. */','typedef struct PathVector {const char* name;xbytesview root,mid,leaf,custom;} PathVector;']
    records=[]
    for row in rows:
        fields=[]
        for field in ('root','mid','leaf','custom'):
            data=row[field].public_bytes(serialization.Encoding.DER);symbol='path_'+row['name']+'_'+field
            lines.append('static const uint8 '+symbol+'[]={'+','.join(hex(b) for b in data)+'};')
            fields.append('{'+symbol+',sizeof('+symbol+')}');(out/(row['name']+'.'+field+'.der')).write_bytes(data)
        records.append('{"'+row['name']+'",'+','.join(fields)+'}')
    lines.append('static const PathVector pathVectors[]={'+','.join(records)+'};')
    lines.append('static const int64_t pathTime=1893456000000000LL;')
    (out/'x509_path_vectors.h').write_text('\n'.join(lines)+'\n')
    return len(rows)
