#ifndef XL6_TLS_RESUME_OWNER_H
#define XL6_TLS_RESUME_OWNER_H
struct xtlsresume;
/* Permanent typed native operations avoid selecting the resume implementation
 * merely to create/destroy an ordinary verified client context. No language
 * edge or module callback: the immutable native credential cannot form cycles. */
typedef struct XL6_TlsResumeOpsV1 {
    struct xtlsresume* (*Ref)(const struct xtlsresume*);
    void (*Drop)(struct xtlsresume*);
} XL6_TlsResumeOpsV1;
typedef struct XL6_TlsResumeOwnerV1 {struct xtlsresume* value;const XL6_TlsResumeOpsV1* ops;} XL6_TlsResumeOwnerV1;
#endif
