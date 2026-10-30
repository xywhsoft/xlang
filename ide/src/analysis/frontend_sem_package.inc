/* Flat facts own package strings separately from their optional shared type
 * forest. Staging first borrows a unit,then copies before atomic publication. */
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static void xlsSemPackageFactsForget(xarray* facts)
{for(size_t i=0;i<facts->Count;++i)((xls_sem_fact*)xrtArrayGet(facts,i))->packageLink=NULL;}
static void xlsSemPackageFactsFree(xarray* facts)
{
    for(size_t i=0;i<facts->Count;++i){xls_sem_fact* f=xrtArrayGet(facts,i);xrtFree(f->packageLink);f->packageLink=NULL;}
}
static int xlsSemPackageFactsCopy(xarray* facts,size_t limit,size_t* bytes)
{
    *bytes=0;
    for(size_t i=0;i<facts->Count;++i){
        xls_sem_fact* f=xrtArrayGet(facts,i);const char* borrowed=f->packageLink;f->packageLink=NULL;
        if(!borrowed)continue;
        size_t length=strlen(borrowed);
        if(length==SIZE_MAX || length+1>limit-*bytes || !(f->packageLink=xrtStrDupN(borrowed,length))){
            /* Later entries still borrow the old unit. Never release them. */
            for(size_t j=i+1;j<facts->Count;++j)((xls_sem_fact*)xrtArrayGet(facts,j))->packageLink=NULL;
            xlsSemPackageFactsFree(facts);*bytes=0;return 0;
        }
        *bytes+=length+1;
    }
    return 1;
}
static int xlsSemPackageFactsCapture(const xarray* nodes,xarray* facts,size_t limit,size_t* bytes)
{
    *bytes=0;
    for(size_t i=0;i<facts->Count;++i){
        const XL6_AstNode* node=((const xls_sem_raw*)xrtArrayConstGet(nodes,i))->node;
        if(!node->compiledPackageLink)continue;
        xls_sem_fact* f=xrtArrayGet(facts,i);size_t length=strlen(node->compiledPackageLink);
        if(length==SIZE_MAX || length+1>limit-*bytes || !(f->packageLink=xrtStrDupN(node->compiledPackageLink,length))){
            xlsSemPackageFactsFree(facts);*bytes=0;return 0;
        }
        *bytes+=length+1;
    }
    return 1;
}
static void xlsSemPackageFactApply(XL6_AstNode* node,xls_sem_fact* f)
{
    node->semanticReceiverSymbolId=f->receiver;node->compiledPackageError=f->packageError;node->compiledPackageRoot=f->packageRoot;
    xrtFree(node->compiledPackageLink);node->compiledPackageLink=f->packageLink;f->packageLink=NULL;
}
#else
#define xlsSemPackageFactsForget(facts) ((void)0)
#define xlsSemPackageFactsFree(facts) ((void)0)
#define xlsSemPackageFactsCopy(facts,limit,bytes) ((*(bytes)=0),1)
#define xlsSemPackageFactsCapture(nodes,facts,limit,bytes) ((*(bytes)=0),1)
#define xlsSemPackageFactApply(node,fact) ((void)0)
#endif
