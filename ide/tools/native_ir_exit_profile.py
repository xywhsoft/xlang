"""Fail-closed qualification for native exit-plan result reuse.

This is a producer-input equivalence proof for one exact compiler cohort,
not a generic read-set extractor or a final semantic/type proof.
"""
from pathlib import Path
import hashlib,json
from check_native_bridge_catalog import function
from native_ir_derived_overlay import XGE_PAIR,XGE_CONTRACT_SHA256,contract_digest
from native_c_interop_profile import pins_for,profile

FUNCTIONS = {'xl6IrRefreshExitPlans': '379f739b465a5203b91dcf26574fd0f2dac4c9938fee5b4d91d27808e65e8392', 'xl6IrExitPlanEvaluationOrigin': 'c8b82bec9107fba07e3cfd0e39155cdd9e806cf947de6ce1c55ab5d58f6c46d7', 'xl6IrNodeGet': 'a1d6375145b7123c25224c480e656eac97cf0e136d27f16c0fe07193023de520', 'xl6IrGrow': '40f4ee1a3e5ecbb52c00eb2eedf480f925bff8fba040d5039d1c6025835e1be4', 'xl6IrAppendExitCleanupActions': '4c197474fb4c5268424de74c47bc9d4c280c4ffaea73534c5ced1b5485b38e23', 'xl6IrPatternBindingScope': '0e9857e532bfee38edece27b4fe529079070984462333f54b5acbf4fa2a1ac30', 'xl6IrTypeGet': '753e997667f51bbbff554f5ed70be9664f33f94390926062ef81546c5e185dd5', 'xl6IrCleanupEnclosingCallable': '28761d28fc4a81ddb99e5b02b6555a83c973e1ba712aa0a7f7ee119b5b9b037e', 'xl6IrDeclarationBindingCount': '31e32c6dd79b425150358c3e062f243e8fb1c3fcb146e7ceda2f843d4a67cfc4', 'xl6IrPatternBindingVariant': '2838aeef7f0ba1b6565f8403f0091a12d053d3425719bda7879db8886d6db32d', 'xl6IrEnumVariantGet': '092bb33ceb5fcb1e5408d890c19a17f39c5b90a67900599dd4e6bb23f2daafc4', 'xl6IrEnumLayoutGet': '3a5d08ed093bf7b447d733fd72757595f92223665a8053b7e0ebce48ae8b44a0', 'xl6IrDeclarationIsProduct': 'e0ef40c92f038388d6e911fec98e9e903351dc80d908ba6f2478b4a0659cd7cd', 'xl6IrBuildTypeContainsTypeParam': '6398665f402bfae3ea56cd557f4f31d8aaff40abd2828f0bba887d8c8fe8d426', 'xl6IrDeclarationBindingAt': '143cc8a280ab0b4547e88e65378c3553b92317250382326f88735daffea39207', 'xl6IrNativeHandleDestroySymbol': '60802f902ca95d3e33e7ee837070be69c24870a61a08bf95e48c3a57b70cff90', 'xl6IrScopeBindingOrigin': '28f011bc9ac86f436ae7fa1d542a914098c47ff9cdfea11e2b2c2d07838b58a3', 'xl6IrExitPlanHasRoute': 'd43c6a021849d87529bdb9d4828be038e5d28d137a649a8b45d9ceb2af17b66e', 'xl6IrCleanupDeclarationIsLiveAtExit': 'acfbf8b033583aacb9089352da295e7ff32a4937818423a1bd2dc10a20f72657', 'xl6IrNodeContains': '696d368544dbe7f3321d5e1a3fdb7f2003f2e42dbbbdefd2a9fdbe5b41a2d52d', 'xl6IrNodeHasDistinctEvaluationFailure': 'a5f5e3537945369814c1814de4d9e3cefa239065a1998d7e0043ac84c3da6c63', 'xl6IrNodeRequiresThrowExitPlan': '7d992ee88a38164c31697fc061b159afc227c615c02286f60204fc8d2a2f4500', 'xl6IrValueInitializerMayRaise': 'c5de4684021bac4a94726a03c2f836d0d308be7a9bd7603b6e83c3430b04ae18', 'xl6IrNodeIsResolvedSyntaxCall': 'be380478f5504624e5a1a5833c67423005f93bdfdf51f6709d6f92c65284e465', 'xl6IrNodeIsPropertyUpdate': 'd4e0e086d0f7353194404008739e7914c450ba8187c5e98416c2cfbdedf180a1', 'xl6IrNodeHasFallibleMemberWrite': '0a9032c0f3961e98eb2ad7ff1a641a1bb4f14eee92edf5ac6d0d236c96420586', 'xl6IrNodeIsDynamicMemberRead': 'a3ecc12f284bc6d41db38f72b9072a5f264b18e2dcb2e4d69ad2365dcbbcf675', 'xl6IrDynamicMemberReadReceiver': 'd52bdef3ad136fcdc68a6e3fd63726d7bf0cdb16450326372720669ff6bcaf6c', 'xl6IrTypeIsOpenDynamicBuiltin': '23e706ce2d040abf610a6ac58c3b3278161bd738aaae32eff55531716a3de960', 'xl6IrNodeIsDynamicMemberWrite': '3d042f46bb32ee254c92e1511c09bacc40b6ef550148458880148ea8e9237049', 'xl6IrRefreshExitRoutes': '94d8a0d4e42de7c45c5ec28c10b7b64e3ee8e1245cb33378675c64516bd6e303', 'xl6IrWalkExitRoute': 'c00d763f2a525781a75214cfb619b61e9e4141cd43b0ec046fdbb1e5b7600b6e', 'xl6IrVisitExitStep': 'ca01b23a7a9d318bbb453430523f7586f641d3ac23832c36947a0a9cf9affb95', 'xl6IrCleanupDeclarationScope': '9073a9e0dcc66a49f83f1dacd96766a1feca456e3c9e786553baa24cdedd4670', 'xl6IrScopeFailureTarget': 'fde56c6793d13d80fbf6378b98c89052de5db576c7c1bc77aeda54ce6a6a3e38', 'xl6IrFunctionHasIndependentFailureBoundary': 'c90bad64283a3236bace57847c9a18f3c8c696b7cf4857c5b6a6dc3a58c6345c', 'xl6IrAppendExitStep': '3846d50a1a94a2bf4266dc69ef704a2740223da0a8ca0e26b8890bc06edacee8', 'xl6IrGeneratorClosePointForNode': '08da48d410972a1ab694e6e47993c1c5ec61a80bcd79114ce884713341c1a991', 'xl6IrGeneratorTryNeedsCloseBoundary': '1f99e9be31803dddf066ea69c0a7acc134cbdf6e0d2b17f1947a5700fac7316d', 'xl6IrGeneratorSubtreeHasYield': '7717ebf543e625efa4f3b3bc91e36ca0993277778303391876eb0ce0f8de3f9d', 'xl6IrGeneratorDeclaration': 'c8dc04784f997c4428ab5f76216a5122a4975e7d470dd419e7e2a7d9e7db6d60', 'xl6IrAppendGeneratorClosePlan': '936127336c50aaf35e2dd23ec344cffb547baa2c2b60f41021ec7b42202b823b', 'xl6IrGeneratorNodeNeedsClosePlan': 'add30c3d796ff688ae0973eb153379d9f1a318e2331c292a30ed2eb535ec6eaa', 'xl6IrNodeRequiresAnyExitPlan': 'd2d0f1e234b053fbaa86d7ef9a5d25b367e618a8714d53be015c1a35f76c1d36'}

def qualify(frontend):
    frontend=Path(frontend)
    source=frontend/'src/ir/xl6_ir.c';header=frontend/'src/ir/xl6_ir.h'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    if (sha(source),sha(header))!=XGE_PAIR:
        return dict(enabled=False,reason='Unreviewed exit producer; retain native production')
    pins=pins_for(frontend)
    if contract_digest(pins)!=XGE_CONTRACT_SHA256:
        return dict(enabled=False,reason='Unreviewed exit dependency cohort; retain native production')
    profile(frontend)
    text=source.read_text(encoding='utf8')
    for name,digest in FUNCTIONS.items():
        if hashlib.sha256(function(text,name).encode()).hexdigest()!=digest:
            raise ValueError('Exit producer function contract changed: '+name)
    return dict(enabled=True,sourceSha256=sha(source),headerSha256=sha(header),dependencyContractSha256=contract_digest(pins),functions=FUNCTIONS,
        readSet='27 integer node fields, five-way text predicate, ordered children/runtime map and seven complete value tables',
        outputs='cleanup actions, exit plans, exit steps and per-node generator close ABI',
        ownership='XRT-owned immutable asset; current module owns separate output arrays; full original verifier retained',
        limitBytes=8*1024*1024,sourceBound=True)
