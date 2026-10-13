"""Validate complete six-fixture hosted acceptance, not full53 completion.
Reject incomplete mode matrices, changed products/fixtures, failed phases and
invented lifecycle totals. Actual execution evidence is supplied by the two
real shared-product gates, not synthesized by this aggregator.
"""
from pathlib import Path
import argparse,copy,hashlib,itertools,json

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

FAMILIES={'stack':('stdlib_stack_ownership',1,25),
    'tree':('stdlib_tree_ownership',5,12),'queue':('stdlib_concurrent_queue_ownership',6,25)}
CLEAR=('stdlib_stack_clear_error','stdlib_stack_clear_error_continuation')

def validate(graph,clear,products,fixtures):
    for report in (graph,clear):
        assert report['Complete'] and report['InputsUnchanged'] and not report['Failures']
        assert not report['Full53Acceptance'] and report['Rows'] and all(r['Exit']==0 for r in report['Rows'])
        for name,digest in products.items():
            matched=[r for r in report['Inputs'] if Path(r['Path']).name==name]
            assert matched and all(r['Sha256'].lower()==digest for r in matched),name
    assert graph['EmbeddedVfsJitWithoutFallback'] and not graph['ProviderOverrides']
    assert not graph.get('ExplicitSubset',False) and not graph['RuntimeInstrumented']
    assert clear['EmbeddedVfsWithoutFallback'] and not clear['RuntimeInstrumented']
    names=set(fixtures);covered={};measured=0
    for label,(stem,count,lifetimes) in FAMILIES.items():
        rows=[p for p in graph['Proofs'] if p['Case']==label]
        expected={(product,opt,mode,family) for product in products for opt in ('O0','O2')
            for mode,family in [(0,0)]+list(itertools.product((1,2,3),range(count)))}
        actual=[(p['Product'],p['Optimization'],p['Mode'],p['Family']) for p in rows]
        assert len(actual)==len(set(actual)) and set(actual)==expected,label
        assert all(p['MeasuredLifetimes']==lifetimes for p in rows)
        measured+=len(rows)*lifetimes;covered[stem]=dict(Modes=len(rows),MeasuredLifecycles=len(rows)*lifetimes)
    stem='stdlib_concurrent_queue_continuation';rows=[p for p in graph['Proofs'] if p['Case']=='continuation']
    actual=[(p['Product'],p['Optimization'],p['Terminal'],p['Timing']) for p in rows]
    expected=set(itertools.product(products,('O0','O2'),('success','reject','cancel','close'),('ready','pending-watch','pending-commit')))
    assert len(actual)==len(set(actual)) and set(actual)==expected
    assert all(p['Activations']==2 and p['FinallyCallsPerActivation']==1 for p in rows)
    measured+=2*len(rows);covered[stem]=dict(Modes=len(rows),MeasuredLifecycles=2*len(rows))
    assert len(graph['Proofs'])==204
    for stem in CLEAR:
        rows=[p for p in clear['Proofs'] if p['Case']==stem and 'Product' in p]
        actual=[(p['Product'],p['Optimization']) for p in rows]
        expected=set(itertools.product(products,('O0','O2')))
        assert len(actual)==len(set(actual)) and set(actual)==expected
        assert all(p['Lifecycles']==25 for p in rows)
        aot=[p for p in clear['Proofs'] if p['Case']==stem and p.get('IndependentAot')]
        assert len(aot)==1
        measured+=25*len(rows);covered[stem]=dict(Modes=len(rows),MeasuredLifecycles=25*len(rows),IndependentAot=True)
    assert len(clear['Proofs'])==10 and set(covered)==names and measured==3364
    for stem,item in covered.items():
        report=clear if stem in CLEAR else graph
        matched=[r for r in report['Inputs'] if Path(r['Path']).name==stem+'.xl']
        assert matched and all(r['Sha256'].lower()==fixtures[stem] for r in matched),stem
        item['FixtureSha256']=fixtures[stem]
    return covered,measured

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build-report','graph-report','clear-report','out'):parser.add_argument('--'+n,type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists()
    pinned_reports=[dict(Path=str(p.resolve()),Sha256=sha(p)) for p in
        (Path(__file__),a.build_report,a.graph_report,a.clear_report)]
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'))
    graph=json.loads(a.graph_report.read_text(encoding='utf-8-sig'));clear=json.loads(a.clear_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    products={Path(r['Path']).name:r['Sha256'].lower() for r in build['Products']
        if Path(r['Path']).suffix in ('.dll','.so')};assert len(products)==2
    stems={x[0] for x in FAMILIES.values()}|set(CLEAR)|{'stdlib_concurrent_queue_continuation'}
    fixtures={n:sha(root/'test/cases'/(n+'.xl')) for n in stems}
    covered,measured=validate(graph,clear,products,fixtures)
    for report in (build,graph,clear):
        for row in report['Inputs']+report.get('Products',[]):assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    controls=[]
    for kind in ('missing-pending-commit','wrong-fixture','short-lifecycle','wrong-product','failed-phase'):
        bad_graph,bad_clear=copy.deepcopy(graph),copy.deepcopy(clear)
        if kind=='missing-pending-commit':
            bad_graph['Proofs'].remove(next(p for p in bad_graph['Proofs'] if p.get('Timing')=='pending-commit'))
        elif kind=='wrong-fixture':
            next(r for r in bad_graph['Inputs'] if Path(r['Path']).name=='stdlib_stack_ownership.xl')['Sha256']='0'*64
        elif kind=='short-lifecycle':next(p for p in bad_clear['Proofs'] if 'Lifecycles' in p)['Lifecycles']=24
        elif kind=='wrong-product':
            next(r for r in bad_graph['Inputs'] if Path(r['Path']).name in products)['Sha256']='0'*64
        else:bad_clear['Rows'][0]['Exit']=1
        try:validate(bad_graph,bad_clear,products,fixtures)
        except AssertionError:controls.append(kind)
        else:raise AssertionError('Corrupt hosted proof accepted: '+kind)
    inputs=pinned_reports
    inputs+= [dict(Path=str(root/'test/cases'/(n+'.xl')),Sha256=s) for n,s in sorted(fixtures.items())]
    assert all(sha(r['Path'])==r['Sha256'] for r in inputs),'Hosted acceptance inputs changed'
    result=dict(Complete=True,InputsUnchanged=True,Inputs=inputs,CoveredFixtures=covered,
        MeasuredLifecycles=measured,NegativeControls=controls,Full53Acceptance=False,
        WorkerOomAcceptance=False,ModuleUnmapAcceptance=False,Scope=__doc__)
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Hosted coverage: 6 fixtures, 3364 measured lifecycles, 5 corrupt-report refusals',flush=True)

if __name__=='__main__':main()
