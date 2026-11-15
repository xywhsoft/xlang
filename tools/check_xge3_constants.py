"""Compare every exported XGE/XUI/Document constant with the configured C SDK.

Names, source values and parser presence are insufficient: generated language
expressions run against an independent C oracle compiled from the public names.
The oracle uses the actual SDK configuration and tests every string byte/length.
No product wrappers, engine initialization, graphics or network are added here.
"""
from pathlib import Path
from collections import Counter
import argparse, ast, hashlib, json, os, re, shutil, subprocess, time
from check_xge3_bindings import inventory, is_build_macro, c_text_constant, constant_value
from check_xge3_full_surface import source_bindings
from foreign_compiler_cohort import load_compiler_cohort


NATIVE_ONLY = {
    'XGE3D_TRANSFORM_IDENTITY': 'C aggregate initializer; requires a value-construction contract, not a scalar constant',
    'XUI_TEXT_PAINT_OWNERSHIP_V1_TAG': 'native text-painter ownership protocol tag',
    'XUI_PROXY_OWNED_ANYTHREAD_FINALIZE': 'native proxy retirement capability; cannot confer language thread safety',
    'XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR': 'native export allocator ABI marker; not a user-selected allocator',
}


def parser_checks():
    positives = [('"a"\n "b\\n"', 'ab\n'), ('"\\x41" "B"', 'AB'),
                 ('"\\1012"', 'A2'), ('"\\?\\\\\\\""', '?\\"'),
                 ('"\\xc3\\xa9"', 'é'), ('"中文" ""', '中文'), ('"a\\0b"', 'a\0b')]
    for expression, expected in positives:
        assert c_text_constant(expression) == expected
    negatives = ['L"wide"', 'u8"other"', '"a" + "b"', '"a" tail',
                 '"\\x41B"', '"\\400"', '"\\u0041"', '"\\q"', '"\\x"', '"a\nb"']
    for expression in negatives:
        try: c_text_constant(expression)
        except (ValueError, UnicodeError): pass
        else: raise AssertionError(expression)
    assert not is_build_macro('XUI_TOAST_CLOSE_API')
    assert is_build_macro('XUI_API') and is_build_macro('XGE_HAS_DEBUGMODE')
    assert constant_value('"a"\n "b"', {}, {}) == 'ab'
    return dict(positive=len(positives), negative=len(negatives), classification=3)


def collect(root, native, out):
    data = inventory(native, out/'native-inventory.json')
    public = {}
    for header in data['headers']+data['supplementaryHeaders']+data['dependencyHeaders']:
        assert not header['parseErrors'], header['header']
        rows = list(header['constants'])+list(header['enumConstants'])
        for typ in header['types']:
            if typ['type']['kind'] == 'enum':
                rows += [dict(v, line=typ['line'], conditions=typ['conditions']) for v in typ['type'].get('values') or []]
        for row in rows:
            public.setdefault(row['name'], []).append(dict(header=Path(header['header']).name, **row))
    _, _, _, constants, inputs = source_bindings(root)
    excluded, missing = {}, []
    for name in public:
        if name in constants: continue
        if is_build_macro(name): excluded[name] = 'native build configuration or import/export decoration'
        elif name in NATIVE_ONLY: excluded[name] = NATIVE_ONLY[name]
        else: missing.append(name)
    assert not missing, 'Unreviewed public constants: '+str(missing)
    assert not set(constants)-set(public), 'Language constants missing from native inventory: '+str(set(constants)-set(public))
    declarations = []
    for name, bindings in sorted(constants.items()):
        assert len(bindings) == 1, (name, bindings)
        binding = bindings[0]; path = root/binding['source']
        line = path.read_text(encoding='utf-8-sig').splitlines()[binding['line']-1]
        match = re.fullmatch(r'export const (\w+) '+name+r'\s*=\s*(.+)', line)
        assert match, line
        typ, expression = match.groups(); value = ast.literal_eval(expression)
        if typ == 'string':
            assert isinstance(value, str); group = 'text'
        elif typ in ('c_float', 'c_double'):
            assert isinstance(value, (int, float)); group = 'floating'
        elif typ in ('c_uint', 'uint32', 'uint64'):
            assert isinstance(value, int) and 0 <= value <= 0xffffffffffffffff; group = 'unsigned'
        else:
            assert typ in ('c_int', 'int64') and isinstance(value, int) and -(1<<63) <= value < (1<<63); group = 'signed'
        declarations.append(dict(name=name, type=typ, group=group, value=value, **binding))
    # Exercise the same evaluator used by source generation on every new or
    # corrected declaration, in addition to the independent compiled C oracle.
    repaired=('XGE_GLSL_MATERIAL_UV','XGE_STATUS_DEFERRED','XGE_ERROR_BUSY',
              'XUI_TOAST_CLOSE_API','XUI_CODE_TOKEN_STYLE_RESOLVER',
              'XUI_CODE_DOCUMENT_BATCH','XUI_STATUS_DEFERRED','XUI_PROXY_VERSION')
    values={row['name']:row['value'] for row in declarations}
    macros={row['name']:row for header in data['headers'] for row in header['functionMacros']}
    for name in repaired:
        for definition in public[name]:
            assert constant_value(definition['expression'], values, macros)==values[name],name
    return dict(publicNames=len(public), publicDefinitions=sum(map(len,public.values())),
                exported=len(declarations), groups=dict(Counter(v['group'] for v in declarations)),
                excluded=excluded, nativeDefinitions=public, declarations=declarations, bindingInputs=inputs,
                generatorReplay=list(repaired))


def emit_fixture(coverage, out):
    """Use C macro/enum names as oracle; never duplicate expected literals."""
    header=out/'constants_oracle.h'; c=out/'constants_oracle.c'; xl=out/'constants.xl'
    h=['#ifndef XGE_CONSTANTS_ORACLE_H', '#define XGE_CONSTANTS_ORACLE_H', '#include <xge3_runtime.h>', '#ifdef __cplusplus', 'extern "C" {', '#endif']
    body=['#include "constants_oracle.h"', '#include <inttypes.h>', '#include <stdio.h>', '#include <string.h>']
    language=['import xge', 'import xui']; statements=[]; functions=[]; selftest=['int main(void) { int failed=0;']
    types={'signed':('int64_t','int64','PRId64'), 'unsigned':('uint64_t','uint64','PRIu64'), 'floating':('double','c_double',None)}
    for group,(ct,lt,fmt) in types.items():
        rows=[r for r in coverage['declarations'] if r['group']==group]
        if not rows:continue
        function='fixtureConstants'+group.title(); count=len(rows)
        h.append(f'int {function}(size_t, const {ct}*, size_t);')
        body.append(f'static const {ct} expected_{group}[] = {{'+','.join(f'({ct})({r["name"]})' for r in rows)+'};')
        body.append(f'static const char* names_{group}[] = {{'+','.join(json.dumps(r['name']) for r in rows)+'};')
        body.append(f'int {function}(size_t offset, const {ct}* values, size_t count) {{ size_t i; if(offset>{count} || count>{count}-offset || !values) return -1; for(i=0;i<count;++i) if(values[i]!=expected_{group}[offset+i]) {{')
        if fmt:
            body.append(f'fprintf(stderr,"%s: expected=%" {fmt} " actual=%" {fmt} "\\n",names_{group}[offset+i],expected_{group}[offset+i],values[i]);')
        else: body.append(f'fprintf(stderr,"%s: expected=%.17g actual=%.17g\\n",names_{group}[offset+i],expected_{group}[offset+i],values[i]);')
        body.append('return (int)i+1; } return 0; }')
        language.append(f'extern "{function}" @header("constants_oracle.h") @no_raise @c_array_group(1,1,1,"size_t",2) @c_array_input(1,0,"const {ct}*","value") func check{group.title()}(c_size offset,array<{lt}> values,int size=-1) as c_int')
        for start in range(0,count,128):
            batch=rows[start:start+128];name=f'{group}Batch{start}'
            functions += [f'func {name}()',f' array<{lt}> values = [', ',\n'.join(f'  {lt}({r["language"]})' for r in batch), ' ]', f' assert check{group.title()}(c_size({start}),values)==c_int(0)', 'end', '']
            statements.append(f' {name}()')
        selftest.append(f'{{ {ct} values[{count}]; memcpy(values,expected_{group},sizeof(values)); if({function}(0,values,{count})!=0) return 1; values[0]+={1 if fmt else "0.5"}; if({function}(0,values,{count})<=0) return 2; if({function}(0,values,{count+1})!=-1 || {function}({count+1},values,0)!=-1) return 3; ++failed; }}')
    texts=[r for r in coverage['declarations'] if r['group']=='text']
    h.append('int fixtureConstantsText(int index,const uint8_t* data,size_t size);')
    body.append('static const struct { const char* name; const char* data; size_t size; } expected_text[] = {')
    body += [f' {{"{r["name"]}", {r["name"]}, sizeof({r["name"]})-1}},' for r in texts]
    body.append('};\nint fixtureConstantsText(int i,const uint8_t* data,size_t size) {')
    body.append(f' if(i<0 || i>={len(texts)} || !data) return -1;\n if(size!=expected_text[i].size || memcmp(data,expected_text[i].data,size)!=0) {{ fprintf(stderr,"%s: text bytes/length mismatch\\n",expected_text[i].name); return 1; }} return 0; }}')
    language.append('extern "fixtureConstantsText" @header("constants_oracle.h") @no_raise @c_span(1,"const uint8_t*","size_t") func checkText(c_int index,string text) as c_int')
    statements += [f' assert checkText(c_int({i}),{r["language"]})==c_int(0)' for i,r in enumerate(texts)]
    selftest += [f'{{ int i; for(i=0;i<{len(texts)};++i) {{ unsigned char wrong[4096]; size_t n=expected_text[i].size; if(n==0 || n>sizeof(wrong)) return 4; if(fixtureConstantsText(i,(const uint8_t*)expected_text[i].data,n)!=0) return 5; memcpy(wrong,expected_text[i].data,n); wrong[0]^=1; if(fixtureConstantsText(i,wrong,n)!=1 || fixtureConstantsText(i,(const uint8_t*)expected_text[i].data,n-1)!=1) return 6; ++failed; }} }}',f'printf("Oracle selftest: %d corruption/count probes rejected\\n",failed); return failed=={len(types)+len(texts)}?0:7; }}']
    h += ['#ifdef __cplusplus','}','#endif','#endif']
    body += ['#ifdef FIXTURE_CONSTANTS_SELFTEST',*selftest,'#endif']
    language += ['', *functions, 'func main(...) as int', *statements, f' println("Constant parity: {len(coverage["declarations"])} native values passed")', ' return 0', 'end']
    for path,lines in ((header,h),(c,body),(xl,language)):path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    return xl,c,header


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-root','compiler','compiler-report','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,native,compiler,build,dynamic,static,out=[v.resolve() for v in
        (a.root,a.native_root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out)]
    assert not out.is_relative_to(root) and not out.is_relative_to(native)
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir()
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,rows=[],parserChecks=parser_checks(),maximumNumericBatch=128)
    tools={n:Path(shutil.which(n)).resolve() for n in ('gcc','g++','objdump')}
    metadata=json.loads(build.read_text(encoding='utf-8'))
    inputs={str(f):sha(f) for f in [compiler,build,Path(metadata['sourceFreeze']),Path(__file__).resolve(),Path(__file__).with_name('check_xge3_bindings.py'),Path(__file__).with_name('check_xge3_full_surface.py'),Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),*extra,*tools.values()]}
    inputs.update({str(f):sha(f) for f in compiler.parent.iterdir() if f.name in ('xl.dll','xlc.dll','libxl.so','libxlc.so')})
    for directory in (sdk,dynamic,static):inputs.update({str(f):sha(f) for f in directory.rglob('*') if f.is_file()})
    report['inputs']=inputs
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(temp,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
        key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(compiler.parent),env.get(key,'')))
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=z.returncode==0,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log;return z.stdout+z.stderr
    try:
        coverage=collect(root,native,out);report['coverage']=coverage
        for relative,digest in coverage['bindingInputs'].items():inputs[str(root/relative)]=digest
        for f in (native/'include').glob('*.h'):inputs[str(f)]=sha(f)
        for row in coverage['declarations']:
            for package in (dynamic,static):
                source=Path(row['source']);relative=source.relative_to('lib/bindings')
                # Base xge/xui sources are selected by these manifests.
                staged=package/relative
                assert staged.is_file() and sha(root/source)==sha(staged),staged
        fixture,c,header=emit_fixture(coverage,out)
        ci=['-I'+str(f) for f in (out,root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
        for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('oracle-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',header])
        for opt in ('O0','O2'):
            binary=out/('oracle-'+opt+('.exe' if win else ''))
            run('oracle-'+opt+'-build',[tools['gcc'],'-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-DFIXTURE_CONSTANTS_SELFTEST',*ci,c,'-o',binary])
            run('oracle-'+opt+'-run',[binary])
        includes=['--inc-path',out,'--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        generated=out/'constants-generated.c';public=out/'constants-public.h'
        run('language-emit',[compiler,'cgen',fixture,'--emit',generated,'--emit-header',public,*includes])
        for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('language-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':output=run('language-jit',[compiler,'run',fixture,c,*includes],packages)
            else:
                binary=out/('constants-'+kind+('.exe' if win else ''))
                run('language-'+kind+'-build',[compiler,'build',fixture,c,'-o',binary,*includes],packages)
                output=run('language-'+kind+'-execute',[binary],packages)
                table=run('language-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                imports=re.findall(rb'DLL Name:\s*(\S+)',table) if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                lower={v.lower() for v in imports}
                assert not lower.intersection({b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'})
                # This fixture deliberately uses only constants: native dead
                # stripping may remove the engine even in the hosted profile.
                if kind=='static':assert not any(b'xge3_host' in v for v in lower)
                report.setdefault('programs',[]).append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode() for v in imports]))
            assert f'Constant parity: {coverage["exported"]} native values passed'.encode() in output
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
