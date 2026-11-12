"""Check unique host/resident source ownership against an actual expanded catalog.

The supplied catalog is an external preflight input. The normal producer repeats
the check against a freshly preprocessed catalog on every complete build.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys
from precompile_builtin import component_sources,validate_embedded_source_rosters
p=argparse.ArgumentParser(description=__doc__)
for n in ('api','before-api','catalog','out'):p.add_argument('--'+n,required=True,type=Path)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
inputs={str(q.resolve()):sha(q)for q in(a.api,a.before_api,a.catalog,Path(__file__),Path(__file__).with_name('precompile_builtin.py'),Path(__file__).with_name('runtime_layout.py'))}
def arrays(api):
 text=api.read_text(encoding='utf-8')
 return {n:re.findall(r'"([^"\n]+\.c)"',re.search(r'static const char\* '+n+r'\[\] = \{(.*?)\};',text,re.S)[1])for n in('runtimeSources','compilerSources')}
text=a.catalog.read_text();text=text[text.index('static const XL6_ArtifactRuntimeComponent xl6ArtifactComponents[]'):];text=text[:text.index('_Static_assert')]
while re.search(r'"\s+"',text):text=re.sub(r'"\s+"','',text)
components,placed=component_sources(text);assert len(components)>1000
validate_embedded_source_rosters(arrays(a.api),components)
old=arrays(a.before_api);duplicates=sorted(set(old['runtimeSources'])&set(components));assert len(duplicates)==8,duplicates
try:validate_embedded_source_rosters(old,components)
except ValueError as e:
 assert all(n in str(e)for n in duplicates);before_error=str(e)
else:raise AssertionError('Actual old duplicated roster was accepted')
rejected=0
for lists,resident in [({'runtimeSources':['a.c','a.c'],'compilerSources':['b.c']},['c.c']),({'runtimeSources':['a.c'],'compilerSources':['a.c']},['c.c']),({'runtimeSources':['a.c'],'compilerSources':['b.c']},['a.c']),({'runtimeSources':['a.c'],'compilerSources':['b.c']},['c.c','c.c'])]:
 try:validate_embedded_source_rosters(lists,resident)
 except ValueError:rejected+=1
 else:raise AssertionError('Duplicated source roster was accepted')
assert rejected==4
try:component_sources('static const T table[] = {{"src/repeated.c",0},{"src/repeated.c",0}};')
except ValueError:rejected+=1
else:raise AssertionError('Raw duplicate component rows were silently deduplicated')
validate_embedded_source_rosters({'runtimeSources':['a.c'],'compilerSources':['b.c']},['c.c'])
assert all(sha(Path(q))==h for q,h in inputs.items())
report=dict(passed=True,shippingDelivery=False,fullCompilerQualified=False,scope=__doc__,inputs=inputs,residentSources=len(components),nativeTranslationUnitSources=len(placed),removedHostOverlaps=duplicates,beforeError=before_error,syntheticDuplicationsRejected=rejected,positiveLayouts=2)
(a.out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('Unique current source roster accepted; actual 8-source overlap and 5 duplicated layouts rejected')
