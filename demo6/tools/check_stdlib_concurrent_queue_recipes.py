"""Closed source XLIR Queue schema and actual native factory/owned-Array proof.
This is a prerequisite gate, not acceptance of the public language Queue cutover.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    parser.add_argument('--runtime',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    assert bool(args.public_directory)!=bool(args.runtime)
    assert (os.name=='nt')==bool(args.public_directory)
    cc=shutil.which('gcc'); assert cc and not out.exists()
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    for folder in ('src','lib','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    for name in ('stdlib_concurrent_queue_recipes.xl','xlir_concurrent_queue_recipe_contract.c',
                 'stdlib_concurrent_queue_recipe_host.c'):
        files.add(root/'test/cases'/name)
    objects=[]
    if args.public_directory:
        manifest=args.public_directory.resolve()/'build.json'; files.add(manifest)
        build=json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
        dll=root/'release/xl.dll'; files.add(dll)
        links=[str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        runtime=args.runtime.resolve()
        files.update(runtime/f for f in ('libxl.so','libxlc.so'))
        links=['-L'+str(runtime),'-lxlc','-lxl','-Wl,-rpath,'+str(runtime),'-pthread','-ldl','-lm']
    inputs=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(files)]
    out.mkdir(parents=True); private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for folder in (root/'src/runtime',root/'release/include'):
        for f in folder.glob('*.h'): shutil.copy2(f,private/f.name)
    if args.public_directory: shutil.copy2(dll,out/'xl.dll')
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'LanguageQueueMigrationComplete':False,
        'Full53Complete':False,'Scope':__doc__,'Platform':sys.platform,'Inputs':inputs,'Rows':rows}
    suffix='.exe' if os.name=='nt' else ''
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(out/'include'),'-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-missing-field-initializers','-Wno-unused-but-set-variable','-Wno-pointer-sign']
    if not args.public_directory: flags+=['-D_GNU_SOURCE']
    try:
        contract=out/('contract'+suffix)
        contract_objects=[o for o in objects if Path(o).name!='xl6_ir_cgen.o']
        run('contract.build',[cc,*flags,'-O0','test/cases/xlir_concurrent_queue_recipe_contract.c',
            *contract_objects,*links,'-o',contract])
        generated=out/'recipes.c'
        log=run('contract.verify',[contract,'test/cases/stdlib_concurrent_queue_recipes.xl',generated])
        proof=re.search(r'Queue recipes: (\d+) complete, (\d+) rejected mutations, (\d+) pending, (\d+) descendant rejections, layouts=(\d+) families=(\d+)',log)
        assert proof,log
        counts=list(map(int,proof.groups()))
        assert counts[0]>=60 and counts[1]==counts[0]*16 and counts[2]>=3 and counts[3]>0 and counts[5]==7
        assert {2,3,4,7,8,9,10,11,12}<={i for i in range(32) if counts[4]&(1<<i)}
        report['Proof']={'Recipes':counts[0],'Mutations':counts[1],'Pending':counts[2],
            'DescendantMutations':counts[3],'LayoutMask':counts[4],'FamilyMask':counts[5]}
        instructions=re.search(r'Queue instructions: (\d+) actual creates, (\d+) rejected before output; (\d+) same-slot domain rejections',log)
        assert instructions
        creates,rejected,same_slot=map(int,instructions.groups())
        assert creates>=2 and rejected==creates*3 and same_slot>0
        report['Proof'].update(Creates=creates,InstructionMutations=rejected,SameSlotMutations=same_slot)
        probe=generated.read_text(encoding='utf-8').split('/* CLOSED_QUEUE_RECIPE_NATIVE_PROOF */',1)[1]
        assert 'xlir_special_queue_' not in probe and 'xlir_channel_' not in probe
        assert 'xrtValueArray(' not in probe and 'xrtValueArrayGet(' not in probe and 'for (' not in probe
        for family in ('Spsc','Mpsc','Mpmc'):
            for call in ('CreateTakeV1','PopArrayV2','PushArrayV2'):
                assert 'xl6'+family+'Queue'+call in probe
        for level in (0,2):
            obj=out/f'recipes.O{level}.o'; binary=out/(f'factory.O{level}'+suffix)
            run(f'O{level}.compile',[cc,*flags,f'-O{level}','-Dmain=xl6QueueRecipeLanguageMain','-c',generated,'-o',obj])
            run(f'O{level}.build',[cc,*flags,f'-O{level}','test/cases/stdlib_concurrent_queue_recipe_host.c',
                obj,*objects,*links,'-o',binary])
            run(f'O{level}.native-factories',[binary])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Queue recipe gate: {len(rows)} phases, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
