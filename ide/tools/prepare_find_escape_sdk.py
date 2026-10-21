"""Build a private SDK66 with bounded, scalar-valid find escape decoding."""
from datetime import datetime, timezone
import difflib, hashlib, json, os, shutil, subprocess, time, uuid
from pathlib import Path

IDE=Path(__file__).resolve().parents[1]
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,v):
    t=p.with_suffix('.tmp');t.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(t,p)

HEX_HELPER=r'''/* Check the remaining length before reading any escape digits. */
static int __xuiFindReadHex(const char* sText, int iAvailable, int iDigits, unsigned int* pValue)
{
	unsigned int iValue = 0u;
	int i;
	if ( iAvailable < iDigits ) return 0;
	for ( i = 0; i < iDigits; i++ ) {
		int iHex = __xuiFindHex(sText[i]);
		if ( iHex < 0 ) return 0;
		iValue = (iValue << 4) | (unsigned int)iHex;
	}
	*pValue = iValue;
	return 1;
}

'''
HEX_CASE=r'''		case 'x':
			if ( !__xuiFindReadHex(sText + i + 1, iTextLength - i - 1, 2, &iCodepoint) ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid hex escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			if ( iCodepoint == 0u ) {
				__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			i += 2;
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, (char)iCodepoint);
			break;
		case 'u':
			if ( !__xuiFindReadHex(sText + i + 1, iTextLength - i - 1, 4, &iCodepoint) ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid unicode escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			i += 4;
			if ( iCodepoint >= 0xD800u && iCodepoint <= 0xDBFFu ) {
				unsigned int iLow;
				/* A UTF-16 high surrogate must be followed by a complete low one. */
				if ( iTextLength - i - 1 < 6 || sText[i + 1] != '\\' || sText[i + 2] != 'u' ||
				     !__xuiFindReadHex(sText + i + 3, iTextLength - i - 3, 4, &iLow) ||
				     iLow < 0xDC00u || iLow > 0xDFFFu ) {
					__xuiFindSetError(sError, iErrorCapacity, "invalid unicode surrogate pair");
					iRet = XUI_ERROR_INVALID_ARGUMENT;
					goto fail;
				}
				iCodepoint = 0x10000u + ((iCodepoint - 0xD800u) << 10) + (iLow - 0xDC00u);
				i += 6;
			} else if ( iCodepoint >= 0xDC00u && iCodepoint <= 0xDFFFu ) {
				__xuiFindSetError(sError, iErrorCapacity, "unpaired unicode low surrogate");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			if ( iCodepoint == 0u ) {
				__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			iRet = __xuiFindAppendUtf8(&sOutput, &iLength, &iCapacity, iCodepoint);
			break;
'''

def main():
    parent_receipt=IDE/'build/editor-sdk-dpi-paint-3b1f3d941ddd/sdk-consumer-build.json'
    parent=read(parent_receipt);descriptor=Path(parent['sourceManifest']);before=read(descriptor)
    assert parent['exitCode']==0 and parent['sourceUnchanged'] and sha(descriptor)==parent['sourceManifestSha256']
    root=IDE/'build'/('editor-sdk-find-escapes-'+uuid.uuid4().hex[:12]);source=root/'source';source.mkdir(parents=True)
    protected={parent_receipt:sha(parent_receipt),descriptor:sha(descriptor),Path(__file__):sha(__file__)}
    for row in before['files']:
        path=Path(parent['source'])/row['path'];assert sha(path)==row['sha256'];protected[path]=row['sha256']
        target=source/row['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    for row in parent['outputs']:assert sha(row['path'])==row['sha256'];protected[Path(row['path'])]=row['sha256']
    target=source/'src/xui_find.c';old=target.read_text(encoding='utf-8');content=old
    marker='static int __xuiFindAppendBytes('
    assert content.count(marker)==1;content=content.replace(marker,HEX_HELPER+marker)
    declarations='\tint iHex0;\n\tint iHex1;'
    decode_start=content.index('static int __xuiFindDecodeEscapes(')
    decode_end=content.index('static int __xuiFindDupMaybeEscaped(',decode_start)
    decoder=content[decode_start:decode_end]
    assert decoder.count(declarations)==1;decoder=decoder.replace(declarations,'\tint iTextLength;')
    content=content[:decode_start]+decoder+content[decode_end:]
    marker='\t*psOutput = NULL;\n\tsOutput = NULL;'
    assert content.count(marker)==1;content=content.replace(marker,'\t*psOutput = NULL;\n\tiTextLength = (int)strlen(sText);\n\tsOutput = NULL;')
    start=content.index("\t\tcase 'x':");end=content.index('\t\tdefault:',start);content=content[:start]+HEX_CASE+content[end:]
    # Regex replacements retain backreferences but share the scalar/NUL rules.
    append_start=content.index('static int __xuiFindAppendReplacement(')
    append_end=content.index('XUI_API int xuiFindText(',append_start)
    append=content[append_start:append_end]
    assert append.count(declarations)==1;append=append.replace(declarations,'\tint iTextLength;')
    marker='\tsReplacement = (pEngine->sReplacement != NULL) ? pEngine->sReplacement : "";'
    assert append.count(marker)==1;append=append.replace(marker,marker+'\n\tiTextLength = (int)strlen(sReplacement);')
    start=append.index("\t\t\t\tcase 'x':");end=append.index('\t\t\t\tdefault:',start)
    case=HEX_CASE.replace('sText','sReplacement').replace('&sOutput, &iLength, &iCapacity','psOutput, pLength, pCapacity')
    case=''.join(line for line in case.splitlines(True) if '__xuiFindSetError(' not in line).replace('goto fail;','return iRet;')
    case=''.join('\t\t'+line if line.strip() else line for line in case.splitlines(True))
    append=append[:start]+case+append[end:];content=content[:append_start]+append+content[append_end:]
    marker='static int __xuiFindEngineCreate('
    prototype='static int __xuiFindAppendReplacement(xui_find_engine_t* pEngine, char** psOutput, int* pLength, int* pCapacity, const char* sText, const xui_find_result_t* pResult);\n\n'
    assert content.count(marker)==1;content=content.replace(marker,prototype+marker)
    marker='\t\tif ( iRet != XUI_OK ) return iRet;\n\t}\n\tif ( (iFlags & XUI_FIND_REGEX) != 0 ) {\n#ifdef XRT_NO_REGEX'
    validation='''\t\tif ( iRet != XUI_OK ) return iRet;
		if ( (iFlags & (XUI_FIND_REGEX | XUI_FIND_ESCAPE)) == (XUI_FIND_REGEX | XUI_FIND_ESCAPE) ) {
			/* Validate before matching, including when there are no matches. */
			xui_find_result_t tEmpty = {0};
			char* sValidation = NULL;
			int iLength = 0, iCapacity = 0;
			iRet = __xuiFindAppendReplacement(pEngine, &sValidation, &iLength, &iCapacity, "", &tEmpty);
			xrtFree(sValidation);
			if ( iRet != XUI_OK ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid replacement escape");
				return iRet;
			}
		}
	}
	if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
#ifdef XRT_NO_REGEX'''
    assert content.count(marker)==1;content=content.replace(marker,validation)
    marker='\t__xuiFindSetError(sError, iErrorCapacity, (iRet == XUI_OK) ? "" : "not found");'
    assert content.count(marker)==1
    content=content.replace(marker,'\tif ( iRet == XUI_OK ) __xuiFindSetError(sError, iErrorCapacity, "");\n\telse if ( iRet == XUI_ERROR_UNSUPPORTED ) __xuiFindSetError(sError, iErrorCapacity, "not found");')
    target.write_text(content,encoding='utf-8')
    patch=IDE/'third_party/sdk_patches/find-escapes-v1';patch.mkdir(parents=True,exist_ok=True)
    payload=patch/'xui_find.c'
    if payload.exists() and payload.read_text(encoding='utf-8')!=content:
        previous=IDE/'build/editor-sdk-find-escapes-5f61326f9189/source/src/xui_find.c'
        assert sha(payload)==sha(previous)
        shutil.copy2(payload,root/'previous-find-patch.c')
    payload.write_text(content,encoding='utf-8')
    protected[payload]=sha(payload)
    (root/'find-escapes.diff').write_text(''.join(difflib.unified_diff(old.splitlines(True),content.splitlines(True),fromfile='SDK65/src/xui_find.c',tofile='SDK66/src/xui_find.c')),encoding='utf-8')
    ui=source/'src/xui_code_edit.c';ui_old=ui.read_text(encoding='utf-8')
    marker='''	} else if ( iRet != XUI_OK ) {
		__xuiCodeEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pCodeEdit), XUI_TR_FIND_NOT_FOUND));
	}
}

static void __xuiCodeEditFindWindowClose'''
    replacement='''	} else if ( iRet != XUI_OK ) {
		__xuiCodeEditFindWindowSetStatus(pData, xuiTranslate(xuiWidgetGetContext(pCodeEdit),
			iRet == XUI_ERROR_INVALID_ARGUMENT ? XUI_TR_FIND_INVALID_PATTERN : XUI_TR_FIND_NOT_FOUND));
	}
}

static void __xuiCodeEditFindWindowClose'''
    assert ui_old.count(marker)==1;ui_content=ui_old.replace(marker,replacement)
    marker='''cleanup:
	xrtFree(sText);
	return iRet;
}

static int __xuiCodeEditApplyFindResult'''
    replacement='''cleanup:
	if ( iRet != XUI_OK ) {
		__xuiCodeEditClearFindResults(pData);
		(void)__xuiCodeEditSyncFindResultList(pWidget, pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	xrtFree(sText);
	return iRet;
}

static int __xuiCodeEditApplyFindResult'''
    assert ui_content.count(marker)==1;ui_content=ui_content.replace(marker,replacement)
    scope_start=ui_content.index('XUI_API int xuiCodeFindScopeFindAll(')
    scope_end=ui_content.index('XUI_API int xuiCodeFindScopeGetResultCount(',scope_start)
    scope=ui_content[scope_start:scope_end]
    assert scope.count('return iRet;')==2;scope=scope.replace('return iRet;','goto fail;')
    marker='\treturn XUI_OK;\n}'
    replacement='''	return XUI_OK;
fail:
	pScope->iResultCount = 0;
	pScope->iActiveIndex = -1;
	for ( i = 0; i < pScope->iEditorCount; i++ ) {
		pEditor = pScope->pEditors[i].pEditor;
		pData = __xuiCodeEditGetData(pEditor);
		if ( pData == NULL ) continue;
		__xuiCodeEditClearFindResults(pData);
		(void)xuiWidgetInvalidate(pEditor, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}'''
    assert scope.count(marker)==1;scope=scope.replace(marker,replacement)
    ui_content=ui_content[:scope_start]+scope+ui_content[scope_end:];ui.write_text(ui_content,encoding='utf-8')
    ui_payload=patch/'xui_code_edit.c'
    if ui_payload.exists() and ui_payload.read_text(encoding='utf-8')!=ui_content:
        previous=IDE/'build/editor-sdk-find-escapes-98b02431d4c3/source/src/xui_code_edit.c'
        assert sha(ui_payload)==sha(previous);shutil.copy2(ui_payload,root/'previous-find-ui-patch.c')
    ui_payload.write_text(ui_content,encoding='utf-8');protected[ui_payload]=sha(ui_payload)
    (root/'find-status.diff').write_text(''.join(difflib.unified_diff(ui_old.splitlines(True),ui_content.splitlines(True),fromfile='SDK65/src/xui_code_edit.c',tofile='SDK66/src/xui_code_edit.c')),encoding='utf-8')
    strict=Path(parent['command'][3]);assert sha(strict)==parent['strictBuildScriptSha256'];protected[strict]=sha(strict)
    shutil.copy2(strict,root/'build_strict_sdk.bat');shutil.copy2(__file__,root/Path(__file__).name)
    files=[dict(path=p.relative_to(source).as_posix(),sha256=sha(p),size=p.stat().st_size) for p in sorted(source.rglob('*')) if p.is_file()]
    manifest=root/'source-manifest.json'
    write(manifest,dict(root=str(root),source=str(source),files=files,parentManifest=str(descriptor),parentManifestSha256=sha(descriptor),sdkVersion=66,privateRevision=3,publicHeadersChanged=False,changedSources=['src/xui_find.c','src/xui_code_edit.c'],findEscapePatchSha256=sha(payload),findStatusPatchSha256=sha(ui_payload),productionChanged=False))
    log=root/'actual-sdk-build.log';receipt=root/'sdk-build.json';command=[os.environ['COMSPEC'],'/d','/c',str(root/'build_strict_sdk.bat'),'ide']
    record=dict(status='building',source=str(source),sourceManifest=str(manifest),sourceManifestSha256=sha(manifest),utc=datetime.now(timezone.utc).isoformat(),command=command,log=str(log),wholeSdkWerror=True,strictBuildScriptSha256=sha(strict),productionChanged=False,sdkInstalled=False,compiler=parent['compiler'],compilerPath=parent['compilerPath'],compilerSha256=parent['compilerSha256'])
    assert sha(record['compilerPath'])==record['compilerSha256'];protected[Path(record['compilerPath'])]=record['compilerSha256']
    write(receipt,record);print(root,flush=True);env=dict(os.environ);env['PATH']=r'E:\software\w64devkit\bin;'+env.get('PATH','');started=time.monotonic()
    with log.open('xb') as stream:
        child=subprocess.Popen(command,cwd=source,env=env,stdout=stream,stderr=stream,creationflags=0x08000000);record['pid']=child.pid
        while child.poll() is None:
            record.update(elapsedSeconds=round(time.monotonic()-started,3),lastConfirmedLiveUtc=datetime.now(timezone.utc).isoformat());write(receipt,record);time.sleep(.5)
    for path,digest in protected.items():assert sha(path)==digest,str(path)
    record['sourceUnchanged']=all(sha(source/r['path'])==r['sha256'] for r in files);assert record['sourceUnchanged']
    record.update(status='sdk-build-finished',exitCode=child.returncode,elapsedSeconds=round(time.monotonic()-started,3),logSha256=sha(log),parentUnchanged=True,finishedUtc=datetime.now(timezone.utc).isoformat(),outputs=[dict(path=str(source/'build/ide'/name),sha256=sha(source/'build/ide'/name),size=(source/'build/ide'/name).stat().st_size) for name in ('xge.dll','xge.lib','xge_res.o') if (source/'build/ide'/name).exists()])
    write(receipt,record);print(json.dumps(dict(exitCode=child.returncode,report=str(receipt),elapsedSeconds=record['elapsedSeconds'])),flush=True)
    if child.returncode:print(log.read_text(encoding='utf-8',errors='replace')[-4000:],flush=True)
    return child.returncode
if __name__=='__main__':raise SystemExit(main())
