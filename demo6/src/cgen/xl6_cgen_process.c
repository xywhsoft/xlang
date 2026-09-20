/*
    进程运行时只声明当前 XRT Process ABI。
    XLang 的命令、结果和实时进程对象是私有包装，不复制旧版 XRT 布局。
*/
static void xl6CgenEmitProcessRuntime(XL6_Output* out, int hasWaitAsync)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts(
        "#ifndef XL6_PROCESS_RUNTIME\n"
        "#define XL6_PROCESS_RUNTIME\n"
        "#include <stdint.h>\n"
        "#include <string.h>\n"
        "#define XL6_PROCESS_LIVE_MAGIC 0x584C3650u\n\n",
        out);

    xl6OutputPuts(
        "typedef xbytesview xl6_xbytesview;\n\n",
        out);

    xl6OutputPuts(
        "typedef struct {\n"
        "    xprocessconfig Config; xprocessrunoptions Run; char* Program; char* Command; char* WorkDir;\n"
        "    char** Args; size_t ArgCount; xprocessenv* Env; size_t EnvCount;\n"
        "    size_t ReadChunkSize; size_t ReplayLimit; size_t EventLimit;\n"
        "} xl6_process_command;\n"
        "typedef struct { xprocessresult Value; bool SpawnFailed; } xl6_process_result;\n"
        "typedef struct { xprocesspipelineresult Value; bool InfrastructureFailed; } xl6_process_pipeline_result;\n"
        "typedef struct { unsigned char* Data; size_t Size; size_t Capacity; uint64_t Base; bool Done; } xl6_process_replay;\n"
        "typedef struct {\n"
        "    uint32_t Magic; xprocess* Process; uint64_t StdoutOffset; uint64_t StderrOffset; uint64_t NextEvent;\n"
        "    size_t ReadChunkSize; size_t ReplayLimit; size_t EventLimit; bool ExitEventRead;\n"
        "    xl6_process_replay Stdout; xl6_process_replay Stderr;\n"
        "} xl6_process_live;\n\n",
        out);

    xl6OutputPuts(
        "static char* xl6_process_copy_text(const char* text)\n"
        "{\n"
        "    return xl6_xrt_string_dup(text != 0 ? text : \"\", text != 0 ? strlen(text) : 0u);\n"
        "}\n\n"
        "static void* xl6_process_empty_string(void)\n"
        "{\n"
        "    return xl6_xrt_string_dup(\"\", 0u);\n"
        "}\n\n"
        "static void* xl6_process_copy_bytes(const void* data, size_t size)\n"
        "{\n"
        "    return data != 0 && size != 0u ? xl6_xrt_string_dup((const char*)data, size) : xl6_process_empty_string();\n"
        "}\n\n"
        "static uint64_t xl6_process_deadline(long long timeoutMs)\n"
        "{\n"
        "    return timeoutMs > 0 ? xrtDeadlineAfter((uint64_t)timeoutMs * 1000u) : XRT_DEADLINE_NEVER;\n"
        "}\n\n",
        out);

    xl6OutputPuts(
        "static void xl6_process_command_destroy(xl6_process_command* command)\n"
        "{\n"
        "    size_t i;\n"
        "    if ( command == 0 ) { return; }\n"
        "    xrtFree(command->Program); xrtFree(command->Command); xrtFree(command->WorkDir);\n"
        "    for ( i = 0u; i < command->ArgCount; ++i ) { xrtFree(command->Args[i]); }\n"
        "    xrtFree(command->Args);\n"
        "    for ( i = 0u; i < command->EnvCount; ++i ) { xrtFree((void*)command->Env[i].Name); xrtFree((void*)command->Env[i].Value); }\n"
        "    xrtFree(command->Env); xrtFree(command);\n"
        "}\n\n"
        "static xl6_process_command* xl6_process_command_alloc(void)\n"
        "{\n"
        "    xl6_process_command* command = (xl6_process_command*)xrtCalloc(1u, sizeof(xl6_process_command));\n"
        "    if ( command == 0 ) { return 0; }\n"
        "    if ( !xrtProcessConfigInit(&command->Config) || !xrtProcessRunOptionsInit(&command->Run) ) { xrtFree(command); return 0; }\n"
        "    command->Config.HideWindow = true; command->Config.Stdin.Mode = XPROCESS_IO_PIPE;\n"
        "    command->Config.Stdout.Mode = XPROCESS_IO_PIPE; command->Config.Stderr.Mode = XPROCESS_IO_PIPE;\n"
        "    command->ReadChunkSize = 65536u; command->ReplayLimit = 16u * 1024u * 1024u; command->EventLimit = 4096u;\n"
        "    return command;\n"
        "}\n\n",
        out);

    xl6OutputPuts(
        "static xl6_process_command* xl6_process_command_create_file(const char* program, xvalue* args)\n"
        "{\n"
        "    size_t count = args != 0 ? xrtValueCount(args) : 0u; size_t i;\n"
        "    xl6_process_command* command = xl6_process_command_alloc();\n"
        "    if ( command == 0 ) { return 0; }\n"
        "    command->Program = xl6_process_copy_text(program);\n"
        "    if ( command->Program == 0 ) { xl6_process_command_destroy(command); return 0; }\n"
        "    command->Config.Target = XPROCESS_EXEC; command->Config.Program = command->Program;\n"
        "    if ( count != 0u ) {\n"
        "        command->Args = (char**)xrtCalloc(count, sizeof(char*));\n"
        "        if ( command->Args == 0 ) { xl6_process_command_destroy(command); return 0; }\n"
        "        for ( i = 0u; i < count; ++i ) {\n"
        "            command->Args[i] = xl6_process_copy_text(xl6_xrt_value_text(xrtValueArrayGet(args, i)));\n"
        "            if ( command->Args[i] == 0 ) { command->ArgCount = i; xl6_process_command_destroy(command); return 0; }\n"
        "        }\n"
        "        command->ArgCount = count; command->Config.Args = (const char* const*)command->Args; command->Config.ArgCount = count;\n"
        "    }\n"
        "    return command;\n"
        "}\n\n"
        "static xl6_process_command* xl6_process_command_create_shell(const char* text)\n"
        "{\n"
        "    xl6_process_command* command = xl6_process_command_alloc();\n"
        "    if ( command == 0 ) { return 0; }\n"
        "    command->Command = xl6_process_copy_text(text);\n"
        "    if ( command->Command == 0 || !xrtProcessShellConfigInit(&command->Config, command->Command) ) { xl6_process_command_destroy(command); return 0; }\n"
        "    command->Config.HideWindow = true; command->Config.Stdin.Mode = XPROCESS_IO_PIPE;\n"
        "    command->Config.Stdout.Mode = XPROCESS_IO_PIPE; command->Config.Stderr.Mode = XPROCESS_IO_PIPE;\n"
        "    return command;\n"
        "}\n\n",
        out);

    xl6OutputPuts(
        "void* xl6_xrt_process_command_create_file(const char* program, xvalue* args) { return xl6_process_command_create_file(program, args); }\n"
        "void* xl6_xrt_process_command_create_shell(const char* command) { return xl6_process_command_create_shell(command); }\n"
        "long long xl6_xrt_process_command_destroy(void* command) { xl6_process_command_destroy((xl6_process_command*)command); return 0; }\n\n"
        "static xl6_process_result* xl6_process_result_alloc(void)\n"
        "{\n"
        "    return (xl6_process_result*)xrtCalloc(1u, sizeof(xl6_process_result));\n"
        "}\n\n"
        "long long xl6_xrt_process_result_destroy(void* value)\n"
        "{\n"
        "    xl6_process_result* result = (xl6_process_result*)value;\n"
        "    if ( result != 0 ) { xrtProcessResultUnit(&result->Value); xrtFree(result); }\n"
        "    return 0;\n"
        "}\n\n",
        out);

    xl6OutputPuts(
        "static xl6_process_result* xl6_process_command_run(xl6_process_command* command, const void* input, size_t inputSize, long long timeoutMs)\n"
        "{\n"
        "    xl6_process_result* result = xl6_process_result_alloc();\n"
        "    if ( result == 0 ) { return 0; }\n"
        "    if ( command == 0 ) { result->SpawnFailed = true; return result; }\n"
        "    command->Run.Input.Data = (const unsigned char*)input; command->Run.Input.Size = inputSize;\n"
        "    command->Run.Deadline = xl6_process_deadline(timeoutMs);\n"
        "    if ( !xrtProcessRun(&command->Config, &command->Run, &result->Value) ) { result->SpawnFailed = true; }\n"
        "    return result;\n"
        "}\n\n"
        "void* xl6_xrt_process_run_shell_result(const char* commandText, int timeoutMs)\n"
        "{\n"
        "    xl6_process_command* command = xl6_process_command_create_shell(commandText);\n"
        "    xl6_process_result* result = xl6_process_command_run(command, 0, 0u, timeoutMs);\n"
        "    xl6_process_command_destroy(command); return result;\n"
        "}\n\n"
        "void* xl6_xrt_process_run_file_result(const char* program, xvalue* args, int timeoutMs)\n"
        "{\n"
        "    xl6_process_command* command = xl6_process_command_create_file(program, args);\n"
        "    xl6_process_result* result = xl6_process_command_run(command, 0, 0u, timeoutMs);\n"
        "    xl6_process_command_destroy(command); return result;\n"
        "}\n\n",
        out);

    xl6OutputPuts(
        "long long xl6_xrt_process_result_exit_code(void* value)\n"
        "{\n"
        "    xl6_process_result* result = (xl6_process_result*)value;\n"
        "    return result != 0 && result->Value.Status.Kind == XPROCESS_EXIT_CODE ? result->Value.Status.Code : -1;\n"
        "}\n"
        "int xl6_xrt_process_result_success(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && !r->SpawnFailed && xrtProcessResultSuccess(&r->Value); }\n"
        "int xl6_xrt_process_result_timed_out(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->Value.Wait==XWAIT_TIMEOUT; }\n"
        "int xl6_xrt_process_result_cancelled(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->Value.Wait==XWAIT_CANCELLED; }\n"
        "int xl6_xrt_process_result_spawn_failed(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->SpawnFailed; }\n"
        "int xl6_xrt_process_result_killed(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->Value.Status.Stop!=XPROCESS_STOP_NONE; }\n"
        "int xl6_xrt_process_result_stdout_truncated(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->Value.StdoutTruncated; }\n"
        "int xl6_xrt_process_result_stderr_truncated(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 && r->Value.StderrTruncated; }\n"
        "long long xl6_xrt_process_result_duration_ms(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 ? (long long)(r->Value.Duration/1000u) : 0; }\n"
        "void* xl6_xrt_process_result_stdout_copy(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 ? xl6_process_copy_bytes(r->Value.Stdout,r->Value.StdoutSize) : xl6_process_empty_string(); }\n"
        "void* xl6_xrt_process_result_stderr_copy(void* value) { xl6_process_result* r=(xl6_process_result*)value; return r!=0 ? xl6_process_copy_bytes(r->Value.Stderr,r->Value.StderrSize) : xl6_process_empty_string(); }\n\n",
        out);

    xl6OutputPuts(
        "static xl6_process_live* xl6_process_live_create(xprocess* process, size_t readChunkSize, size_t replayLimit, size_t eventLimit)\n"
        "{\n"
        "    xl6_process_live* live;\n"
        "    if ( process == 0 ) { return 0; }\n"
        "    live = (xl6_process_live*)xrtCalloc(1u, sizeof(xl6_process_live));\n"
        "    if ( live == 0 ) { xrtProcessDestroy(process); return 0; }\n"
        "    live->Magic=XL6_PROCESS_LIVE_MAGIC; live->Process=process; live->ReadChunkSize=readChunkSize!=0u?readChunkSize:65536u;\n"
        "    live->ReplayLimit=replayLimit; live->EventLimit=eventLimit;\n"
        "    return live;\n"
        "}\n\n"
        "static xl6_process_live* xl6_process_live_value(void* value)\n"
        "{\n"
        "    xl6_process_live* live=(xl6_process_live*)value; return live!=0 && live->Magic==XL6_PROCESS_LIVE_MAGIC ? live : 0;\n"
        "}\n\n"
        "void* xl6_xrt_process_wrap(void* value)\n"
        "{\n"
        "    xl6_process_live* live=xl6_process_live_value(value); return live!=0 ? live : xl6_process_live_create((xprocess*)value,65536u,16u*1024u*1024u,4096u);\n"
        "}\n\n"
		"void* xl6_xrt_process_command_start(void* value)\n"
		"{\n"
		"    xl6_process_command* command = (xl6_process_command*)value;\n"
		"    xprocess* process;\n"
		"    if ( command == 0 ) { return 0; }\n"
		"    process = xrtProcessSpawn(&command->Config);\n"
		"    return xl6_process_live_create(\n"
		"        process,\n"
		"        command->ReadChunkSize,\n"
		"        command->ReplayLimit,\n"
		"        command->EventLimit\n"
		"    );\n"
		"}\n\n"
        "void* xl6_xrt_process_spawn_file(const char* program,xvalue* args) { xl6_process_command* c=xl6_process_command_create_file(program,args); void* p=xl6_xrt_process_command_start(c); xl6_process_command_destroy(c); return p; }\n"
        "void* xl6_xrt_process_spawn_shell(const char* text) { xl6_process_command* c=xl6_process_command_create_shell(text); void* p=xl6_xrt_process_command_start(c); xl6_process_command_destroy(c); return p; }\n\n",
        out);

    xl6OutputPuts(
        "static int xl6_process_write_all(xprocess* process,const void* data,size_t size) { const unsigned char* p=(const unsigned char*)data; size_t offset=0u; while(offset<size){int64_t n=xrtProcessWrite(process,p+offset,size-offset);if(n<=0)return 0;offset+=(size_t)n;}return 1; }\n"
        "long long xl6_xrt_process_destroy(void* value) { xl6_process_live* l=xl6_process_live_value(value); if(l!=0){l->Magic=0u;xrtProcessDestroy(l->Process);xrtFree(l->Stdout.Data);xrtFree(l->Stderr.Data);xrtFree(l);} return 0; }\n"
        "int xl6_xrt_process_running(void* value) { xl6_process_live* l=xl6_process_live_value(value); return l!=0 && xrtProcessState(l->Process)==XPROCESS_RUNNING; }\n"
        "long long xl6_xrt_process_handle_exit_code(void* value) { xl6_process_live* l=xl6_process_live_value(value); xprocessstatus s; memset(&s,0,sizeof(s)); return l!=0 && xrtProcessStatus(l->Process,&s) && s.Kind==XPROCESS_EXIT_CODE ? s.Code : -1; }\n"
        "int xl6_xrt_process_write(void* value,const char* text) { xl6_process_live* l=xl6_process_live_value(value); size_t n=text!=0?strlen(text):0u; return l!=0 && xl6_process_write_all(l->Process,text!=0?text:\"\",n); }\n"
        "int xl6_xrt_process_close_stdin(void* value) { xl6_process_live* l=xl6_process_live_value(value); return l!=0 && xrtProcessClose(l->Process,XPROCESS_STDIN); }\n"
        "int xl6_xrt_process_wait(void* value,long long timeoutMs) { xl6_process_live* l=xl6_process_live_value(value); xwaitresult r; if(l==0)return 0; r=timeoutMs<0?xrtProcessWait(l->Process):xrtProcessWaitFor(l->Process,(uint64_t)timeoutMs*1000u); return r==XWAIT_OK; }\n"
        "int xl6_xrt_process_terminate(void* value) { xl6_process_live* l=xl6_process_live_value(value); return l!=0 && xrtProcessTerminate(l->Process); }\n"
        "int xl6_xrt_process_kill(void* value) { xl6_process_live* l=xl6_process_live_value(value); return l!=0 && xrtProcessKill(l->Process); }\n"
        "int xl6_xrt_process_kill_tree(void* value) { xl6_process_live* l=xl6_process_live_value(value); return l!=0 && xrtProcessKillTree(l->Process); }\n\n",
        out);

    xl6CgenEmitProcessRunRuntime(out, XL6_CGEN_PROCESS_RUN_ALL);

    xl6CgenEmitProcessExtendedRuntime(out, hasWaitAsync);
    xl6OutputPuts("#endif\n\n", out);
}
