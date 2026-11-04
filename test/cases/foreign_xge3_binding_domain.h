#ifndef XL6_TEST_FOREIGN_BINDING_DOMAIN_H
#define XL6_TEST_FOREIGN_BINDING_DOMAIN_H
#include <xrt_decl.h>
#include "src/runtime/xl6_foreign_view.h"
typedef struct BindingDomainRoot BindingDomainRoot;
extern const XL6_ForeignViewDescriptorV1 bindingDomainRootPolicy;
extern const XL6_OwnershipDomainV1 bindingNativeDomain;
int binding_domain_create(BindingDomainRoot**);
void binding_domain_destroy(BindingDomainRoot*);
int binding_domain_read(BindingDomainRoot*);
int binding_domain_check(void);
int binding_domain_live(void);
#endif
