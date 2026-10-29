#ifndef XL6_TEMPLATE_ABI_H
#define XL6_TEMPLATE_ABI_H
XL6_TEXT_API void* xl6TemplateCreateV1(xstrview,const xvalue*,int64_t);
XL6_TEXT_API void* xl6TemplateCreateExtensionsV1(xstrview,const xvalue*,const xvalue*,int64_t);
XL6_TEXT_API void* xl6TemplateFileCreateV1(xstrview,const xvalue*,int64_t,int64_t,int64_t);
XL6_TEXT_API xstrview xl6TemplateReadFileV1(xstrview,int64_t,int64_t,int64_t);
XL6_TEXT_API void* xl6TemplateVfsCreateV1(void*,xstrview,const xvalue*,int64_t,int64_t,int64_t);
XL6_TEXT_API xstrview xl6TemplateReadVfsV1(void*,xstrview,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6TemplateDestroyV1(void*);
XL6_TEXT_API xstrview xl6TemplateSourceV1(void*);
XL6_TEXT_API uint64_t xl6TemplateIdentityV1(void*);
XL6_TEXT_API int64_t xl6TemplateExtensionCountV1(void*);
XL6_TEXT_API int64_t xl6TemplateNodeCountV1(void*);
XL6_TEXT_API xvalue* xl6TemplateNodeV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TemplateErrorLocationV1(xerror*,int64_t);
XL6_TEXT_API xstrview xl6TemplateRenderV1(void*,const xvalue*,const xvalue*,struct xlir_closure*);
XL6_TEXT_API int64_t xl6TemplateAppendV1(void*,void*,const xvalue*,const xvalue*,struct xlir_closure*);
XL6_TEXT_API uint64_t xl6TemplateWriteToV1(void*,void*,const xvalue*,const xvalue*,struct xlir_closure*);
XL6_TEXT_API void* xl6TemplateCallCreateV1(void);
XL6_TEXT_API void xl6TemplateCallDestroyV1(void*);
XL6_TEXT_API bool xl6TemplateCallIsIncludeV1(void*);
XL6_TEXT_API uint64_t xl6TemplateCallRegistryV1(void*);
XL6_TEXT_API int64_t xl6TemplateCallIndexV1(void*);
XL6_TEXT_API xstrview xl6TemplateCallNameV1(void*);
XL6_TEXT_API bool xl6TemplateCallResolveV1(void*,void*);
XL6_TEXT_API int64_t xl6TemplateCallArgumentCountV1(void*);
XL6_TEXT_API xvalue* xl6TemplateCallArgumentV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TemplateCallFindV1(void*,xstrview,int64_t);
XL6_TEXT_API xvalue* xl6TemplateCallEvalV1(void*,int64_t);
XL6_TEXT_API bool xl6TemplateCallWriteV1(void*,xstrview,int64_t);
XL6_TEXT_API bool xl6TemplateCallRenderBodyV1(void*);
XL6_TEXT_API bool xl6TemplateCallRenderCurrentV1(void*,const xvalue*);
XL6_TEXT_API xstrview xl6TemplateCallRawV1(void*);
XL6_TEXT_API xvalue* xl6TemplateCallScopeV1(void*,int64_t);
XL6_TEXT_API void* xl6TemplateEngineGuardCreateV1(void);
XL6_TEXT_API void xl6TemplateEngineGuardDestroyV1(void*);
XL6_TEXT_API bool xl6TemplateEngineGuardCheckV1(void*);
XL6_TEXT_API bool xl6TemplateEngineValidateV1(void*,xstrview,int64_t,int64_t,int64_t);
#endif
