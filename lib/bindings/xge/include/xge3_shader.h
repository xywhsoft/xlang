#ifndef XL6_XGE3_SHADER_H
#define XL6_XGE3_SHADER_H
#include "xge3_render_resources.h"
XL6_XGE_NATIVE_POLICY(xl6Xge3ShaderPolicy,"xge.shader",xge_shader_t,xgeShaderFree)
#define xl6Xge3ShaderPolicy (*xl6Xge3ShaderPolicyGet())
XL6_XGE_RECORD_POLICY(xl6Xge3ShaderVariantsPolicy,"xge.shaderVariants",xge_shader_variant_set_t,xgeShaderVariantSetFree)
#define xl6Xge3ShaderVariantsPolicy (*xl6Xge3ShaderVariantsPolicyGet())
#endif
