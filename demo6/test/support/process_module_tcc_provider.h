/* Real Windows TCC provider, sharing the exact current-source resident runtime.
 * Bind the host's actual PE exports before the provider's historical defaults.
 * This is linker export discovery, not a hand-maintained shadow stdlib registry.
 * Source API graph/admission/retirement remains the same as in the DLL lane. */
#ifndef _WIN32
#error This TCC physical page witness requires Windows
#endif
static void* relocated_entry;
static void (*provider_snapshot)(xmemdebugsnapshot*);
static xmemdebugsnapshot provider_before;
static void compiler_error(void* unused,const char* message)
{ (void)unused; fprintf(stderr,"TCC: %s\n",message); }
static void bind_host_exports(TCCState* output)
{
    HMODULE resident=GetModuleHandleA(NULL); assert(resident);
    const unsigned char* base=(const unsigned char*)resident;
    const IMAGE_DOS_HEADER* dos=(const IMAGE_DOS_HEADER*)base;
    assert(dos->e_magic==IMAGE_DOS_SIGNATURE && dos->e_lfanew>0);
    const IMAGE_NT_HEADERS* nt=(const IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
    assert(nt->Signature==IMAGE_NT_SIGNATURE);
    IMAGE_DATA_DIRECTORY directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    assert(directory.VirtualAddress && directory.Size>=sizeof(IMAGE_EXPORT_DIRECTORY));
    const IMAGE_EXPORT_DIRECTORY* exports=(const IMAGE_EXPORT_DIRECTORY*)(base+directory.VirtualAddress);
    const DWORD* names=(const DWORD*)(base+exports->AddressOfNames);
    for(DWORD i=0;i<exports->NumberOfNames;++i) {
        const char* name=(const char*)(base+names[i]); FARPROC address=GetProcAddress(resident,name);
        assert(address && tcc_add_symbol(output,name,(const void*)address)==0);
    }
}
static void load_artifact(void)
{
    HMODULE runtime=GetModuleHandleA("xl.dll"); assert(runtime);
    int (*register_environment)(TCCState*,int); FARPROC symbol=GetProcAddress(runtime,"xlTccRegisterXrtEnvironment");
    memcpy(&register_environment,&symbol,sizeof(register_environment)); assert(register_environment);
    provider_snapshot(&provider_before);
    state=tcc_new(); assert(state); tcc_set_error_func(state,NULL,compiler_error);
    assert(tcc_set_output_type(state,TCC_OUTPUT_MEMORY)==0);
    assert(tcc_add_include_path(state,PROCESS_INCLUDE_DIRECTORY)==0);
    assert(tcc_add_include_path(state,".")==0);
    tcc_define_symbol(state,"PROCESS_GENERATED_C",PROCESS_GENERATED_C_VALUE);
    tcc_define_symbol(state,"PROCESS_GENERATED_H",PROCESS_GENERATED_H_VALUE);
    tcc_define_symbol(state,"PROCESS_BINDING_H",PROCESS_BINDING_H_VALUE);
    bind_host_exports(state);
    /* First-injected runtime addresses win; defaults supply only C/TCC services. */
    assert(register_environment(state,TCC_OUTPUT_MEMORY));
    assert(tcc_add_file(state,"test/cases/xlir_process_future_module_artifact.c")==0);
    assert(tcc_relocate(state)==0);
    relocated_entry=tcc_get_symbol(state,"process_lifetime_start"); assert(relocated_entry);
    MEMORY_BASIC_INFORMATION memory;
    assert(VirtualQuery(relocated_entry,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_COMMIT);
    assert(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static void* lookup(TCCState* input,const char* name)
{ assert(input==state); return tcc_get_symbol(input,name); }
static void delete_state(TCCState* input)
{
    assert(input==state && !module->codeLeases && !module->activeCalls);
    assert(atomic_load(&cleanups)==2 && !atomic_load(&deleted));
    tcc_delete(input);
    MEMORY_BASIC_INFORMATION memory;
    assert(VirtualQuery(relocated_entry,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
    atomic_fetch_add(&deleted,1);
}
static void provider_check_balanced(void)
{
    xmemdebugsnapshot after; provider_snapshot(&after);
    assert(provider_before.LiveCount==after.LiveCount && provider_before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-provider_before.AllocCount==after.FreeCount-provider_before.FreeCount);
    assert(provider_before.InvalidFreeCount==after.InvalidFreeCount && provider_before.DoubleFreeCount==after.DoubleFreeCount);
    assert(provider_before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool find_provider_cache(const xmemdebugallocation* allocation,void* data)
{
    size_t* count=data;
    if(allocation->Size==sizeof("<static libtcc>") &&
        memcmp(allocation->Address,"<static libtcc>",sizeof("<static libtcc>"))==0) ++*count;
    return true;
}
static void provider_init(void)
{
    HMODULE runtime=GetModuleHandleA("xl.dll"); assert(runtime);
    bool (*enable)(bool); size_t (*visit)(xmemdebugallocationvisitor,ptr);
    FARPROC symbol=GetProcAddress(runtime,"xrtMemDebugSnapshot"); memcpy(&provider_snapshot,&symbol,sizeof(provider_snapshot));
    symbol=GetProcAddress(runtime,"xrtMemDebugEnable"); memcpy(&enable,&symbol,sizeof(enable));
    symbol=GetProcAddress(runtime,"xrtMemDebugVisitLive"); memcpy(&visit,&symbol,sizeof(visit));
    assert(provider_snapshot && enable && visit && enable(true));
    for(unsigned i=0;i<2;++i) {
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        load_artifact(); tcc_delete(state);
        MEMORY_BASIC_INFORMATION memory;
        assert(VirtualQuery(relocated_entry,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
        if(i) provider_check_balanced();
    }
    size_t found=0; xmemdebugsnapshot after; provider_snapshot(&after); visit(find_provider_cache,&found);
    assert(found==1 && after.LiveCount==1 && after.LiveBytes==sizeof("<static libtcc>"));
    printf("TCC provider cache: one explicit process-lifetime cache; next 25 transactions checked separately\n");
}
