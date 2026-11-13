xls_context* xideRenameScopeProvider(const xide_rename_scope* scope,const xls_context* claimed,uint64_t* owners,int (*cancelled)(void*),void* user,enum xide_rename_scope_error* why)
{
 enum xide_rename_scope_error error=XIDE_RENAME_SCOPE_ARGUMENT;xls_context* expected=NULL;uint64_t candidates=0,matched=0;
 if(owners)*owners=0;
 const xls_context_spec* claim=claimed?xlsContextSpec(claimed):NULL;
 if(!scope || !claim || !owners || (claim->kind!=XLS_CONTEXT_XLANG&&claim->kind!=XLS_CONTEXT_PROJECT) || claim->lists[XLS_CONTEXT_MEMBERS].count || strcmp(claim->analysisProfile,XIDE_ANALYSIS_PROFILE) || !xidePathHasExtension(claim->entry,".xl") || !xrtPathIsAbs(claim->entry))goto done;
 if(stopped(cancelled,user)){error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
 if(!xideRenameScopeOwners(scope,claim->entry,&candidates))goto done;
 for(size_t i=0;i<xideRenameScopeCount(scope);++i){
  if(stopped(cancelled,user)){error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
  if(!(candidates&(UINT64_C(1)<<i)))continue;
  const xls_context* configured=xideRenameScopeAt(scope,i);xls_context_spec local=*xlsContextSpec(configured);
  /* A coarse rejection only; full authoritative canonical bytes below still
   * check every option, including fields added to the core spec in the future. */
  if(local.kind!=claim->kind || strcmp(local.project,claim->project) || strcmp(local.configuration,claim->configuration))continue;
  if(xlsContextEqual(configured,claimed)){
   matched|=UINT64_C(1)<<i;if(!expected)expected=xlsContextRetain((xls_context*)configured);continue;
  }
  local.entry=claim->entry;local.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};enum xls_context_error cause=XLS_CONTEXT_ARGUMENT;
  xls_context* provider=XIDE_RENAME_PROVIDER_CREATE(&local,16u*1024u*1024u,&cause);
  if(!provider){
   /* A valid claimed context fits the core limit. An oversized projection
    * cannot equal it; an allocation failure leaves owner matching unknown. */
   if(cause==XLS_CONTEXT_LIMIT)continue;
   error=cause==XLS_CONTEXT_MEMORY?XIDE_RENAME_SCOPE_MEMORY:XIDE_RENAME_SCOPE_ARGUMENT;goto done;
  }
  if(xlsContextEqual(provider,claimed)){matched|=UINT64_C(1)<<i;if(!expected){expected=provider;provider=NULL;}}
  XIDE_RENAME_PROVIDER_RELEASE(provider);
 }
 if(stopped(cancelled,user)){error=XIDE_RENAME_SCOPE_CANCELLED;goto done;}
 if(!expected || !matched)goto done;
 *owners=matched;error=XIDE_RENAME_SCOPE_OK;
done:
 if(error){XIDE_RENAME_PROVIDER_RELEASE(expected);expected=NULL;}
 if(why)*why=error;
 return expected;
}
