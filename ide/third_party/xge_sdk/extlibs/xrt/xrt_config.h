#ifndef XGE_XRT_CONFIG_H
#define XGE_XRT_CONFIG_H

#include "xge_config.h"

/* XRT is part of xge.dll's public runtime surface. */
#if defined(XGE_BUILD_DLL) || defined(XUI_BUILD_DLL)
	#ifndef XRT_BUILD_SHARED
		#define XRT_BUILD_SHARED
	#endif
#elif defined(XGE_DLL) || defined(XUI_DLL)
	#ifndef XRT_USE_SHARED
		#define XRT_USE_SHARED
	#endif
#endif

/* XGE's shared XRT profile. Keep this list identical in every translation unit. */
#define XRT_MODULE_ARRAY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_COND
#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_MAP
#define XRT_MODULE_MUTEX
#define XRT_MODULE_PATH
#define XRT_MODULE_POOL
#define XRT_MODULE_RANDOM
#define XRT_MODULE_SLOT_MAP
#define XRT_MODULE_SPIN
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_THREAD
#define XRT_MODULE_TIME_TEXT

/* Preserve the historical public runtime in default, UI and tool profiles. */
#if XGE_ENABLE_XUI || XGE_ENABLE_PARTICLES || XGE_BUILD_PROFILE == XGE_PROFILE_DEFAULT || XGE_BUILD_PROFILE == XGE_PROFILE_FULL_DEV || defined(XGE_XRT_PROFILE_IDE) || defined(XGE_XRT_PROFILE_MAPGEN)
#define XRT_MODULE_FILE_TEXT
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_WALK
#define XRT_MODULE_JSON
#define XRT_MODULE_STRING_GLOB
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_VALUE
#define XRT_MODULE_VALUE_COLLECTION
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_XLON
#define XRT_MODULE_REGEX_CORE
#define XRT_MODULE_REGEX_MATCH
#endif

/*
 * Optional profile for CPU-heavy procedural generation tools.  Keep these
 * capabilities behind an explicit build flag so the default XGE DLL retains
 * its current footprint while specialized hosts can use the same exported
 * XRT runtime instance instead of linking a second copy.
 */
#if defined(XGE_XRT_PROFILE_MAPGEN)
	#define XRT_MODULE_HASH64
	#define XRT_MODULE_TASK_GROUP_POOL
	#define XRT_MODULE_DEFLATE
#endif

/* IDE extensions and XGE share the runtime exported by xge.dll. */
#if defined(XGE_XRT_PROFILE_IDE)
	#define XWORK_MODULE_XWORK
	#include "extlibs/xwork/include/xwork/features.h"
	#include "extlibs/xllm-session/include/xllm-session/features.h"
	#include "extlibs/xllm/include/xllm/features.h"
	#define XRT_MODULE_CODEC_HEX
	#define XRT_MODULE_CODEC_PERCENT
	#define XRT_MODULE_FILE_TEMP
	#define XRT_MODULE_PROCESS_RUN
	#define XRT_MODULE_SEM
	#define XRT_MODULE_XID
	#define XRT_MODULE_NET_TCP_DIAL_SYNC
	#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
	#define XRT_MODULE_TLS_STREAM_FUTURE
	#define XRT_MODULE_TLS_VERIFY
	#define XRT_MODULE_X509_STORE_SYSTEM
	#define XRT_MODULE_HTTP1_BODY
	#define XRT_MODULE_CRYPTO_SHA256
#endif

#endif
