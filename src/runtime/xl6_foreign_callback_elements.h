#ifndef XL6_FOREIGN_CALLBACK_ELEMENTS_H
#define XL6_FOREIGN_CALLBACK_ELEMENTS_H

#include <stddef.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include <xrt_decl.h>
#endif

/* Instantiate over an authoritative native element type. The callback owns
 * the descriptor; the native publisher owns the array. */
#define XL6_FOREIGN_CONST_ELEMENTS(view,element) \
    typedef struct view { \
        const element* Data; \
        size_t Count; \
    } view; \
    static inline const element* view##At(const view* span,size_t index) \
    { \
        if(!span || !span->Data || index>=span->Count){ \
            xrtSetErrorKind(XERR_RANGE); \
            return NULL; \
        } \
        return span->Data+index; \
    }

/* For pointer-free elements whose values can outlive the callback. */
#define XL6_FOREIGN_CONST_ELEMENTS_COPY(view,element) \
    static inline int view##Read(const view* span,size_t index,element* output) \
    { \
        const element* item=view##At(span,index); \
        if(!item)return -1; \
        *output=*item; \
        return 0; \
    }

#endif
