"""Audit the public XACME inventory against the thin language facade."""
from pathlib import Path
import argparse,json,re
p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1];package=root/'release/extlibs/xacme'
header=(package/'upstream/xacme_decl.h').read_text(encoding='utf-8')
functions=sorted(set(re.findall(r'XRT_API\s+[\w\s*]+?\b(xrtAcme\w+)\s*\(',header)))
mapping={
 'AccountConfigInit':'AccountConfig()', 'ClientConfigInit':'ClientConfig()',
 'ObtainConfigInit':'ObtainConfig()', 'GrantUnit':'automatic cstring lifetime',
 'CleanupPending':'CleanupPending(timeoutMs): remaining count',
 'DnsProviderValidate':'DnsProvider.Validate()', 'DnsProviderCount':'DnsProvider.Count()',
 'DnsProviderId':'DnsProvider.ProviderId(index)',
}
for suffix in ('Create','Cleanup','Destroy','AccountPem','Issue','IssueEx','IssueStored','Revoke','Rollover','Deactivate'):
 mapping['Client'+suffix]='Client.'+('automatic lifetime (Cleanup exposes readiness)' if suffix=='Destroy' else suffix+'()')
for suffix in ('SaveAccount','LoadAccount','SaveCert','LoadCert','LoadCertCa','SaveGrant','LoadGrant','ListDomains','NeedRenew'):
 mapping['Store'+suffix]='Store.'+suffix+'()'
mapping['Obtain']='Obtain()'
for name in ('Ali','Cf','Tencent','Aws','Huawei'):
 mapping['Dns'+name+'ConfigInit']=name+'Config()'
 mapping['Dns'+name]='DnsProvider.'+name+'()'
 mapping['Dns'+name+'ProviderUnit']='DnsProvider.Unit() / automatic lifetime'
assert set(mapping)=={f.removeprefix('xrtAcme') for f in functions},(functions,set(mapping))
constants={}
clean=re.sub(r'/\*.*?\*/','',header,flags=re.S)
for body in re.findall(r'typedef enum xacme\w*\s*{(.*?)}',clean,re.S):
 value=-1
 for entry in body.split(','):
  match=re.fullmatch(r'\s*(XACME_\w+)(?:\s*=\s*(\d+))?\s*',entry)
  if not match:continue
  value=int(match[2]) if match[2] else value+1;constants[match[1]]=value
source=(package/'upstream/xacme.h').read_text(encoding='utf-8')
for name,value in re.findall(r'#define (XACME_DIRECTORY_\w+) \\\n\s*"([^"]+)"',source):constants[name]=value
constants['XACME_DNS_CAP_PROPAGATE']=1
result={'upstreamRevision':json.loads((package/'upstream/source.json').read_text())['revision'],
 'functions':{f:mapping[f.removeprefix('xrtAcme')] for f in functions},'constants':constants,
 'rules':{'grant':'(cstring,cstring) or (cstring,cstring,bool); transactional transfer, xrtFree',
 'borrowedEngine':'typed NetEngine overload; retained runtime owner and native entry per call',
 'cleanup':'false/nonzero retains native owner and all dependencies for explicit retry',
 'errors':'native failure carrier and original xerror, including absent OOM diagnostic'}}
api=package/'api-map.json';const='\n// Upstream public constants. Values audited by tools/check_xacme_api.py.\n'
const+=''.join(f'export const {"string" if isinstance(v,str) else "int"} {k} = {json.dumps(v)}\n' for k,v in sorted(constants.items()))
facade=package/'src/xacme.xl';text=facade.read_text(encoding='utf-8')
text=re.split(r'\nexport const XACME_DIRECTORY_',text,1)[0]
text=text.split('\n// Upstream public constants.')[0]
expected=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
if args.write:api.write_text(expected,encoding='utf-8');facade.write_text(text+const,encoding='utf-8')
else:
 assert api.read_text(encoding='utf-8')==expected,'stale XACME API map'
 current=facade.read_text(encoding='utf-8')
 for k,v in constants.items():assert f'export const {"string" if isinstance(v,str) else "int"} {k} = {json.dumps(v)}' in current,k
print(f'XACME API: {len(functions)} functions and {len(constants)} constants covered')
