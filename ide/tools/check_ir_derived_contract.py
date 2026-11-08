"""Reject future/mixed dependencies before deriving the current pure pass pair."""
from pathlib import Path
import argparse,json
from unittest.mock import patch
from native_final_c_interop_profile import CURRENT_PINS
from native_ir_derived_overlay import current_contract,derive,CURRENT_PAIR


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);cases=[]
    assert current_contract(CURRENT_PINS) and current_contract(dict(reversed(list(CURRENT_PINS.items()))));cases.append('known-complete-contract-independent-of-key-order')
    for key in CURRENT_PINS:
        altered={**CURRENT_PINS,key:'0'*64};assert not current_contract(altered),key
    cases.append('reject-each-of-457-mutated-dependencies')
    altered=dict(CURRENT_PINS);del altered[next(iter(altered))];assert not current_contract(altered);assert not current_contract({**CURRENT_PINS,'unreviewed.inc':'0'*64});cases.append('reject-missing-and-extra-dependencies')
    text='source not yet derived';original,meta=derive(text,out,*CURRENT_PAIR);assert original==text and not meta['enabled'];cases.append('same-ir-pair-without-complete-cohort-retains-original')
    with patch('native_c_interop_profile.pins_for',return_value={**CURRENT_PINS,'src/package/xl6_package_native_link.inc':'0'*64}):
        original,meta=derive(text,out,*CURRENT_PAIR,frontend=out);assert original==text and not meta['enabled']
    cases.append('same-ir-pair-with-new-role-contract-retains-original')
    original,meta=derive(text,out,'0'*64,CURRENT_PAIR[1],frontend=out);assert original==text and not meta['enabled'];cases.append('unknown-source-pair-retains-original')
    assert not (out/'frontend').exists();(out/'result.json').write_bytes((json.dumps({'status':'passed','cases':cases,'individualMutations':len(CURRENT_PINS)},indent=2)+'\n').encode());print(json.dumps({'status':'passed','cases':len(cases),'individualMutations':len(CURRENT_PINS)}))


if __name__=='__main__':main()
