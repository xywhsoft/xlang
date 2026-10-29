"""Actual language cases for ordinary, independently verified profile data."""
from pathlib import Path
import json,re
from x509_profile_fixtures import fixtures
from check_x509_owned import children,content
from check_stdlib_x509_data import literal


def language_fixtures(root,directory):
    rows,n=fixtures(root,directory);cert=literal(rows[0]['der']);leaf=literal(rows[1]['der']);sparse=literal(rows[2]['der'])
    def save(name,lines):
        path=directory/(name+'.xl');path.write_text('\n'.join(lines)+'\n',encoding='utf8');return path
    general=[f'auto names=X509GeneralNames.fromDer({literal(n["san"])})','assert names.count == 8',f'assert names.toDer() == {literal(n["san"])}']
    kinds={0:'other',1:'email',2:'dns',4:'directory',6:'uri',7:'ip',8:'registeredId'}
    for i,raw in enumerate(children(n['san'])):
        v='item'+str(i);kind=raw[0]&31
        general += [f'auto {v}=names.at({i})',f'assert {v}.kind == X509GeneralNameKind.{kinds[kind]}',f'assert not {v}.isConstraintBase',
                    f'assert {v}.toDer() == {literal(raw)}',f'assert {v}.value == {literal(content(raw))}']
        if kind in (1,2,6):general += [f'assert {v}.text == '+literal(content(raw),True)]
        if kind==4:general += [f'assert {v}.directoryName().toDer() == {literal(n["directory"])}']
        if kind==8:general += [f'assert {v}.registeredOid == "1.2.3.4"']
    general_case=save('profiles.general',general)
    oids=[f'auto oids=X509Oids.fromDer({literal(n["eku"])})','assert oids.count == 3',f'assert oids.toDer() == {literal(n["eku"])}',
          'assert oids.oid(0) == "1.3.6.1.5.5.7.3.1"','assert oids.at(2) == b"\\x2a\\x03\\x04"',
          'assert oids.contains("1.2.3.4") and oids.containsOid(b"\\x2a\\x03\\x04")','assert not oids.contains("1.2.3.99")']
    aki=[f'auto aki=X509AuthorityKeyIdentifier.fromDer({literal(n["aki"])})',f'assert aki.toDer() == {literal(n["aki"])}',
         'assert aki.hasKeyId and aki.hasIssuer and aki.hasSerial',f'assert aki.keyId == {literal(n["ski"])}',f'assert aki.serialContent == {literal(n["aki_serial"])}',
         'auto issuer=aki.issuerNames()','assert issuer != null','if issuer != null',f' assert issuer.toDer() == {literal(n["aki_issuer"])}','end']
    distribution=[f'auto points=X509DistributionPoints.fromDer({literal(n["points"])})','assert points.count == 2',f'assert points.toDer() == {literal(n["points"])}',
        'auto first=points.at(0)','assert first.hasName and first.hasReasons and first.hasIssuer','assert first.nameKind == X509DistributionNameKind.fullName',
        f'assert first.toDer() == {literal(n["point0"])}',f'assert first.nameDer == {literal(n["name0"])}',
        'assert first.reasons == (X509ReasonFlags.keyCompromise | X509ReasonFlags.caCompromise)',
        'auto full=first.fullNames()','auto issuer=first.issuerNames()','assert full != null and issuer != null',
        'if full != null',f' assert full.toDer() == {literal(n["fullnames"])}','end','if issuer != null',f' assert issuer.toDer() == {literal(n["point_issuer"])}','end',
        'auto relative=points.at(1)','assert relative.nameKind == X509DistributionNameKind.relativeName','assert not relative.hasReasons and not relative.hasIssuer',
        'assert relative.fullNames() == null and relative.issuerNames() == null',f'assert relative.relativeNameDer() == {literal(n["relative"])}',
        f'assert X509DistributionPoint.fromDer({literal(n["point0"])}).toDer() == {literal(n["point0"])}']
    dist_case=save('profiles.distribution',distribution)
    constraints=[f'auto constraints=X509NameConstraints.fromDer({literal(n["constraints"])})','assert constraints.hasPermitted and constraints.hasExcluded',
        f'assert constraints.toDer() == {literal(n["constraints"])}',f'constraints.check(Certificate.fromDer({leaf}))',
        'auto permitted=constraints.permitted()','auto excluded=constraints.excluded()','assert permitted != null and excluded != null',
        'if permitted != null',' assert permitted.count == 3',f' assert permitted.toDer() == {literal(n["permitted"])}',
        ' auto subtree=permitted.at(0)',' assert not subtree.hasMinimum and not subtree.hasMaximum',' assert subtree.minimumMagnitude == b"" and subtree.maximumMagnitude == b""',
        ' assert X509GeneralSubtree.fromDer(subtree.toDer()).toDer() == subtree.toDer()',
        ' auto base=subtree.base()',' assert base.isConstraintBase and base.kind == X509GeneralNameKind.dns',' assert base.text == "example.invalid"',
        f' auto names=X509GeneralNames.fromDer({literal(n["san"])})',' assert names.at(2).isWithin(base)',
        ' auto ipbase=permitted.at(1).base()',' assert ipbase.isConstraintBase and ipbase.kind == X509GeneralNameKind.ip',' assert ipbase.value == b"\\xc0\\x00\\x02\\x00\\xff\\xff\\xff\\x00"',
        ' assert names.at(4).isWithin(ipbase)',
        ' assert X509GeneralSubtrees.fromDer(permitted.toDer()).count == permitted.count','end',
        'if excluded != null',' assert excluded.count == 1',f' assert excluded.toDer() == {literal(n["excluded"])}','end',
        f'auto distances=X509GeneralSubtree.fromDer({literal(n["distances"])})','assert distances.hasMinimum and distances.hasMaximum',
        'assert distances.minimumMagnitude == b"\\x80" and distances.maximumMagnitude == b"\\x01\\x00"']
    constraint_case=save('profiles.constraints',constraints)
    nullable=[f'auto sparse=Certificate.fromDer({sparse})','auto missing=sparse.subjectKeyId()','assert missing == null',
              f'auto cert=Certificate.fromDer({cert})','auto present=cert.subjectKeyId()','assert present != null',
              'if present != null',f' assert present == {literal(n["ski"])}','end',
              'bytes? localMissing=null','array<bytes?> values=[present,missing,localMissing]','assert values[0] != null and values[1] == null and values[2] == null',
              'auto environment=env.get("__XL6_PROFILE_NULLABLE_STRING_PROBE__")','if environment != null then assert environment.length >= 0 end']
    nullable_case=save('profiles.nullable',nullable)
    projected=[f'auto cert=Certificate.fromDer({cert})','auto san=cert.subjectAltNames()','auto ian=cert.issuerAltNames()',
        'auto ku=cert.keyUsage()','auto basic=cert.basicConstraints()','auto eku=cert.extendedKeyUsage()','auto aki=cert.authorityKeyId()',
        'auto points=cert.crlDistributionPoints()','auto freshest=cert.freshestCrl()','auto constraints=cert.nameConstraints()',
        'assert san != null and ian != null and ku != null and basic != null and eku != null',
        'assert aki != null and points != null and freshest != null and constraints != null',
        'if san != null then assert san.count == 8 end','if ian != null then assert ian.count == 1 end',
        'if ku != null',' assert (ku.flags & X509KeyUsageFlags.digitalSignature) != X509KeyUsageFlags.none',' assert (ku.flags & X509KeyUsageFlags.decipherOnly) != X509KeyUsageFlags.none','end',
        'if basic != null then assert basic.ca and basic.hasPathLimit and basic.pathLimit == 0 end',
        'if eku != null then assert eku.count == 3 end','if aki != null then assert aki.hasIssuer end',
        'if points != null then assert points.count == 2 end','if freshest != null then assert freshest.count == 1 end',
        f'auto sparse=Certificate.fromDer({sparse})','assert sparse.subjectAltNames() == null and sparse.issuerAltNames() == null',
        'assert sparse.keyUsage() == null and sparse.basicConstraints() == null and sparse.extendedKeyUsage() == null',
        'assert sparse.authorityKeyId() == null and sparse.crlDistributionPoints() == null and sparse.freshestCrl() == null and sparse.nameConstraints() == null',
        f'auto leaf=Certificate.fromDer({leaf})','auto leafBasic=leaf.basicConstraints()','assert leafBasic != null','int caught=0',
        'if leafBasic != null',' assert not leafBasic.ca and not leafBasic.hasPathLimit',' try','  auto missing=leafBasic.pathLimit',' catch RuntimeError error','  caught += 1',' end','end',
        'try',f' auto bad=X509GeneralNames.fromDer({literal(n["san"])},0,1)','catch RangeError error',' caught += 1','end','assert caught == 2']
    all_lines=[]
    for index,body in enumerate([general,oids,aki,distribution,constraints,nullable,projected]):all_lines+=['func check'+str(index)+'()']+[' '+s for s in body]+['end']
    all_lines+=['check'+str(i)+'()' for i in range(7)];all_case=save('profiles.all',all_lines)
    enums=save('profiles.enums',['array<X509GeneralNameKind> kinds=[X509GeneralNameKind.dns,X509GeneralNameKind.ip]','assert kinds[0] == X509GeneralNameKind.dns',
        'array<X509KeyUsageFlags> usage=[X509KeyUsageFlags.digitalSignature | X509KeyUsageFlags.crlSign]','assert (usage[0] & X509KeyUsageFlags.crlSign) != X509KeyUsageFlags.none',
        'array<X509ReasonFlags> reasons=[X509ReasonFlags.keyCompromise]','assert reasons[0] == X509ReasonFlags.keyCompromise',
        'array<X509DistributionNameKind> forms=[X509DistributionNameKind.relativeName]','assert forms[0] == X509DistributionNameKind.relativeName'])
    threaded=save('profiles.threads',['func detached(bytes der) as X509DistributionPoint',' return X509DistributionPoints.fromDer(der).at(0)','end',
        'func reader(X509DistributionPoint point) as func() as int',' return func() as int','  if point.hasReasons and point.reasons == (X509ReasonFlags.keyCompromise | X509ReasonFlags.caCompromise) then return 0 end','  return 1',' end','end',
        f'auto point=detached({literal(n["points"])})','auto first=thread(reader(point))','auto second=thread(reader(point))',
        'assert first.waitTimeout(30000) and second.waitTimeout(30000)','assert first.exitCode() == 0 and second.exitCode() == 0'])
    declarations=[];current=None;owners={}
    for line in (root/'src/stdlib/xl6_stdlib_x509_profiles.inc').read_text(encoding='utf8').splitlines():
        if not line.lstrip().startswith('"'):continue
        decoded=json.loads(line.strip().rstrip(';')).strip()
        if decoded.startswith('class '):current=decoded.split()[1];owners[current]='v'+str(len(owners))
        if 'extern ' in decoded and 'destroy()' not in decoded:declarations.append((current,decoded))
    owners['Certificate']='cert'
    for line in (root/'src/stdlib/xl6_stdlib_x509_certificate_profiles.inc').read_text().splitlines():declarations.append(('Certificate',json.loads(line.strip()).strip()))
    code=['int marker=1','func contracts('+','.join(kind+' '+var for kind,var in owners.items())+')']
    for owner,declaration in declarations:
        match=re.search(r'(func|get) (\w+)\(([^)]*)\)',declaration);kind,method,parameters=match.groups();args=[]
        for parameter in filter(None,parameters.split(',')):
            if '=' in parameter:continue
            typ=parameter.strip().split()[0];args.append(owners.get(typ,{'bytes':'b"der"','int':'0','string':'"1.2.3.4"'}.get(typ,'')))
        expression=(owner if 'static extern' in declaration else owners[owner])+'.'+method+('('+','.join(args)+')' if kind=='func' else '')
        code.append(' '+(('auto result'+str(len(code))+' = ') if ') as ' in declaration else '')+expression)
    code.append('end');contract=save('profiles.contract',code)
    return [all_case,general_case,dist_case,constraint_case,nullable_case,enums,threaded],contract
