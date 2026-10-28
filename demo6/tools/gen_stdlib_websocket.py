"""One exact table for WebSocket declarations, resident ABI and ownership.

This generates metadata, never a protocol algorithm or a runtime dispatcher.
"""
from pathlib import Path
import argparse, json

ROOT = Path(__file__).resolve().parents[1]
CLASSES, CALLS = {}, []
STATIC_CLASSES = {'WsHandshake'}
ENUMS = {
 'WsOpcode': [('continuation',0),('text',1),('binary',2),('close',8),('ping',9),('pong',10)],
 'WsRole': [('client',0),('server',1)],
 'WsMaskPolicy': [('any',0),('required',1),('forbidden',2)],
 'WsFrameFlags': [('none',0),('final',1),('masked',2),('rsv1',4),('rsv2',8),('rsv3',16)],
 'WsMessageFlags': [('none',0),('messageBegin',1),('messageEnd',2),('control',4),('extended',8),('compressed',16)],
 'WsDeflateFlags': [('none',0),('serverNoContext',1),('clientNoContext',2),('serverMaxWindow',4),('clientMaxWindow',8),('clientMaxWindowAny',16)],
 'WsStreamState': [('open',0),('closing',1),('closed',2)],
 'WsCloseFlags': [('none',0),('sent',1),('received',2),('clean',4),('remote',8)],
 'WsEventKind': [('messageBegin',0),('messageData',1),('messageEnd',2),('ping',3),('pong',4),('backpressure',5),('writable',6),('drain',7),('error',8),('close',9)],
 'WsDeflateStrategy': [('default',0),('filtered',1),('huffman',2),('rle',3),('fixed',4)],
}
COMPONENTS = [
 ('WS_FRAME','xl6_ws_frame.c', ['NATIVE_HANDLE'], ['WEBSOCKET_FRAME','BUFFER','VALUE','UNICODE_TEXT']),
 ('WS_CLOSE','xl6_ws_close.c', ['NATIVE_HANDLE'], ['WEBSOCKET_CLOSE','BUFFER','VALUE','UNICODE_TEXT']),
 ('WS_HANDSHAKE','xl6_ws_handshake.c', [], ['WEBSOCKET_HANDSHAKE','BUFFER','VALUE','UNICODE_TEXT']),
 ('WS_KEYGEN','xl6_ws_keygen.c', [], ['WEBSOCKET_KEYGEN']),
 ('WS_MESSAGE','xl6_ws_message.c', ['WS_FRAME','NATIVE_HANDLE'], ['WEBSOCKET_MESSAGE','ATOMIC']),
 ('WS_EXTENSION','xl6_ws_extension.c', ['NATIVE_HANDLE'], ['WEBSOCKET_EXTENSION','BUFFER','VALUE','UNICODE_TEXT']),
 ('WS_DEFLATE','xl6_ws_deflate.c', ['NATIVE_HANDLE'], ['WEBSOCKET_DEFLATE','BUFFER','VALUE']),
 ('WS_INFLATER','xl6_ws_inflater.c', ['WS_DEFLATE','NATIVE_HANDLE'], ['WEBSOCKET_INFLATER','ATOMIC','BUFFER']),
 ('WS_DEFLATER','xl6_ws_deflater.c', ['WS_DEFLATE','NATIVE_HANDLE'], ['WEBSOCKET_DEFLATER','ATOMIC','BUFFER']),
 ('WS_UPGRADE','xl6_ws_upgrade.c',['WS_HANDSHAKE','WS_DEFLATE','HTTP_HEAD','HTTP_HEADERS','NATIVE_HANDLE'],['WEBSOCKET_UPGRADE','WEBSOCKET_UPGRADE_DEFLATE']),
 ('WS_UPGRADE_CALLBACK','xl6_ws_upgrade_callback.c',['WS_UPGRADE','NATIVE_CLASS_CALLBACK'],['WEBSOCKET_UPGRADE_DEFLATE']),
 ('WS_STREAM_OPTIONS','xl6_ws_stream_options.c',['NATIVE_HANDLE'],['WEBSOCKET_STREAM']),
 ('WS_EVENT','xl6_ws_event.c',['WS_MESSAGE','NATIVE_HANDLE','EXECUTION_SOURCE'],['WEBSOCKET_STREAM']),
 ('WS_STREAM','xl6_ws_stream.c',['WS_EVENT','WS_STREAM_OPTIONS','EXECUTION_SOURCE','NATIVE_CLASS_CALLBACK','BUFFER_CORE'],['WEBSOCKET_STREAM']),
 ('WS_STREAM_COMPRESSED','xl6_ws_stream_compressed.c',['WS_STREAM'],['WEBSOCKET_STREAM_DEFLATE']),
 ('WS_STREAM_REF','xl6_ws_stream_ref.c',['WS_STREAM','NET_BYTES'],['WEBSOCKET_STREAM_REF']),
 ('WS_STREAM_TAKE','xl6_ws_stream_take.c',['WS_STREAM','BUFFER_CORE'],['WEBSOCKET_STREAM']),
 # An Upgrade is negotiated at run time: attach must accept either plain or
 # permessage-deflate, including receive-only applications. Keep its codec
 # capability at the factory, rather than hiding it in a send operation.
 ('WS_STREAM_TCP','xl6_ws_stream_tcp.c',['WS_STREAM','EXECUTION_TCP','NET_BUFFER_CORE'],['WEBSOCKET_UPGRADE_STREAM','WEBSOCKET_STREAM_DEFLATE']),
 ('WS_STREAM_TLS','xl6_ws_stream_tls.c',['WS_STREAM','EXECUTION_TLS_STREAM','NET_BUFFER_CORE','OWNERSHIP_DOMAIN_WS_STREAM_TLS_NATIVE'],['WEBSOCKET_STREAM_TLS','WEBSOCKET_UPGRADE_STREAM','WEBSOCKET_STREAM_DEFLATE']),
 ('OWNERSHIP_DOMAIN_WS_STREAM_TLS_NATIVE','xl6_ws_stream_tls_domain.c',['WS_STREAM','OWNERSHIP_CATALOG'],['WEBSOCKET_STREAM_TLS']),
 ('WS_WAIT','xl6_ws_wait.c',['WS_STREAM','FUTURE_PAYLOAD','OWNERSHIP_DOMAIN_NATIVE_FUTURE'],['FUTURE','CANCEL']),
 ('OWNERSHIP_DOMAIN_WS_WAIT','xl6_ws_wait_domain.c',['WS_WAIT','OWNERSHIP_CATALOG'],['FUTURE','CANCEL']),
]
def cls(name,component): CLASSES[name]=dict(component=component,methods=[])
def arg(t,n,d=None):return t,n,d
def method(c,n,r,*args,kind='func',static=False,pure=False,no_raise=False,component=None,symbol=None,nonblocking=True,callback=None,predicate=None,services=False,field=None,private=False):
 row=dict(owner=c,name=n,result=r,args=list(args),kind=kind,static=static,pure=pure,no_raise=no_raise,
  nonblocking=nonblocking,component=component or CLASSES[c]['component'],symbol=symbol or 'xl6'+c+n[0].upper()+n[1:]+'V2',callback=callback,predicate=predicate,services=services,field=field,private=private)
 CALLS.append(row);CLASSES[c]['methods'].append(row);return row
def destroy(c):method(c,'destroy','void',kind='destroy',no_raise=True)
def get(c,n,t,no_raise=False):method(c,n,t,kind='get',pure=True,no_raise=no_raise)
def inputs(c,n,r,*tail,static=False,leading=(),component=None,nonblocking=True):
 for t,s in [('string','Text'),('bytes','Bytes'),('Buffer','Buffer'),('cbytes','Span')]:
  method(c,n,r,*leading,arg(t,'input'),*tail,arg('int','size','-1'),static=static,symbol='xl6'+c+n[0].upper()+n[1:]+s+'V2',component=component,nonblocking=nonblocking)

cls('WsFrameConfig','WS_FRAME')
method('WsFrameConfig','create','void',arg('uint64','maxPayload','uint64(1048576)'),arg('WsMaskPolicy','mask','WsMaskPolicy.any'),arg('WsFrameFlags','allowedRsv','WsFrameFlags.none'),arg('int','allowedOpcodes','1799'),kind='create')
destroy('WsFrameConfig')
for n,t in [('maxPayload','uint64'),('mask','WsMaskPolicy'),('allowedRsv','WsFrameFlags'),('allowedOpcodes','int')]:get('WsFrameConfig',n,t,True)
cls('WsFrame','WS_FRAME')
method('WsFrame','make','WsFrame',arg('WsOpcode','opcode'),arg('uint64','payloadSize'),arg('WsFrameFlags','flags','WsFrameFlags.final'),arg('int','mask','0'),static=True,symbol='xl6WsFrameMakeDefaultV2')
method('WsFrame','make','WsFrame',arg('WsOpcode','opcode'),arg('uint64','payloadSize'),arg('WsFrameConfig','config'),arg('WsFrameFlags','flags','WsFrameFlags.final'),arg('int','mask','0'),static=True)
inputs('WsFrame','parse','WsFrame?',arg('WsFrameConfig','config'),static=True)
for t,s in [('string','Text'),('bytes','Bytes'),('Buffer','Buffer'),('cbytes','Span')]:
 method('WsFrame','parse','WsFrame?',arg(t,'input'),arg('int','size','-1'),static=True,symbol='xl6WsFrameParseDefault'+s+'V2')
destroy('WsFrame')
for n,t in [('opcode','WsOpcode'),('payloadSize','uint64'),('flags','WsFrameFlags'),('mask','int'),('headerSize','int'),('final','bool'),('masked','bool')]:get('WsFrame',n,t,True)
method('WsFrame','writeInto','int',arg('Buffer','output'))
method('WsFrame','toBytes','bytes',pure=True)
method('WsFrame','applyMask','void',arg('Buffer','data'),arg('int','mask'),arg('uint64','offset','uint64(0)'),arg('int','size','-1'),static=True)
cls('WsClose','WS_CLOSE')
method('WsClose','make','WsClose',arg('int','code','0'),arg('string','reason','""'),arg('int','size','-1'),static=True)
inputs('WsClose','parse','WsClose',static=True)
method('WsClose','codeValid','bool',arg('int','code'),static=True,pure=True)
destroy('WsClose');get('WsClose','code','int',True);get('WsClose','reason','string')
method('WsClose','writeInto','int',arg('Buffer','output'));method('WsClose','toBytes','bytes',pure=True)
cls('WsHandshake','WS_HANDSHAKE')
method('WsHandshake','key','string',static=True,component='WS_KEYGEN')
for n,r,args in [
 ('keyValid','bool',[arg('string','key'),arg('int','size','-1')]),
 ('accept','string',[arg('string','key'),arg('int','size','-1')]),
 ('acceptValid','bool',[arg('string','key'),arg('string','accept')]),
 ('protocolsValid','bool',[arg('string','protocols'),arg('int','size','-1')]),
 ('protocolsHas','bool',[arg('string','protocols'),arg('string','protocol')]),
 ('protocolSelect','string?',[arg('string','client'),arg('string','server')]),
 ('protocolCount','int',[arg('string','protocols'),arg('int','size','-1')]),
 ('protocolAt','string',[arg('string','protocols'),arg('int','index'),arg('int','size','-1')]),
]:method('WsHandshake',n,r,*args,static=True,pure=True)
cls('WsMessageConfig','WS_MESSAGE')
method('WsMessageConfig','create','void',arg('int','maxSize','16777216'),arg('WsFrameFlags','firstRsv','WsFrameFlags.none'),arg('WsFrameFlags','continuationRsv','WsFrameFlags.none'),arg('WsFrameFlags','controlRsv','WsFrameFlags.none'),arg('bool','validateText','true'),kind='create');destroy('WsMessageConfig')
cls('WsMessageInfo','WS_MESSAGE');destroy('WsMessageInfo')
for n,t in [('flags','WsMessageFlags'),('rsv','WsFrameFlags'),('opcode','WsOpcode'),('frameOpcode','WsOpcode'),('payloadSize','uint64'),('offset','int')]:get('WsMessageInfo',n,t,True)
cls('WsMessageState','WS_MESSAGE')
method('WsMessageState','create','void',kind='create',symbol='xl6WsMessageStateCreateDefaultV2')
method('WsMessageState','create','void',arg('WsMessageConfig','config'),kind='create');destroy('WsMessageState')
method('WsMessageState','beginFrame','WsMessageInfo',arg('WsFrame','frame'))
inputs('WsMessageState','writePayload','void')
method('WsMessageState','endFrame','void');method('WsMessageState','reset','void')
get('WsMessageState','activeSize','int');get('WsMessageState','failed','bool')
cls('WsExtensions','WS_EXTENSION');inputs('WsExtensions','parse','WsExtensions',static=True);destroy('WsExtensions');get('WsExtensions','length','int',True)
method('WsExtensions','at','WsExtension',arg('int','index'),pure=True)
cls('WsExtension','WS_EXTENSION');destroy('WsExtension')
get('WsExtension','name','string');get('WsExtension','parameters','string');get('WsExtension','parameterCount','int',True)
for n,t in [('parameterNameAt','string'),('parameterValueAt','bytes?'),('parameterFlagsAt','int')]:method('WsExtension',n,t,arg('int','index'),pure=True)
method('WsExtension','writeInto','int',arg('Buffer','output'));method('WsExtension','toString','string',pure=True)
method('WsExtension','make','WsExtension',arg('string','name'),arg('string','parameters','""'),static=True)
for c in ['WsDeflateOffer','WsDeflateResponse']:
 cls(c,'WS_DEFLATE');method(c,'create','void',arg('WsDeflateFlags','flags','WsDeflateFlags.none'),arg('int','serverWindowBits','15'),arg('int','clientWindowBits','15'),kind='create');destroy(c)
 inputs(c,'parse',c,static=True)
 for n,t in [('flags','WsDeflateFlags'),('serverWindowBits','int'),('clientWindowBits','int')]:get(c,n,t,True)
 method(c,'toString','string',pure=True);method(c,'writeInto','int',arg('Buffer','output'))
method('WsDeflateOffer','accept','WsDeflateResponse',pure=True)
method('WsDeflateOffer','check','void',arg('WsDeflateResponse','response'),pure=True)
cls('WsDeflateDirection','WS_DEFLATE');destroy('WsDeflateDirection');get('WsDeflateDirection','windowBits','int',True);get('WsDeflateDirection','noContextTakeover','bool',True)
method('WsDeflateResponse','direction','WsDeflateDirection',arg('WsRole','role'),arg('bool','sending'),pure=True)
for stem,comp in [('Inflater','WS_INFLATER'),('Deflater','WS_DEFLATER')]:
 cfg='Ws'+stem+'Config';c='Ws'+stem;cls(cfg,comp)
 args=[arg('uint64','outputLimit','uint64(67108864)'),arg('int','windowBits','15'),arg('bool','noContextTakeover','false'),arg('bool','retain','false')]
 if stem=='Deflater':args += [arg('int','level','6'),arg('WsDeflateStrategy','strategy','WsDeflateStrategy.default')]
 method(cfg,'create','void',*args,kind='create');destroy(cfg)
 method(cfg,'withDirection',cfg,arg('WsDeflateDirection','direction'),pure=True)
 for n,t in [('outputLimit','uint64'),('windowBits','int'),('noContextTakeover','bool'),('retain','bool')]+([('level','int'),('strategy','WsDeflateStrategy')] if stem=='Deflater' else []):get(cfg,n,t,True)
 cls(c,comp);method(c,'create','void',kind='create',symbol='xl6'+c+'CreateDefaultV2');method(c,'create','void',arg(cfg,'config'),kind='create');destroy(c)
 method(c,'begin','void',arg('bool','compressed','true'))
 inputs(c,'writeInto','int',arg('Buffer','output'))
 method(c,'endInto','int',arg('Buffer','output'));method(c,'reset','void',symbol='xl6'+c+'ResetDefaultV2');method(c,'reset','void',arg(cfg,'config'));get(c,'outputSize','uint64')
 if stem=='Deflater':
  method(c,'flushInto','int',arg('Buffer','output'));method(c,'abort','void');method(c,'bound','int',arg('int','inputSize'),static=True,pure=True)

cls('WsClientOptions','WS_UPGRADE')
method('WsClientOptions','create','void',arg('string','protocols','""'),kind='create');destroy('WsClientOptions')
method('WsClientOptions','withDeflate','WsClientOptions',arg('WsDeflateOffer','offer'),arg('bool','required','false'),pure=True)
for n,t in [('protocols','string'),('deflateEnabled','bool'),('requireDeflate','bool')]:get('WsClientOptions',n,t,t=='bool')
method('WsClientOptions','offer','WsDeflateOffer?',pure=True)
cls('WsServerOptions','WS_UPGRADE')
method('WsServerOptions','create','void',arg('string','protocols','""'),arg('bool','enableDeflate','false'),arg('bool','requireDeflate','false'),kind='create');destroy('WsServerOptions')
method('WsServerOptions','withDeflateResponse','WsServerOptions',arg('WsDeflateResponse','response'),arg('bool','required','false'),pure=True)
for n,t in [('protocols','string'),('deflateEnabled','bool'),('requireDeflate','bool')]:get('WsServerOptions',n,t,t=='bool')
cls('WsUpgrade','WS_UPGRADE');destroy('WsUpgrade')
method('WsUpgrade','client','WsUpgrade',arg('string','host'),static=True,symbol='xl6WsUpgradeClientDefaultV2')
method('WsUpgrade','client','WsUpgrade',arg('string','host'),arg('WsClientOptions','options'),static=True)
method('WsUpgrade','server','WsUpgrade',arg('Http1Head','request'),static=True,symbol='xl6WsUpgradeServerDefaultV2',pure=True)
method('WsUpgrade','server','WsUpgrade',arg('Http1Head','request'),arg('WsServerOptions','options'),static=True,pure=True)
method('WsUpgrade','server','WsUpgrade',arg('Http1Head','request'),arg('WsServerOptions','options'),arg('func(WsDeflateOffer) as WsDeflateResponse?','select'),static=True,component='WS_UPGRADE_CALLBACK',symbol='xl6WsUpgradeServerSelectedV2',nonblocking=False,callback=(3,'WsDeflateOffer','WsDeflateResponse'))
method('WsUpgrade','checkResponse','WsUpgrade',arg('Http1Head','response'),pure=True)
for n,t in [('ready','bool'),('role','WsRole'),('key','string'),('protocol','string'),('extensions','string'),('deflateEnabled','bool')]:get('WsUpgrade',n,t,t in ('bool','WsRole'))
method('WsUpgrade','headers','HttpHeaders',pure=True)
method('WsUpgrade','deflateResponse','WsDeflateResponse?',pure=True)

cls('WsStreamOptions','WS_STREAM_OPTIONS')
method('WsStreamOptions','create','void',arg('int','messageLimit','16777216'),arg('uint64','frameLimit','uint64(16777216)'),arg('int','sendLimit','16777216'),arg('int','controlReserve','4096'),arg('int','closeTimeoutMillis','5000'),arg('bool','autoPong','true'),kind='create');destroy('WsStreamOptions')
method('WsStreamOptions','withCompression','WsStreamOptions',arg('WsInflaterConfig','inflater'),arg('WsDeflaterConfig','deflater'),pure=True)
for n,t in [('messageLimit','int'),('frameLimit','uint64'),('sendLimit','int'),('controlReserve','int'),('closeTimeoutMillis','int'),('autoPong','bool')]:get('WsStreamOptions',n,t,True)
cls('WsEvent','WS_EVENT');destroy('WsEvent')
for n,t in [('kind','WsEventKind'),('opcode','WsOpcode'),('flags','WsMessageFlags'),('offset','int'),('payloadSize','uint64'),('pending','int'),('closeFlags','WsCloseFlags'),('localCode','int'),('remoteCode','int'),('status','NetStatus'),('errorCode','int')]:get('WsEvent',n,t,True)
for n,t in [('data','bytes'),('text','string'),('reason','string'),('errorMessage','string'),('errorDomain','string')]:get('WsEvent',n,t)
get('WsEvent','stream','WsStream');get('WsEvent','dataSize','uint64',True)
cls('WsStream','WS_STREAM');destroy('WsStream')
method('WsStream','prepareEventsNative','WsStream',arg('func(WsEvent) as bool','events'),static=True,private=True,predicate=(1,'WsEvent'),services=True,nonblocking=False)
for transport,component in [('TcpStream','WS_STREAM_TCP'),('TlsStream','WS_STREAM_TLS')]:
 method('WsStream','bindNative','void',arg(transport,'transport'),arg('NetBuffer','buffered'),arg('WsUpgrade','upgrade'),arg('WsStreamOptions','options'),private=True,symbol='xl6WsStreamBind'+transport[:3]+'V2',component=component,nonblocking=False)
 method('WsStream','workerOf','uint64',arg(transport,'transport'),static=True,symbol='xl6WsStream'+transport[:3]+'WorkerV2',component=component)
 method('WsStream','engineOf','NetEngine',arg(transport,'transport'),static=True,symbol='xl6WsStream'+transport[:3]+'EngineV2',component=component)
for n,t in [('state','WsStreamState'),('role','WsRole'),('pending','uint64'),('writable','uint64'),('paused','bool'),('worker','uint64')]:get('WsStream',n,t)
method('WsStream','deflateEnabled','bool',kind='get',pure=True,component='WS_STREAM_COMPRESSED')
method('WsStream','deflateResponse','WsDeflateResponse?',pure=True,component='WS_STREAM_COMPRESSED')
method('WsStream','engine','NetEngine')
method('WsStream','protocol','string');method('WsStream','closeInfo','WsEvent')
method('WsStream','throwIfFailed','void')
method('WsStream','pause','void');method('WsStream','resume','void');method('WsStream','abort','void')
method('WsStream','close','NetStatus',arg('int','code','1000'),arg('string','reason','""'),arg('int','size','-1'),nonblocking=False)
for n in ['sendText','sendBinary','sendTextCompressed','sendBinaryCompressed','ping','pong']:
 inputs('WsStream',n,'NetStatus',component='WS_STREAM_COMPRESSED' if n.endswith('Compressed') else 'WS_STREAM',nonblocking=False)
method('WsStream','sendRef','NetStatus',arg('WsOpcode','opcode'),arg('NetBytes','data'),arg('int','size','-1'),component='WS_STREAM_REF',nonblocking=False)
method('WsStream','sendTake','NetStatus',arg('WsOpcode','opcode'),arg('Buffer','data'),arg('int','size','-1'),component='WS_STREAM_TAKE',nonblocking=False)
method('WsStream','receiveInto','void',arg('Buffer','target'),arg('bool','dataEvents','false'),field=(2,'Buffer'))
method('WsStream','receiveEvents','void')
get('WsStream','writeLimit','uint64')
method('WsStream','requiredSize','uint64',arg('int','payloadBytes'))
inputs('WsStream','requiredSize','uint64')
for n in ['writable','drained','closed']:
 args=([arg('uint64','minimum')] if n=='writable' else [])+[arg('uint64','deadlineUs')]
 for cancel in [False,True]:
  method('WsStream','wait'+n.title()+('Cancel' if cancel else '')+'OnWorker','future<NetStatus>',*args,*([arg('CancellationToken','cancel')] if cancel else []),component='WS_WAIT',symbol='xl6WsWait'+n.title()+('Cancel' if cancel else '')+'V2',nonblocking=False)

def ctype(t):
 if t.startswith('future<'):return 'struct xfuture*'
 if t.startswith('func('):return 'struct xlir_closure*'
 t=t.rstrip('?');return {'void':'void','int':'int64_t','uint64':'uint64_t','bool':'bool','string':'xstrview','bytes':'xvalue*','cbytes':'xbytesview'}.get(t,'int64_t' if t in ENUMS or t=='NetStatus' else 'void*')
def abi(t):
 if t.startswith('future<'):return 'XL6_IR_C_ABI_RUNTIME_FUTURE_PTR'
 if t.startswith('func('):return 'XL6_IR_C_ABI_CLOSURE_PTR'
 t=t.rstrip('?');return {'void':'XL6_IR_C_ABI_VOID','int':'I','uint64':'U','bool':'B','string':'S','bytes':'Y','cbytes':'XL6_IR_C_ABI_C_BYTES'}.get(t,'I' if t in ENUMS or t=='NetStatus' else 'V')
def owner(t):
 t=t.rstrip('?');return 'xl6'+t+'DestroyV2' if t in CLASSES and t not in STATIC_CLASSES else {'CancellationToken':'xl6CancellationTokenDestroyV1','Buffer':'xl6BufferDestroyV1','NetBuffer':'xl6NetBufferDestroyV1','NetBytes':'xl6NetBytesDestroyV1','NetEngine':'xl6NetEngineDestroyV1','TcpStream':'xl6TcpStreamDestroyV1','TlsStream':'xl6TlsStreamDestroyV1','Http1Head':'xl6Http1HeadDestroyV2','HttpHeaders':'xl6HttpHeadersDestroyV2'}.get(t)
def parameters(row):return ([] if row['static'] or row['kind']=='create' else [arg(row['owner'],'self')])+row['args']
def result(row):return row['owner'] if row['kind']=='create' else row['result']
def declaration(row):
 effects=''.join('@'+n+' ' for n in ['pure','no_raise','nonblocking'] if row[n])
 args=', '.join(t+' '+n+(' = '+d if d is not None else '') for t,n,d in row['args'])
 signature=row['kind']+'('+args+')' if row['kind'] in ('create','destroy') else row['kind']+' '+row['name']+'('+args+')'+(' as '+row['result'] if row['result']!='void' else '')
 return '    '+('private ' if row['private'] else '')+('static ' if row['static'] else '')+'extern "'+row['symbol']+'" '+effects+signature+'\n'
def outputs():
 files={};header=['/* Generated exact resident WebSocket ABI. */\n#ifndef XL6_WS_ABI_H\n#define XL6_WS_ABI_H\n#include "xl6_native_class_abi.h"\n#include "xl6_execution_language.h"\n'];symbols=[];contracts=[];enumcontracts=[];source=[];upgrade=[];stream=[]
 for n,items in ENUMS.items():
  source.append('enum '+n+' as int\n'+''.join('    '+k+' = '+str(v)+'\n' for k,v in items)+'end\n')
  enumcontracts.append('static const XL6_IrNumericEnumMemberContract xl6Ir'+n+'Members[]={'+','.join('{"'+k+'",'+str(v)+',0}' for k,v in items)+'};\nstatic const XL6_IrNumericEnumContract xl6Ir'+n+'Contract={"'+n+'",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir'+n+'Members,'+str(len(items))+'};\n')
 for row in CALLS:
  ps=parameters(row);r=result(row);sym=row['symbol'];attrs=[]
  physical=[ctype(t) for t,n,d in ps]
  if row['callback']:
   physical.insert(0,'XL6_NativeClassCallbackContextV1')
   index,argument,returned=row['callback'];attrs+=['.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1','.nativeClassCallbackParameter='+str(index),'.nativeClassCallbackArgumentOwner="'+owner(argument)+'"','.nativeClassCallbackResultOwner="'+owner(returned)+'"','.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE']
  if row['predicate']:
   physical.insert(0,'XL6_NativeClassResultContextV1');index,argument=row['predicate'];attrs+=['.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_PREDICATE_V1','.nativeClassCallbackParameter='+str(index),'.nativeClassCallbackArgumentOwner="'+owner(argument)+'"','.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE']
  if row['field']:
   index,argument=row['field'];physical[index-1]='const struct xvalue*';physical.insert(0,'XL6_NativeClassFieldContextV1');attrs+=['.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1','.nativeClassParameter='+str(index),'.nativeClassOwner="'+owner(argument)+'"','.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE']
  if row['services']:
   physical.insert(0,'XL6_ExecutionServicesV1');attrs+=['.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1']
  header.append('XL6_TEXT_API '+ctype(r)+' '+sym+'('+(','.join(physical) or 'void')+');\n')
  if owner(r):attrs.append('.resultOwner="'+owner(r)+'"')
  if r.endswith('?'):attrs+=['.nullableResult=1','.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE']
  if r=='future<NetStatus>':attrs+=['.resultFutureElementAbi=I','.resultFutureEnum=&xl6IrNetStatusEnumContract','.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE']
  if any(owner(t) for t,n,d in ps):attrs.append('.parameterOwners={'+','.join('"'+owner(t)+'"' if owner(t) and not (row['field'] and row['field'][0]==i+1) else 'NULL' for i,(t,n,d) in enumerate(ps))+'}')
  if row['kind']=='destroy':attrs.append('.nativeHandlePolicy="xl6'+row['owner']+'HandlePolicyV2Get"')
  attrs+=['.'+{'no_raise':'noRaise'}.get(k,k)+'=1' for k in ['pure','no_raise','nonblocking'] if row[k]]
  if r.rstrip('?') in ENUMS:attrs.append('.resultEnum=&xl6Ir'+r.rstrip('?')+'Contract')
  if any(t in ENUMS for t,n,d in ps):attrs.append('.parameterEnums={'+','.join('&xl6Ir'+t+'Contract' if t in ENUMS else 'NULL' for t,n,d in ps)+'}')
  if r=='NetStatus':attrs.append('.resultEnum=&xl6IrNetStatusEnumContract')
  contracts.append('{"'+sym+'",T,'+abi(r)+','+str(len(ps))+',{'+(','.join(abi(t) for t,n,d in ps) or '0')+'},'+','.join(attrs)+'},\n')
  symbols.append('XL6_TEXT_EXPORT('+sym+','+row['component']+')\n')
 for n,c in CLASSES.items():
  extra=(ROOT/'src/stdlib/ws_stream_methods.xl.in').read_text(encoding='utf-8') if n=='WsStream' else ''
  if n=='WsStream':
   for name in ['Writable','Drained','Closed']:
    extra+='\n    async func wait'+name+'UntilAsync('+('uint64 minimum, ' if name=='Writable' else '')+'uint64 deadlineUs, CancellationToken? cancel = null) as NetStatus\n        auto token = cancel\n        if token != null then\n            return await (await self.engine().submit(func() as future<NetStatus>\n                return self.wait'+name+'CancelOnWorker('+('minimum, ' if name=='Writable' else '')+'deadlineUs, token)\n            end, self.worker))\n        end\n        return await (await self.engine().submit(func() as future<NetStatus>\n            return self.wait'+name+'OnWorker('+('minimum, ' if name=='Writable' else '')+'deadlineUs)\n        end, self.worker))\n    end\n'
    extra+='    func wait'+name+'Async('+('uint64 minimum = uint64(1), ' if name=='Writable' else '')+'int timeoutMillis = -1, CancellationToken? cancel = null) as future<NetStatus>\n        return self.wait'+name+'UntilAsync('+('minimum, ' if name=='Writable' else '')+'Http1.deadline(timeoutMillis), cancel)\n    end\n'
  (upgrade if c['component']=='WS_UPGRADE' else stream if n in ('WsStream','WsEvent') else source).append('class '+n+'\n'+('' if n in STATIC_CLASSES else '    private native point handle\n')+''.join(declaration(r) for r in c['methods'])+extra+'end\n')
 files['src/stdlib/xl6_stdlib_ws.inc']='/* Generated declarations. */\n'+''.join('const char* '+name+'=\n'+'\n'.join('    '+json.dumps(line+'\n') for line in ''.join(parts).splitlines())+';\n' for name,parts in [('xl6StdlibWsSource',source),('xl6StdlibWsUpgradeSource',upgrade),('xl6StdlibWsStreamSource',stream)])
 files['src/stdlib/xl6_stdlib_ws_types.inc']=''.join('XL6_WS_TYPE("'+n+'")\n' for n in list(CLASSES)+list(ENUMS))
 header.append('#endif\n');files['src/runtime/xl6_ws_abi.h']=''.join(header);files['src/runtime/xl6_ws_symbols.inc']=''.join(symbols)
 files['src/ir/xl6_ir_ws_contracts.inc']=''.join(contracts);files['src/ir/xl6_ir_ws_enums.inc']=''.join(enumcontracts)
 groups={}
 for n,c in CLASSES.items():
  if n not in STATIC_CLASSES:groups.setdefault(c['component'],[]).append(n)
 domains=[];decl=[];policies=[];ids=[];components=[]
 for comp,types in groups.items():
  loader='Ws'+''.join(s.title() for s in comp.removeprefix('WS_').split('_'))
  content='#include "include/xruntime_decl.h"\n#include "xl6_native_handle.h"\n#include "xl6_ownership_domain_impl.h"\n'
  for n in types:
   content+='extern const XL6_NativeHandlePolicyV1* xl6'+n+'HandlePolicyV2Get(void);\n'
   decl.append('XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6'+n+'HandlePolicyV2Get(void);\n')
   policies.append('XL6_NATIVE_RESOURCE_POLICY("xl6'+n+'DestroyV2",'+comp+',xl6'+n+'HandlePolicyV2Get)\n')
  content+='static bool load(XL6_OwnershipCatalog* catalog)\n{return '+' && '.join('xl6OwnershipCatalogAddHandleV1(catalog,xl6'+n+'HandlePolicyV2Get())' for n in types)+';}\nXL6_OWNERSHIP_DOMAIN('+loader+',load,NULL)\n'
  if comp!='WS_STREAM':files['src/runtime/xl6_'+comp.lower()+'_domain.c']=content
  domains.append('XL6_OWNERSHIP_DOMAIN_ENTRY('+comp+',OWNERSHIP_DOMAIN_'+comp+',xl6OwnershipDomain'+loader+'V1Get)\n')
 files['src/runtime/xl6_ws_resource_policies.inc']=''.join(policies);files['src/runtime/xl6_ws_policy_declarations.h']=''.join(decl)
 domains.append('XL6_OWNERSHIP_DOMAIN_ENTRY(WS_STREAM_TLS,OWNERSHIP_DOMAIN_WS_STREAM_TLS_NATIVE,xl6OwnershipDomainWsStreamTlsV1Get)\n')
 domains.append('XL6_OWNERSHIP_DOMAIN_ENTRY(WS_WAIT,OWNERSHIP_DOMAIN_WS_WAIT,xl6OwnershipDomainWsWaitV1Get)\n')
 files['src/runtime/xl6_ws_ownership_domains.inc']=''.join(domains)
 files['src/runtime/xl6_ws_ownership_domains.h']=''.join('XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomain'+('Ws'+''.join(s.title() for s in c.removeprefix('WS_').split('_')))+'V1Get(void);\n' for c in groups)+'XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainWsStreamTlsV1Get(void);\nXL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainWsWaitV1Get(void);\n'
 for c,f,deps,mods in COMPONENTS:
  ids.append('    XL6_ARTIFACT_'+c+',\n');components.append('{"src/runtime/'+f+'",D('+','.join('R('+d+')' for d in deps)+'),{'+','.join('"XRT_MODULE_'+m+'"' for m in mods)+'}},\n')
 for c in groups:
  ids.append('    XL6_ARTIFACT_OWNERSHIP_DOMAIN_'+c+',\n');components.append('{"src/runtime/xl6_'+c.lower()+'_domain.c",D(R(OWNERSHIP_CATALOG),R('+c+')),{NULL}},\n')
 files['src/compile/xl6_ws_component_ids.inc']=''.join(ids);files['src/compile/xl6_ws_components.inc']=''.join(components)
 files['docs/websocket-language-api.json']=json.dumps(dict(schema=1,classes=CLASSES,enums=ENUMS,calls=CALLS),ensure_ascii=False,indent=2)+'\n'
 return files
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args();stale=[]
 for n,s in outputs().items():
  f=ROOT/n
  if f.is_file() and f.read_text(encoding='utf-8-sig')==s:continue
  if a.check:stale.append(n)
  else:f.write_text(s,encoding='utf-8',newline='\n')
 if stale:raise SystemExit('Stale WebSocket metadata: '+', '.join(stale))
 print(f'WebSocket: {len(CLASSES)} classes, {len(CALLS)} resident declarations')
if __name__=='__main__':main()
