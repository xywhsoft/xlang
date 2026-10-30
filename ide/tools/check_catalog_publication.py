"""Actual filesystem commit, rollback, interrupted-process and writer-lock checks."""
import argparse,json,os,shutil,subprocess,sys,time,uuid
from pathlib import Path
from catalog_publish import publish,recover,pair_state,locked,chain
from catalog_sources import verify,sha
IDE=Path(__file__).resolve().parents[1]

def worker(pair,data,work,point,ready):
    def checkpoint(name):
        if name==point:
            Path(ready).write_text(str(os.getpid()),encoding='utf-8')
            while True: time.sleep(1)
    publish(Path(pair),Path(data),Path(work),checkpoint)

def check(old,new,output):
    output=chain(output)/('publication-'+uuid.uuid4().hex[:10]); output.mkdir(parents=True)
    old,new=chain(old),chain(new); old_state=pair_state(old); new_state=pair_state(new); rows=[]
    assert old_state and new_state and old_state!=new_state
    def fresh(label):
        root=output/label; root.mkdir(); data=root/'data'; data.mkdir()
        shutil.copy2(old/'commands.json',data/'commands.json'); shutil.copytree(old/'catalog-sources',data/'catalog-sources')
        return root,data,root/'journal'
    root,data,work=fresh('commit'); transaction=publish(new,data,work)
    assert pair_state(data)==new_state and json.loads((transaction/'transaction.json').read_text())['state']=='committed'
    rows.append({'case':'complete-commit','passed':True})
    def no_step(name): raise AssertionError('An identical pair must not mutate the installed pair')
    same=publish(new,data,work,no_step)
    assert pair_state(data)==new_state and json.loads((same/'transaction.json').read_text())['alreadyInstalled']
    rows.append({'case':'identical-pair-preserved','passed':True})
    for point in ('new-bundle-installed','catalog-installed',None):
        root=output/('initial-'+str(point)); root.mkdir(); data=root/'data'; data.mkdir(); work=root/'journal'
        def initial_fail(name):
            if name==point: raise RuntimeError('Injected initial publication exception')
        if point:
            try: publish(new,data,work,initial_fail)
            except RuntimeError: pass
            else: raise AssertionError('Expected initial publication exception')
            assert pair_state(data) is None
        else:
            publish(new,data,work); assert pair_state(data)==new_state
        rows.append({'case':'initial-'+str(point),'passed':True})
    for point in ('prepared','previous-bundle-moved','new-bundle-installed','catalog-installed'):
        root,data,work=fresh('exception-'+point)
        def fail(name):
            if name==point: raise RuntimeError('Injected owned publication exception')
        try: publish(new,data,work,fail)
        except RuntimeError: pass
        else: raise AssertionError('Expected publication exception')
        assert pair_state(data)==old_state
        rows.append({'case':'exception-'+point,'passed':True,'previousPairRestored':True})
    for label,point in (('previous-bundle-moved','previous-bundle-moved'),('new-bundle-installed','new-bundle-installed'),('catalog-installed','catalog-installed'),('external-change','new-bundle-installed')):
        root,data,work=fresh('interrupted-'+label); ready=root/'ready.txt'; log=root/'worker.log'
        arguments=[sys.executable,'-X','utf8',str(Path(__file__)),'--worker',str(new),str(data),str(work),point,str(ready)]
        with log.open('xb') as stream:
            child=subprocess.Popen(arguments,stdout=stream,stderr=subprocess.STDOUT)
            try:
                limit=time.monotonic()+20
                while not ready.exists():
                    assert child.poll() is None,'Publication worker exited before its checkpoint'
                    if time.monotonic()>limit: raise TimeoutError('Publication checkpoint not observed')
                    time.sleep(.05)
                assert child.poll() is None and int(ready.read_text())==child.pid
                if point=='previous-bundle-moved':
                    try: publish(new,data,work)
                    except ValueError as error: assert 'Another catalog publication' in str(error)
                    else: raise AssertionError('Concurrent publisher acquired an owned live writer lock')
                child.kill(); code=child.wait(timeout=10)
            finally:
                if child.poll() is None: child.kill(); child.wait(timeout=10)
        if label=='external-change':
            index=data/'catalog-sources/index.json'; original=index.read_bytes(); changed=original+b'\n'; index.write_bytes(changed)
            catalog_before=sha(data/'commands.json')
            try: recover(data,work)
            except ValueError: pass
            else: raise AssertionError('Externally changed source index was accepted for recovery')
            assert index.read_bytes()==changed and sha(data/'commands.json')==catalog_before
            index.write_bytes(original)
        results=recover(data,work); expected=old_state if point=='previous-bundle-moved' else new_state
        assert pair_state(data)==expected and results and not recover(data,work)
        rows.append({'case':'interrupted-'+label,'passed':True,'actualWorkerExit':code,'recovered':results,'logSha256':sha(log)})
    root,data,work=fresh('invalid-pair'); invalid=root/'invalid'; shutil.copytree(new/'catalog-sources',invalid/'catalog-sources'); shutil.copy2(new/'commands.json',invalid/'commands.json')
    (invalid/'commands.json').write_bytes((invalid/'commands.json').read_bytes()+b'\n')
    try: publish(invalid,data,work)
    except ValueError: pass
    else: raise AssertionError('Mismatched source pair was published')
    assert pair_state(data)==old_state; rows.append({'case':'invalid-pair-rejected','passed':True})
    assert pair_state(old)==old_state and pair_state(new)==new_state
    report={'status':'passed','old':old_state,'new':new_state,'checks':rows,'originalPairsUnchanged':True}
    (output/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8'); print(output,len(rows),'publication checks passed')

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--old',type=Path); parser.add_argument('--new',type=Path); parser.add_argument('--output',type=Path); parser.add_argument('--worker',nargs=5)
    args=parser.parse_args()
    if args.worker: worker(*args.worker)
    else:
        if not all((args.old,args.new,args.output)): parser.error('--old, --new and --output are required')
        check(args.old,args.new,args.output)

if __name__=='__main__': main()
