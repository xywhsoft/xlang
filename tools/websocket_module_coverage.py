"""Approved module roles and language witnesses; execution evidence is separate."""
from pathlib import Path
import argparse,json
from gen_stdlib_websocket import CLASSES,CALLS

ROLES=[
 ('websocket_frame',['WsFrame','WsFrameConfig'],['values_v2','frame_trim']),
 ('websocket_close',['WsClose'],['values_v2','close_trim']),
 ('websocket_message',['WsMessageState','WsMessageConfig','WsMessageInfo'],['values_v2','stream_v2']),
 ('websocket_handshake',['WsHandshake'],['values_v2','handshake_trim']),
 ('websocket_keygen',['WsHandshake'],['values_v2','keygen_trim']),
 ('websocket_extension',['WsExtensions','WsExtension'],['extensions_v2']),
 ('websocket_deflate',['WsDeflateOffer','WsDeflateResponse','WsDeflateDirection'],['extensions_v2','upgrade_v2']),
 ('websocket_inflater',['WsInflater','WsInflaterConfig'],['codecs_v2','tls_v2']),
 ('websocket_deflater',['WsDeflater','WsDeflaterConfig'],['codecs_v2','tls_v2']),
 ('websocket_stream',['WsStream','WsStreamOptions','WsEvent'],['stream_v2','cycle_v2','callback_error_v2']),
 ('websocket_stream_ref',['WsStream'],['send_v2']),
 ('websocket_stream_tls',['WsStream'],['tls_v2','upgrade_tls_loopback']),
 ('websocket_stream_deflate',['WsStream'],['tls_v2']),
 ('websocket_upgrade',['WsUpgrade','WsClientOptions','WsServerOptions'],['upgrade_v2']),
 ('websocket_upgrade_deflate',['WsUpgrade','WsClientOptions','WsServerOptions'],['upgrade_v2','tls_v2']),
 ('websocket_upgrade_stream',['WsStream','WsStreamOptions'],['handoff_v2','upgrade_loopback','upgrade_tls_loopback']),
]
def render(root):
 assert len(ROLES)==16 and len({r[0] for r in ROLES})==16
 rows=[]
 for module,classes,cases in ROLES:
  assert all(n in CLASSES for n in classes)
  files=['test/cases/stdlib_websocket_'+n+'.xl' for n in cases]
  assert all((root/n).is_file() for n in files)
  rows.append(dict(Module=module,LanguageClasses=classes,Fixtures=files))
 return json.dumps(dict(ApprovedModules=16,Classes=len(CLASSES),ExactResidentDeclarations=len(CALLS),
  DeclaredRoleCoverageOnly=True,RuntimeAcceptance='tools/check_stdlib_websocket.py results.json; native XRT receipts are separate',
  Excluded='xws extension group/writer facade; raw pointers, borrowed callback views and ownership machinery remain internal',
  Modules=rows),ensure_ascii=False,indent=2)+'\n'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--check',action='store_true');a=p.parse_args()
 root=a.root.resolve();file=root/'docs/websocket-module-coverage.json';content=render(root)
 if a.check:assert file.read_text(encoding='utf-8-sig')==content,'WebSocket role manifest is stale'
 else:file.write_text(content,encoding='utf-8')
 print('WebSocket approved module roles: 16/16; execution evidence remains separate')
if __name__=='__main__':main()
