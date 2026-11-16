#include "foreign_xge3_host_value.h"
#include <type_traits>
#include <cstddef>
static_assert(std::is_standard_layout<xge_host_session_desc_t>::value,"native standard layout");
static_assert(std::is_trivially_copyable<xge_host_session_desc_t>::value,"native value ownership");
static_assert(sizeof(xge_host_session_desc_t)==20 && alignof(xge_host_session_desc_t)==4,"native value ABI");
static_assert(offsetof(xge_host_session_desc_t,dpiScale)==12,"native member offset");
static_assert(offsetof(xge_host_session_desc_t,framebuffer)==16,"native member offset");
extern "C" xge_host_session_desc_t fixtureHostValueCpp(xge_host_session_desc_t value)
{value.dpiScale+=1;value.framebuffer^=0x80000000u;return value;}
