"""Generate ordered node-range producers from reviewed native phase bodies.

These are actual loop slices,not full production followed by filtering.
Callers preserve native node order and run finalizers only at the last slice.
"""
import hashlib,re
from check_native_bridge_catalog import function

PHASES={
 2:('xl6IrBuildNamedCallableAbis','37b65db4fa11dfa1d2b3c18b40c514dc4d56c9b7d177e44e4f6190107921e9fe'),
 3:('xl6IrInternProjectionDomains','f51e25a40e34a55ce68514082733e406cbfec709194fa1c2797d6651ce021767'),
 5:('xl6IrBuildOwnedReadResults','6efb95d93045ef52c2f27980088ef3d1c8e54d9403e97070f628d86aedd84750'),
 18:('xl6IrBuildImplicitNameAbis','300f44c99886e2eb88bc72dc965dec6b0eeba641aef1fb9359f7840d77168457'),
 22:('xl6IrNormalizeExpressionCallKinds','21568755af5a6a1e933ae77029238b02c93598349d77652438012b2e6f38a0a2'),
 24:('xl6IrBuildIntrinsicCalls','3120cfad2beb00e1b5c5e4e067e55c85e52ced95ef1539893cb810c766668f32'),
 26:('xl6IrBuildCallSites','39432f3f294f03ecb8092654e944305e4f9c1f4c42db5b9b615cc27c578889b2'),
 27:('xl6IrBuildValueInitializers','90f68c259fdf3a0579c546e0705e054b4d7a02f821ddccd5768acdee46f2cc93'),
 28:('xl6IrBuildNativeHandleBinds','c64166bd35ea2e4f40415ca254f2724c5e3aaba40ee28daacfce0f8fc6f61b90'),
 29:('xl6IrBuildNativeHandleProjects','3254ce2deca373cd5a200bdb683cc592866456b71f6c32221a29108879d702de'),
 30:('xlsIrBuildOperatorAbisOriginal','37df25a022ef4ec2ebbc1023e2be66d1ad8405a566e668a8cd0adb31738714bc'),
 31:('xl6IrBuildCastAbis','cf0de818e2041e90c44f222a9d8a14c8f154600bc4ad32e22a5881aca5d109e6'),
 32:('xlsIrBuildMemberAbisOriginal','553651e99a200848c918039d766e1575b7a7c4792210289f1defd96622d2cc52'),
 33:('xl6IrBuildDynamicCalls','f3bedcf4f69410495f8c4e1af498f51abb0fcea26700751aa5b13067c622df7c'),
 36:('xlsIrBuildImplicitConversionAbisOriginal','63fba0c708c85c32c47bc5b0340bc6d573478924b363bb85f77b7d7282849b41'),
}

def once(source,old,new):
    if source.count(old)!=1:raise ValueError('Native range seam changed: '+old)
    return source.replace(old,new,1)

def derive(source,cases,phase_names):
    helpers=[];selected=[]
    for phase,(name,digest) in PHASES.items():
        body=function(source,name)
        if hashlib.sha256(body.encode()).hexdigest()!=digest:
            raise ValueError('Native node-range phase body changed: '+name)
        body=once(body,'static int '+name+'(XL6_IrModule* module)',
                  'static int xlsIrPipelineNodes'+str(phase)+'(XL6_IrModule* module,size_t first,size_t end)')
        # Exactly one node loop;do not interchange multiple dependent scans.
        loops=list(re.finditer(r'\bfor\s*\(',body))
        if len(loops)!=1:raise ValueError('Native range loop count changed: '+name)
        at=loops[0].start();start=body.index('(',at);depth=1;i=start+1
        while depth:
            depth+=(body[i]=='(')-(body[i]==')');i+=1
        header=body[at:i]
        if phase==30:
            expected='for ( i = xlsIrDerivedNext(module,XLS_IR_DERIVED_OPERATOR,0); i < module->nodeCount && (XLS_IR_DERIVED_BODY_HIT(XLS_IR_DERIVED_OPERATOR),1); i = xlsIrDerivedNext(module,XLS_IR_DERIVED_OPERATOR,i+1) )'
            if header!=expected:raise ValueError('Native operator enumeration changed')
        elif not re.fullmatch(r'for\s*\(\s*(?:size_t\s+)?i\s*=\s*0\s*;\s*i\s*<\s*module->nodeCount\s*;\s*\+\+i\s*\)',header):
            raise ValueError('Native range enumeration changed: '+name)
        declaration='size_t 'if phase==3 else''
        replacement='for ('+declaration+'i=first;i<end && (XLS_IR_PIPELINE_NODE_VISIT('+str(phase)+',i),1);++i)'
        body=once(body,header,replacement)
        if phase==27:body=once(body,'module->valueInitializerCount = 0;','if(!first)module->valueInitializerCount = 0;')
        for tail in('return xl6IrBuildCallSiteContracts(module);','return xl6IrBuildOwnedReadResults(module);'):
            if tail in body:body=once(body,tail,'if(end==module->nodeCount)'+tail+'\n    return 1;')
        helpers.append(body)
        selected.append(once(cases[phase],phase_names[phase]+'(module)',
                             'xlsIrPipelineNodes'+str(phase)+'(module,first,end)'))
    support='static int xlsIrPipelineNodeRanges(size_t phase)\n{switch(phase){'+''.join('case '+str(p)+':'for p in PHASES)+'return 1;default:return 0;}}\n'
    dispatch='''static int xlsIrPipelinePhaseNodesOriginal(XL6_IrModule* module,XL6_DiagList* diags,
 size_t phase,size_t first,size_t end)
{
    const char* buildFailure=NULL;
    switch(phase){
'''+''.join(selected)+'''    default:return 0;
    }
    if(buildFailure!=NULL){xl6IrDiag(diags,NULL,buildFailure);return 0;}
    return 1;
}
'''
    code='''#ifndef XLS_IR_PIPELINE_NODE_VISIT
#define XLS_IR_PIPELINE_NODE_VISIT(phase,node) ((void)0)
#endif
'''+support+'\n'.join(helpers)+'\n'+dispatch
    return code,dict(phases=list(PHASES),functions={n:h for n,h in PHASES.values()},scope='Ordered native node-range production;no reuse eligibility or skipped input')
