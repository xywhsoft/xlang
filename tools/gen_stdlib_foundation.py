"""Exact, declarative OS queries and opt-in memory diagnostics.

Algorithms and process-wide counters stay in XRT. This table emits declarations,
closed ABI/effects and component dependencies, never a Cgen implementation.
"""
from pathlib import Path
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]
STATS = ['MallocCalls','MallocBytes','CallocCalls','CallocBytes','ReallocCalls',
 'ReallocBytes','MemDupCalls','MemDupBytes','FreeCalls','TempCalls','TempBytes',
 'BlockAllocCalls','BlockAllocBytes','BlockFreeCalls','BlockFreeBytes',
 'PooledAllocCalls','PooledAllocBytes','DirectAllocCalls','DirectAllocBytes',
 'BackingAllocCalls','BackingAllocBytes','BackingReallocCalls',
 'BackingReallocBytes','BackingFreeCalls']
DEBUG = ['LiveCount','LiveBytes','PeakCount','PeakBytes','QuarantineCount',
 'QuarantineBytes','AllocCount','FreeCount','ReallocCount','DoubleFreeCount',
 'InvalidFreeCount','OverflowCount','UnderflowCount','UseAfterFreeCount',
 'TempCurrentBytes','TempPeakBytes','TempResetCount','EventCount']

def lower(name): return name[0].lower()+name[1:]
def cstring(name, source, array=False):
    declaration='static const char '+name+'[]' if array else 'const char* '+name
    return declaration+'=\n'+''.join('    '+json.dumps(line+'\n',ensure_ascii=False)+'\n' for line in source.splitlines())+';\n'

def effects(pure=False,no_raise=False):
    return ('@pure ' if pure else '')+('@no_raise ' if no_raise else '')+'@nonblocking '

def outputs():
    files={}; calls=[]; classes=[]; functions=[]
    def call(symbol,result,params=(),owner=None,component='SYSTEM_INFO',pure=False,no_raise=False,policy=None):
        row=dict(symbol=symbol,result=result,params=list(params),owner=owner,component=component,
            pure=pure,noRaise=no_raise,policy=policy)
        calls.append(row); return row
    def function(ns,name,result,symbol,params=(),**kw):
        internal='__xl_std_'+ns+'_'+name
        call(symbol,result,[t for t,n in params],**kw)
        source='extern "'+symbol+'" '+effects(kw.get('pure',False),kw.get('no_raise',False))+'func '+internal+'('+', '.join(t+' '+n for t,n in params)+')'+(' as '+result if result!='void' else '')+'\n'
        key='xl6StdFoundation_'+ns+'_'+name
        functions.append((ns,name,internal,result,key,source))
    for name,result in [('platform','string'),('architecture','string'),('pointerBits','int'),('littleEndian','bool'),('logicalProcessorCount','int')]:
        function('os',name,result,'xl6System'+name[0].upper()+name[1:]+'V1',pure=name!='logicalProcessorCount',no_raise=name in ('pointerBits','littleEndian'))
    function('memory','statsEnabled','bool','xl6MemoryStatisticsEnabledV1',component='MEMORY_STATS',no_raise=True)
    function('memory','enableStats','void','xl6MemoryStatsEnableV1',[('bool','enabled')],component='MEMORY_STATS',no_raise=True)
    function('memory','resetStats','void','xl6MemoryStatsResetV1',component='MEMORY_STATS',no_raise=True)
    function('memory','stats','MemoryStats','xl6MemoryStatsCaptureV1',component='MEMORY_STATS')
    function('memory','debugEnabled','bool','xl6MemoryDebugEnabledV1',component='MEMORY_DIAGNOSTICS',no_raise=True)
    function('memory','debugSnapshot','MemoryDebugSnapshot','xl6MemoryDebugCaptureV1',component='MEMORY_DIAGNOSTICS')
    for name in ('reportText','reportJson'):
        function('memory',name,'string','xl6Memory'+name[0].upper()+name[1:]+'V1',[('int','limit')],component='MEMORY_REPORT')
        ns,n,internal,result,key,source=functions[-1]
        functions[-1]=(ns,n,internal,result,key,source.replace('int limit','int limit = 16777216'))
    for name,comp,fields in [('MemoryStats','MEMORY_STATS',STATS),('MemoryDebugSnapshot','MEMORY_DIAGNOSTICS',DEBUG)]:
        destroy='xl6'+name+'DestroyV1'; policy='xl6'+name+'HandlePolicyV1Get'
        text='class '+name+'\n    private native point handle\n    extern "'+destroy+'" '+effects(no_raise=True)+'destroy()\n'
        call(destroy,'void',[name],owner=name,component=comp,no_raise=True,policy=policy)
        for field,t in [('Enabled','bool')]+([(f,'uint64') for f in ['ClassStep','ClassCutoff','ClassCount']+fields] if name=='MemoryStats' else [(f,'uint64') for f in fields]):
            sym='xl6'+name+field+'V1'; text+='    extern "'+sym+'" '+effects(pure=True,no_raise=True)+'get '+lower(field)+'() as '+t+'\n'
            call(sym,t,[name],owner=name,component=comp,pure=True,no_raise=True)
        if name=='MemoryStats':
            for method in ('classCalls','classBytes'):
                sym='xl6MemoryStats'+method[0].upper()+method[1:]+'V1'
                text+='    extern "'+sym+'" '+effects(pure=True)+'func '+method+'(int index) as uint64\n'
                call(sym,'uint64',[name,'int'],owner=name,component=comp,pure=True)
        classes.append(text+'end\n')
    pool_owner='__ObjectPoolControl'
    for method,result,params in [('Create',pool_owner,()),('Destroy','void',(pool_owner,)),
            ('Check','void',(pool_owner,)),('Enter','void',(pool_owner,)),('Leave','void',(pool_owner,)),
            ('Close','void',(pool_owner,)),('Idle','void',(pool_owner,)),('Closed','bool',(pool_owner,))]:
        call('xl6ObjectPoolControl'+method+'V1',result,params,owner=pool_owner if params else None,
            component='OBJECT_POOL_CONTROL',no_raise=method in ('Destroy','Leave'),
            policy='xl6ObjectPoolControlHandlePolicyV1Get' if method=='Destroy' else None)
    types={'void':('void','XL6_IR_C_ABI_VOID'),'int':('int64_t','I'),'uint64':('uint64_t','U'),
        'bool':('bool','B'),'string':('xstrview','S')}
    owners={n:'xl6'+n+'DestroyV1' for n in ('MemoryStats','MemoryDebugSnapshot')}
    owners[pool_owner]='xl6ObjectPoolControlDestroyV1'
    def ct(t):return types[t][0] if t in types else 'void*'
    def abi(t):return types[t][1] if t in types else 'V'
    header=['#ifndef XL6_FOUNDATION_ABI_H\n#define XL6_FOUNDATION_ABI_H\n#include "xl6_runtime_export.h"\n#include "xl6_native_handle.h"\n#include <stdint.h>\n#include <stdbool.h>\n']
    contracts=[];symbols=[]
    for c in calls:
        header.append('XL6_TEXT_API '+ct(c['result'])+' '+c['symbol']+'('+(','.join(ct(t) for t in c['params']) or 'void')+');\n')
        attrs=['.nonblocking=1']
        if c['pure']:attrs.append('.pure=1')
        if c['noRaise']:attrs.append('.noRaise=1')
        if c['result'] in owners:attrs.append('.resultOwner="'+owners[c['result']]+'"')
        if c['owner']:attrs.append('.parameterOwners={"'+owners[c['owner']]+'"}')
        if c['policy']:attrs.append('.nativeHandlePolicy="'+c['policy']+'"')
        contracts.append('{"'+c['symbol']+'",T,'+abi(c['result'])+','+str(len(c['params']))+',{'+(','.join(abi(t) for t in c['params']) or '0')+'},'+','.join(attrs)+'},\n')
        symbols.append('XL6_TEXT_EXPORT('+c['symbol']+','+c['component']+')\n')
    header.extend('XL6_TEXT_API const XL6_NativeHandlePolicyV1* '+owner.replace('DestroyV1','HandlePolicyV1Get')+'(void);\n' for owner in owners.values())
    header.append('#endif\n')
    files['src/runtime/xl6_foundation_abi.h']=''.join(header)
    files['src/runtime/xl6_foundation_symbols.inc']=''.join(symbols)
    files['src/ir/xl6_ir_foundation_contracts.inc']=''.join(contracts)
    files['src/stdlib/xl6_stdlib_foundation.inc']=''.join(cstring(key,src,True) for ns,n,internal,r,key,src in functions)+cstring('xl6StdlibMemorySource',''.join(classes))+cstring('xl6StdlibObjectPoolsSource',(ROOT/'src/stdlib/object_pools.xl.in').read_text(encoding='utf-8'))
    files['src/runtime/xl6_memory_stats_fields.inc']=''.join('XL6_MEMORY_STAT('+f+')\n' for f in STATS)
    files['src/runtime/xl6_memory_debug_fields.inc']=''.join('XL6_MEMORY_DEBUG('+f+')\n' for f in DEBUG)
    files['docs/foundation-language-api.json']=json.dumps(dict(schema=1,calls=calls,functions=[dict(namespace=ns,name=n,result=r) for ns,n,internal,r,key,src in functions]),indent=2)+'\n'
    return files,functions

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
    files,functions=outputs();stale=[]
    for name,text in files.items():
        path=ROOT/name
        if path.is_file() and path.read_text(encoding='utf-8-sig')==text:continue
        if a.check:stale.append(name)
        else:path.write_text(text,encoding='utf-8',newline='\n')
    if stale:raise SystemExit('Stale foundation metadata: '+', '.join(stale))
    print('Foundation: '+str(len(json.loads(files['docs/foundation-language-api.json'])['calls']))+' resident declarations')
if __name__=='__main__':main()
