"""Publish a verified catalog/source pair with durable recovery and a writer lock."""
import argparse,contextlib,hashlib,json,os,shutil,uuid
from pathlib import Path
from catalog_sources import plain,sha,verify

def write_json(path,data):
    temporary=path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    with temporary.open('x',encoding='utf-8') as stream:
        stream.write(json.dumps(data,ensure_ascii=False,indent=2)+'\n'); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary,path)

def chain(path):
    path=Path(path).absolute()
    # Recovery journals retain several complete copies. Their digest filenames
    # exceed MAX_PATH in normal Windows workspaces, including our fault fixtures.
    if os.name=='nt' and not str(path).startswith('\\\\?\\'):
        value=str(path)
        path=Path('\\\\?\\UNC\\'+value[2:] if value.startswith('\\\\') else '\\\\?\\'+value)
    for parent in (path,*path.parents):
        if parent.exists(): plain(parent,True)
    return path

@contextlib.contextmanager
def locked(work,data):
    import msvcrt
    work=chain(work); work.mkdir(parents=True,exist_ok=True)
    key=hashlib.sha256(str(data).casefold().encode('utf-8')).hexdigest()
    directory=work/key; directory.mkdir(exist_ok=True); chain(directory)
    lock=directory/'writer.lock'
    if lock.exists(): plain(lock)
    with lock.open('a+b') as stream:
        if not stream.tell(): stream.write(b'0'); stream.flush()
        stream.seek(0)
        try: msvcrt.locking(stream.fileno(),msvcrt.LK_NBLCK,1)
        except OSError as error: raise ValueError('Another catalog publication owns this destination') from error
        try: yield directory
        finally: stream.seek(0); msvcrt.locking(stream.fileno(),msvcrt.LK_UNLCK,1)

def pair_state(data):
    catalog=data/'commands.json'; bundle=data/'catalog-sources'
    if not catalog.exists() and not bundle.exists(): return None
    plain(catalog); info=verify(bundle,catalog)
    return {'catalogSha256':sha(catalog),'sourceIndexSha256':sha(bundle/'index.json'),'available':info['available'],'unavailable':info['unavailable']}

def file_hash(path):
    if not path.exists(): return None
    plain(path); return sha(path)

def record_state(transaction,record,state):
    record['state']=state; write_json(transaction/'transaction.json',record)

def replace_catalog(source,data):
    temporary=data/('commands.'+uuid.uuid4().hex+'.pending')
    shutil.copyfile(source,temporary)
    with temporary.open('r+b') as stream: stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary,data/'commands.json')

def rollback(transaction,record,data):
    old,new=record['old'],record['new']; prior=transaction/'previous'; staged=transaction/'new'
    if pair_state(staged)!=new or (old is not None and pair_state(prior)!=old): raise ValueError('Recovery snapshots changed')
    current_hash=file_hash(data/'commands.json')
    if current_hash not in (old['catalogSha256'] if old else None,new['catalogSha256']): raise ValueError('Catalog changed outside the publication')
    record_state(transaction,record,'rolling-back')
    bundle=data/'catalog-sources'
    if bundle.exists():
        try: current_index=verify(bundle,staged/'commands.json'); is_new=sha(bundle/'index.json')==new['sourceIndexSha256']
        except (OSError,ValueError): is_new=False
        if is_new:
            os.rename(bundle,transaction/('rolled-back-sources-'+uuid.uuid4().hex))
        elif old is None or verify(bundle,prior/'commands.json')['catalogSha256']!=old['catalogSha256'] or sha(bundle/'index.json')!=old['sourceIndexSha256']:
            raise ValueError('Sources changed outside the publication')
    if old is not None and not bundle.exists():
        moved=transaction/'previous-moved'
        if verify(moved,prior/'commands.json')['catalogSha256']!=old['catalogSha256'] or sha(moved/'index.json')!=old['sourceIndexSha256']: raise ValueError('Moved previous sources changed')
        os.rename(moved,bundle)
    if old is not None and current_hash!=old['catalogSha256']: replace_catalog(prior/'commands.json',data)
    elif old is None and current_hash is not None: os.rename(data/'commands.json',transaction/('rolled-back-catalog-'+uuid.uuid4().hex+'.json'))
    if pair_state(data)!=old: raise ValueError('Catalog rollback differs from the previous pair')
    record_state(transaction,record,'rolled-back')

def recover_locked(directory,data):
    results=[]
    for transaction in sorted(directory.glob('transaction-*')):
        chain(transaction); marker=transaction/'transaction.json'
        # The journal is persisted before the first destination mutation. A
        # pre-journal staging interruption therefore has no recovery action.
        if not marker.exists(): continue
        plain(marker)
        record=json.loads(marker.read_text(encoding='utf-8'))
        if record.get('format')!='xlang-catalog-publication-1' or record.get('destination')!=str(data): raise ValueError('Invalid catalog publication journal')
        if record['state'] in ('committed','rolled-back'): continue
        old,new=record['old'],record['new']
        staged=transaction/'new'; prior=transaction/'previous'; moved=transaction/'previous-moved'
        if pair_state(staged)!=new: raise ValueError('Staged catalog pair changed; publication recovery refused')
        if old is not None and pair_state(prior)!=old: raise ValueError('Previous catalog pair changed; publication recovery refused')
        if record['state']=='rolling-back':
            rollback(transaction,record,data); results.append('rolled-back'); continue
        current_catalog=file_hash(data/'commands.json'); bundle=data/'catalog-sources'
        try: current=pair_state(data)
        except (OSError,ValueError): current=None
        if current==new:
            record_state(transaction,record,'committed'); results.append('committed'); continue
        if current==old and (old is not None or (current_catalog is None and not bundle.exists())):
            record_state(transaction,record,'rolled-back'); results.append('rolled-back'); continue
        # An interrupted bundle install is safe to complete only when the catalog
        # is still the captured old version and the new bundle verifies exactly.
        expected_old=old['catalogSha256'] if old else None
        if current_catalog==expected_old and bundle.exists():
            info=verify(bundle,staged/'commands.json')
            if sha(bundle/'index.json')!=new['sourceIndexSha256']: raise ValueError('Installed source index changed; recovery refused')
            replace_catalog(staged/'commands.json',data)
            if pair_state(data)!=new: raise ValueError('Recovered catalog pair differs')
            record_state(transaction,record,'committed'); results.append('committed'); continue
        if current_catalog==expected_old and not bundle.exists() and old is not None:
            if not moved.exists() or verify(moved,prior/'commands.json')['catalogSha256']!=old['catalogSha256']: raise ValueError('Moved previous bundle is unavailable')
            if sha(moved/'index.json')!=old['sourceIndexSha256']: raise ValueError('Moved previous index changed')
            os.rename(moved,bundle)
            if pair_state(data)!=old: raise ValueError('Previous pair restoration differs')
            record_state(transaction,record,'rolled-back'); results.append('rolled-back'); continue
        raise ValueError('Destination changed outside the recorded publication; originals and journal preserved')
    return results

def recover(data,work):
    data=chain(data)
    with locked(work,data) as directory: return recover_locked(directory,data)

def publish(pair,data,work,checkpoint=None):
    pair=chain(pair); new=pair_state(pair)
    if new is None: raise ValueError('Generated pair is absent')
    data=chain(data); data.mkdir(parents=True,exist_ok=True)
    with locked(work,data) as directory:
        if os.stat(data).st_dev!=os.stat(directory).st_dev:
            raise ValueError('Catalog transaction work must be on the same filesystem as its destination')
        recover_locked(directory,data); old=pair_state(data)
        transaction=directory/('transaction-'+uuid.uuid4().hex); transaction.mkdir()
        staged=transaction/'new'; staged.mkdir()
        shutil.copy2(pair/'commands.json',staged/'commands.json'); shutil.copytree(pair/'catalog-sources',staged/'catalog-sources')
        if pair_state(staged)!=new: raise ValueError('Source pair changed while staging')
        if old is not None:
            previous=transaction/'previous'; previous.mkdir()
            shutil.copy2(data/'commands.json',previous/'commands.json'); shutil.copytree(data/'catalog-sources',previous/'catalog-sources')
            if pair_state(previous)!=old or pair_state(data)!=old: raise ValueError('Destination changed while backing up')
        record={'format':'xlang-catalog-publication-1','destination':str(data),'old':old,'new':new,'state':'prepared'}
        record_state(transaction,record,'prepared')
        if old==new:
            record['alreadyInstalled']=True; record_state(transaction,record,'committed')
            return transaction
        def step(name):
            record_state(transaction,record,name)
            if checkpoint: checkpoint(name)
        try:
            step('prepared')
            if old is not None: os.rename(data/'catalog-sources',transaction/'previous-moved')
            step('previous-bundle-moved')
            # Keep staged sources intact so an interrupted publication is recoverable.
            incoming=transaction/'incoming'; shutil.copytree(staged/'catalog-sources',incoming)
            os.rename(incoming,data/'catalog-sources'); step('new-bundle-installed')
            replace_catalog(staged/'commands.json',data); step('catalog-installed')
            if pair_state(data)!=new: raise ValueError('Published catalog pair differs')
            record_state(transaction,record,'committed')
        except BaseException:
            rollback(transaction,record,data)
            raise
        return transaction

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pair',type=Path)
    parser.add_argument('--data',type=Path,required=True)
    parser.add_argument('--work',type=Path,default=Path(__file__).resolve().parents[1]/'build/catalog-publications')
    parser.add_argument('--recover',action='store_true')
    args=parser.parse_args()
    if not args.recover and not args.pair: parser.error('--pair is required to publish')
    try:
        result=recover(args.data,args.work) if args.recover else str(publish(args.pair,args.data,args.work))
    except (OSError,ValueError,KeyError) as error: parser.exit(1,str(error)+'\n')
    print(json.dumps({'status':'recovered' if args.recover else 'committed','result':result},ensure_ascii=False))

if __name__=='__main__': main()
