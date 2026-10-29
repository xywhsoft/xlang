"""Ordinary offline issuer/subject pairs, CRLs and full-message signatures."""
from datetime import datetime,timezone
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import ec,ed25519,ed448,padding,rsa
from check_key_format_wire import vectors
from check_x509_owned import children,content


def fixtures(root,out):
    keys=vectors(root);ed=keys[3]['key'];message=b'a\0b\xe4\xbd\xa0'
    extended=keys[:4]+[dict(name='pss',key=keys[0]['key']),dict(name='ed448',key=ed448.Ed448PrivateKey.from_private_bytes(bytes(range(57))))]
    name=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary signature issuer')])
    subject=x509.Name([x509.NameAttribute(x509.NameOID.COMMON_NAME,'ordinary subject')])
    begin=datetime(2020,1,1,tzinfo=timezone.utc);end=datetime(2040,1,1,tzinfo=timezone.utc)
    rows=[];lines=[]
    def span(label,data):
        lines.append('static const uint8 '+label+'[]={'+(','.join(hex(x) for x in data) or '0')+'};')
        return '{'+label+','+str(len(data))+'}'
    for index,item in enumerate(extended):
        key=item['key'];pss=item['name']=='pss';is_ed=isinstance(key,(ed25519.Ed25519PrivateKey,ed448.Ed448PrivateKey))
        digest=None if is_ed else (hashes.SHA384() if item['name']=='p384' else hashes.SHA256())
        pad=padding.PSS(mgf=padding.MGF1(hashes.SHA384()),salt_length=20) if pss else padding.PKCS1v15()
        options={'rsa_padding':pad} if isinstance(key,rsa.RSAPrivateKey) else {}
        issuer=(x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(index+1)
            .not_valid_before(begin).not_valid_after(end).add_extension(x509.BasicConstraints(True,None),True).sign(key,digest,**options))
        certificate=(x509.CertificateBuilder().subject_name(subject).issuer_name(name).public_key(ed.public_key()).serial_number(index+101)
            .not_valid_before(begin).not_valid_after(end).sign(key,digest,**options))
        crl=x509.CertificateRevocationListBuilder().issuer_name(name).last_update(begin).next_update(end).sign(key,digest,**options)
        def sign(data):
            if is_ed:return key.sign(data)
            if isinstance(key,rsa.RSAPrivateKey):return key.sign(data,pad,digest)
            return key.sign(data,ec.ECDSA(digest))
        def verify(signature,data,parameters=None,algorithm=None):
            if is_ed:key.public_key().verify(signature,data)
            elif isinstance(key,rsa.RSAPrivateKey):key.public_key().verify(signature,data,parameters or pad,algorithm or digest)
            else:key.public_key().verify(signature,data,parameters or ec.ECDSA(digest))
        verify(issuer.signature,issuer.tbs_certificate_bytes,issuer.signature_algorithm_parameters,issuer.signature_hash_algorithm)
        verify(certificate.signature,certificate.tbs_certificate_bytes,certificate.signature_algorithm_parameters,certificate.signature_hash_algorithm)
        verify(crl.signature,crl.tbs_certlist_bytes,crl.signature_algorithm_parameters,crl.signature_hash_algorithm)
        signature=sign(message);verify(signature,message)
        data=dict(name=item['name'],certificate=certificate.public_bytes(serialization.Encoding.DER),
            issuer=issuer.public_bytes(serialization.Encoding.DER),crl=crl.public_bytes(serialization.Encoding.DER),
            spki=key.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo),
            algorithm=children(certificate.public_bytes(serialization.Encoding.DER))[1],message=message,signature=signature,
            supported=not isinstance(key,ed448.Ed448PrivateKey))
        rows.append(data)
        for label,value in data.items():
            if isinstance(value,bytes):(out/(data['name']+'.'+label+'.der')).write_bytes(value)
        fields=[span('fixture_'+str(index)+'_'+label,data[label]) for label in ('certificate','issuer','crl','spki','algorithm','message','signature')]
        lines.append('static const SignatureVector fixture_'+str(index)+'={'+','.join(fields)+','+('true' if data['supported'] else 'false')+'};')
    lines.insert(0,'typedef struct SignatureVector {xbytesview certificate,issuer,crl,spki,algorithm,message,signature;bool supported;} SignatureVector;')
    lines+=['static const SignatureVector* const vectors[]={'+','.join('&fixture_'+str(i) for i in range(len(rows)))+'};']
    for index,item in enumerate(keys):
        der=item['key'].public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
        data=span('managed_spki_'+str(index),der);lines.append('static const xbytesview managed_spki_view_'+str(index)+'='+data+';')
    lines.append('static const xbytesview* const managed_spkis[]={'+','.join('&managed_spki_view_'+str(i) for i in range(len(keys)))+'};')
    (out/'x509_signature_vectors.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
    return rows
