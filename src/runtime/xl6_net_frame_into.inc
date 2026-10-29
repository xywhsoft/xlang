bool FRAME_NEXT_INTO(void* raw,void* target,bool consume)
{
    xbuffer* output=xl6BufferChecked(target);if(!output){xl6NetBufferFailedV1();return false;}
    FRAME_TYPE* framer=raw;XL6_NetBufferGuardV1 guard={0};xnetframe frame={0};bool ok=true;size_t oldSize=output->Size;
    if(!xl6NetBufferEnterV1(&framer->input,&guard)){xl6NetBufferFailedV1();return false;}xnetframestatus status=FRAME_PARSE(framer,&frame);
    if(status==XNET_FRAME_ERROR)ok=false;
    else if(status==XNET_FRAME_READY){ok=(!consume||xl6NetFrameCanAdvanceV1(framer->revision))&&
            xl6NetBufferWriteIntoV1(&framer->input,output,frame.PayloadOffset,frame.PayloadSize);
        if(ok&&consume){ok=xrtNetFrameConsume(&framer->input.native,&frame);if(ok)++framer->revision;}
        if(!ok)output->Size=oldSize;}
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();return ok&&status==XNET_FRAME_READY;
}
#undef FRAME_NEXT_INTO
#undef FRAME_TYPE
#undef FRAME_PARSE
