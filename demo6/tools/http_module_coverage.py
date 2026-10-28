"""Inventory every pinned HTTP public definition and its language role.

This is source/API coverage, not execution coverage or a conformance certificate.
Only direct resident calls seed the native dependency walk. Raw view/cursor and
duplicate allocation helpers remain internal; no missing function is hidden by
counting a class name alone.
"""
from pathlib import Path
import argparse,hashlib,json,re
SOURCES={
 'http':['http'],'http_expect':['http_expect'],'http_upgrade':['http_upgrade'],
 'http_te':['http_te'],'http_upgrade_write':['http_upgrade_write'],
 'http_encoding':['http_encoding','http_content_encoding'],'http_decode':['http_decode'],
 'http_param':['http_param'],'http_connection':['http_connection'],'http_trailer':['http_trailer'],
 'http_host':['http_host'],'http_param_host':['http_param_host'],'http_target':['http_target'],
 'http1_head':['http1','http1_write'],'http1_net':['http1_net'],'http1_tls':['http1_tls'],
 'http1_body':['http1_body','http1_body_write'],'http1_message':['http1_message']}
MASK=re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',re.S)
DEF=re.compile(r'XRT_API\s+[^;{}]*?\b(xrtHttp\w+)\s*\([^;{}]*\)\s*\{')
SECTION=re.compile(r'(?m)^/\* source: (src/http/[^ ]+) \*/$')
DISPOSITIONS={
 'xrtHttp1BodyLimitsInit':'Http1Limits / BodyReader arguments materialize explicit language defaults; maxBody=-1 is explicit unlimited.',
 'xrtHttp1LimitsInit':'Http1Limits / Head parser arguments materialize explicit language defaults.',
 'xrtHttp1BodyTrailers':'BodyReader owns descriptor capacity for maxTrailers up front; reset replaces it. No mid-parse borrowed-array importer.',
 'xrtHttp1MessageBodyView':'Replaced by owned Message.body() and copyBodyInto(); short-lived views do not escape.',
 'xrtHttpAuthorityValid':'HttpAuthority is immutable and valid by successful construction.',
 'xrtHttpFieldParse':'HttpHeaders.parse uses the native field iterator and returns owned names/byte values.',
 'xrtHttpFieldWrite':'HttpHeaders.add plus writeInto/toBytes uses the native complete field-block writer.',
}
for family,language in [('Connection','HttpConnection'),('Directive','HttpParameters.directives'),('Param','HttpParameters'),
    ('Expect','HttpExpectations'),('Te','HttpTe'),('Upgrade','HttpUpgrades'),('TokenList','HttpTokens')]:
    for suffix in ['Count','Find','Has']:
        DISPOSITIONS['xrtHttp'+family+suffix]=language+' owns a parsed index: length/find/contains queries do not reparse input.'
    for suffix in ['CursorInit','Next','Valid','Parse']:
        DISPOSITIONS['xrtHttp'+family+suffix]=language+' uses native validated field/list parsing and owns results; raw cursor/element variants remain internal.'
DISPOSITIONS['xrtHttpExpectationParse']='HttpExpectations.parse/fromHeaders validates complete elements and preserves values, flags and parameters.'
DISPOSITIONS['xrtHttpTeCodingParse']='HttpTe.parse/fromHeaders owns coding, q-value, flags and parameters from native field parsing.'
for suffix in ['CursorInit','Next']:
    DISPOSITIONS['xrtHttpParamValue'+suffix]='HttpParameters.valueAt/copyValueInto invokes native decoded-value writing; no ephemeral decoded-byte cursor is exposed.'
def analyze(root):
    header=root/'lib/xrt/xrt.h';text=header.read_text(encoding='utf-8-sig');sections=list(SECTION.finditer(text))
    definitions={};moduleOf={n+'.c':module for module,names in SOURCES.items() for n in names}
    for index,section in enumerate(sections):
        source=section[1];module=moduleOf.get(Path(source).name)
        if not module:continue
        # The next marker can belong to a different subsystem.
        end=text.find('\n/* source: ',section.end());end=len(text) if end<0 else end
        body=text[section.end():end];clean=MASK.sub(lambda m:' '*len(m[0]),body)
        for match in DEF.finditer(clean):
            pos=match.end();level=1
            while level:
                if clean[pos]=='{':level+=1
                elif clean[pos]=='}':level-=1
                pos+=1
            definitions[match[1]]=dict(Module=module,Source=source,Calls=sorted(set(re.findall(r'\b(xrtHttp\w+)\s*\(',clean[match.end():pos]))))
    resident={}
    for source in sorted((root/'src/runtime').glob('xl6_http*')):
        if source.suffix not in ('.c','.h'):continue
        # Parser arguments to static C macros/function parameters are direct
        # references too; they need not be immediately followed by '('.
        for name in set(re.findall(r'\b(xrtHttp\w+)\b',MASK.sub(lambda m:' '*len(m[0]),source.read_text(encoding='utf-8-sig')))):
            resident.setdefault(name,[]).append(source.relative_to(root).as_posix())
    reached=set(resident);pending=list(reached)
    while pending:
        name=pending.pop()
        for child in definitions.get(name,{}).get('Calls',[]):
            if child not in reached:reached.add(child);pending.append(child)
    rows=[]
    for name,data in sorted(definitions.items()):
        role='resident' if name in resident else 'native-dependency' if name in reached else 'needs-disposition'
        reason=DISPOSITIONS.get(name)
        if role=='needs-disposition' and reason:role='language-equivalent-or-internal'
        rows.append(dict(Name=name,**data,Role=role,ResidentSources=resident.get(name,[]),Disposition=reason))
    return dict(Scope=__doc__,Modules=SOURCES,DefinitionCount=len(rows),Definitions=rows,
        PinnedHeaderSha256=hashlib.sha256(header.read_bytes()).hexdigest(),ExecutionCoverage=False,
        RoleCoverageComplete=bool(rows) and all(r['Role']!='needs-disposition' for r in rows))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    result=analyze(a.root.resolve());a.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    missing=[r['Name'] for r in result['Definitions'] if r['Role']=='needs-disposition']
    print(json.dumps(dict(Modules=len(SOURCES),Definitions=result['DefinitionCount'],NeedsDisposition=missing),indent=2))
if __name__=='__main__':main()
