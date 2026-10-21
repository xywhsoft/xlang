"""Compare actual mixed-project TCC scalars with the real IDE/clangd bridge."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid

TYPES=('_Bool','char','signed char','unsigned char','short','int','long',
       'long long','void *','float','double','long double')
MACROS=('__TINYC__','__GNUC__','__GNUC_MINOR__','__GNUC_PATCHLEVEL__','__clang__',
        '_MSC_VER','_WIN32','_WIN64','__SIZEOF_POINTER__','__SIZEOF_LONG__','__STDC_VERSION__')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def check(compiler,server,fixture,legacy,output):
    compiler,server,fixture,legacy=map(lambda p:Path(p).resolve(),(compiler,server,fixture,legacy))
    work=Path(output).absolute()/('c-native-scalars-'+uuid.uuid4().hex);work.mkdir(parents=True)
    protected={str(p):sha(p) for p in (compiler,compiler.with_name('xl.dll'),compiler.with_name('xlc.dll'),server,fixture,legacy)}
    records=[];started=datetime.now(timezone.utc).isoformat();status='failed'
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''))
    def run(label,args):
        args=list(map(str,args));(work/(label+'.args.json')).write_text(json.dumps(args,indent=2),encoding='utf8')
        p=subprocess.run(args,cwd=work,env=env,capture_output=True,timeout=180)
        out=work/(label+'.stdout');err=work/(label+'.stderr');out.write_bytes(p.stdout);err.write_bytes(p.stderr)
        records.append({'label':label,'args':args,'exitCode':p.returncode,'stdout':str(out),'stdoutSha256':sha(out),'stderr':str(err),'stderrSha256':sha(err)})
        assert p.returncode==0,(label,p.returncode,p.stdout[-1500:],p.stderr[-1500:])
        return p
    try:
        (work/'checker-input.py').write_bytes(Path(__file__).read_bytes())
        xl=work/'main.xl';native=work/'native.c';project=work/'app.xlp';exe=work/'probe.exe'
        xl.write_text('extern "native_scalar_probe" func nativeProbe() as c_int\nfunc main(...) as int\n return nativeProbe()\nend\n',encoding='utf8')
        declarations=''.join('#ifdef '+m+'\n#define VALUE_'+m+' '+m+'\n#else\n#define VALUE_'+m+' -1\n#endif\n' for m in MACROS)
        fields=[('size_'+str(i),'sizeof('+t+')') for i,t in enumerate(TYPES)]
        fields += [('align_'+str(i),'_Alignof('+t+')') for i,t in enumerate(TYPES)]
        fields += [(m,'VALUE_'+m) for m in MACROS]
        fields += [('char_signed','((char)-1 < 0)'),('long_double_struct','sizeof(struct { char c; long double value; })')]
        fmt='{'+','.join('\\"'+k+'\\":%lld' for k,_ in fields)+'}\\n'
        native.write_text('int printf(const char *, ...);\n'+declarations+'int native_scalar_probe(void) {\n printf("'+fmt+'",\n'+',\n'.join('(long long)('+e+')' for _,e in fields)+');\n return 0;\n}\n',encoding='utf8')
        project.write_text(json.dumps({'schema':1,'name':'Native scalar comparison','kind':'exe','entry':'main.xl','files':['main.xl','native.c'],'cSources':['native.c']}),encoding='utf8')
        for p in (xl,native,project):protected[str(p)]=sha(p)
        run('mixed-build',[compiler,'build',project,'-o',exe])
        measured=json.loads(run('mixed-execute',[exe]).stdout)
        bridge=json.loads(run('compiler-environment',[compiler,'c-bridge',project,'--json','--c-environment']).stdout)
        assert bridge['ok'];environment=bridge['cBridge']['nativeEnvironment'];profile=environment['scalarTarget']
        assert profile['schema']==1 and profile['scope']=='tcc-win64-scalar-target'
        for key,native_key in (('tinyCVersion','__TINYC__'),('pointerBytes','size_8'),('longBytes','size_6'),('longDoubleBytes','size_11'),('longDoubleAlign','align_11')):
            assert profile[key]==measured[native_key],(key,profile[key],measured[native_key])
        assert environment['cPreprocessorGraphComplete'] is False and environment['abiEquivalenceGuaranteed'] is False
        text=declarations
        for i,t in enumerate(TYPES):
            for prefix,expr in (('size_','sizeof'),('align_','_Alignof')):
                key=prefix+str(i);text+='_Static_assert('+expr+'('+t+') == '+str(measured[key])+', "native_'+key+'");\n'
        for m in MACROS:text+='_Static_assert(VALUE_'+m+' == '+str(measured[m])+', "native_'+m+'");\n'
        text+='_Static_assert(((char)-1 < 0) == '+str(measured['char_signed'])+', "native_char_signed");\n'
        text+='_Static_assert(sizeof(struct { char c; long double value; }) == '+str(measured['long_double_struct'])+', "native_long_double_struct");\n'
        text+='_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), double), "draft ABI");\ndouble answer(void) { return xl_add(1,2); }\n'
        check_source=work/'measured-scalar-check.c';check_source.write_text(text,encoding='utf8');protected[str(check_source)]=sha(check_source)
        result=run('real-ide-bridge',[fixture,work/'native',server,compiler,legacy,check_source])
        assert b'measured native scalar/identity assertions' in result.stdout
        reports=list((work/'native').glob('bridge-native-*/prepared-commands.json'));assert len(reports)==1
        database=json.loads(reports[0].read_text(encoding='utf8'));args=database[0]['arguments']
        for flag in ('-mlong-double-64','-U__clang__','-U__GNUC__','-U__GNUC_MINOR__','-U__GNUC_PATCHLEVEL__'):
            assert flag in args,flag
        assert '__TINYC__='+str(measured['__TINYC__']) in args
        status='native-compiler-and-ide-bridge-passed'
    finally:
        unchanged=all(sha(Path(p))==v for p,v in protected.items())
        proof={'status':status,'startedUtc':started,'finishedUtc':datetime.now(timezone.utc).isoformat(),'types':TYPES,'measured':locals().get('measured'),'profile':locals().get('profile'),'records':records,'protected':protected,'protectedUnchanged':unchanged,'scope':'Twelve primitive sizes and alignments, eleven identity/platform macros, signed char, one long double aggregate, real unsaved IDE bridge and incompatible-target rejection; not complete preprocessing or ABI equivalence.'}
        report=work/'evidence.json';report.write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        print(json.dumps({'status':status,'report':str(report),'reportSha256':sha(report)},ensure_ascii=False),flush=True)
        assert unchanged
    return report
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','server','fixture','legacy','output'):ap.add_argument('--'+name,required=True)
    a=ap.parse_args();check(a.compiler,a.server,a.fixture,a.legacy,a.output)
