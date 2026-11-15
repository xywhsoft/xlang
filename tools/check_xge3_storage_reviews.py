"""Exercise fail-closed native storage reviews, then replay generation in isolation.

This is a source/tooling gate. A matching fingerprint is change detection,
not proof of ABI correctness, runtime acceptance, or complete API coverage.
No generated package is installed or copied back into the repository.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,sys,time
from check_xge3_bindings import storage_review_match,native_function_sha,annotate_existing_bindings,write_binding_manifest,reviewed_fixed_output_count,authored_binding_index,record_extent_constants,inventory,generate
from xge_replay_contracts import CONTRACTS,validate as validate_xge_contracts
from stage_xge3_replay_contracts import compare_selected
from xui_managed_contracts import validate as validate_managed,METHODS,CALLBACKS,RECORDS,TYPE_SHAPES,managed_units


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
    files.extend(Path(__file__).with_name(name+'.py')for name in('xui_gallery_contracts','xui_geometry_contracts','xui_snapshot_contracts','xui_managed_contracts','xge_replay_contracts','stage_xge3_replay_contracts'))
    import pycparser
    files.extend(Path(pycparser.__file__).parent.rglob('*.py'));files.append(Path(sys.executable))
    files.extend(f for f in(root/'lib/bindings').rglob('*')if f.is_file())
    files.extend(p for p in(native/'include').rglob('*')if p.is_file())
    files.extend(native/r['sourceFile']for r in reviews['entries'].values())
    for item in reviews.get('fixedOutputEntries',{}).values():
        files.append(native/item['sourceFile']);files.extend(native/d['sourceFile']for d in item.get('dependencies',[]))
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
        registry=out/'registry/xge';(registry/'src').mkdir(parents=True)
        manifest=registry/'xlang.package.json';manifest.write_text(json.dumps(dict(artifact=dict(xlSources=['src/records.xl','src/authored.xl']))))
        (registry/'src/records.xl').write_text('export extern "generated_t" struct Generated\nend\n')
        (registry/'src/authored.xl').write_text('export extern "authored_t" struct Authored\nend\nexport extern "nativeRead" func read() as c_int\n')
        (registry/'src/unpublished.xl').write_text('export extern "hidden_t" struct Hidden\nend\n')
        def registry_check():
            records,functions=authored_binding_index(registry)
            require(records=={'authored_t':'Authored'}and set(functions)=={'nativeRead'})
        check('authored-registry-respects-published-and-generated-boundaries',registry_check)
        def reject_registry():
            try:authored_binding_index(registry)
            except ValueError:pass
            else:raise AssertionError('invalid authored registry accepted')
        authored=registry/'src/authored.xl';old=authored.read_bytes();authored.write_bytes(old+b'export extern "authored_t" struct Other\nend\n')
        check('authored-registry-rejects-conflicting-alias',reject_registry);authored.write_bytes(old)
        old=manifest.read_bytes();manifest.write_text(json.dumps(dict(artifact=dict(xlSources=['../external.xl']))))
        check('authored-registry-rejects-source-escape',reject_registry);manifest.write_bytes(old)
        synthetic=dict(functionMacros=[],constants=[dict(name='AXES',expression='8'),dict(name='AXES_ALIAS',expression='AXES'),dict(name='TEXT',expression='"8"'),dict(name='CYCLE1',expression='CYCLE2'),dict(name='CYCLE2',expression='CYCLE1')])
        check('record-extents-resolve-integers-not-text-or-cycles',lambda:require(record_extent_constants(synthetic)=={'AXES':8,'AXES_ALIAS':8}))
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
        ui_header=next(h for h in rows['headers']if Path(h['header']).name=='xui.h')
        check('reviewed-managed-native-signatures-and-resolved-records',lambda:validate_managed(root,ui_header,native_function_sha))
        def reject_managed(candidate=ui_header,source=root):
            try:validate_managed(source,candidate,native_function_sha)
            except ValueError:return
            raise AssertionError('stale managed contract accepted')
        for name in METHODS:
            if name.startswith('xl6'):continue
            for label in('missing','changed'):
                changed=dict(ui_header,functions=[dict(f,declaration=f['declaration']+' changed')if f['name']==name else f for f in ui_header['functions']if label!='missing'or f['name']!=name])
                check('managed-function-reject-'+name+'-'+label,lambda c=changed:reject_managed(c))
        for name in RECORDS.keys()|CALLBACKS.keys():
            for label in('missing','changed'):
                changed=dict(ui_header,types=[dict(t,declaration=t['declaration']+' changed')if t['name']==name else t for t in ui_header['types']if label!='missing'or t['name']!=name])
                check('managed-type-reject-'+name+'-'+label,lambda c=changed:reject_managed(c))
        for name in TYPE_SHAPES:
            # Keep the forward typedef spelling identical while changing its
            # resolved shape. A declaration-only pin must not accept this.
            shape=copy.deepcopy(next(t['type']for t in ui_header['types']if t['name']==name))
            if shape.get('fields'):shape['fields'][0]['type']=dict(kind='named',names=['double'],qualifiers=[])
            else:shape['to']['parameters'][0]['type']=dict(kind='named',names=['double'],qualifiers=[])
            changed=dict(ui_header,types=[dict(t,type=shape)if t['name']==name else t for t in ui_header['types']])
            check('managed-resolved-layout-reject-'+name,lambda c=changed:reject_managed(c))
        inline=out/'managed-inline.h';inline.write_text(Path(ui_header['header']).read_text(encoding='utf-8-sig').replace('case XUI_EVENT_DRAG_LEAVE:case XUI_EVENT_DROP:break;','case XUI_EVENT_DRAG_LEAVE:case XUI_EVENT_DROP+1:break;'))
        check('managed-inline-helper-drift-rejected',lambda:reject_managed(dict(ui_header,header=str(inline))))
        runtime=out/'managed-runtime/lib/bindings/_xge_native/include/xge3_runtime.h';runtime.parent.mkdir(parents=True)
        runtime.write_text((root/'lib/bindings/_xge_native/include/xge3_runtime.h').read_text().replace('int xl6Xui3CopyToClipboard(','void xl6Xui3CopyToClipboard('))
        check('managed-runtime-declaration-drift-rejected',lambda:reject_managed(source=out/'managed-runtime'))
        unit=out/'managed-units.xl'
        sample='export extern "a_t*" class A @lifetime("callback")\n native point handle\n extern "read" @no_raise func read(c_int value=1) as string?\nend\nexport extern "b_t*" class B @lifetime("callback")\n native point handle\nend\n'
        unit.write_text(sample);original_units=managed_units(unit)
        unit.write_text('// same declarations in a different class order\n'+sample[sample.index('export extern "b_t*"'):]+sample[:sample.index('export extern "b_t*"')])
        check('managed-unit-class-order-independent',lambda:require(managed_units(unit)==original_units))
        for label,text in{
            'class-membership':sample.replace(' extern "read" @no_raise func read(c_int value=1) as string?\n','').replace('class B @lifetime("callback")\n','class B @lifetime("callback")\n extern "read" @no_raise func read(c_int value=1) as string?\n'),
            'nullable':sample.replace('as string?','as string'),
            'default':sample.replace('value=1','value=2'),
            'effect':sample.replace('@no_raise ',''),
            'duplicate':sample.replace(' extern "read"',' extern "read" @no_raise func read(c_int value=1) as string?\n extern "read"'),
        }.items():
            unit.write_text(text)
            check('managed-unit-detects-'+label,lambda:require(managed_units(unit)!=original_units))
        for label,text in{'unterminated':sample.rsplit('end',1)[0],'extra-end':sample+'end\n','body':sample.replace(' native point handle',' print("not a declaration")',1),'duplicate-class':sample+sample}.items():
            unit.write_text(text)
            def invalid_units():
                try:managed_units(unit)
                except ValueError:return
                raise AssertionError('invalid managed unit accepted')
            check('managed-unit-rejects-'+label,invalid_units)
        by_name={f['name']:f for h in rows['headers']for f in h['functions']}
        check('all-reviewed-direct-declarations-match-native',lambda:require(validate_xge_contracts(by_name)=={k:v[1]for k,v in CONTRACTS.items()}))
        for name in CONTRACTS:
            for label in('missing','changed'):
                changed=dict(by_name)
                if label=='missing':changed.pop(name)
                else:changed[name]=dict(changed[name],declaration=changed[name]['declaration']+' changed')
                def reject_override(candidate=changed):
                    try:validate_xge_contracts(candidate)
                    except ValueError:pass
                    else:raise AssertionError('stale reviewed declaration accepted')
                check('reviewed-direct-reject-'+name+'-'+label,reject_override)
        for name in('xgeShapeExGetOBB','xgeShapeExSceneGetOBB'):
            function=by_name[name];fixed=reviews['fixedOutputEntries'][name]
            check('fixed-output-matching-'+name,lambda f=function,v=fixed:require(reviewed_fixed_output_count(native,f,v,2,'xge_vec2_t',4)==4))
            def invalid_fixed(candidate,f=function):
                try:reviewed_fixed_output_count(native,f,candidate,2,'xge_vec2_t',4)
                except ValueError:pass
                else:raise AssertionError('invalid fixed output review accepted')
            for label,patch in{'missing':None,'zero':dict(count=0),'boolean':dict(count=True),'negative':dict(count=-1),'scalar':dict(count=1),'oversized':dict(count=0x80000000),'wrong-parameter':dict(parameter=1),'float-parameter':dict(parameter=2.0),'wrong-element':dict(elementCType='xge_vec3_t'),'changed-declaration':dict(nativeDeclaration='changed'),'stale-body':dict(functionSha256='0'*64),'no-dependencies':dict(dependencies=[]),'wrong-storage':dict(storage='native-copy-or-synchronous-read')}.items():
                candidate=None if patch is None else dict(fixed,**patch)
                check('fixed-output-reject-'+name+'-'+label,lambda c=candidate,fail=invalid_fixed:fail(c))
        report['reviewedFixedOutputs']=2
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
        shape_generated=generated/'lib/bindings/xge/src/shapes.xl';shape_current=root/'lib/bindings/xge/src/shapes.xl'
        check('shape-fixed-output-replay-text-stable',lambda:require(shape_generated.read_text(encoding='utf-8-sig')==shape_current.read_text(encoding='utf-8-sig')))
        check('nine-selected-files-preserve-complete-declaration-units',lambda:report.update(selectedDeclarationUnits=compare_selected(root,generated)))
        def record_block(path,native):
            return re.search(r'export extern "'+re.escape(native)+r'" struct .*?\nend',path.read_text(encoding='utf-8-sig'),re.S).group()
        for package,native_record in(('xge','xge_gamepad_state_t'),('xui','xui_terminal_cell_t')):
            rel=Path('lib/bindings')/package/'src/records.xl'
            check('complete-symbolic-array-record-'+native_record,lambda rel=rel,n=native_record:require(record_block(generated/rel,n)==record_block(root/rel,n)))
        report['shapeNormalizedSha256']=hashlib.sha256(shape_generated.read_text(encoding='utf-8-sig').encode()).hexdigest()
        changed=[p for p in(generated/'lib/bindings').rglob('*')if p.is_file()and (not(root/p.relative_to(generated)).is_file()or sha(p)!=sha(root/p.relative_to(generated)))]
        report['generatedByteChanges']=[p.relative_to(generated).as_posix()for p in changed]
        report['generatedChanges']=[p.relative_to(generated).as_posix()for p in changed if not(root/p.relative_to(generated)).is_file()or p.suffix not in('.xl','.json')or p.read_text(encoding='utf-8-sig')!=(root/p.relative_to(generated)).read_text(encoding='utf-8-sig')]
        report['sourceComparison']='UTF-8 with normalized line endings for .xl/.json; byte changes recorded separately. Dependency witnesses and package preservation remain byte-exact.'
        report['inventoryCounts']=rows['counts'];report['dependencyCount']=len(actual['dependencies'])
        report['inputsUnchanged']=all(Path(p).is_file()and sha(p)==digest for p,digest in inputs.items());require(report['inputsUnchanged'])
        report.update(complete=True,passed=True);save()
    except BaseException as error:
        report['error']=repr(error);save();raise


if __name__=='__main__':main()
