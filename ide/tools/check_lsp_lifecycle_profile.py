"""Check the actual private timer runtime, cancellation wrapper and signature path.

No test image is installed. Three real C omissions must fail the same oracle.
"""
from pathlib import Path
import argparse
import io
import json
import os
import subprocess
import threading

from build_lsp_lifecycle_profile import read,sha
from profile_lsp_lifecycle import frames,verify_profile
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();profile=args.profile.resolve(strict=True);build=read(profile/'result.json');verify_profile(build)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    baseline=read(Path(build['normalBuild'])/'build-result.json');labels=build['labels']
    report=dict(status='running',accepted=False,commands=[],cases=[],protected={str(Path(__file__).resolve()):sha(__file__),str(profile/'result.json'):sha(profile/'result.json')})
    for name in ('profile_lsp_lifecycle.py','build_lsp_lifecycle_profile.py','check_language_definitions.py','check_language_snapshots.py','check_lsp_automatic_diagnostics.py'):
        path=Path(__file__).with_name(name).resolve();report['protected'][str(path)]=sha(path)
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    env=dict(os.environ);env['PATH']=str(Path(baseline['compiler']).parent)+os.pathsep+env.get('PATH','')
    def run(command,name):
        report['commands'].append(command);save();value=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=60)
        (out/(name+'-stdout.log')).write_bytes(value.stdout);(out/(name+'-stderr.log')).write_bytes(value.stderr)
        if value.returncode:raise ValueError('fixture failed: '+name+' exit '+str(value.returncode))
        return value
    command=next(c for c in baseline['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem=='check')
    flags=command[:command.index('-c')]+['-I'+str(profile)]
    link=next(c for c in baseline['commands'] if '-c' not in c and str(Path(build['normalBuild'])/'xlang-ls.exe') in c)
    libraries=[arg for arg in link if arg.startswith('-l') or arg=='-Wl,--gc-sections']
    def compile_file(source,name,extra=()):
        path=out/(name+'.c');path.write_text(source,encoding='utf8',newline='\n');obj=out/(name+'.o')
        run(flags+list(extra)+['-c',str(path),'-o',str(obj)],name+'-compile');return obj
    def executable(obj,name,diagnostic):
        exe=out/(name+'.exe');run([command[0],str(obj),str(diagnostic),str(Path(build['normalBuild'])/'xlang-analysis-core.a'),*libraries,'-o',str(exe)],name+'-link');return exe
    session=labels.index('session.total');worker=labels.index('worker.total');retire=labels.index('retire.total')
    runtime='''#include "profile.h"
static xatomic32 arrived;
static int32 task(ptr ignored){
 (void)ignored;xrtAtomic32FetchAdd(&arrived,1,XMEMORY_RELAXED);
 while(xrtAtomic32Load(&arrived,XMEMORY_RELAXED)<8)xrtThreadYield();
 for(unsigned i=0;i<100;++i){
  xlsLifecycleBegin();double begin=xrtTimer();xlsLifecycleAdd(SESSION,begin);
  xlsLifecycleBegin();xlsLifecycleAdd(RETIRE,begin);xlsLifecycleEmit("retire");
  xlsLifecycleAdd(SESSION,begin);xlsLifecycleAdd(WORKER,begin);xlsLifecycleEmit("check");
 }
 return 0;
}
int main(void){
 xthread* threads[8];xrtAtomic32Init(&arrived,0);
 for(unsigned i=0;i<8;++i){threads[i]=xrtThreadCreate(task,NULL,0);if(!threads[i])return 71;}
 for(unsigned i=0;i<8;++i){if(xrtThreadWaitFor(threads[i],10000)!=XWAIT_OK)return 72;xrtThreadDestroy(threads[i]);}
 return 0;
}
'''.replace('SESSION',str(session)).replace('RETIRE',str(retire)).replace('WORKER',str(worker))
    def runtime_oracle(stderr):
        records=frames(stderr,set(labels));threads={value['thread'] for value in records}
        if len(records)!=1600 or len(threads)!=8:raise ValueError('lost or mixed concurrent frames')
        counts={thread:0 for thread in threads}
        for value in records:
            counts[value['thread']]+=1;cells=value['cells']
            if value['group']=='check':
                if set(cells)!={'session.total','worker.total'} or cells['session.total']['calls']!=2 or cells['worker.total']['calls']!=1:
                    raise ValueError('nested scope overwrote or contaminated outer cells')
            elif value['group']=='retire':
                if set(cells)!={'retire.total'} or cells['retire.total']['calls']!=1:raise ValueError('inner scope contamination')
            else:raise ValueError('unexpected runtime scope')
        if any(count!=200 for count in counts.values()):raise ValueError('lost per-thread frames')
        return records
    try:
        obj=compile_file(runtime,'runtime');exe=executable(obj,'runtime',profile/'diagnostic.o')
        value=run([str(exe)],'runtime-run');records=runtime_oracle(value.stderr.decode('utf8'))
        report['cases'].append(dict(name='actual-runtime-eight-threads-nested-scopes',frames=len(records),passed=True));save()
        diagnostic=(profile/'diagnostic.c').read_text('utf8')
        variants={
            'overwrite-parent':('memset(frames[depth],0,sizeof(frames[depth]))','memset(frames[0],0,sizeof(frames[0]))'),
            'zero-sequence':('++sequence','(sequence=0)'),
        }
        for name,(old,new) in variants.items():
            if diagnostic.count(old)!=1:raise ValueError('runtime mutation seam changed: '+name)
            mutated=compile_file(diagnostic.replace(old,new),name);exe=executable(obj,name,mutated);value=run([str(exe)],name+'-run')
            try:runtime_oracle(value.stderr.decode('utf8'))
            except ValueError as error:report['cases'].append(dict(name=name,passed=True,actualCMutantRejected=True,reason=str(error)))
            else:raise ValueError('actual runtime omission was accepted: '+name)
        cancelled='''#include "WORKER_SOURCE"
int main(void){
 xls_worker worker={0};query_input input={.worker=&worker};xtaskvalue output={0};
 xcancel* cancel=xrtCancelCreate();if(!cancel || !xrtCancelRequest(cancel))return 73;
 xtaskoutcome outcome=execute(cancel,&input,&output);xrtCancelDestroy(cancel);
 return outcome==XTASK_CANCELLED ? 0 : 74;
}
'''
        worker_source=profile/'worker.c'
        def cancel_oracle(stderr):
            records=frames(stderr,set(labels))
            if len(records)!=1 or records[0]['group']!='check' or set(records[0]['cells'])!={'worker.total'}:
                raise ValueError('cancelled early return lost its complete worker scope')
        obj=compile_file(cancelled.replace('WORKER_SOURCE',worker_source.as_posix()),'cancelled',('-include',str(profile/'profile.h')))
        exe=executable(obj,'cancelled',profile/'diagnostic.o');value=run([str(exe)],'cancelled-run');cancel_oracle(value.stderr.decode('utf8'))
        report['cases'].append(dict(name='actual-cancelled-worker-return',passed=True))
        source=worker_source.read_text('utf8');old='xlsLifecycleEmit("check");'
        if source.count(old)!=1:raise ValueError('worker mutation seam changed')
        mutant=out/'worker-without-final-frame.inc';mutant.write_text(source.replace(old,'(void)0;'),encoding='utf8',newline='\n')
        obj=compile_file(cancelled.replace('WORKER_SOURCE',mutant.as_posix()),'cancelled-omit',('-include',str(profile/'profile.h')))
        exe=executable(obj,'cancelled-omit',profile/'diagnostic.o');value=run([str(exe)],'cancelled-omit-run')
        try:cancel_oracle(value.stderr.decode('utf8'))
        except ValueError as error:report['cases'].append(dict(name='omit-worker-final-frame',passed=True,actualCMutantRejected=True,reason=str(error)))
        else:raise ValueError('actual worker omission accepted')
        body='func add(int left,int right) as int\n return left+right\nend\nfunc main(...) as int\n return add(1,2)\nend\n';answers=[]
        folder=out/'signature';folder.mkdir();path=folder/'main.xl'
        for variant in ('normal','diagnostic'):
            client=AutomaticClient(Path(build['programs'][variant]['path']),folder/'absent/xl.exe',folder)
            stream=client.process.stderr;chunks=[];reader=threading.Thread(target=lambda:chunks.append(stream.read()),daemon=True);reader.start();client.process.stderr=io.BytesIO()
            try:
                client.initialize();client.open(path,body,1)
                answer=client.call('textDocument/signatureHelp',dict(textDocument=dict(uri=path.as_uri()),position=position(body,body.index('1,2')+2)))
                if not answer or not answer['signatures']:raise ValueError('signature fast return produced no answer')
                # Native thread identities necessarily differ between processes.
                # Use the same source path and compare all other result/counter data.
                answer['analysis'].pop('workerThreadId');answer['analysis'].pop('captureThreadId');answers.append(answer)
            finally:
                client.stop();reader.join(5);stderr=b''.join(chunks);(out/('signature-'+variant+'-stderr.log')).write_bytes(stderr)
                (out/('signature-'+variant+'-transcript.json')).write_text(json.dumps(client.transcript,indent=2)+'\n',encoding='utf8',newline='\n')
            if client.process.returncode or reader.is_alive():raise ValueError('signature process/reader failed')
            if variant=='normal' and stderr:raise ValueError('normal signature oracle wrote stderr')
            if variant=='diagnostic':
                records=frames(stderr.decode('utf8'),set(labels));snapshots=[r for r in records if r['group']=='snapshot']
                if len(snapshots)!=1 or snapshots[0]['cells']['adapter.total']['calls']!=1:raise ValueError('signature early return lost outer adapter scope')
                report['cases'].append(dict(name='actual-signature-early-return',frames=len(records),passed=True))
        if answers[0]!=answers[1]:raise ValueError('signature result differs from uninstrumented host')
        for bad in ['XLS_LIFECYCLE_ERROR depth\n','XLS_LIFECYCLE {','XLS_LIFECYCLE {"group":"check","group":"retire"}']:
            try:frames(bad,set(labels))
            except (ValueError,TypeError):report['cases'].append(dict(name='malformed-frame-rejected',passed=True))
            else:raise ValueError('invalid frame accepted')
        verify_profile(build)
        for path,digest in report['protected'].items():
            if sha(path)!=digest:raise ValueError('protected input changed: '+path)
        report.update(status='passed',outputs={str(p):sha(p) for p in out.iterdir() if p.is_file() and p.name!='result.json'})
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
