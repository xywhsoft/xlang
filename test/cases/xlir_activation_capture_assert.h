#ifndef XLIR_ACTIVATION_CAPTURE_ASSERT_H
#define XLIR_ACTIVATION_CAPTURE_ASSERT_H
/* Compare emission to the actual verified arenas, including protected loans,
 * empty frames and action order; not to a source-name inference or count only. */
static void assert_activation_layout(const char* text,const char* prefix,XL6_SymbolId symbol,
    const XL6_IrContinuationFrameSlot* slots,size_t count,size_t first,
    const XL6_IrContinuationFrameAction* actions,size_t actionCount)
{
    static const char* kinds[]={NULL,"XL6_CAPTURE_TRIVIAL","XL6_CAPTURE_STRING","XL6_CAPTURE_XVALUE",
        "XL6_CAPTURE_FUTURE","XL6_CAPTURE_CLOSURE","XL6_CAPTURE_CELL","XL6_CAPTURE_ERROR",
        "XL6_CAPTURE_VALUE_ITERATOR","XL6_CAPTURE_GENERATOR"};
    char expected[512];
    bool nativeLayout=true;
    snprintf(expected,sizeof(expected),"XL6_ActivationCaptureLayoutV%u %s_layout_%u",nativeLayout?2u:1u,prefix,symbol);assert(strstr(text,expected));
    if(!strcmp(prefix,"xlir_generator")){
        snprintf(expected,sizeof(expected),"xlir_generator_frame_allocate(sizeof(*frame), &%s_layout_%u)",prefix,symbol);assert(strstr(text,expected));
        snprintf(expected,sizeof(expected),"xlir_generator_create(frame, xlir_generator_resume_%u, &xlir_generator_item_layout_%u)",symbol,symbol);assert(strstr(text,expected));
        assert(strstr(text,"xl6GeneratorRuntimeV3Get()->Create")&&strstr(text,"xlir_generator_frame_discard(frame)"));
        assert(!strstr(text,"xlir_generator_frame_trace_")&&!strstr(text,"xlir_generator_item_trace_")&&
            !strstr(text,"xlir_generator_frame_free_")&&!strstr(text,"xlir_generator_item_free_"));
    }else{
        snprintf(expected,sizeof(expected),"sizeof(XL6_ContinuationFrameLayoutV3), &%s_layout_%u",prefix,symbol);assert(strstr(text,expected));
        snprintf(expected,sizeof(expected),"xlir_async_frame_allocate(sizeof(*frame), &xlir_async_machine_storage_%u)",symbol);assert(strstr(text,expected));
        assert(strstr(text,"xl6ContinuationFrameRuntimeV2Get()->Cleanup(frame, outcome)")&&
            strstr(text,"xl6ContinuationFrameLaunchV1(frame,")&&!strstr(text,"xlir_async_machine_frame_trace_"));
    }
    for(size_t i=0;i<count;++i){
        assert(slots[i].destroyAbi>0&&slots[i].destroyAbi<sizeof(kinds)/sizeof(*kinds));
        int written=snprintf(expected,sizeof(expected),"{{%s, offsetof(%s_frame_%u, f%lu), sizeof(((%s_frame_%u*)0)->f%lu)}, ",
            kinds[slots[i].destroyAbi],prefix,symbol,(unsigned long)i,prefix,symbol,(unsigned long)i);assert(written>0&&(size_t)written<sizeof(expected));
        if(nativeLayout){strcat(expected,"NULL}, ");written=(int)strlen(expected);}
        if(slots[i].initBit==SIZE_MAX)snprintf(expected+written,sizeof(expected)-(size_t)written,"SIZE_MAX}");
        else snprintf(expected+written,sizeof(expected)-(size_t)written,"offsetof(%s_frame_%u, i%lu)}",prefix,symbol,(unsigned long)slots[i].initBit);
        assert(strstr(text,expected));
    }
    if(actionCount){
        snprintf(expected,sizeof(expected),"%s_cleanup_%u[] = {",prefix,symbol);const char* cursor=strstr(text,expected);assert(cursor);cursor+=strlen(expected);
        for(size_t i=0;i<actionCount;++i){
            snprintf(expected,sizeof(expected),"%s%lu",i?", ":"",(unsigned long)(actions[i].frameSlot-first));
            assert(!strncmp(cursor,expected,strlen(expected)));cursor+=strlen(expected);
        }
        assert(!strncmp(cursor,"};",2));
    }
}
#endif
