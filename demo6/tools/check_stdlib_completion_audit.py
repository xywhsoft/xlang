"""Read-only requirement/evidence audit on one platform's native filesystem.

Compose immutable B350 product evidence with the later guide/declaration gates.
Old tools/docs may be recovered ONLY as exact recorded bytes with provenance;
never recover changed production inputs, replace a provider or rewrite a report.
Assertion markers are reviewed traceability, not runtime branch instrumentation.
Pending evidence remains pending even if every existing report is green.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,sys,traceback
import check_stdlib_product_witnesses as witnesses
from stdlib53_pruning_contract import MODULES,PROFILES
from stdlib53_completion_requirements import BEHAVIORS,MODULE_BEHAVIORS


def digest(raw):return hashlib.sha256(raw).hexdigest()


class Evidence:
    def __init__(self,task,history):
        self.task=task;self.history=history;self.files={};self.recovered={};self.cache={}
        provenance=self.read(history/'provenance.json')
        assert provenance['Complete'] and provenance['OriginalReportsUnmodified']
        self.allowed={(r['OriginalPath'],r['Sha256']):r for r in provenance['RecoveredInputs']}

    def record(self,path,expected=None):
        path=Path(path);key=str(path)
        if key not in self.cache:self.cache[key]=digest(path.read_bytes()) if path.is_file() else None
        current=self.cache[key]
        if expected is None or current==expected:
            assert current is not None,key
            self.files[key]=current;return path
        # Preserve the exact historical test definition, not a changed current
        # definition with a backdated green result. No production fallback.
        relative=path.relative_to(self.task);assert relative.parts[0] in ('tools','docs'),key
        row=self.allowed[key,expected];assert row['Sha256']==expected
        recovered=Path(row['RecoveredPath']);assert recovered.parent==self.history
        assert digest(recovered.read_bytes())==expected,key
        self.files[str(recovered)]=expected;self.recovered[key,expected]=row;return recovered

    def read(self,path,expected=None):
        actual=self.record(path,expected);return json.loads(actual.read_text(encoding='utf-8-sig'))

    def certificate(self,path):
        body=self.read(path);assert body['Complete'] and body['InputsUnchanged']
        for row in body['VerifiedFiles']:self.record(row['Path'],row['Sha256'])
        for row in body.get('Inputs',[]):self.record(row['Path'],row['Sha256'])
        return body

    def unchanged(self):
        return all(digest(Path(p).read_bytes())==value for p,value in self.files.items())


def fixture_proof(name,markers,task,products,aot,product_inputs,aot_inputs,evidence):
    source=task/'test/cases'/(name+'.xl');actual=evidence.record(source);value=digest(actual.read_bytes())
    assert value in product_inputs.get(name,()) and value in aot_inputs.get(name,()),('Source mismatch',name)
    assert name in products and name in aot,('Fixture did not execute on this product lane',name)
    body=actual.read_text(encoding='utf-8-sig')
    assert markers and all(len(s)>=12 and s!='assert' and s in body for s in markers),('Missing concrete reviewed assertion',name)
    return dict(Name=name,Source=str(source),SourceSha256=value,Assertions=markers,
                ProductRows=products[name]['Rows'],AotRows=aot[name]['Rows'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','product-certificate','guide-certificate','abi-certificate','history-root','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();assert not args.out.exists()
    task=Path(__file__).resolve().parents[1];root=args.root.resolve();assert root.is_dir();win=os.name=='nt'
    if win:witnesses.physical=lambda value:Path(value)
    e=Evidence(task,args.history_root.resolve());failures=[];controls=[];out=args.out.resolve();out.mkdir(parents=True)
    for file in (Path(__file__).resolve(),task/'tools/stdlib53_completion_requirements.py',task/'tools/stdlib53_pruning_contract.py',
                 task/'tools/check_stdlib_product_witnesses.py',task/'docs/标准库设计.md'):
        e.record(file)
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Platform='windows' if win else 'linux',
        Modules=[],Behaviors=[],Controls=controls,Failures=failures,PendingEvidence=[],
        RuntimeBranchInstrumentation=False,NewCompilerProductRun=False,Full53Acceptance=False)
    try:
        product_certificate=e.certificate(args.product_certificate.resolve())
        assert product_certificate['NativeFileSystemEvidenceValidation'] and product_certificate['CandidateCount']==328
        guide_certificate=e.certificate(args.guide_certificate.resolve())
        assert guide_certificate['DocumentationContractAcceptance'] and guide_certificate['GuideControls']==33
        abi_certificate=e.certificate(args.abi_certificate.resolve())
        assert abi_certificate['ScopedDeclarationSafetyAcceptance'] and abi_certificate['NegativeProductPhases']==426
        old={name:e.read(row['Path'],row['Sha256']) for name,row in product_certificate['Reports'].items()}
        product_names=('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so')
        for name in product_names:
            rows=[r for r in old['build']['Products'] if Path(r['Path']).name==name]
            assert len(rows)==1 and Path(rows[0]['Path']).resolve()==root/'release'/name
        build_receipt=product_certificate['Reports']['build']
        for later in (guide_certificate,abi_certificate):
            pins={str(Path(r['Path'])):r['Sha256'] for r in later['VerifiedFiles']}
            assert pins.get(str(Path(build_receipt['Path'])))==build_receipt['Sha256'],'Later gate used another build'
            assert all(pins.get(str(Path(r['Path'])))==r['Sha256'] for r in old['build']['Products']),'Later gate used another product'
        product,aot,strict,pruning=[old[n] for n in ('products','aot','strict','pruning')]
        assert all(d['Complete'] and d['InputsUnchanged'] and not d.get('Failures') for d in (product,aot,strict,pruning))
        assert product['AllExecutablePassed'] and product['SourceProductAcceptance'] and product['EmbeddedVfsWithoutFallback']
        assert aot['AllExecutablePassed'] and aot['RelocatedAotInventoryAcceptance'] and not aot['ExplicitSubset']
        assert len(product['Cases'])==len(aot['Cases'])==len(strict['Cases'])==328
        assert strict['StrictCInventoryAcceptance'] and len(strict['Controls'])==6 and all(c['Rejected'] for c in strict['Controls'])
        assert all(c['Passed'] for c in strict['Cases']) and not strict['ProviderOverrides']
        assert witnesses.product_hashes(product,win)==witnesses.product_hashes(aot,win)
        passed=witnesses.passing_cases(product,False);aot_passed=witnesses.passing_cases(aot,True)
        product_inputs=witnesses.source_inputs(product);aot_inputs=witnesses.source_inputs(aot)
        gates={c['Name']:c for c in product['Children']};assert len(gates)==(28 if win else 29)
        assert all(c['Passed'] and c['Exit']==0 for c in gates.values())
        # Later independently executed gates use these exact same build/products.
        # Do not rewrite the old parent to pretend it executed the new runner.
        gates['guide-contracts']=dict(Passed=True,Certificate=str(args.guide_certificate.resolve()),CurrentGuideControls=33)
        gates['abi-rejection']=dict(Passed=True,Certificate=str(args.abi_certificate.resolve()),RejectionPhases=426)
        assert pruning['NativeModuleRoleAcceptance'] and pruning['ModuleCount']==53 and len(pruning['Proofs'])==48
        assert len(pruning['NativeObjects'])==53 and len(pruning['Controls'])==6
        assert all({o['Optimization'] for o in n['Objects']}=={'O0','O2'} for n in pruning['NativeObjects'])
        assert sum(len(n['Objects']) for n in pruning['NativeObjects'])==106
        roles={r['Name']:r for r in pruning['Modules']}
        assert set(roles)==set(MODULES)==set(MODULE_BEHAVIORS)
        assert all(r['Verified'] and tuple(r['Profiles'])==tuple(MODULES[n][1].split()) and r['Mode']==MODULES[n][0] for n,r in roles.items())
        task_doc=task/'docs/三组标准库接入长期任务.md';e.record(task_doc)
        table=task_doc.read_text(encoding='utf-8-sig').split('## 53 模块覆盖表',1)[1].split('## 起始缺陷证据',1)[0]
        purposes={parts[1].strip():parts[2].strip() for line in table.splitlines() if line.startswith('| ') for parts in [line.split('|')] if parts[1].strip() in MODULES}
        assert set(purposes)==set(MODULE_BEHAVIORS), 'Approved purpose table is incomplete'
        for key,contract in BEHAVIORS.items():
            assert all(name in gates and gates[name]['Passed'] for name in contract['Gates']),(key,contract['Gates'])
            evidence=[fixture_proof(f['Name'],f['Assertions'],task,passed,aot_passed,product_inputs,aot_inputs,e) for f in contract['Fixtures']]
            report['Behaviors'].append(dict(Id=key,Requirement=contract['Requirement'],AssertionEvidenceVerified=True,
                                           RequiredGates=contract['Gates'],Fixtures=evidence))
        assert len(report['Behaviors'])==49
        for name,refs in MODULE_BEHAVIORS.items():
            report['Modules'].append(dict(Name=name,ApprovedPurpose=purposes[name],Role=roles[name]['Mode'],
                PruningProfiles=roles[name]['Profiles'],NativeRoleEvidenceVerified=True,Behaviors=refs.split(),
                AssertionEvidenceVerified=all(b in BEHAVIORS for b in refs.split()),
                PendingEvidence=['buffer-native-suites'] if name in ('buffer','buffer_hex','buffer_base64') else []))
        # Positive functional cases do not replace the original three Buffer
        # native fault/reentry/thread suites. No current direct-product receipt
        # for those suites is part of the existing certificates being audited.
        report['PendingEvidence']=[dict(Id='buffer-native-suites',Modules=['buffer','buffer_hex','buffer_base64'],
            Requirement='当前 DLL/SO 直接执行三份既有 Buffer 原生核心/值迭代器/codec 故障、线程和重入专项',
            Sources=['stdlib_buffer_core_host.c','stdlib_buffer_value_host.c','stdlib_buffer_codec_host.c'])]
        report['DeferredByUser']=[dict(Id='windows-dst',Requirement='隔离 Windows DST gap/fold 实测',Passed=False)]
        # Audit-oracle controls, not additional compiler executions.
        def control(name,operation):
            try:operation()
            except (AssertionError,KeyError):controls.append(dict(Name=name,Rejected=True))
            else:raise AssertionError(('Bad evidence accepted',name))
        control('missing-module',lambda:assertion(set(roles)-{'time'}==set(MODULES)))
        control('missing-assertion',lambda:fixture_proof('stdlib_time_core',['assert nonexistent_completion_marker'],task,passed,aot_passed,product_inputs,aot_inputs,e))
        control('declaration-only-is-not-behavior',lambda:fixture_proof('stdlib_dict_recipes',['assert behavior_has_run'],task,passed,aot_passed,product_inputs,aot_inputs,e))
        subset=dict(passed);subset.pop('stdlib_time_core')
        control('unexecuted-fixture',lambda:fixture_proof('stdlib_time_core',['assert caught == 8'],task,subset,aot_passed,product_inputs,aot_inputs,e))
        wrong=copy.deepcopy(product_inputs);wrong['stdlib_time_core']={'0'*64}
        control('different-source',lambda:fixture_proof('stdlib_time_core',['assert caught == 8'],task,passed,aot_passed,wrong,aot_inputs,e))
        control('old-guide-is-not-current',lambda:assertion(15==guide_certificate['GuideControls']))
        control('missing-pruning-role',lambda:assertion(len(roles)-1==53))
        report['InputsUnchanged']=e.unchanged()
        report['Complete']=report['InputsUnchanged'] and len(report['Modules'])==53 and len(controls)==7
        report['HistoricalInputsRecovered']=list(e.recovered.values())
        report['EvidenceFilesVerified']=len(e.files)
        report['FixtureCount']=len({f['Name'] for r in report['Behaviors'] for f in r['Fixtures']})
        report['CurrentGateCount']=len(gates)
        report['VerifiedFiles']=[dict(Path=k,Sha256=v) for k,v in sorted(e.files.items())]
        report['ProductReports']=product_certificate['Reports']
    except Exception as error:
        failures.append(repr(error));report['FailureTraceback']=traceback.format_exc()
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report.get(k) for k in ('Complete','InputsUnchanged','FixtureCount','EvidenceFilesVerified','Failures','PendingEvidence')},ensure_ascii=True),flush=True)
    return 0 if report['Complete'] else 1


def assertion(value):assert value


if __name__=='__main__':raise SystemExit(main())
