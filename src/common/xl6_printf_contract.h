#ifndef XL6_PRINTF_CONTRACT_H
#define XL6_PRINTF_CONTRACT_H
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* Shared checked C-printf grammar. No positional arguments, %n, wide text or
 * unbounded numeric widths; * consumes a promoted C int before the value.
 * Type validation is supplied by XLIR's canonical call-argument recipe. */
typedef bool (*xl6_printf_argument_check)(void*,char conversion,char length);
static inline bool xl6PrintfContract(const unsigned char* text,size_t size,
    xl6_printf_argument_check argument,void* context)
{
    if (!text || size>1048576u || memchr(text,0,size)) return false;
    for (size_t i=0;i<size;++i) {
        if (text[i]!='%') continue;
        if (++i==size) return false;
        if (text[i]=='%') continue;
        unsigned flags=0;
        bool precisionGiven=false;
        const char* flagCharacters="-+ #0";
        while (i<size && strchr(flagCharacters,text[i])) {
            flags |= 1u << (unsigned)(strchr(flagCharacters,text[i])-flagCharacters); ++i;
        }
        if (i<size && text[i]=='*') { if (argument && !argument(context,'*',0)) return false; ++i; }
        else {
            unsigned width=0;
            while (i<size && text[i]>='0' && text[i]<='9') {
                unsigned digit=text[i++]-'0';
                if (width>(1048576u-digit)/10u) return false;
                width=width*10u+digit;
            }
        }
        if (i<size && text[i]=='.') {
            precisionGiven=true;
            ++i;
            if (i<size && text[i]=='*') { if (argument && !argument(context,'*',0)) return false; ++i; }
            else {
                unsigned precision=0;
                while (i<size && text[i]>='0' && text[i]<='9') {
                    unsigned digit=text[i++]-'0';
                    if (precision>(1048576u-digit)/10u) return false;
                    precision=precision*10u+digit;
                }
            }
        }
        char length=0;
        if (i<size && strchr("hljztL",text[i])) {
            length=(char)text[i++];
            if (i<size && text[i]==(unsigned char)length && (length=='h' || length=='l')) {
                length=length=='h' ? 'H':'q'; ++i;
            }
        }
        if (i==size) return false;
        char conversion=(char)text[i];
        if (!strchr("diouxXfFeEgGaAcsp",conversion)) return false;
        unsigned allowed=strchr("fFeEgGaA",conversion) ? 31u :
            (conversion=='d' || conversion=='i') ? 23u :
            strchr("oxX",conversion) ? 25u : conversion=='u' ? 17u : 1u;
        if ((flags & ~allowed) || (precisionGiven && strchr("cp",conversion))) return false;
        if ((conversion=='s' || conversion=='c' || conversion=='p') && length) return false;
        if (strchr("fFeEgGaA",conversion) && length && length!='l') return false;
        if (strchr("diouxX",conversion) && length=='L') return false;
        if (argument && !argument(context,conversion,length)) return false;
    }
    return true;
}
#endif
