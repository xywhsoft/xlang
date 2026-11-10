/* Included ONCE at the end of the qualified compiler lexer TU. Use the actual
 * native DFA tables and private keyword resolver; keep one authority and one
 * copy of those tables. Constants are derived from that TU's real match loop
 * and IDENT action, not from a parallel spelling/category table. */
#include "analysis/identifier.h"
#include <xrt.h>
int xlsIdentifierValid(const char* text,size_t length)
{
    if(!text || !length || length>XLS_IDENTIFIER_BYTES || memchr(text,0,length) ||
       !xrtUtf8Valid((xstrview){text,length},NULL))return 0;
    yy_state_type state=1+2*INITIAL+1; /* INITIAL, beginning-of-line. */
    for(size_t i=0;i<length;++i){
        YY_CHAR c=yy_ec[(unsigned char)text[i]];
        while(yy_chk[yy_base[state]+c]!=state){
            state=(int)yy_def[state];
            if(state>=XLS_IDENTIFIER_META)c=yy_meta[c];
        }
        state=yy_nxt[yy_base[state]+c];
        if(state==XLS_IDENTIFIER_JAM)return 0;
    }
    return yy_accept[state]==XLS_IDENTIFIER_RULE && xl6LookupKeyword(text,(int)length)==XL6_TK_IDENT;
}
