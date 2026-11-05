/* Sorted generated names provide allocation-free canonical and quoted lookup. */
static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContractSpan(const char* name,size_t length)
{
    size_t low=0,high=sizeof(xl6IrDirectBridgeContracts)/sizeof(xl6IrDirectBridgeContracts[0]);
    if(!name)return NULL;
    while(low<high){
        size_t middle=low+(high-low)/2;
        const XL6_IrDirectBridgeContract* row=&xl6IrDirectBridgeContracts[middle];
        size_t common=length<row->nameLength?length:row->nameLength;
        int compared=memcmp(name,row->name,common);
        if(!compared)compared=length<row->nameLength?-1:length>row->nameLength?1:0;
        if(!compared)return row;
        if(compared<0)high=middle;else low=middle+1;
    }
    return NULL;
}
static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContract(const char* name)
{ return name?xl6IrDirectBridgeContractSpan(name,strlen(name)):NULL; }
static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContractSpelling(const char* name)
{
    if(!name)return NULL;
    size_t length=strlen(name);
    return length>=2 && name[0]=='"' && name[length-1]=='"'
        ?xl6IrDirectBridgeContractSpan(name+1,length-2):xl6IrDirectBridgeContractSpan(name,length);
}
