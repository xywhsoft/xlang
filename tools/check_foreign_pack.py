"""CPU-only pack gate over a hash-verified compiler cohort.

Accepts an actual complete source build or an explicitly partial frontend
overlay. Build qualification is recorded without certifying shipping/VFS.
No Native device, graphics, window, network or security tests are run.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
from foreign_validation_build import add_build_arguments,load_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(parser)
    args=parser.parse_args();root,out=[p.resolve() for p in (args.root,args.out)]
    build=load_build(args.build,args.overlay)
    cohort,basePath,base,objects,inputs,driver,runtime=[build[k] for k in ('cohort','basePath','base','objects','inputs','driver','runtime')]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    fixture=root/'test/cases/foreign_pack.xl';native=root/'test/cases/foreign_pack_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    header=root/'test/cases/foreign_pack.h';irFixture=root/'test/cases/foreign_pack_ir.c'
    pinned=[driver,runtime,fixture,native,host,header,Path(__file__).resolve()]
    if irFixture.exists():pinned.append(irFixture)
    inputs.update((str(p),sha(p)) for p in pinned);out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=build["fullSourceRebuild"],buildManifest=build["buildManifest"],overlay=build["overlay"],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();result=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,result.returncode,flush=True);assert (result.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'PackDesc native_pack_' in text and '&native_pack_' in text
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True);shutil.copy2(header,headers/header.name)
    sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or str(Path(base['Objects'][0]['Source']).parents[2]/'release'/'win_x64'))
    includes=['-I'+str(p) for p in (out/'fixture-inputs',cohort,basePath.parent/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+s for s in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        exe=out/(level+('.exe' if os.name=='nt' else '.out'))
        run(level+'-build',['gcc',*common,*warnings,'-'+level,generated,native,host,*objects,runtime,*systems,'-o',exe]);run(level+'-run',[exe])
        if irFixture.exists():
            gate=out/('ir-'+level+('.exe' if os.name=='nt' else '.out'))
            run('ir-'+level+'-build',['gcc',*common,'-'+level,irFixture,*objects,runtime,*systems,'-o',gate]);run('ir-'+level+'-run',[gate,fixture])
    original=fixture.read_text();pack='@c_pack(1,"PackDesc",0,7)';field='@c_pack_field(1,"textSize",3,"text_size","int")'
    changes={
        'duplicate-group':(pack,pack+' '+pack),
        'overlap-group':(pack,pack+' @c_pack(2,"PackDesc",1,2)'),
        'missing-group':(pack,''),'physical-position':(pack,'@c_pack(2,"PackDesc",0,7)'),
        'negative-position':(pack,'@c_pack(-1,"PackDesc",0,7)'),
        'empty-group':(pack,'@c_pack(1,"PackDesc",0,0)'),
        'overflow-group':(pack,'@c_pack(1,"PackDesc",60,7)'),
        'record-injection':(pack,'@c_pack(1,"PackDesc*",0,7)'),
        'unknown-kind':(field,'@c_pack_field(1,"textSize",3,"other","int")'),
        'missing-text-length':(field,''),
        'bad-count-abi':(field,'@c_pack_field(1,"textSize",3,"text_size","uint32_t")'),
        'wrong-logical-source':(field,'@c_pack_field(1,"textSize",0,"text_size","int")'),
        'missing-span-count':('@c_pack_field(1,"dashCount",6,"span_count")',''),
        'duplicate-field':(field,field+' '+field),
        'field-injection':(field,'@c_pack_field(1,"textSize;abort()",3,"text_size","int")'),
        'writable-span':('@c_span(6,"const float*","int")','@c_span(6,"float*","int")'),
        'qualifier-overlap':(pack,pack+' @c_const(3)'),
        'value-overlap':(pack,pack+' @c_value(3,"PackRoot")'),
        'caller-view':('@lifetime("owned") @policy("packPolicy")','@lifetime("caller")'),
    }
    for name,(needle,replacement) in changes.items():
        assert needle in original,name
        case=out/(name+'.xl');case.write_text(original.replace(needle,replacement,1),encoding='utf8');target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    alias=next(line for line in original.splitlines() if 'func twoAlias(' in line)
    additional={
        'same-native-fields':original.replace(alias,alias.replace('"value",0,"value"','"other",0,"value"')),
        'flat-callback':'''extern "pack_flat" @header("test/cases/foreign_pack.h") @no_raise @c_pack(0,"PackSmall",0,1) @c_pack_field(0,"value",0,"value") func flat(c_int value) as c_int
extern "pack_use" @header("test/cases/foreign_pack.h") @no_raise @c_static_callback(0) func use(func(c_int) as c_int callback)
use(flat)
''',
    }
    for name,code in additional.items():
        case=out/(name+'.xl');case.write_text(code,encoding='utf8');target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    for name,badText in {
        'field-type':header.read_text().replace('int textSize;','size_t textSize;'),
        'descriptor-const':header.read_text().replace('PackRoot*,const PackDesc*,PackCallback','PackRoot*,PackDesc*,PackCallback'),
        'size-field':header.read_text().replace('uint32_t size;','int32_t size;'),
        'record-union':header.read_text().replace('typedef struct PackSmall','typedef union PackSmall'),
    }.items():
        bad=out/('bad-'+name)/'test/cases';bad.mkdir(parents=True);(bad/header.name).write_text(badText)
        run('reject-native-'+name,['gcc','-I'+str(bad.parents[1]),*common,*warnings,'-fsyntax-only',generated],False)
    report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
