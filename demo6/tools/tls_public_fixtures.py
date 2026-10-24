"""Default TLS facade acceptance, reusing ordinary local-only protocol cases.

No replacement TLS classes, extern declarations, or native pointer access may
appear in the transformed cases. The underlying fixtures own all key data and
create only literal self-loopback connections.
"""
from pathlib import Path
import re
from tls_dial_fixtures import api_rows, language_fixtures
from tls_data_fixtures import data_language_fixture


def public_text(root, source):
    start=source.index('func clientContext()')
    source='import concurrent\n'+source[start:]
    source=source.replace('ResidentTlsClient','TlsStream').replace('ResidentTlsListener','TlsServer')
    for row in api_rows(root):
        method=row['symbol'].removeprefix('xl6Tls').removesuffix('V1').removesuffix('Default')
        if method.startswith('ProxyDial'):method='dialProxy'+method[len('ProxyDial'):]
        elif method.startswith('Dial'):method='dial'+method[len('Dial'):]
        else:raise AssertionError(method)
        source=re.sub(r'\b'+row['alias']+r'\(', 'TlsStream.'+method+'(',source)
    # The public surface overloads by input type. Test aliases are not API.
    source=re.sub(r'\.(?:sendText|sendBytes|sendBuffer)(\w*)\(',r'.send\1(',source)
    source=re.sub(r'\.sendVector(?:Text|Bytes|Buffer)(\w*)\(',r'.sendVector\1(',source)
    assert not re.search(r'^\s*(?:class|extern)\b',source,re.M)
    assert 'nativeHandle(' not in source and 'ResidentTls' not in source
    return source


def fixtures(root, out):
    out=Path(out)
    direct,proxy,_=language_fixtures(root,out)
    data=data_language_fixture(root,out)
    result=[]
    for name,path in [('dial',direct),('proxy',proxy),('data',data)]:
        target=out/('public_tls_'+name+'.xl')
        target.write_text(public_text(root,path.read_text(encoding='utf8')),encoding='utf8')
        result.append(target)
    return result
