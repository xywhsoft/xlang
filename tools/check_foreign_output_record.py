"""CPU-only borrowed output-record projection gate over a hash-verified compiler cohort.

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
    fixture=root/'test/cases/foreign_output_record.xl';native=root/'test/cases/foreign_output_record_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    header=root/'test/cases/foreign_output_record.h';irFixture=root/'test/cases/foreign_output_record_ir.c'
    pinned=[driver,runtime,fixture,native,host,header,Path(__file__).resolve()]
    if irFixture.exists():pinned.append(irFixture)
    inputs.update((str(p),sha(p)) for p in pinned);out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=build["fullSourceRebuild"],buildManifest=build["buildManifest"],overlay=build["overlay"],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();result=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,result.returncode,flush=True);assert (result.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'OutputRecordNative native_raw = {0};' in text and 'native_admitted' in text
    assert 'borrowed output record field type' in text and 'native_raw.first' in text and 'native_raw.point' in text
    assert 'xl6ForeignResultProbeReserveRootV1' in text and 'xl6ForeignResultProbeMaterializeRootV1' in text
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
    original=fixture.read_text(encoding='utf-8-sig')
    group='@c_output_record(1,"OutputRecordNative","borrowed")'
    field='@c_output_field(1,"first","retain",retain,"zero",rollback)'
    changes={
        'duplicate-group':group+' '+group,'missing-group':'','negative-physical':'@c_output_record(-1,"OutputRecordNative","borrowed")',
        'incomplete-physical':'@c_output_record(63,"OutputRecordNative","borrowed")','owned-record':'@c_output_record(1,"OutputRecordNative","owned")',
        'record-expression':'@c_output_record(1,"Native);bad(","borrowed")','missing-record-argument':'@c_output_record(1,"OutputRecordNative")',
        'view-overlap':group+' @c_output_view(1,"text")','size-wrong-slot':group+' @c_output_size(63,"size")'}
    negatives={name:original.replace(group,change,1) for name,change in changes.items()}
    fieldChanges={'duplicate-field':field+' '+field,'missing-field':'','bare-pointer':'@c_output_field(1,"first")',
        'negative-ordinal':'@c_output_field(-1,"first","retain",retain,"zero",rollback)',
        'large-ordinal':'@c_output_field(63,"first","retain",retain,"zero",rollback)',
        'private-field':'@c_output_field(1,"_first","retain",retain,"zero",rollback)',
        'nested-field':'@c_output_field(1,"first.value","retain",retain,"zero",rollback)',
        'text-unsupported':'@c_output_field(1,"first","text")',
        'unknown-conversion':'@c_output_field(1,"first","copy",retain,"zero",rollback)',
        'missing-retain-helper':'@c_output_field(1,"first","retain",missing,"zero",rollback)',
        'bad-retain-helper':'@c_output_field(1,"first","retain",rollback,"zero",rollback)',
        'bad-rollback-helper':'@c_output_field(1,"first","retain",retain,"zero",retain)',
        'bad-success':'@c_output_field(1,"first","retain",retain,"other",rollback)'}
    negatives.update({name:original.replace(field,change,1) for name,change in fieldChanges.items()})
    negatives['retained-pure']=original.replace('@no_raise '+group,'@pure @no_raise '+group,1)
    negatives['missing-no-raise']=original.replace('@no_raise '+group,group,1)
    negatives['nullable-pod']=original.replace('as (c_uint,Node,Point,Node?,c_int)','as (c_uint,Node,Point,Node?,c_int?)',1)
    alias=next(line for line in original.splitlines() if 'func readAlias(' in line)
    negatives['same-native-field-contract']=original.replace(alias,alias.replace('@c_output_field(0,"code")','@c_output_field(0,"otherCode")'))
    negatives['same-native-retain-contract']=original.replace(alias,alias.replace(field,field.replace('"zero"','"positive"')))
    negatives['same-native-physical-record']=original.replace(alias,alias.replace('"OutputRecordNative"','"OutputRecordOther"'))
    for name,code in negatives.items():
        case=out/(name+'.xl');case.write_text(code,encoding='utf8');target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    originalHeader=header.read_text(encoding='utf-8-sig')
    headerChanges={'producer-pointer':('int output_record_read(OutputRecordNode*,OutputRecordNative*);','int output_record_read(OutputRecordNode*,void*);'),
        'producer-status':('int output_record_read(OutputRecordNode*,OutputRecordNative*);','void output_record_read(OutputRecordNode*,OutputRecordNative*);'),
        'field-signedness':('uint32_t code;','int32_t code;'),'field-retained-pointer':('OutputRecordNode* first;','void* first;'),
        'field-pod-record':('OutputRecordPoint point;','float point;'),'field-scalar':('int mode;','float mode;'),
        'union-record':('typedef struct OutputRecordNative {','typedef union OutputRecordNative {')}
    for name,(before,after) in headerChanges.items():
        assert before in originalHeader
        bad=out/('bad-'+name+'/test/cases');bad.mkdir(parents=True)
        (bad/header.name).write_text(originalHeader.replace(before,after),encoding='utf-8')
        run('reject-native-'+name,['gcc','-I'+str(bad.parents[1]),*common,*warnings,'-fsyntax-only',generated],False)
        run('reject-native-public-'+name,['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(bad.parents[1]),*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
    bad=out/'bad-cpp-nontrivial/test/cases';bad.mkdir(parents=True)
    (bad/header.name).write_text(originalHeader.replace('typedef struct OutputRecordNative {','typedef struct OutputRecordNative {\n#ifdef __cplusplus\n~OutputRecordNative() {}\n#endif'),encoding='utf-8')
    run('reject-native-cpp-nontrivial',['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(bad.parents[1]),*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
    report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()
if __name__=='__main__':main()


