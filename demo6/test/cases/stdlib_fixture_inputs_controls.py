"""Positive/refusal controls for explicit fixture replacement and addition."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,sys

p=argparse.ArgumentParser()
for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();assert not a.out.exists();task=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(task/'tools'));import stdlib_fixture_inputs as f
spec=importlib.util.spec_from_file_location('inventory',a.root/'tools/discover_stdlib_three_groups.py')
inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
old=a.root/'test/cases/stdlib_spsc_pressure.xl';new=task/'test/cases/stdlib_wait_queue_pressure.xl'
other=a.root/'test/cases/stdlib_math.xl';native=task/'test/cases/stdlib_queue_product_plan.c'
paths=[Path(__file__),Path(f.__file__),Path(inventory.__file__),old,new,other,native]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
inputs=[dict(Path=str(path.resolve()),Sha256=sha(path)) for path in paths];controls=[]
assert f.select([old],[old])==[old.resolve()];controls.append('existing replacement')
assert f.extend([old],[],inventory.PREFIX)==[old];controls.append('empty addition')
assert f.extend([old],[new],inventory.PREFIX)==[old,new.resolve()];controls.append('explicit addition')
for name,call in (
 ('unknown replacement',lambda:f.select([old],[new])),
 ('duplicate replacement',lambda:f.select([old],[old,old])),
 ('existing addition',lambda:f.extend([old],[old],inventory.PREFIX)),
 ('duplicate addition',lambda:f.extend([old],[new,new],inventory.PREFIX)),
 ('outside family',lambda:f.extend([old],[other],inventory.PREFIX)),
 ('native source',lambda:f.extend([old],[native],inventory.PREFIX)),
 ('missing source',lambda:f.extend([old],[new.with_name('stdlib_wait_queue_absent_control.xl')],inventory.PREFIX))):
 try:call()
 except AssertionError:controls.append(name)
 else:raise AssertionError('control was not refused: '+name)
assert len(controls)==10
a.out.write_text(json.dumps(dict(Complete=True,InputsUnchanged=all(sha(Path(r['Path']))==r['Sha256'] for r in inputs),Inputs=inputs,Controls=controls),indent=2)+'\n',encoding='utf-8')
print('fixture inventory: ten positive/refusal controls passed',flush=True)
