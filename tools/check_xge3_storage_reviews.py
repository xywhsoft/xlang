"""Exercise fail-closed native storage reviews, then replay generation in isolation.

This is a source/tooling gate. A matching fingerprint is change detection,
not proof of ABI correctness, runtime acceptance, or complete API coverage.
No generated package is installed or copied back into the repository.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,sys,time
from check_xge3_bindings import storage_review_match,native_function_sha,annotate_existing_bindings,write_binding_manifest,inventory,generate


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--native-root',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();native=a.native_root.resolve();out=a.out.resolve()
    assert not out.is_relative_to(root) and not out.is_relative_to(native)
    out.mkdir(parents=True,exist_ok=False)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    catalog=root/'docs/xge3-xui3-api-map-storage.json';reviews=json.loads(catalog.read_text(encoding='utf8'))
    files=[Path(__file__).resolve(),Path(__file__).with_name('check_xge3_bindings.py'),catalog,root/'src/lexer/xl6_lexer_gen.c']
    files.extend(Path(__file__).with_name(name+'.py')for name in('xui_gallery_contracts','xui_geometry_contracts','xui_snapshot_contracts'))
    import pycparser
    files.extend(Path(pycparser.__file__).parent.rglob('*.py'));files.append(Path(sys.executable))
    files.extend(f for f in(root/'lib/bindings').rglob('*')if f.is_file())
    files.extend(p for p in(native/'include').rglob('*')if p.is_file())
    files.extend(native/r['sourceFile']for r in reviews['entries'].values())
    for review in reviews['fieldEntries'].values():
        files.append(native/review['sourceFile']);files.extend(native/d['sourceFile']for d in review.get('dependencies',[]))
    inputs={str(path):sha(path)for path in files}
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,runtimeQualification=False,candidateReadyForDelivery=False,generatedOutputsPromoted=False,pythonVersion=sys.version,pycparserVersion=pycparser.__version__,inputs=inputs,checks=[])
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');os.replace(tmp,out/'results.json')
    def check(name,action):
        started=time.monotonic();action();report['checks'].append(dict(name=name,passed=True,seconds=time.monotonic()-started));save();print(name,flush=True)
    def require(condition,message='check failed'):
        if not condition:raise AssertionError(message)
    try:
        fixture=out/'fixture';(fixture/'src').mkdir(parents=True);(fixture/'include').mkdir()
        consumer=fixture/'src/consumer.c';consumer.write_text('static int consume(const char* text) { return text[0]; }\n',encoding='utf8')
        dependency=fixture/'src/helper.inl';dependency.write_text('/* reviewed implementation */\n',encoding='utf8')
        review=dict(sourceFile='src/consumer.c',nativeHelper='consume',functionSha256=native_function_sha(consumer.read_text(),'consume'),storage='native-copy-or-synchronous-read',dependencies=[dict(sourceFile='src/helper.inl',sha256=sha(dependency))])
        check('matching-body-and-dependency',lambda:require(storage_review_match(fixture,review)==(True,None)))
        legacy={k:v for k,v in review.items()if k!='dependencies'}
        check('legacy-body-only-scope',lambda:require(storage_review_match(fixture,legacy)==(True,None)))
        def rejected(candidate,reason=None):
            matching,message=storage_review_match(fixture,candidate);require(not matching and message)
            if reason:require(reason in message,message)
        for name,patch in{
            'missing-review':None,'missing-body':dict(sourceFile='src/absent.c'),
            'wrong-body':dict(functionSha256='0'*64),'unknown-function':dict(nativeHelper='absent'),
            'invalid-function':dict(nativeHelper='consume();'),'invalid-body-digest':dict(functionSha256=3),
            'empty-dependencies':dict(dependencies=[]),'invalid-dependencies':dict(dependencies={}),
            'invalid-dependency':dict(dependencies=[None]),'extra-dependency-key':dict(dependencies=[dict(review['dependencies'][0],extra=True)]),
            'missing-dependency':dict(dependencies=[dict(sourceFile='src/absent.inl',sha256='0'*64)]),
            'invalid-dependency-digest':dict(dependencies=[dict(sourceFile='src/helper.inl',sha256=3)]),
            'duplicate-dependency':dict(dependencies=review['dependencies']*2),
        }.items():
            candidate=None if patch is None else dict(review,**patch)
            check('reject-'+name,lambda c=candidate:rejected(c))
        for label,path in{'parent':'../outside.c','absolute':str(consumer),'drive':'C:/outside.c','backslash':'src\\consumer.c','empty-segment':'src//consumer.c','dot-segment':'src/./consumer.c'}.items():
            check('reject-entry-path-'+label,lambda p=path:rejected(dict(review,sourceFile=p)))
            candidate=copy.deepcopy(review);candidate['dependencies'][0]['sourceFile']=path
            check('reject-dependency-path-'+label,lambda c=candidate:rejected(c))
        escaped=out/'external.inl';escaped.write_bytes(dependency.read_bytes());link=fixture/'src/external.inl'
        try:link.symlink_to(escaped)
        except OSError as error:report['skippedChecks']=[dict(name='reject-external-symlink',reason=str(error))]
        else:
            candidate=copy.deepcopy(review);candidate['dependencies'][0]['sourceFile']='src/external.inl'
            check('reject-external-symlink',lambda:rejected(candidate,'external source'))
        original=dependency.read_bytes();dependency.write_bytes(original+b'/* changed dependency, unchanged entry */\n')
        check('unchanged-body-changed-dependency-rejected',lambda:rejected(review,'native dependency changed'))
        dependency.write_bytes(original)
        # Test actual annotation, not only the fingerprint helper. A stale
        # prior result must not survive a later non-strict reinspection.
        mini=out/'mini';folder=mini/'lib/bindings/xui';folder.mkdir(parents=True);(mini/'docs').mkdir()
        (folder/'xlang.package.json').write_text(json.dumps(dict(name='xui',artifact=dict(xlSources=['test.xl']))))
        (folder/'test.xl').write_text('export extern "proxy_t" class Proxy @lifetime("owned")\n extern "consume" @c_field_call @c_span(0,"const char*","int") func consume(bytes input)\nend\n')
        (mini/'docs/xge3-xui3-api-map-storage.json').write_text(json.dumps(dict(fieldEntries={'proxy_t.consume':review})))
        data=dict(headers=[dict(header=str(fixture/'include/xui.h'),functions=[],callableFields=[dict(record='proxy_t',member='consume')])])
        check('strict-annotation-matching',lambda:annotate_existing_bindings(mini,data,strict_storage=True))
        dependency.write_bytes(original+b'/* new helper */\n')
        def strict_reject():
            try:annotate_existing_bindings(mini,copy.deepcopy(data),strict_storage=True)
            except ValueError as error:require('native dependency changed' in str(error))
            else:raise AssertionError('strict annotation accepted changed dependency')
        check('strict-annotation-rejects-stale-dependency',strict_reject)
        def non_strict():
            annotate_existing_bindings(mini,data)
            field=data['headers'][0]['callableFields'][0];binding=field['bindings'][-1]
            require(binding['storageReview']['state']=='changed'and 'storageContract'not in field)
            require(not binding['evidence']['abiAccepted']and not binding['evidence']['runtimeAccepted'])
        check('non-strict-marks-changed-clears-prior-contract',non_strict);dependency.write_bytes(original)
        manifest_path=out/'manifest.json'
        seed=dict(format='xlang-package-1',name='test',abiVersion='2',kind='xlang-extlib',artifact=dict(kind='source',xlSources=['src/generated.xl'],includeDirs=['include']))
        check('manifest-first-generation',lambda:write_binding_manifest(manifest_path,seed))
        require(json.loads(manifest_path.read_text())==seed)
        authored=copy.deepcopy(seed);authored.update(version='9.0',dependencies=[dict(name='authored')],requiresNativeFeatures=['KEEP'])
        authored['artifact'].update(xlSources=['src/first.xl','src/generated.xl','src/last.xl'],includeDirs=['custom','include'],cSources=['custom.c'])
        manifest_path.write_text(json.dumps(authored,indent=1)+'\n');before=manifest_path.read_bytes()
        check('manifest-existing-preserved-byte-exact',lambda:write_binding_manifest(manifest_path,seed));require(manifest_path.read_bytes()==before)
        next_seed=copy.deepcopy(seed);next_seed['artifact']['xlSources'].append('src/new.xl')
        check('manifest-appends-new-unit-keeps-metadata',lambda:write_binding_manifest(manifest_path,next_seed))
        expected=copy.deepcopy(authored);expected['artifact']['xlSources'].append('src/new.xl');require(json.loads(manifest_path.read_text())==expected)
        for label,patch in{'identity':dict(name='other'),'abi':dict(abiVersion='3'),'binary-artifact':dict(artifact=dict(kind='binary')),'duplicate-source':dict(artifact=dict(kind='source',xlSources=['x','x'])),'invalid-source':dict(artifact=dict(kind='source',xlSources=[3]))}.items():
            manifest_path.write_text(json.dumps(dict(authored,**patch)));old=manifest_path.read_bytes()
            def reject_manifest():
                try:write_binding_manifest(manifest_path,seed)
                except ValueError:pass
                else:raise AssertionError('invalid manifest accepted')
                require(manifest_path.read_bytes()==old,'rejected manifest was modified')
            check('manifest-rejects-'+label,reject_manifest)
        # The current reviewed entry, all 14 dependencies and each independent
        # mutation are tested against the exact native source cohort.
        actual=reviews['fieldEntries']['xui_proxy_t.textDraw']
        for name,item in reviews['fieldEntries'].items():
            check('native-field-'+name,lambda v=item:require(storage_review_match(native,v)==(True,None)))
        mutated=out/'native-mutations'
        for rel in{actual['sourceFile'],*[d['sourceFile']for d in actual['dependencies']]}:
            dest=mutated/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(native/rel,dest)
        for item in actual['dependencies']:
            file=mutated/item['sourceFile'];old=file.read_bytes();file.write_bytes(old+b'\n/* review witness changed */\n')
            # Appending after the entry body leaves its function digest intact.
            check('reject-real-dependency-'+item['sourceFile'],lambda:require(not storage_review_match(mutated,actual)[0]))
            file.write_bytes(old)
        # Only the isolated destination is mutated by generate().
        generated=out/'generated';generated.mkdir()
        for rel in('src/lexer/xl6_lexer_gen.c','docs/xge3-xui3-api-map-storage.json'):
            dest=generated/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/rel,dest)
        shutil.copytree(root/'lib/bindings',generated/'lib/bindings')
        rows=inventory(native,out/'native-inventory.json')
        check('primary-header-inventory-no-parse-errors',lambda:require(rows['counts']['parseErrors']==0))
        check('full-generator-strict-replay',lambda:generate(generated,rows))
        first={str(p):sha(p)for p in(generated/'lib/bindings').rglob('*')if p.is_file()}
        def stable_replay():
            generate(generated,inventory(native,out/'native-inventory-replay.json'))
            second={str(p):sha(p)for p in(generated/'lib/bindings').rglob('*')if p.is_file()}
            require(first==second,'generator replay is not deterministic')
        check('full-generator-second-replay-identical',stable_replay)
        for rel in('xge/xlang.package.json','xui/xlang.package.json','xge/three_d/xlang.package.json','xge/debug/xlang.package.json'):
            check('manifest-replay-preserves-'+rel,lambda r=rel:require((generated/'lib/bindings'/r).read_bytes()==(root/'lib/bindings'/r).read_bytes()))
        (out/'generated-inventory.json').write_text(json.dumps(rows,indent=2)+'\n')
        import re
        block=lambda text:re.search(r'export extern "xui_widget_t\*" class Page .*?\nend',text,re.S).group()
        check('page-block-replay-stable',lambda:require(block((generated/'lib/bindings/xui/src/managed.xl').read_text())==block((root/'lib/bindings/xui/src/managed.xl').read_text())))
        report['generatedChanges']=[p.relative_to(generated).as_posix()for p in(generated/'lib/bindings').rglob('*')if p.is_file()and (not(root/p.relative_to(generated)).is_file()or sha(p)!=sha(root/p.relative_to(generated)))]
        report['inventoryCounts']=rows['counts'];report['dependencyCount']=len(actual['dependencies'])
        report['inputsUnchanged']=all(Path(p).is_file()and sha(p)==digest for p,digest in inputs.items());require(report['inputsUnchanged'])
        report.update(complete=True,passed=True);save()
    except BaseException as error:
        report['error']=repr(error);save();raise


if __name__=='__main__':main()
