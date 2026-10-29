"""Generate declarations and exact ABI metadata; never generate HTTP algorithms.

The table is shared by language declarations, closed IR contracts, resident
symbol selection and ownership admission. Implementations remain ordinary C.
Run with --check to reject stale generated files.
"""
from pathlib import Path
import argparse, json
from http1_transport_source import sources as transport_sources

ROOT = Path(__file__).resolve().parents[1]
CLASSES = {}
CALLS = []
ENUMS = {
    'HttpVersion': [('v10',10),('v11',11)],
    'HttpMethod': [('invalid',0),('other',1),('get',2),('head',4),('post',8),('put',16),('delete',32),('connect',64),('options',128),('trace',256),('patch',512)],
    'HttpTargetForm': [('origin',1),('absolute',2),('authority',3),('asterisk',4)],
    'HttpBodyMode': [('none',0),('fixed',1),('chunked',2),('close',3),('tunnel',4)],
    'HttpBodyStatus': [('more',0),('data',1),('done',2)],
    'HttpCoding': [('none',0),('identity',1),('gzip',2),('deflate',4)],
    'HttpExpectationResult': [('none',0),('continue100',1),('unsupported',2)],
    'HttpConnectionStatus': [('close',0),('persist',1)],
    'HttpDecodeMode': [('identity',0),('content',1),('passthrough',2)],
    'HttpAuthorityFlags': [('none',0),('hasPort',1),('ipLiteral',2),('emptyPort',4),('portValue',8)],
    'HttpParameterFlags': [('none',0),('hasValue',1),('quoted',2)],
    'HttpExpectationFlags': [('none',0),('hasValue',1),('quoted',2),('hasParameters',4)],
    'HttpTeFlags': [('none',0),('present',1),('acceptsTrailers',2),('hasTransferCodings',4)],
    'HttpTeCodingFlags': [('none',0),('trailers',1),('hasParameters',2),('hasWeight',4)],
    'HttpConnectionOptions': [('none',0),('response',1),('proxy',2),('allowHttp10KeepAlive',4)],
    'HttpHeadFlags': [('none',0),('keepAlive',1),('connectionClose',2),('upgrade',4),('contentLength',8),('chunked',16),('transferEncoding',32),('otherTransferCoding',64)],
    'HttpContentEncodingFlags': [('none',0),('present',1),('identity',2),('unknown',4),('legacy',8)],
    'HttpMessageKind': [('request',1),('response',2)],
    'HttpTargetFlags': [('none',0),('hasScheme',1),('hasAuthority',2),('hasQuery',4)],
    'HttpAcceptEncodingFlags': [('none',0),('present',1),('gzip',2),('deflate',4),('identity',8),('wildcard',16)],
}
STATIC_CLASSES = {'Http','Http1','HttpTrailers'}
COMPONENTS = [
 ('HTTP_COMMON','xl6_http_common.c',[],['HTTP','UNICODE_TEXT','VALUE']),
 ('HTTP_HEADERS','xl6_http_headers.c',['NATIVE_HANDLE'],['HTTP','BUFFER','ATOMIC','UNICODE_TEXT','VALUE']),
 ('HTTP_PARAMETERS','xl6_http_parameters.c',['NATIVE_HANDLE'],['HTTP_PARAM','HTTP_PARAM_HOST','BUFFER','VALUE','UNICODE_TEXT']),
 ('HTTP_AUTHORITY','xl6_http_authority.c',['NATIVE_HANDLE'],['HTTP_HOST']),
 ('HTTP_TARGET','xl6_http_target.c',['HTTP_AUTHORITY','NATIVE_HANDLE'],['HTTP_TARGET']),
 ('HTTP_TOKENS','xl6_http_tokens.c',['HTTP_HEADERS','NATIVE_HANDLE'],['HTTP','BUFFER','VALUE']),
 ('HTTP_EXPECT','xl6_http_expect.c',['HTTP_HEADERS','HTTP_PARAMETERS','NATIVE_HANDLE'],['HTTP_EXPECT','VALUE']),
 ('HTTP_UPGRADE','xl6_http_upgrade.c',['HTTP_HEADERS','NATIVE_HANDLE'],['HTTP_UPGRADE_WRITE','BUFFER']),
 ('HTTP_TE','xl6_http_te.c',['HTTP_HEADERS','HTTP_PARAMETERS','NATIVE_HANDLE'],['HTTP_TE']),
 ('HTTP_CONNECTION','xl6_http_connection.c',['HTTP_HEADERS','HTTP_TOKENS','NATIVE_HANDLE'],['HTTP_CONNECTION']),
 ('HTTP_TRAILER','xl6_http_trailer.c',['HTTP_HEADERS'],['HTTP_TRAILER','BUFFER']),
 ('HTTP_ENCODING','xl6_http_encoding.c',['HTTP_HEADERS','NATIVE_HANDLE'],['HTTP_ENCODING','ATOMIC','BUFFER']),
 ('HTTP_HEAD','xl6_http_head.c',['HTTP_HEADERS','NATIVE_HANDLE'],['HTTP1_HEAD','VALUE']),
 ('HTTP_BODY','xl6_http_body.c',['HTTP_HEAD','HTTP_HEADERS','NATIVE_HANDLE'],['HTTP1_BODY','ATOMIC','BUFFER']),
 ('HTTP_WRITE','xl6_http_write.c',['HTTP_HEADERS'],['HTTP1_HEAD','BUFFER','VALUE']),
 ('HTTP_BODY_WRITE','xl6_http_body_write.c',['HTTP_HEADERS'],['HTTP1_BODY','BUFFER','VALUE']),
 ('HTTP_MESSAGE','xl6_http_message.c',['HTTP_HEAD','HTTP_BODY','NATIVE_HANDLE'],['HTTP1_MESSAGE','VALUE','BUFFER']),
 ('HTTP_DECODE','xl6_http_decode.c',['HTTP_HEADERS','NATIVE_HANDLE'],['HTTP_DECODE','ATOMIC','BUFFER']),
 ('HTTP_NET','xl6_http_net.c',['HTTP_HEAD','NET_BUFFER_CORE'],['HTTP1_NET']),
 ('HTTP_TLS','xl6_http_tls.c',['HTTP_HEAD','TLS_STREAM_LANGUAGE','TLS_SYNC_COMMON','TASK_NET'],['HTTP1_TLS']),
 ('HTTP_BODY_NET','xl6_http_body_net.c',['HTTP_BODY','NET_BUFFER_CORE'],['HTTP1_BODY','NET_BUFFER']),
 ('HTTP_TRANSFER','xl6_http_transfer.c',['HTTP_PARAMETERS','NATIVE_HANDLE'],['HTTP1_HEAD']),
 ('HTTP_DEADLINE','xl6_http_deadline.c',[],['TIME']),
]

def cls(name, component):
    CLASSES[name] = dict(component=component, methods=[])

def call(owner, name, result, args=(), *, symbol=None, kind='func', static=False,
         pure=False, no_raise=False, namespace=None, component=None, nonblocking=True):
    symbol = symbol or 'xl6'+owner+name[0].upper()+name[1:]+'V2'
    row = dict(owner=owner, name=name, result=result, args=list(args), symbol=symbol,
        kind=kind, static=static, pure=pure, no_raise=no_raise, namespace=namespace,
        component=component or CLASSES[owner]['component'],nonblocking=nonblocking)
    CALLS.append(row)
    if owner in CLASSES: CLASSES[owner]['methods'].append(row)
    return row

def arg(t,n,default=None):return (t,n,default)
def method(c,n,r,*a,**k):return call(c,n,r,a,**k)
def destroy(c):method(c,'destroy','void',kind='destroy',no_raise=True)
def get(c,n,r):method(c,n,r,kind='get',pure=True)

cls('Http','HTTP_COMMON')
for name,result,args in [
    ('statusText','string',[arg('int','status')]),
    ('tokenValid','bool',[arg('string','text'),arg('int','size','-1')]),
    ('tokenEqual','bool',[arg('string','left'),arg('string','right')]),
    ('methodCode','HttpMethod',[arg('string','text'),arg('int','size','-1')]),
    ('methodEqual','bool',[arg('string','left'),arg('string','right')]),
    ('methodSafe','bool',[arg('string','text'),arg('int','size','-1')]),
    ('methodIdempotent','bool',[arg('string','text'),arg('int','size','-1')]),
    ('responseContentAllowed','bool',[arg('string','method'),arg('int','status')]),
    ('trimOws','string',[arg('string','text'),arg('int','size','-1')]),
    ('quality','int',[arg('string','text'),arg('int','size','-1')]),
    ('contentLength','uint64',[arg('string','text'),arg('int','size','-1')]),
    ('fieldValueValid','bool',[arg('bytes','data'),arg('int','size','-1')]),
    ('fieldNameEqual','bool',[arg('string','left'),arg('string','right')]),
]:method('Http',name,result,*args,static=True,pure=True,namespace='http')

cls('HttpHeaders','HTTP_HEADERS')
method('HttpHeaders','create','void',arg('int','capacity','16'),kind='create')
destroy('HttpHeaders')
for n,t in [('length','int'),('capacity','int'),('parsedBytes','int')]:get('HttpHeaders',n,t)
for inp,suffix in [('string','Text'),('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
    method('HttpHeaders','parse','HttpHeaders',arg(inp,'block'),arg('int','size','-1'),static=True,symbol='xl6HttpHeadersParse'+suffix+'V2')
method('HttpHeaders','clone','HttpHeaders',pure=True)
method('HttpHeaders','reserve','void',arg('int','capacity'))
method('HttpHeaders','clear','void')
for t,s in [('string','Text'),('bytes','Bytes'),('cbytes','Span')]:
    method('HttpHeaders','add','void',arg('string','name'),arg(t,'value'),arg('int','size','-1'),symbol='xl6HttpHeadersAdd'+s+'V2')
    method('HttpHeaders','set','void',arg('string','name'),arg(t,'value'),arg('int','size','-1'),symbol='xl6HttpHeadersSet'+s+'V2')
method('HttpHeaders','remove','int',arg('string','name'))
method('HttpHeaders','nameAt','string',arg('int','index'),pure=True)
method('HttpHeaders','valueAt','bytes',arg('int','index'),pure=True)
method('HttpHeaders','textAt','string',arg('int','index'),pure=True)
method('HttpHeaders','copyValueInto','void',arg('int','index'),arg('Buffer','output'))
method('HttpHeaders','find','int',arg('string','name'),arg('int','start','0'),pure=True)
method('HttpHeaders','count','int',arg('string','name'),pure=True)
method('HttpHeaders','contains','bool',arg('string','name'),pure=True)
method('HttpHeaders','getBytes','bytes?',arg('string','name'),pure=True)
method('HttpHeaders','getText','string?',arg('string','name'),pure=True)
method('HttpHeaders','getUniqueBytes','bytes?',arg('string','name'),pure=True)
method('HttpHeaders','writeInto','int',arg('Buffer','output'))
method('HttpHeaders','toBytes','bytes',pure=True)
method('HttpHeaders','select','HttpHeaders',arg('string','name'),pure=True)
method('HttpHeaders','tokenCount','int',arg('string','name'),pure=True)
method('HttpHeaders','hasToken','bool',arg('string','name'),arg('string','token'),pure=True)
method('HttpHeaders','expectations','HttpExpectationResult',pure=True,component='HTTP_EXPECT')
method('HttpHeaders','connectionStatus','HttpConnectionStatus',arg('HttpVersion','version','HttpVersion.v11'),arg('HttpConnectionOptions','options','HttpConnectionOptions.none'),pure=True,component='HTTP_CONNECTION')
method('HttpHeaders','teQuality','int',arg('string','coding'),pure=True,component='HTTP_TE')
method('HttpHeaders','teAcceptsTrailers','bool',pure=True,component='HTTP_TE')
method('HttpHeaders','selectEncoding','HttpCoding',arg('HttpCoding','available'),arg('HttpCoding','preferred','HttpCoding.gzip'),pure=True,component='HTTP_ENCODING')

cls('HttpParameters','HTTP_PARAMETERS')
for name,mode in [('parse',0),('directives',1)]:
    method('HttpParameters',name,'HttpParameters',arg('string','text'),arg('int','size','-1'),static=True)
method('HttpParameters','nameValues','HttpParameters',arg('string','text'),arg('int','separator','59'),arg('bool','ignoreEmpty','false'),arg('int','size','-1'),static=True)
destroy('HttpParameters');get('HttpParameters','length','int')
for n,t in [('nameAt','string'),('valueAt','bytes'),('rawValueAt','bytes'),('flagsAt','int'),('tokenValidAt','bool'),('hostValidAt','bool')]:method('HttpParameters',n,t,arg('int','index'),pure=True)
method('HttpParameters','tokenEqualAt','bool',arg('int','index'),arg('string','token'),pure=True)
method('HttpParameters','find','int',arg('string','name'),pure=True)
method('HttpParameters','copyValueInto','int',arg('int','index'),arg('Buffer','output'))
for n,r,args in [
 ('quotedValid','bool',[arg('string','text'),arg('int','size','-1')]),
 ('quote','string',[arg('string','value'),arg('int','size','-1')]),
 ('unquote','bytes',[arg('string','quoted'),arg('int','size','-1')]),
 ('build','string',[arg('string','name'),arg('string','value'),arg('HttpParameterFlags','flags','HttpParameterFlags.hasValue'),arg('int','size','-1')]),
]:method('HttpParameters',n,r,*args,static=True,pure=True)

cls('HttpAuthority','HTTP_AUTHORITY')
method('HttpAuthority','parse','HttpAuthority',arg('string','text'),arg('int','size','-1'),static=True,pure=True)
destroy('HttpAuthority')
for n,t in [('text','string'),('host','string'),('portText','string'),('flags','int'),('hasPort','bool'),('hasPortValue','bool')]:get('HttpAuthority',n,t)
method('HttpAuthority','port','int',arg('int','defaultPort','0'),pure=True)
for n in ['hostValid','ipv4Valid','ipv6Valid']:
    method('HttpAuthority',n,'bool',arg('string','text'),arg('int','size','-1'),static=True,pure=True)
method('HttpAuthority','hostEqual','bool',arg('string','left'),arg('string','right'),static=True,pure=True)

cls('HttpTarget','HTTP_TARGET')
method('HttpTarget','parse','HttpTarget',arg('string','method'),arg('string','target'),arg('int','size','-1'),static=True,pure=True)
destroy('HttpTarget')
for n,t in [('form','HttpTargetForm'),('flags','int'),('method','string'),('text','string'),('scheme','string'),('authorityText','string'),('path','string'),('query','string')]:get('HttpTarget',n,t)
method('HttpTarget','authority','HttpAuthority',arg('string','host','""'),pure=True)

for c in ['HttpTokens','HttpWeightedTokens','HttpList']:
    cls(c,'HTTP_TOKENS')
    method(c,'parse',c,arg('string','text'),arg('int','size','-1'),static=True,pure=True)
    destroy(c);get(c,'length','int')
    method(c,'at','string' if c!='HttpList' else 'bytes',arg('int','index'),pure=True)
method('HttpTokens','fromHeaders','HttpTokens',arg('HttpHeaders','headers'),arg('string','name'),static=True)
method('HttpTokens','contains','bool',arg('string','token'),pure=True)
method('HttpTokens','writeInto','int',arg('Buffer','output'))
method('HttpTokens','toString','string',pure=True)
method('HttpWeightedTokens','qualityAt','int',arg('int','index'),pure=True)
method('HttpList','textAt','string',arg('int','index'),pure=True)

for c,comp in [('HttpExpectations','HTTP_EXPECT'),('HttpUpgrades','HTTP_UPGRADE'),('HttpTe','HTTP_TE')]:
    cls(c,comp)
    method(c,'parse',c,arg('string','text'),arg('int','size','-1'),static=True,pure=True)
    method(c,'fromHeaders',c,arg('HttpHeaders','headers'),static=True)
    destroy(c);get(c,'length','int')
for n,t in [('nameAt','string'),('valueAt','bytes'),('parametersAt','HttpParameters'),('flagsAt','int')]:method('HttpExpectations',n,t,arg('int','index'),pure=True)
get('HttpExpectations','result','HttpExpectationResult')
for n in ['protocolAt','versionAt']:method('HttpUpgrades',n,'string',arg('int','index'),pure=True)
method('HttpUpgrades','toString','string',pure=True)
method('HttpUpgrades','writeInto','int',arg('Buffer','output'))
for n,t in [('codingAt','string'),('parametersAt','HttpParameters'),('qualityAt','int'),('flagsAt','int')]:method('HttpTe',n,t,arg('int','index'),pure=True)
for n,t in [('fieldCount','int'),('transferCodingCount','int'),('flags','int'),('acceptsTrailers','bool')]:get('HttpTe',n,t)
method('HttpTe','quality','int',arg('string','coding'),pure=True)

cls('HttpConnection','HTTP_CONNECTION')
method('HttpConnection','fromHeaders','HttpConnection',arg('HttpHeaders','headers'),arg('HttpVersion','version','HttpVersion.v11'),arg('int','flags','0'),static=True)
destroy('HttpConnection');get('HttpConnection','length','int');get('HttpConnection','status','HttpConnectionStatus')
method('HttpConnection','optionAt','string',arg('int','index'),pure=True)
method('HttpConnection','contains','bool',arg('string','option'),pure=True)
cls('HttpTrailers','HTTP_TRAILER')
for n,r,args in [('nameValid','bool',[arg('string','name')]),('sectionValid','bool',[arg('HttpHeaders','trailers')]),
 ('declaredCount','int',[arg('HttpHeaders','headers')]),('declared','bool',[arg('HttpHeaders','headers'),arg('string','name')]),
 ('names','string',[arg('HttpHeaders','trailers')]),('writeNamesInto','int',[arg('HttpHeaders','trailers'),arg('Buffer','output')])]:
    method('HttpTrailers',n,r,*args,static=True)

cls('HttpAcceptEncoding','HTTP_ENCODING')
method('HttpAcceptEncoding','create','void',kind='create');destroy('HttpAcceptEncoding')
method('HttpAcceptEncoding','fromHeaders','HttpAcceptEncoding',arg('HttpHeaders','headers'),static=True)
method('HttpAcceptEncoding','add','void',arg('string','value'),arg('int','size','-1'))
method('HttpAcceptEncoding','reset','void')
get('HttpAcceptEncoding','flags','int');get('HttpAcceptEncoding','valid','bool')
method('HttpAcceptEncoding','quality','int',arg('HttpCoding','coding'))
method('HttpAcceptEncoding','select','HttpCoding',arg('HttpCoding','available'),arg('HttpCoding','preferred','HttpCoding.gzip'))
cls('HttpContentEncodings','HTTP_ENCODING')
method('HttpContentEncodings','fromHeaders','HttpContentEncodings',arg('HttpHeaders','headers'),static=True)
destroy('HttpContentEncodings')
for n in ['length','fieldCount','decoderCount','unknownCount','flags']:get('HttpContentEncodings',n,'int')
method('HttpContentEncodings','nameAt','string',arg('int','index'),pure=True)
method('HttpContentEncodings','codingAt','HttpCoding',arg('int','index'),pure=True)
method('HttpContentEncodings','writeInto','int',arg('Buffer','output'))
method('HttpContentEncodings','toString','string',pure=True)
method('Http','coding','HttpCoding',arg('string','token'),static=True,pure=True,namespace='http',component='HTTP_ENCODING')
method('Http','codingName','string',arg('HttpCoding','coding'),static=True,pure=True,namespace='http',component='HTTP_ENCODING')

HEAD_LIMITS=[arg('int','maxHead','65536'),arg('int','maxStartLine','8192'),arg('int','maxFieldLine','8192'),arg('int','maxFields','100')]
BODY_LIMITS=[arg('int','maxBody','16777216'),arg('int','maxChunkLine','8192'),arg('int','maxTrailer','16384'),arg('int','maxTrailerLine','8192'),arg('int','maxTrailers','32')]
cls('Http1Head','HTTP_HEAD')
for direction in ['request','response']:
    for inp,suffix in [('string','Text'),('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
        method('Http1Head',direction,'Http1Head?',arg(inp,'input'),*HEAD_LIMITS,arg('int','size','-1'),static=True,
            symbol='xl6Http1Head'+direction.title()+suffix+'V2')
destroy('Http1Head')
for n,t in [('kind','int'),('version','HttpVersion'),('flags','int'),('statusCode','int'),('contentLength','uint64'),('headerBytes','int'),('fieldCount','int'),('methodCode','HttpMethod'),('method','string'),('target','string'),('reason','string')]:get('Http1Head',n,t)
method('Http1Head','headers','HttpHeaders',pure=True)
method('Http1Head','fieldName','string',arg('int','index'),pure=True)
method('Http1Head','fieldValue','bytes',arg('int','index'),pure=True)
method('Http1Head','fieldText','string',arg('int','index'),pure=True)
method('Http1Head','getBytes','bytes?',arg('string','name'),pure=True)
method('Http1Head','getText','string?',arg('string','name'),pure=True)
method('Http1Head','bodyPlan','HttpBodyPlan',arg('string','requestMethod','"GET"'),component='HTTP_BODY')
method('Http1Head','bodyMode','HttpBodyMode',arg('string','requestMethod','"GET"'),pure=True,component='HTTP_BODY')

cls('HttpBodyPlan','HTTP_BODY')
method('HttpBodyPlan','create','void',arg('HttpBodyMode','mode'),arg('uint64','length','0'),kind='create')
destroy('HttpBodyPlan');get('HttpBodyPlan','mode','HttpBodyMode');get('HttpBodyPlan','length','uint64')
cls('Http1BodyReader','HTTP_BODY')
method('Http1BodyReader','create','void',arg('HttpBodyPlan','plan'),*BODY_LIMITS,kind='create')
method('Http1BodyReader','create','void',arg('Http1Head','head'),arg('string','requestMethod','"GET"'),*BODY_LIMITS,kind='create',symbol='xl6Http1BodyReaderCreateHeadV2')
destroy('Http1BodyReader')
for inp,s in [('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
    method('Http1BodyReader','readInto','HttpBodyStatus',arg(inp,'input'),arg('Buffer','output'),arg('bool','end','false'),arg('int','size','-1'),symbol='xl6Http1BodyReaderRead'+s+'V2')
for n,t in [('done','bool'),('failed','bool'),('consumed','int'),('received','uint64'),('wireBytes','uint64'),('remaining','uint64'),('mode','HttpBodyMode')]:get('Http1BodyReader',n,t)
method('Http1BodyReader','reset','void',arg('HttpBodyPlan','plan'),*BODY_LIMITS)
method('Http1BodyReader','trailers','HttpHeaders')

cls('Http1','HTTP_WRITE')
method('Http1','targetValid','bool',arg('string','target'),arg('int','size','-1'),static=True,pure=True,namespace='http1')
for direction,args in [('request',[arg('string','method'),arg('string','target')]),('response',[arg('int','status'),arg('string','reason')])]:
    method('Http1',direction+'Head','bytes',*args,arg('HttpHeaders','headers'),arg('HttpVersion','version','HttpVersion.v11'),static=True,namespace='http1')
    method('Http1','write'+direction.title()+'Head','int',*args,arg('HttpHeaders','headers'),arg('Buffer','output'),arg('HttpVersion','version','HttpVersion.v11'),static=True,namespace='http1')
method('Http1','chunkLine','bytes',arg('uint64','size'),arg('string','extensions','""'),static=True,namespace='http1',component='HTTP_BODY_WRITE')
method('Http1','writeChunkLine','int',arg('uint64','size'),arg('string','extensions'),arg('Buffer','output'),static=True,namespace='http1',component='HTTP_BODY_WRITE')
for inp,s in [('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
    method('Http1','chunk','bytes',arg(inp,'input'),arg('int','size','-1'),static=True,symbol='xl6Http1Chunk'+s+'V2',component='HTTP_BODY_WRITE',namespace='http1')
    method('Http1','writeChunk','int',arg(inp,'input'),arg('Buffer','output'),arg('int','size','-1'),static=True,symbol='xl6Http1WriteChunk'+s+'V2',component='HTTP_BODY_WRITE',namespace='http1')
method('Http1','chunkEnd','bytes',arg('HttpHeaders','trailers'),static=True,namespace='http1',component='HTTP_BODY_WRITE')
method('Http1','writeChunkEnd','int',arg('HttpHeaders','trailers'),arg('Buffer','output'),static=True,namespace='http1',component='HTTP_BODY_WRITE')
method('Http1','parseTrailers','HttpHeaders?',arg('Buffer','input'),*BODY_LIMITS,arg('int','size','-1'),static=True,component='HTTP_BODY')

cls('Http1Message','HTTP_MESSAGE')
for direction in ['request','response']:
    for inp,s in [('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
        a=[arg(inp,'input'),arg('bool','end','false')]
        if direction=='response':a+=[arg('string','requestMethod','"GET"')]
        method('Http1Message',direction,'Http1Message?',*a,*HEAD_LIMITS,*BODY_LIMITS,arg('int','size','-1'),static=True,symbol='xl6Http1Message'+direction.title()+s+'V2')
destroy('Http1Message');get('Http1Message','wireBytes','int');get('Http1Message','bodyBytes','int')
method('Http1Message','head','Http1Head',pure=True)
method('Http1Message','bodyPlan','HttpBodyPlan',pure=True)
method('Http1Message','trailers','HttpHeaders',pure=True)
method('Http1Message','body','bytes',pure=True)
method('Http1Message','copyBodyInto','int',arg('Buffer','output'))

cls('HttpContentDecoder','HTTP_DECODE')
DECODE_OPTIONS=[arg('int','outputLimit','16777216'),arg('int','gzipHeaderLimit','65536'),arg('int','maxCodings','4'),arg('bool','passthroughUnknown','false')]
method('HttpContentDecoder','create','void',arg('HttpHeaders','headers'),*DECODE_OPTIONS,kind='create')
destroy('HttpContentDecoder')
method('HttpContentDecoder','reset','void',arg('HttpHeaders','headers'),*DECODE_OPTIONS)
for inp,s in [('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
    method('HttpContentDecoder','write','void',arg(inp,'input'),arg('Buffer','output'),arg('bool','final','false'),arg('int','size','-1'),symbol='xl6HttpContentDecoderWrite'+s+'V2')
method('HttpContentDecoder','finish','void',arg('Buffer','output'))
for n,t in [('done','bool'),('failed','bool'),('mode','HttpDecodeMode'),('inputSize','uint64'),('outputSize','uint64')]:get('HttpContentDecoder',n,t)

for direction in ['request','response']:
    method('Http1Head',direction+'FromNetBuffer','Http1Head?',arg('NetBuffer','input'),*HEAD_LIMITS,static=True,
        symbol='xl6Http1Head'+direction.title()+'NetBufferV2',component='HTTP_NET')
    method('Http1Head',direction+'FromTls','Http1Head?',arg('TlsStream','input'),*HEAD_LIMITS,arg('int','timeoutMillis','-1'),static=True,
        symbol='xl6Http1Head'+direction.title()+'TlsV2',component='HTTP_TLS',nonblocking=False)
method('Http1BodyReader','readFromNetBuffer','HttpBodyStatus',arg('NetBuffer','input'),arg('Buffer','output'),arg('bool','end','false'),component='HTTP_BODY_NET')
method('Http1BodyReader','discardFromNetBuffer','HttpBodyStatus',arg('NetBuffer','input'),arg('bool','end','false'),component='HTTP_BODY_NET')
for n in ['wire','targetBytes','reasonBytes']:method('Http1Head',n,'bytes',pure=True)
cls('HttpTransferCodings','HTTP_TRANSFER')
method('HttpTransferCodings','parse','HttpTransferCodings',arg('string','text'),arg('int','size','-1'),static=True,pure=True)
destroy('HttpTransferCodings');get('HttpTransferCodings','length','int')
method('HttpTransferCodings','nameAt','string',arg('int','index'),pure=True)
method('HttpTransferCodings','parametersAt','HttpParameters',arg('int','index'),pure=True)
method('HttpUpgrades','elementAt','string',arg('int','index'),pure=True)
method('HttpUpgrades','writeElementInto','int',arg('int','index'),arg('Buffer','output'))
method('Http1','deadline','uint64',arg('int','timeoutMillis','-1'),static=True,namespace='http1',component='HTTP_DEADLINE')
for c,n in [('HttpParameters','parse'),('HttpParameters','directives'),('HttpTokens','parse'),
    ('HttpWeightedTokens','parse'),('HttpList','parse'),('HttpExpectations','parse'),
    ('HttpUpgrades','parse'),('HttpTe','parse'),('HttpTransferCodings','parse')]:
    for t,s in [('bytes','Bytes'),('cbytes','Span'),('Buffer','Buffer')]:
        method(c,n,c,arg(t,'input'),arg('int','size','-1'),static=True,pure=t!='Buffer',symbol='xl6'+c+n[0].upper()+n[1:]+s+'V2')
for t,s in [('string','Text'),('cbytes','Span')]:
    method('Http','fieldValueValid','bool',arg(t,'input'),arg('int','size','-1'),static=True,pure=True,namespace='http',symbol='xl6HttpFieldValueValid'+s+'V2')
for n,r in [('quote','bytes'),('unquote','bytes'),('quotedValid','bool')]:
    for t,s in [('bytes','Bytes'),('cbytes','Span')]:
        method('HttpParameters',n,r,arg(t,'input'),arg('int','size','-1'),static=True,pure=True,symbol='xl6HttpParameters'+n[0].upper()+n[1:]+s+'V2')
for t,s in [('bytes','Bytes'),('cbytes','Span')]:
    method('HttpParameters','build','bytes',arg('string','name'),arg(t,'value'),arg('HttpParameterFlags','flags','HttpParameterFlags.hasValue'),arg('int','size','-1'),static=True,pure=True,symbol='xl6HttpParametersBuild'+s+'V2')

# Flags keep their exact nominal enum contract while retaining the native
# int64 physical call shape. No integer/enum conflation reaches Cgen.
for row in CALLS:
    row['args']=[(t,'endOfStream' if n=='end' else n,d) for t,n,d in row['args']]
    if (row['owner'],row['name']) in [('HttpParameters','valueAt'),('HttpParameters','rawValueAt'),('HttpExpectations','valueAt')]:row['result']='bytes?'
    e={('HttpAuthority','flags'):'HttpAuthorityFlags',('Http1Head','flags'):'HttpHeadFlags',
       ('HttpParameters','flagsAt'):'HttpParameterFlags',('HttpExpectations','flagsAt'):'HttpExpectationFlags',
       ('HttpTe','flags'):'HttpTeFlags',('HttpTe','flagsAt'):'HttpTeCodingFlags',
       ('HttpContentEncodings','flags'):'HttpContentEncodingFlags',('HttpTarget','flags'):'HttpTargetFlags',
       ('HttpAcceptEncoding','flags'):'HttpAcceptEncodingFlags',('Http1Head','kind'):'HttpMessageKind'}.get((row['owner'],row['name']))
    if e:row['result']=e
    if row['owner']=='HttpConnection' and row['name']=='fromHeaders':row['args'][-1]=arg('HttpConnectionOptions','flags','HttpConnectionOptions.none')
    # These objects are immutable, valid by construction, and scalar getters
    # only read their native record. Mutable/gated queries and allocating text
    # getters retain may-raise. No exception scope is needed around a plain read.
    if row['kind']=='get' and row['owner'] in {
        'HttpParameters','HttpAuthority','HttpTarget','HttpTokens','HttpWeightedTokens','HttpList',
        'HttpExpectations','HttpUpgrades','HttpTe','HttpConnection','HttpContentEncodings',
        'Http1Head','HttpBodyPlan','Http1Message','HttpTransferCodings'} and row['result'] in {'int','uint64','bool',*ENUMS}:
        row['no_raise']=True

def ctype(t):
    t=t.rstrip('?')
    return {'void':'void','int':'int64_t','uint64':'uint64_t','bool':'bool','string':'xstrview','bytes':'xvalue*','cbytes':'xbytesview'}.get(t, 'int64_t' if t in ENUMS else 'void*')
def abi(t):
    t=t.rstrip('?')
    return {'void':'XL6_IR_C_ABI_VOID','int':'I','uint64':'U','bool':'B','string':'S','bytes':'Y','cbytes':'XL6_IR_C_ABI_C_BYTES'}.get(t,'I' if t in ENUMS else 'V')
def owner(t):
    t=t.rstrip('?')
    return 'xl6'+t+'DestroyV2' if t in CLASSES and t not in STATIC_CLASSES else {'Buffer':'xl6BufferDestroyV1','NetBuffer':'xl6NetBufferDestroyV1','TlsStream':'xl6TlsStreamDestroyV1'}.get(t)
def parameters(row):
    return ([arg(row['owner'],'self')] if row['kind'] not in ('create',) and not row['static'] and row['owner'] not in STATIC_CLASSES else [])+row['args']
def physical_result(row):return row['owner'] if row['kind']=='create' else row['result']
def declaration(row, namespace=False):
    args=', '.join(t+' '+n+(' = '+d if d is not None else '') for t,n,d in row['args'])
    effects=(' @pure' if row['pure'] else '')+(' @no_raise' if row['no_raise'] else '')+(' @nonblocking' if row['nonblocking'] else '')
    head='    '+('static ' if row['static'] else '') if not namespace else ''
    prefix=head+'extern "'+row['symbol']+'"'+effects+' '
    if namespace:return prefix+'func __xl_std_'+row['namespace']+'_'+row['name']+'('+args+') as '+row['result']+'\n'
    if row['kind'] in ('create','destroy'):return prefix+row['kind']+'('+args+')\n'
    return prefix+row['kind']+' '+row['name']+'('+args+')'+(' as '+row['result'] if row['result']!='void' else '')+'\n'

def outputs():
    files={}; header=['/* Generated by tools/gen_stdlib_http.py. */\n#ifndef XL6_HTTP_ABI_H\n#define XL6_HTTP_ABI_H\n']
    contracts=['/* Generated exact resident signatures. */\n']; symbols=[]; enums=[]; enumcontracts=[]
    for name,items in ENUMS.items():
        enums.append('enum '+name+' as int\n'+''.join('    '+n+' = '+str(v)+'\n' for n,v in items)+'end\n')
        enumcontracts.append('static const XL6_IrNumericEnumMemberContract xl6Ir'+name+'Members[]={'+','.join('{"'+n+'",'+str(abs(v))+','+str(int(v<0))+'}' for n,v in items)+'};\n')
        enumcontracts.append('static const XL6_IrNumericEnumContract xl6Ir'+name+'Contract={"'+name+'",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir'+name+'Members,'+str(len(items))+'};\n')
    for row in CALLS:
        ps=parameters(row);r=physical_result(row);sym=row['symbol']
        header.append('XL6_TEXT_API '+ctype(r)+' '+sym+'('+(','.join(ctype(t) for t,n,d in ps) or 'void')+');\n')
        attrs=[]
        if owner(r):attrs.append('.resultOwner="'+owner(r)+'"')
        if r.endswith('?'):
            attrs.append('.nullableResult=1')
            # NULL is normal absence / More, so it cannot also discriminate
            # errors. Reuse the established optional-owner checked scope.
            attrs.append('.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE')
        if any(owner(t) for t,n,d in ps):attrs.append('.parameterOwners={'+','.join('"'+owner(t)+'"' if owner(t) else 'NULL' for t,n,d in ps)+'}')
        if row['kind']=='destroy':attrs.append('.nativeHandlePolicy="xl6'+row['owner']+'HandlePolicyV2Get"')
        if row['pure']:attrs.append('.pure=1')
        if row['no_raise']:attrs.append('.noRaise=1')
        if row['nonblocking']:attrs.append('.nonblocking=1')
        if r.rstrip('?') in ENUMS:attrs.append('.resultEnum=&xl6Ir'+r.rstrip('?')+'Contract')
        if any(t in ENUMS for t,n,d in ps):attrs.append('.parameterEnums={'+','.join('&xl6Ir'+t+'Contract' if t in ENUMS else 'NULL' for t,n,d in ps)+'}')
        contracts.append('{"'+sym+'",T,'+abi(r)+','+str(len(ps))+',{'+(','.join(abi(t) for t,n,d in ps) or '0')+'},'+','.join(attrs)+'},\n')
        symbols.append('XL6_TEXT_EXPORT('+sym+','+row['component']+')\n')
    header.append('#endif\n');files['src/runtime/xl6_http_abi.h']=''.join(header)
    files['src/runtime/xl6_http_symbols.inc']=''.join(symbols)
    files['src/ir/xl6_ir_http_contracts.inc']=''.join(contracts)
    files['src/ir/xl6_ir_http_enums.inc']=''.join(enumcontracts)
    source=''.join(enums);headsource='';bodysource='';bodyextra='';headextras={}
    for name,data in CLASSES.items():
        part='class '+name+'\n'
        if name not in STATIC_CLASSES:part+='    private native point handle\n'
        if name=='Http1Head':
            part+=''.join(declaration(row) for row in data['methods'] if row['component'] not in ('HTTP_NET','HTTP_TLS'))
            headsource=part
            for comp in ('HTTP_NET','HTTP_TLS'):headextras[comp]=''.join(declaration(row) for row in data['methods'] if row['component']==comp)
        elif name=='Http1BodyReader':
            part+=''.join(declaration(row) for row in data['methods'] if row['component']!='HTTP_BODY_NET')
            bodysource=part;bodyextra=''.join(declaration(row) for row in data['methods'] if row['component']=='HTTP_BODY_NET')
        else:source+=part+''.join(declaration(row) for row in data['methods'])+'end\n'
    def cstring(s):return '\n'.join('    '+json.dumps(line+'\n',ensure_ascii=False) for line in s.splitlines())
    files['src/stdlib/xl6_stdlib_http_transport.inc']='/* Generated from ordinary xlang sources. */\n'+''.join('const char* '+n+'=\n'+cstring(s)+';\n' for n,s in transport_sources(ROOT).items())
    files['src/stdlib/xl6_stdlib_http.inc']='/* Generated declarations, no protocol implementation. */\nconst char* xl6StdlibHttpClassSource =\n'+cstring(source)+';\n'+''.join('const char* '+name+'=\n'+cstring(text)+';\n' for name,text in [('xl6StdlibHttpHeadSource',headsource),('xl6StdlibHttpHeadNetSource',headextras['HTTP_NET']),('xl6StdlibHttpHeadTlsSource',headextras['HTTP_TLS']),('xl6StdlibHttpBodySource',bodysource),('xl6StdlibHttpBodyNetSource',bodyextra)])+''.join('static const char '+row['symbol']+'Body[]=\n'+cstring(declaration(row,True))+';\n' for row in CALLS if row['namespace'])
    registry=''.join('    {"'+r['namespace']+'","'+r['name']+'","__xl_std_'+r['namespace']+'_'+r['name']+'",NULL,NULL,0,"'+r['result']+'",'+r['symbol']+'Body},\n' for r in CALLS if r['namespace'])
    files['src/stdlib/xl6_stdlib_http_functions.inc']=registry
    files['src/stdlib/xl6_stdlib_http_types.inc']=''.join('XL6_HTTP_TYPE("'+n+'")\n' for n in list(CLASSES)+list(ENUMS)+['Http1Limits','Http1TransportState','Http1TcpTransport','Http1TcpTransportHandoff','Http1TlsTransport','Http1TlsTransportHandoff'])
    groups={}
    for n,c in CLASSES.items():
        if n not in STATIC_CLASSES:groups.setdefault(c['component'],[]).append(n)
    policies=[];domains=[];domainheaders=[]
    for comp,types in groups.items():
        loader='Http'+''.join(s.title() for s in comp.removeprefix('HTTP_').split('_'))
        contents='#include "include/xruntime_decl.h"\n#include "xl6_native_handle.h"\n#include "xl6_ownership_domain_impl.h"\n'
        for n in types:
            contents+='extern const XL6_NativeHandlePolicyV1* xl6'+n+'HandlePolicyV2Get(void);\n'
            policies.append('XL6_NATIVE_RESOURCE_POLICY("xl6'+n+'DestroyV2",'+comp+',xl6'+n+'HandlePolicyV2Get)\n')
        contents+='static bool load(XL6_OwnershipCatalog* catalog)\n{return '+ ' && '.join('xl6OwnershipCatalogAddHandleV1(catalog,xl6'+n+'HandlePolicyV2Get())' for n in types)+';}\nXL6_OWNERSHIP_DOMAIN('+loader+',load,NULL)\n'
        files['src/runtime/xl6_'+comp.lower()+'_domain.c']=contents
        domains.append('XL6_OWNERSHIP_DOMAIN_ENTRY('+comp+',OWNERSHIP_DOMAIN_'+comp+',xl6OwnershipDomain'+loader+'V1Get)\n')
        domainheaders.append('XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomain'+loader+'V1Get(void);\n')
    files['src/runtime/xl6_http_resource_policies.inc']=''.join(policies)
    files['src/runtime/xl6_http_ownership_domains.inc']=''.join(domains)
    files['src/runtime/xl6_http_ownership_domains.h']=''.join(domainheaders)
    files['src/runtime/xl6_http_policy_declarations.h']=''.join('XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6'+n+'HandlePolicyV2Get(void);\n' for types in groups.values() for n in types)
    ids=[];components=[]
    for comp,file,deps,modules in COMPONENTS:
        ids.append('    XL6_ARTIFACT_'+comp+',\n')
        components.append('{"src/runtime/'+file+'",D('+','.join('R('+d+')' for d in deps)+'),{'+','.join('"XRT_MODULE_'+m+'"' for m in modules)+'}},\n')
    for comp in groups:
        ids.append('    XL6_ARTIFACT_OWNERSHIP_DOMAIN_'+comp+',\n')
        components.append('{"src/runtime/xl6_'+comp.lower()+'_domain.c",D(R(OWNERSHIP_CATALOG),R('+comp+')),{NULL}},\n')
    files['src/compile/xl6_http_component_ids.inc']=''.join(ids)
    files['src/compile/xl6_http_components.inc']=''.join(components)
    # Index generators deliberately read the literal registry table. Keep the
    # generated block visible there, without changing their preprocessing rules.
    import re
    path=ROOT/'src/stdlib/xl6_stdlib_data.c';text=path.read_text(encoding='utf-8-sig')
    begin='/* BEGIN GENERATED HTTP FUNCTIONS */\n';end='/* END GENERATED HTTP FUNCTIONS */'
    if begin in text:
        first=text.index(begin)+len(begin);last=text.index(end,first)
        text=text[:first]+registry+text[last:];files['src/stdlib/xl6_stdlib_data.c']=text
        table=text.split('const XL6_StdlibFunc xl6StdlibFuncs[] = {',1)[1].split('\n};',1)[0]
        count=len(re.findall(r'(?m)^\s*\{"[^\"]*",\s*"[^\"]*",',table))
        path=ROOT/'src/stdlib/xl6_stdlib_data.h';header=path.read_text(encoding='utf-8-sig')
        files['src/stdlib/xl6_stdlib_data.h']=re.sub(r'(#define XL6_STDLIB_FUNC_COUNT )\d+',r'\g<1>'+str(count),header)
    files['docs/http-language-api.json']=json.dumps(dict(schema=1,classes=CLASSES,enums=ENUMS,calls=CALLS),ensure_ascii=False,indent=2)+'\n'
    return files

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
    stale=[]
    for name,text in outputs().items():
        path=ROOT/name
        if path.is_file() and path.read_text(encoding='utf-8-sig')==text:continue
        if a.check:stale.append(name)
        else:path.write_text(text,encoding='utf-8',newline='\n')
    if stale:raise SystemExit('Stale HTTP metadata: '+', '.join(stale))
    print(f'HTTP: {len(CLASSES)} classes, {len(CALLS)} exact resident entries')
if __name__=='__main__':main()
