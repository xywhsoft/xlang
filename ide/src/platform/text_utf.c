#include "platform/text_utf.h"
#include <stdint.h>
#if defined(_WIN32) && !defined(XIDE_TEXT_UTF_FORCE_XRT)
#include <windows.h>

typedef struct prefix { size_t bytes;int units; } prefix;
static prefix countPrefix(xstrview text,size_t bytes)
{
    if(!bytes) return (prefix){0};
    /* Only the final scalar can cross a valid chunk boundary. Reuse XRT's
     * strict decoder on that at-most-four-byte tail instead of retrying the
     * system conversion over the whole chunk several times. */
    if((unsigned char)text.Data[bytes-1]>=128) {
        size_t at=bytes-1,back=0;
        while(at && back<3 && ((unsigned char)text.Data[at]&0xc0)==0x80) { --at;++back; }
        uint32 scalar=0;size_t read=0;
        if(xrtUtf8Decode((xstrview){text.Data+at,bytes-at},&scalar,&read)!=XUTF_OK || read!=bytes-at) bytes=at;
    }
    if(!bytes) return (prefix){0};
    int units=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.Data,(int)bytes,NULL,0);
    return units ? (prefix){bytes,units} : (prefix){0};
}
#endif
xutfresult xideUtf8To16Prefix(xstrview text,int maximum)
{
    xutfresult out={.Status=XUTF_INVALID,.Error=0};
    if(!text.Data || text.Size>4096 || maximum<0) return out;
#if defined(_WIN32) && !defined(XIDE_TEXT_UTF_FORCE_XRT)
    prefix value=countPrefix(text,text.Size);
    out.Status=value.bytes==text.Size ? XUTF_OK : XUTF_INVALID;
    out.Error=out.Status==XUTF_OK ? SIZE_MAX : value.bytes;
    if(value.units>maximum) {
        size_t low=0,high=value.bytes;prefix best={0};
        while(low<high) {
            size_t mid=low+(high-low+1)/2;prefix current=countPrefix(text,mid);
            if(current.units<=maximum) { low=mid;best=current; }
            else high=mid-1;
        }
        value=best;out.Status=XUTF_NO_SPACE;out.Error=SIZE_MAX;
    }
    out.Read=value.bytes;out.Written=(size_t)value.units;return out;
#else
    out=xrtUtf8To16Buffer(text,NULL,0,XUTF_STRICT);
    if(out.Written>(size_t)maximum) {
        uint16 output[4096];out=xrtUtf8To16Buffer(text,output,(size_t)maximum,XUTF_STRICT);
    }
    return out;
#endif
}
