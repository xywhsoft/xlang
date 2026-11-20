"""Compile every package-selected native bridge unit and index native imports.

This distinguishes a public declaration from a compiler/policy consumer. It
does not infer equivalent behavior, complete ownership, runtime acceptance or
API completeness from a symbol reference. All unreviewed rows remain pending.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package
from check_xge3_full_surface import source_bindings
from xge_replay_contracts import REPLACED as XGE_REPLACEMENTS
from xui_document_replacements import REPLACEMENTS as DOCUMENT_REPLACEMENTS
from xui_managed_contracts import REPLACED as XUI_REPLACEMENTS

# Logical source-group selectors may name an object-like header alias. Prove
# this one against an actual C relocation, rather than assuming a name suffix.
PROVIDER_ALIASES={'xl6Xge3InitPacked':('xge3_init.h','xl6Xge3InitWithCode')}
REPLACED = {**XGE_REPLACEMENTS, **XUI_REPLACEMENTS, **DOCUMENT_REPLACEMENTS}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('surface','compiler','compiler-report','packages','replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();surface,cc,build,packages,replay,out=[v.resolve() for v in
        (a.surface,a.compiler,a.compiler_report,a.packages,a.replay_report,a.out)]
    if out.is_relative_to(packages) or out.is_relative_to(cc.parent):
        p.error('Keep audit outputs outside package/compiler inputs')
    verify_full_compiler(build,cc);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    proof=verify_replay_package(packages,replay)
    audit=json.loads(surface.read_text(encoding='utf8'));assert audit['inputsUnchanged']
    functions={r['symbol']:r for r in audit['items'] if r['kind']=='functions'}
    assert len(functions)==audit['population']['functions'],'Duplicate function identities require review'
    core=packages/'_xge_native';manifest=core/'xlang.package.json';metadata=json.loads(manifest.read_text(encoding='utf8'))
    actual_headers={p.name:sha(p) for p in (core/'include').glob('*.h')}
    assert all(actual_headers.get(name)==h for name,h in audit['publicHeaderSha256'].items()),'Surface/SDK headers differ'
    artifact=metadata['artifact'];groups=artifact.get('cSourceGroups',[])
    sources=[*artifact['cSources'],*[g['source'] for g in groups]]
    assert len(sources)==len(set(sources))
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','nm')}
    sdk=cc.parent/'include';files=[Path(__file__),Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('xge_replay_contracts.py'),Path(__file__).with_name('check_xge3_full_surface.py'),
        Path(__file__).with_name('xui_managed_contracts.py'),Path(__file__).with_name('xui_document_replacements.py'),
        Path(__file__).with_name('check_xge3_bindings.py'),surface,cc,build,*proof,*tools.values()]
    for folder in (sdk,packages):files.extend(f for f in folder.rglob('*') if f.is_file())
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],objects=[],providerAliases=[],
        runtimeAcceptance=False,apiCompletenessClaim=False,shippingDelivery=False,networkTests=False,graphicsTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,capture_output=True,timeout=180)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,
            logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        return data.decode('utf8')
    def symbols(text):
        # x86_64 COFF import indirection names the same authoritative C symbol.
        return {line.split()[-1].removeprefix('__imp_') for line in text.splitlines() if line.split()}
    consumers={}
    try:
        # Reuse the authoritative inventory parser on copies of the actual
        # package-selected declarations. A same-header, stale language package
        # must not inherit the source inventory's declaration counts.
        view=out/'source-view'
        for package,relative in (('xge','xge'),('xui','xui'),('xge3d','xge/three_d'),('xgeDebug','xge/debug')):
            original=packages/package/'xlang.package.json'
            declaration=json.loads(original.read_text(encoding='utf8'))
            target=view/'lib/bindings'/relative;target.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(original,target/'xlang.package.json')
            for name in declaration['artifact']['xlSources']:
                dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(original.parent/name,dest)
        bound,_,_,_,view_pins=source_bindings(view)
        expected={n for n,r in functions.items() if r['declarationPresent']}
        actual=set(bound)&set(functions)
        assert actual==expected,dict(missing=sorted(expected-actual),unexpected=sorted(actual-expected))
        report['bindingView']=dict(source=str(view),files=view_pins,nativeDeclarationsMatch=True)
        flags=['-std=c11','-O2','-ffunction-sections','-fdata-sections',
            '-Werror=incompatible-pointer-types','-Werror=implicit-function-declaration','-Werror=int-conversion',
            '-Werror=return-type','-DXL_STATIC_EMBED=1','-DXRT_MODULE_ALL','-I'+str(sdk),
            *['-I'+str(f) for f in sorted(packages.glob('*/include'))]]
        if any(artifact.get('nativeLink', {}).values()):
            flags.append('-DXL6_NATIVE_LINK_SCHEMA=1')
        for number,source in enumerate(sources):
            name='unit-'+str(number);obj=out/(name+'.o')
            run(name+'-compile',[tools['gcc'],*flags,'-c',core/source,'-o',obj])
            undefined=symbols(run(name+'-undefined',[tools['nm'],'-u',obj]))
            defined=symbols(run(name+'-defined',[tools['nm'],'--defined-only','-g',obj]))
            for group in groups:
                if group['source']!=source:continue
                for alias in set(group['providedSymbols'])-defined:
                    assert alias in PROVIDER_ALIASES,(source,alias)
                    header,target=PROVIDER_ALIASES[alias];assert target in defined,(source,target)
                    probe=out/(alias+'.c');probe.write_text('#include <'+header+'>\nvoid (*proof)(void)=(void(*)(void))&'+alias+';\n',encoding='utf8')
                    output=out/(alias+'.o')
                    macros=run(alias+'-macros',[tools['gcc'],*flags,'-E','-dM',probe])
                    assert '#define '+alias+' '+target in macros.splitlines(),(alias,'not the reviewed object-like alias')
                    run(alias+'-compile',[tools['gcc'],*flags,'-c',probe,'-o',output])
                    refs=symbols(run(alias+'-undefined',[tools['nm'],'-u',output]))
                    assert target in refs and alias not in refs,(alias,refs)
                    report['providerAliases'].append(dict(alias=alias,target=target,header=header,source=source,
                        probeSha256=sha(probe),objectSha256=sha(output)))
            native=sorted(set(functions)&undefined)
            for symbol in native:consumers.setdefault(symbol,[]).append(source)
            report['objects'].append(dict(source=source,path=str(obj),sha256=sha(obj),nativeImports=native,
                otherUndefinedSymbols=sorted(undefined-set(functions))));save()
        rows=[]
        for symbol,item in sorted(functions.items()):
            if item['declarationPresent']:continue
            replacement=REPLACED.get(symbol)
            if replacement:
                assert replacement[1] in functions and functions[replacement[1]]['declarationPresent'],symbol
                assert any(r['language']==replacement[0] for r in bound[replacement[1]]),replacement
            rows.append(dict(id=item['id'],symbol=symbol,family=item['family'],conditions=item['conditions'],
                publicDeclarationPresent=False,compiledNativeConsumers=consumers.get(symbol,[]),
                explicitReplacement=dict(language=replacement[0],native=replacement[1]) if replacement else None,
                contractReview='pending',runtimeAcceptance='not-certified-by-this-audit'))
        verify_replay_package(packages,replay)
        report.update(items=rows,nativeRevision=audit['nativeRevision'],functionPopulation=len(functions),
            directDeclarations=sum(r['declarationPresent'] for r in functions.values()),nonDirectFunctions=len(rows),
            compiledConsumerFunctions=sum(bool(r['compiledNativeConsumers']) for r in rows),
            explicitReplacements=sum(bool(r['explicitReplacement']) for r in rows),complete=True,
            inputsUnchanged=all(sha(f)==h for f,h in inputs.items()) and
                all(sha(view/f)==h for f,h in view_pins.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,passed=False,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
