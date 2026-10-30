"""Offline catalog regression: completeness, negative controls, C reader and publication recovery."""
from pathlib import Path
import argparse,copy,json,os,subprocess,sys
from catalog_inventory import validate
from check_catalog_coverage import check
from catalog_sources import sha
from catalog_publish import publish,pair_state

IDE=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,default=IDE.parent);ap.add_argument('--pair',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--cc',default='gcc');a=ap.parse_args()
    root=a.root.resolve();pair=a.pair.resolve();out=a.output.resolve()
    if out.exists():ap.error('Use a fresh output directory')
    out.mkdir(parents=True);data=json.loads((pair/'commands.json').read_text(encoding='utf-8'));rows=[]
    report={'status':'running','scope':'offline catalog data and native reader; no GUI/LS rebuild or admission','coverage':check(root,pair/'commands.json'),'checks':rows}
    def save():(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    def reject(label,change):
        broken=copy.deepcopy(data);change(broken)
        try:validate(broken,data['apiInventory'])
        except ValueError:rows.append({'case':label,'rejected':True});return
        raise AssertionError('Invalid catalog accepted: '+label)
    env=dict(os.environ,PATH=str(root/'release')+os.pathsep+os.environ.get('PATH',''),PYTHONUTF8='1')
    def run(label,command):
        log=out/(label+'.log')
        with log.open('wb') as stream:result=subprocess.run(list(map(str,command)),cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300)
        rows.append({'case':label,'exit':result.returncode,'logSha256':sha(log)});save()
        if result.returncode:raise ValueError('Catalog regression failed: '+str(log))
    save()
    try:
        for name in ['http1.requestHead','WsStream','crc32.checksum','compress.gzip','WsStream.close']:
            reject('missing-'+name,lambda d,n=name:d.update(entries=[r for r in d['entries'] if r['name']!=n]))
        reject('obsolete-member',lambda d:(d.pop('publicApiSha256'),d['entries'].append(dict(next(r for r in d['entries'] if r['name']=='WsStream.close'),name='WsStream.obsolete'))))
        reject('wrong-default',lambda d:next(r for r in d['entries'] if r['name']=='log.customSink')['parameters'][3].update(optional=False))
        reject('native-storage-visible',lambda d:d['entries'].append(dict(next(r for r in d['entries'] if r['name']=='WsStream'),name='WsStream.handle',kind='method',signature='WsStream.handle as point')))
        if os.stat(out).st_dev!=os.stat(IDE/'data').st_dev:
            prior=pair_state(IDE/'data')
            try:publish(pair,IDE/'data',out/'cross-device-work')
            except ValueError as error:
                if 'same filesystem' not in str(error):raise
            else:raise AssertionError('Cross-device atomic publication was accepted')
            assert pair_state(IDE/'data')==prior
            rows.append({'case':'cross-device-refused-before-mutation','rejected':True})
        host=out/'reader_host.c';host.write_text('#include "xl.h"\n#define main catalog_main\n#include "ide/tests/catalog_test.c"\n#undef main\nint main(int argc,char** argv){if(!xlRuntimeInit())return 3;int r=catalog_main(argc,argv);xlRuntimeUnit();return r;}\n',encoding='utf-8')
        binary=out/'reader_test.exe'
        sources=['ide/src/services/command_catalog.c','ide/src/services/catalog_sources.c','ide/src/platform/path.c','ide/src/platform/file_io.c','ide/src/platform/text_file.c']
        run('native-reader-build',[a.cc,'-std=c11','-O2','-DXLS_COMPILER_XRT','-DXRT_MODULE_ALL','-I.','-Iide/src','-Ilib/xrt','-Irelease/include',host,*sources,'release/xl.dll','-lbcrypt','-lole32','-o',binary])
        run('native-reader',[binary,out,pair/'commands.json'])
        repeat=out/'repeat';repeat.mkdir()
        generation=json.loads((pair/'generation-report.json').read_text(encoding='utf-8'));snapshot=Path(generation['snapshot'])
        run('deterministic-regeneration',[sys.executable,'-X','utf8',IDE/'tools/generate_catalog.py','--root',snapshot,'--raw',pair/'scratch/stdlib.raw.json','--compiler',snapshot/'release/xl.exe','--output',repeat/'commands.json','--source-output',repeat/'catalog-sources','--scratch',repeat/'asts'])
        if sha(repeat/'commands.json')!=sha(pair/'commands.json'):raise ValueError('Catalog generation is not deterministic')
        run('publication-recovery',[sys.executable,'-X','utf8',IDE/'tools/check_catalog_publication.py','--old',IDE/'data','--new',pair,'--output',out/'publication'])
        report['status']='passed'
    finally:save()
    print(json.dumps({'status':report['status'],'checks':len(rows),'directory':str(out)}))
if __name__=='__main__':main()
