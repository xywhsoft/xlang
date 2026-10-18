#ifndef XL6_RUNTIME_SERVICES_H
#define XL6_RUNTIME_SERVICES_H

struct XL6_ContinuationExecutor;

/* Borrowed physical call view. The module owns/traces each actual service
 * slot independently; this by-value aggregate owns nothing and must not be
 * retained. Canonical IR call contracts select exactly which services are
 * populated, independently of payload/field/schema metadata. */
typedef struct XL6_RuntimeServicesV1 {
    struct XL6_ContinuationExecutor* executor;
} XL6_RuntimeServicesV1;

#endif
