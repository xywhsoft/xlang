#ifndef FOREIGN_INLINE_CSTRING_OUTPUT_H
#define FOREIGN_INLINE_CSTRING_OUTPUT_H

#include <__xl_runtime/xrt_decl.h>

typedef struct InlineTextOutput {
    unsigned iSize;
    int number;
    const char* borrowed;
#ifdef XL6_TEST_WRONG_INLINE_TEXT_TYPE
    unsigned char preview[8];
#else
    char preview[8];
#endif
    char suffix[8];
} InlineTextOutput;

int inlineTextRead(int mode, InlineTextOutput* result);
void inlineTextBudget(int count);
void inlineTextReset(void);
void inlineTextHeapBegin(void);
void inlineTextHeapEqual(void);

#endif
