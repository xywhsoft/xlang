#ifndef XLANG_EXPAT_CONFIG_H
#define XLANG_EXPAT_CONFIG_H

/*
	这份配置固定 Expat 的跨平台能力，避免源码包依赖 configure/CMake。
	DTD 与通用实体保留在解析核心中，由 XRT 默认安全策略在事件入口拒绝。
*/
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define STDC_HEADERS 1

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTEORDER 4321
#else
#define BYTEORDER 1234
#endif

#if defined(__linux__)
#define HAVE_GETRANDOM 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
	defined(__NetBSD__)
#define HAVE_ARC4RANDOM_BUF 1
#elif !defined(_WIN32)
#define XML_DEV_URANDOM 1
#endif

#define PACKAGE "expat"
#define PACKAGE_NAME "expat"
#define PACKAGE_STRING "expat 2.8.2"
#define PACKAGE_TARNAME "expat"
#define PACKAGE_VERSION "2.8.2"
#define VERSION "2.8.2"

#define XML_ATTR_INFO 1
#define XML_CONTEXT_BYTES 1024
#define XML_DTD 1
#define XML_GE 1
#define XML_NS 1

#endif
