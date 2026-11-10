"""Reject current projection input drift, preserving historical cohorts."""
from pathlib import Path
import argparse,hashlib,json,shutil
import native_final_c_interop_profile as profile
from native_frontend_inputs import prepare
from native_ir_derived_overlay import current_contract,current_pair


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--frontend',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();source=a.frontend.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);pins=profile.PROJECTION_PINS;assert profile.pins_for(source)is pins;original=profile.profile(source);assert current_contract(pins) and current_pair(pins['src/ir/xl6_ir.c'],pins['src/ir/xl6_ir.h']);cases=[]
 for mode in ('LF','CRLF'):
  tree=out/mode;tree.mkdir()
  for rel in pins:
   data=(source/rel).read_bytes().replace(b'\r\n',b'\n');target=tree/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data if mode=='LF'else data.replace(b'\n',b'\r\n'))
  ready,meta=prepare(tree,out/(mode+'-prepared'));assert meta['sourceRevision']=='current-projection-20261111' and len(meta['originalInputs'])>=len(pins);assert all(profile.sha(ready/rel)==h for rel,h in pins.items());cases.append(mode+'-exact-canonical-projection')
 tree=out/'drift';shutil.copytree(source,tree);rejected=[]
 for rel in pins:
  path=tree/rel;data=path.read_bytes();path.write_bytes(data+b'\n/* unreviewed input */\n')
  try:
   try:profile.profile(tree)
   except ValueError:rejected.append(rel)
   else:raise AssertionError('accepted drift: '+rel)
  finally:path.write_bytes(data)
 for mode,changed in [('extra',{**pins,'unknown.inc':'0'*64}),('missing',{k:v for k,v in pins.items()if k!='src/ir/xl6_ir.h'})]:assert not current_contract(changed);cases.append(mode+'-dependency-contract-rejected')
 for rel in ('src/sem/xl6_sem_foreign_projection_compat.c','src/ir/xl6_ir_foreign_projection_compat.c'):
  path=tree/rel;data=path.read_bytes();path.unlink()
  try:
   try:profile.profile(tree)
   except ValueError:cases.append('missing-'+rel+'-rejected')
   else:raise AssertionError('accepted missing source: '+rel)
  finally:path.write_bytes(data)
 assert len(rejected)==len(pins) and original==profile.profile(source);record=dict(status='passed',cases=cases,mutatedInputsRejected=rejected,sourceInputs=original,projectionContract=hashlib.sha256(json.dumps(pins,sort_keys=True,separators=(',',':')).encode()).hexdigest());(out/'result.json').write_text(json.dumps(record,indent=2)+'\n');print('PASS current projection admission',len(rejected),'input mutations',flush=True)


if __name__=='__main__':main()
