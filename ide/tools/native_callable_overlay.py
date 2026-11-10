"""Compile exact original pure callable helpers in a separate analysis TU.

Do not add a reverse index -> checker -> index dependency, or modify ordinary
checker execution/code generation just to expose on-demand declaration rules.
"""
from pathlib import Path
import hashlib
import re

HELPERS=('xl6TextEquals','xl6SemFuncName','xl6SemIsFuncDecl','xl6SemMemberName',
         'xl6SemIsClassPropertyMember','xl6SemMembersShareOverloadGroup')

def helpers(data):
    out={}
    for name in HELPERS:
        pattern=r'^static [^;{}\n]*\b'+re.escape(name)+r'\([^;{}]*\)\s*\{.*?^\}'
        matches=list(re.finditer(pattern,data,re.MULTILINE|re.DOTALL))
        if len(matches)!=1:raise ValueError('Unknown native callable helper: '+name)
        out[name]=matches[0].group()
    return out

def derive(source,output):
    source,output=Path(source),Path(output)
    data=source.read_text(encoding='utf8')
    bodies=helpers(data)
    result='#include "analysis/frontend_callable_native.h"\n#include <string.h>\n\n'
    result+='\n\n'.join(bodies.values())+'\n\n#include "analysis/frontend_callable_native.inc"\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(result,encoding='utf8',newline='\n')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    return dict(source=str(source),sourceSha256=sha(source),compiledSource=str(output),
                compiledSha256=sha(output),originalHelpersUnchanged=True,
                helperSha256={n:hashlib.sha256(s.encode()).hexdigest()for n,s in bodies.items()},
                originalExecutionPathsChanged=False,additionalOrdinaryAnalysisAllocations=0)
