"""Language contracts for ordinary offline CRL records (not trust verdicts)."""
import json,re
from x509_crl_fixtures import fixtures
from check_stdlib_x509_data import literal


def language_fixtures(root,directory):
    rows,n=fixtures(root,directory);by_name={row['name']:row for row in rows}
    def save(name,lines):
        path=directory/(name+'.xl');path.write_text('\n'.join(lines)+'\n',encoding='utf8');return path
    entries=[f'auto entries=X509CrlEntries.fromDer({literal(rows[0]["entries"])},X509CrlVersion.v2)',
        'assert entries.count == 2 and entries.version == X509CrlVersion.v2',f'assert entries.toDer() == {literal(rows[0]["entries"])}',
        'auto rich=entries.at(0)',f'assert rich.toDer() == {literal(n["entry"])}',f'assert rich.serialContent == {literal(n["serial"])}',
        'assert rich.revokedAt == time(1704164645000000)','assert rich.version == X509CrlVersion.v2',
        f'assert rich.extensionsDer == {literal(n["extensions"])}','assert rich.extensions().count == 3',
        'assert rich.hasReason and rich.reason == X509CrlReason.keyCompromise',
        'assert rich.hasInvalidityDate and rich.invalidityDate == time(1703980800000000)',
        'auto issuer=rich.issuerNames()','assert issuer != null','if issuer != null',f' assert issuer.toDer() == {literal(n["issuer"])}','end',
        f'assert X509CrlEntry.fromDer({literal(n["entry"])},X509CrlVersion.v2).reason == X509CrlReason.keyCompromise',
        'auto plain=entries.at(1)','assert not plain.hasReason and not plain.hasInvalidityDate and plain.issuerNames() == null',
        'assert plain.extensions().count == 0 and plain.extensionsDer == b""','int caught=0',
        'try',' auto absent=plain.reason','catch RuntimeError error',' caught += 1','end',
        'try',' auto absent=plain.invalidityDate','catch RuntimeError error',' caught += 1','end',
        'try',f' auto bad=X509CrlEntries.fromDer({literal(rows[0]["entries"])},X509CrlVersion.v2,0,1)','catch RangeError error',' caught += 1','end','assert caught == 3',
        f'auto empty=X509CrlEntries.fromDer({literal(by_name["sparse"]["entries"])},X509CrlVersion.v2)','assert empty.count == 0 and empty.toDer() == b"\\x30\\x00"',
        f'auto v1=X509CrlEntries.fromDer({literal(by_name["v1"]["entries"])},X509CrlVersion.v1)','assert v1.count == 1 and v1.at(0).version == X509CrlVersion.v1']
    entries_case=save('crl.entries',entries)
    issuing=[f'auto point=X509IssuingDistributionPoint.fromDer({literal(n["idp"])})',f'assert point.toDer() == {literal(n["idp"])}',
        'assert point.hasDistributionPoint and point.nameKind == X509DistributionNameKind.fullName',f'assert point.nameDer == {literal(n["name"])}',
        'assert point.onlyUserCertificates and not point.onlyCaCertificates and not point.onlyAttributeCertificates',
        'assert point.indirect and point.hasReasons and point.reasons == X509ReasonFlags.keyCompromise',
        'auto names=point.fullNames()','assert names != null','if names != null',f' assert names.toDer() == {literal(n["fullnames"])}','end',
        f'auto relative=X509IssuingDistributionPoint.fromDer({literal(n["relative_idp"])})',
        'assert relative.nameKind == X509DistributionNameKind.relativeName and relative.onlyCaCertificates and not relative.indirect',
        f'assert relative.relativeNameDer() == {literal(n["relative"])}','assert relative.fullNames() == null and not relative.hasReasons',
        f'auto indirect=X509IssuingDistributionPoint.fromDer({literal(n["indirect_idp"])})',
        'assert indirect.indirect and not indirect.hasDistributionPoint and indirect.nameDer == b"" and indirect.fullNames() == null',
        'assert indirect.reasons == X509ReasonFlags.none',
        f'auto attribute=X509IssuingDistributionPoint.fromDer({literal(n["attribute_idp"])})','assert attribute.onlyAttributeCertificates']
    issuing_case=save('crl.issuing',issuing)
    parent=[f'auto crl=CertificateRevocationList.fromDer({literal(rows[0]["der"])})','assert crl.entries().count == 2',
        f'auto bySerial=crl.findBySerial({literal(n["serial"])})','assert bySerial != null','if bySerial != null',f' assert bySerial.toDer() == {literal(n["entry"])}','end',
        f'auto found=crl.findCertificate(Certificate.fromDer({literal(n["certificate"])}))','assert found != null','if found != null',' assert found.reason == X509CrlReason.keyCompromise','end',
        'assert crl.findBySerial(b"missing") == null', 'auto number=crl.numberMagnitude','assert number != null','if number != null',' assert number == b"\\x80"','end',
        'assert crl.deltaBaseMagnitude == null','auto aki=crl.authorityKeyId()','auto point=crl.issuingPoint()','auto fresh=crl.freshestCrl()',
        'assert aki != null and point != null and fresh != null','if aki != null',f' assert aki.toDer() == {literal(n["aki"])}','end',
        'if point != null then assert point.indirect end','if fresh != null then assert fresh.count == 1 end',
        f'auto delta=CertificateRevocationList.fromDer({literal(by_name["delta"]["der"])})',
        'auto deltaNumber=delta.numberMagnitude','auto deltaBase=delta.deltaBaseMagnitude','assert deltaNumber != null and deltaBase != null',
        'if deltaNumber != null then assert deltaNumber == b"\\x01\\x00" end','if deltaBase != null then assert deltaBase == b"\\x80" end',
        'assert delta.freshestCrl() == null']
    parent_case=save('crl.parent',parent)
    nullable=[f'auto sparse=CertificateRevocationList.fromDer({literal(by_name["sparse"]["der"])})',
        'assert sparse.numberMagnitude == null and sparse.deltaBaseMagnitude == null',
        'assert sparse.authorityKeyId() == null and sparse.issuingPoint() == null and sparse.freshestCrl() == null',
        'array<bytes?> values=[sparse.numberMagnitude,sparse.deltaBaseMagnitude,null]','assert values[0] == null and values[1] == null and values[2] == null',
        'assert sparse.entries().count == 0',f'auto v1=CertificateRevocationList.fromDer({literal(by_name["v1"]["der"])})',
        'assert v1.entries().version == X509CrlVersion.v1','assert v1.entries().at(0).version == X509CrlVersion.v1']
    nullable_case=save('crl.nullable',nullable)
    utilities=[f'assert x509.parseSubjectKeyId({literal(n["ski"])}) == {literal(n["ski_value"])}',
        f'assert x509.parseCrlNumber({literal(n["number"])}) == b"\\x80"',f'assert x509.parseCrlNumber({literal(n["zero"])}) == b"\\x00"',
        f'assert x509.parseCrlNumber({literal(n["big"])}) == {literal(n["big_value"])}',
        f'assert x509.parseCrlReason({literal(n["reason"])}) == X509CrlReason.keyCompromise',
        f'assert x509.parseInvalidityDate({literal(n["invalidity"])}) == time(1703980800000000)']
    for value,name in [(0,'unspecified'),(1,'keyCompromise'),(2,'caCompromise'),(3,'affiliationChanged'),(4,'superseded'),
        (5,'cessationOfOperation'),(6,'certificateHold'),(8,'removeFromCrl'),(9,'privilegeWithdrawn'),(10,'aaCompromise')]:
        utilities.append(f'assert x509.parseCrlReason(b"\\x0a\\x01\\x{value:02x}") == X509CrlReason.{name}')
    utility_case=save('crl.utilities',utilities)
    enums=save('crl.enums',['array<X509CrlVersion> versions=[X509CrlVersion.v1,X509CrlVersion.v2]','assert versions[0] == X509CrlVersion.v1',
        'array<X509CrlReason> reasons=[X509CrlReason.removeFromCrl,X509CrlReason.aaCompromise]','assert reasons[0] == X509CrlReason.removeFromCrl'])
    threaded=save('crl.threads',['func detached(bytes der) as X509CrlEntry',' return CertificateRevocationList.fromDer(der).entries().at(0)','end',
        'func reader(X509CrlEntry entry) as func() as int',' return func() as int','  if entry.hasReason and entry.reason == X509CrlReason.keyCompromise then return 0 end',
        '  return 1',' end','end',f'auto entry=detached({literal(rows[0]["der"])})','auto first=thread(reader(entry))','auto second=thread(reader(entry))',
        'assert first.waitTimeout(30000) and second.waitTimeout(30000)','assert first.exitCode() == 0 and second.exitCode() == 0'])
    declarations=[];current=None;owners={}
    for line in (root/'src/stdlib/xl6_stdlib_x509_crl_profiles.inc').read_text(encoding='utf8').splitlines():
        if not line.lstrip().startswith('"'):continue
        decoded=json.loads(line.strip().rstrip(';')).strip()
        if decoded.startswith('class '):current=decoded.split()[1];owners[current]='v'+str(len(owners))
        if 'extern ' in decoded and 'destroy()' not in decoded:declarations.append((current,decoded))
    owners.update(CertificateRevocationList='crl',Certificate='cert')
    for line in (root/'src/stdlib/xl6_stdlib_x509_crl_methods.inc').read_text().splitlines():declarations.append(('CertificateRevocationList',json.loads(line.strip()).strip()))
    code=['int marker=1','func contracts('+','.join(kind+' '+var for kind,var in owners.items())+')']
    for owner,declaration in declarations:
        kind,method,parameters=re.search(r'(func|get) (\w+)\(([^)]*)\)',declaration).groups();args=[]
        for parameter in filter(None,parameters.split(',')):
            if '=' in parameter:continue
            typ=parameter.strip().split()[0];args.append(owners.get(typ,{'bytes':'b"der"','int':'0','X509CrlVersion':'X509CrlVersion.v2'}.get(typ,'')))
        expression=(owner if 'static extern' in declaration else owners[owner])+'.'+method+('('+','.join(args)+')' if kind=='func' else '')
        code.append(' auto result'+str(len(code))+' = '+expression)
    code+=[' auto ski=x509.parseSubjectKeyId(b"der")',' auto number=x509.parseCrlNumber(b"der")',
        ' auto reason=x509.parseCrlReason(b"der")',' auto invalidity=x509.parseInvalidityDate(b"der")','end']
    namespace=save('crl.namespace',[f'assert x509.parseSubjectKeyId({literal(n["ski"])}) == {literal(n["ski_value"])}'])
    raw=save('crl.raw',[f'auto values=X509CrlEntries.fromDer({literal(rows[0]["entries"])},X509CrlVersion.v2)',
        'assert values.count == 2',f'assert values.at(0).toDer() == {literal(n["entry"])}'])
    return [entries_case,issuing_case,parent_case,nullable_case,utility_case,enums,threaded,namespace,raw],save('crl.contract',code)
