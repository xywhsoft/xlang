"""Static ownership checks only; does not execute any external-product API."""
from pathlib import Path
import argparse,hashlib,json,tempfile
import gen_xrt_xlang_matrix as matrix

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();assert not a.out.exists()
    checks=[]
    def rejected(name,reason,operation):
        try:operation()
        except ValueError as error:
            assert reason in str(error),(name,str(error));checks.append(name);return
        raise AssertionError(name+' unexpectedly accepted')
    with tempfile.TemporaryDirectory(prefix='xl6-external-api-matrix-') as directory:
        fixture=Path(directory);contract=matrix.EXTERNAL_API_PRODUCTS['xacme']
        declaration=fixture/contract['declarations'];declaration.parent.mkdir(parents=True)
        provider=fixture/contract['provider'];provider.parent.mkdir(parents=True)
        provider.write_text('/* Static fixture, never compiled or executed. */\n')
        consumer=fixture/contract['consumers'][0];consumer.parent.mkdir(parents=True)
        consumer.write_text('void f(void) { xrtAcmeKnown(); }\n')
        runtime=fixture/'release/include/xl_runtime.h';runtime.parent.mkdir(parents=True);runtime.write_text('')
        section='/* public: extlibs/xacme/include/xrt/acme.h */\n'
        prototype='XRT_API bool xrtAcmeKnown(void);\n'
        marker='#if defined(XACME_IMPLEMENTATION) && !defined(XACME_IMPLEMENTATION_ONCE)\n'
        declaration.write_text(section+prototype+marker)
        catalog=matrix._external_api_catalog(fixture,set())
        assert set(catalog)=={'xrtAcmeKnown'};checks.append('exact-public-declaration')
        refs,missing=matrix._source_references(fixture,set(),set(catalog))
        pending,external=matrix._partition_external_references(missing,catalog)
        assert not refs and not pending and len(external)==1;checks.append('exact-direct-consumer')
        consumer.write_text('typedef bool(*Callback)(void); static Callback ops[]={xrtAcmeKnown};\n')
        refs,missing=matrix._source_references(fixture,set(),set(catalog))
        assert not refs and set(missing)=={'xrtAcmeKnown'};checks.append('operation-table-address')
        rejected('wrong-consumer','consumer mismatch',lambda:matrix._partition_external_references(
            {'xrtAcmeKnown':['src/runtime/other.c']},catalog))
        pending,external=matrix._partition_external_references({'xrtAcmeTypo':[contract['consumers'][0]]},catalog)
        assert set(pending)=={'xrtAcmeTypo'} and not external
        rejected('unknown-prefix-is-not-permission','unclassified API reference',lambda:matrix._deferred_product('xrtAcmeTypo'))
        rejected('core-ownership-collision','ownership collision',lambda:matrix._external_api_catalog(fixture,{'xrtAcmeKnown'}))
        declaration.write_text(section+prototype+prototype+marker)
        rejected('core-parser-stays-strict','duplicate public API',lambda:matrix._api_headers(declaration))
        assert len(matrix._external_api_catalog(fixture,set()))==1;checks.append('identical-C-redeclaration')
        declaration.write_text(section+prototype+'XRT_API int xrtAcmeKnown(void);\n'+marker)
        rejected('conflicting-C-redeclaration','duplicate public API',lambda:matrix._external_api_catalog(fixture,set()))
        declaration.write_text(section+prototype+'/* public: extlibs/xacme/include/xrt/other.h */\n'+prototype+marker)
        rejected('conflicting-origin','duplicate public API',lambda:matrix._external_api_catalog(fixture,set()))
        declaration.write_text(section+prototype+marker);provider.unlink()
        rejected('missing-provider','is missing',lambda:matrix._external_api_catalog(fixture,set()))
        provider.write_text('/* Static fixture. */\n');declaration.unlink()
        rejected('missing-public-declaration','is missing',lambda:matrix._external_api_catalog(fixture,set()))
        consumer.unlink();assert not matrix._external_api_catalog(fixture,set());checks.append('inactive-product-is-not-core')
    current,report=matrix.check(root);assert current
    names={row['name'] for row in report['apis']};external=report['externalProductReferences']
    assert external and not names.intersection(row['name'] for row in external)
    assert len(names)==report['runtimeApiCount'] and report['deferredApiCount']==0
    assert all(row['product']=='xacme' and row['references']==['src/runtime/xl6_xacme.c'] for row in external)
    checks.append('actual-core-and-external-counts-separated')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),Path(matrix.__file__).resolve(),root/'docs/XRT与xlang能力矩阵.md',
        root/'tools/xrt_xlang_deferred_apis.txt',root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',
        *[root/value for value in (matrix.EXTERNAL_API_PRODUCTS['xacme']['declarations'],
            matrix.EXTERNAL_API_PRODUCTS['xacme']['provider'],*matrix.EXTERNAL_API_PRODUCTS['xacme']['consumers'])]]
    result=dict(Complete=True,Checks=checks,Inputs={str(p):sha(p) for p in files},
        CoreAPIs=report['coreApiCount'],RuntimeAPIs=report['runtimeApiCount'],
        ExternalReferences=len(external),DeferredAPIs=0,ExecutedExternalAPIs=False,Scope=__doc__)
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in result.items() if k!='Inputs'}),flush=True)
    return 0

if __name__=='__main__':raise SystemExit(main())
