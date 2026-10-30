"""Compare the compact bridge's real lookup/policy with a verified reference.

This supplements the upstream all-field catalog checker. Both frontend roots
must match complete audited cohorts; no engine or installed binary is edited.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
from build_lsp_core import dependencies
from native_current_ir_overlay import derive
from native_current_ir_profile import COMPACT_BRIDGE_PINS, PROFILES


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def block(text,start):
    lexer=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]',re.S)
    brace=text.index('{',start);depth=0
    for match in lexer.finditer(text,brace):
        if match.group()=='{':depth+=1
        elif match.group()=='}':
            depth-=1
            if not depth:return text[start:match.end()]
    raise ValueError('Unclosed audited C declaration')


def function(text,name):
    pattern=r'^(?:static\s+)?(?:const\s+)?[A-Za-z_]\w*\s*\*?\s+'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{'
    matches=list(re.finditer(pattern,text,re.M))
    if len(matches)!=1:raise ValueError('Expected one original bridge function: '+name)
    return block(text,matches[0].start())


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend-root',type=Path,required=True)
    parser.add_argument('--reference-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cc',default=os.environ.get('CC','gcc'))
    args=parser.parse_args();frontend=args.frontend_root.resolve(strict=True);reference=args.reference_root.resolve(strict=True)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    report=dict(status='running',accepted=False,inputs={},outputs={},commands=[])
    def publish():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    env=dict(os.environ)
    if Path(args.cc).is_file():env['PATH']=str(Path(args.cc).resolve().parent)+os.pathsep+env.get('PATH','')
    def run(command,name):
        report['commands'].append(command);publish()
        result=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=60)
        (out/(name+'.log')).write_bytes(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(name+' failed: '+str(result.returncode))
        return result
    try:
        relative='src/ir/xl6_ir_direct_bridge.c'
        if sha(frontend/relative)!=COMPACT_BRIDGE_PINS[relative]:raise ValueError('Expected audited compact bridge')
        if sha(reference/relative)!=PROFILES['current-2084f8ac'][relative]:raise ValueError('Expected audited pre-compact reference')
        for name,root in [('current',frontend),('reference',reference)]:
            _,_,overlay=derive(root,out/name,phases=True)
            report['inputs'].update(overlay['auditedDependencies'])
        old=(reference/relative).read_text('utf8');current=(frontend/relative).read_text('utf8')
        pieces=[block(old,old.index('typedef struct XL6_IrDirectBridgeContract'))+' XL6_IrDirectBridgeContract;',
                block(old,old.index('static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[]'))+';']
        pieces += [function(old,name) for name in ['xl6IrDirectBridgeContract','xl6IrDirectBridgeContractSpelling','xl6IrRuntimeNativeHandlePolicyForName']]
        body='\n'.join(pieces)
        for before,after in [('xl6IrRuntimeNativeHandlePolicyForName','oldPolicy'),('xl6IrDirectBridgeContractSpelling','oldSpelling'),
                             ('xl6IrDirectBridgeContracts','oldContracts'),('xl6IrDirectBridgeContract','oldLookup'),('XL6_IrDirectBridgeContract','OldContract')]:
            body=body.replace(before,after)
        source=out/'bridge_catalog.c';fixture=Path(__file__).resolve().parents[1]/'tests/analysis_bridge_catalog_test.c'
        source.write_text('#include "src/ir/xl6_ir.h"\n#include <string.h>\n'+
            ''.join('#include "src/ir/'+name+'"\n' for name in ['xl6_ir_direct_bridge_types.h','xl6_ir_direct_bridge_enums.inc','xl6_ir_direct_bridge_data.inc','xl6_ir_direct_bridge_lookup.inc'])+
            body+'\n'+function(current,'xl6IrRuntimeNativeHandlePolicyForName')+'\n#include "'+fixture.as_posix()+'"\n',encoding='utf8',newline='\n')
        flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers']
        flags+=['-I'+str(frontend/p) for p in ['','src','src/ir','release','release/include']]
        dep=run([args.cc,*flags,'-MM','-MT','xlsObject',str(source)],'dependencies')
        report['inputs'].update({str(p):sha(p) for p in dependencies(dep.stdout.decode('utf8'))})
        executable=out/('bridge-catalog.exe' if os.name=='nt' else 'bridge-catalog')
        run([args.cc,*flags,str(source),'-o',str(executable)],'compile')
        result=run([str(executable)],'run');match=re.search(rb'analysis_bridge_catalog_test passed: (\d+) checks;',result.stdout)
        if not match:raise RuntimeError('Missing bridge comparison completion receipt')
        report['checks']=int(match[1]);report['outputs'][executable.name]=sha(executable)
        report['inputs'][str(Path(__file__).resolve())]=sha(__file__)
        for path,digest in report['inputs'].items():
            if sha(path)!=digest:raise RuntimeError('Input changed during bridge check: '+path)
        report['status']='passed';print(json.dumps(dict(status='passed',checks=report['checks'],proof=str(out/'result.json'))))
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:publish()


if __name__=='__main__':main()
