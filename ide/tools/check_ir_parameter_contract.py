"""Reject unknown native parameter-verifier cohorts before changing enumeration."""
from pathlib import Path
import argparse,json
from unittest.mock import patch
from native_final_c_interop_profile import CURRENT_PINS
from native_ir_derived_overlay import CURRENT_PAIR
from native_ir_parameter_overlay import derive


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);text='untouched source';cases=[]
 for name in CURRENT_PINS:
  with patch('native_ir_parameter_overlay.pins_for',return_value={**CURRENT_PINS,name:'0'*64}):
   source,meta=derive(text,out,*CURRENT_PAIR);assert source==text and not meta['enabled'],name
 cases.append('all-457-single-dependency-changes-retain-original')
 for pins in ({k:v for k,v in CURRENT_PINS.items() if k!=next(iter(CURRENT_PINS))},{**CURRENT_PINS,'unreviewed.inc':'0'*64}):
  with patch('native_ir_parameter_overlay.pins_for',return_value=pins):
   source,meta=derive(text,out,*CURRENT_PAIR);assert source==text and not meta['enabled']
 cases.append('missing-or-extra-dependency-retains-original')
 with patch('native_ir_parameter_overlay.pins_for',return_value=CURRENT_PINS):
  source,meta=derive(text,out,'0'*64,CURRENT_PAIR[1]);assert source==text and not meta['enabled']
  path=out/'src/ir/xl6_ir_foreign_parameter.c';path.parent.mkdir(parents=True);path.write_text('changed included native verifier',encoding='utf-8')
  try:derive(text,out,*CURRENT_PAIR)
  except ValueError as error:assert 'verifier changed' in str(error)
  else:raise AssertionError('Changed actual included C source was accepted')
 cases+=['unknown-source-pair-retains-original','changed-actual-helper-bytes-rejected']
 (out/'result.json').write_bytes((json.dumps(dict(status='passed',cases=cases,individualMutations=len(CURRENT_PINS)),indent=2)+'\n').encode());print(json.dumps(dict(status='passed',cases=len(cases),individualMutations=len(CURRENT_PINS))))


if __name__=='__main__':main()
