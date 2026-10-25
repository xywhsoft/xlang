/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xacme requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XACME_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XACME_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XACME_SINGLE_HEADER_H
#define XACME_SINGLE_HEADER_H
#define XACME_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xacme/include/xacme/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XACME_FEATURES_H
#define XACME_FEATURES_H

/* acme_obtain 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_OBTAIN)
#ifndef XACME_FEATURE_ACME_OBTAIN
#define XACME_FEATURE_ACME_OBTAIN
#endif
#ifndef XACME_MODULE_ACME_CORE
#define XACME_MODULE_ACME_CORE
#endif
#ifndef XACME_MODULE_ACME_FLOW
#define XACME_MODULE_ACME_FLOW
#endif
#ifndef XACME_MODULE_ACME_STORE
#define XACME_MODULE_ACME_STORE
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XACME_MODULE_ACME_JOSE
#define XACME_MODULE_ACME_JOSE
#endif
#ifndef XACME_MODULE_ACME_CSR
#define XACME_MODULE_ACME_CSR
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_DNS_ALI
#define XACME_MODULE_DNS_ALI
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#endif

/* dns_huawei 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_DNS_HUAWEI)
#ifndef XACME_FEATURE_DNS_HUAWEI
#define XACME_FEATURE_DNS_HUAWEI
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* dns_aws 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_DNS_AWS)
#ifndef XACME_FEATURE_DNS_AWS
#define XACME_FEATURE_DNS_AWS
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* dns_tencent 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_DNS_TENCENT)
#ifndef XACME_FEATURE_DNS_TENCENT
#define XACME_FEATURE_DNS_TENCENT
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* dns_cf 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_DNS_CF)
#ifndef XACME_FEATURE_DNS_CF
#define XACME_FEATURE_DNS_CF
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* acme_flow 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_FLOW)
#ifndef XACME_FEATURE_ACME_FLOW
#define XACME_FEATURE_ACME_FLOW
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XACME_MODULE_ACME_JOSE
#define XACME_MODULE_ACME_JOSE
#endif
#ifndef XACME_MODULE_ACME_CSR
#define XACME_MODULE_ACME_CSR
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_HTTP_TARGET
#define XRT_MODULE_HTTP_TARGET
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#ifndef XRT_MODULE_X509_PROFILE
#define XRT_MODULE_X509_PROFILE
#endif
#ifndef XRT_MODULE_X509_NAME
#define XRT_MODULE_X509_NAME
#endif
#ifndef XRT_MODULE_X509_VERIFY
#define XRT_MODULE_X509_VERIFY
#endif
#ifndef XACME_MODULE_DNS_ALI
#define XACME_MODULE_DNS_ALI
#endif
#ifndef XACME_MODULE_ACME_CORE
#define XACME_MODULE_ACME_CORE
#endif
#ifndef XACME_MODULE_ACME_STORE
#define XACME_MODULE_ACME_STORE
#endif
#endif

/* acme_store 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_STORE)
#ifndef XACME_FEATURE_ACME_STORE
#define XACME_FEATURE_ACME_STORE
#endif
#ifndef XACME_MODULE_ACME_CORE
#define XACME_MODULE_ACME_CORE
#endif
#ifndef XRT_MODULE_FILE
#define XRT_MODULE_FILE
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_FILE_LOCK
#define XRT_MODULE_FILE_LOCK
#endif
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#ifndef XRT_MODULE_X509_PARSE
#define XRT_MODULE_X509_PARSE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#endif

/* acme_core 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_CORE)
#ifndef XACME_FEATURE_ACME_CORE
#define XACME_FEATURE_ACME_CORE
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#endif

/* dns_ali 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_DNS_ALI)
#ifndef XACME_FEATURE_DNS_ALI
#define XACME_FEATURE_DNS_ALI
#endif
#ifndef XACME_MODULE_ACME_DNS
#define XACME_MODULE_ACME_DNS
#endif
#ifndef XACME_MODULE_ACME_HTTP
#define XACME_MODULE_ACME_HTTP
#endif
#ifndef XRT_MODULE_JSON
#define XRT_MODULE_JSON
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* acme_dns 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_DNS)
#ifndef XACME_FEATURE_ACME_DNS
#define XACME_FEATURE_ACME_DNS
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_NET_UDP
#define XRT_MODULE_NET_UDP
#endif
#ifndef XRT_MODULE_NET_UDP_SYNC
#define XRT_MODULE_NET_UDP_SYNC
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM
#endif
#ifndef XRT_MODULE_RANDOM_DEFAULT
#define XRT_MODULE_RANDOM_DEFAULT
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#endif

/* acme_csr 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_CSR)
#ifndef XACME_FEATURE_ACME_CSR
#define XACME_FEATURE_ACME_CSR
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_ASN1_DER
#define XRT_MODULE_ASN1_DER
#endif
#ifndef XRT_MODULE_PEM
#define XRT_MODULE_PEM
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_P256
#define XRT_MODULE_CRYPTO_P256
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER
#define XRT_MODULE_CRYPTO_ECDSA_P256_SIGN_DER
#endif
#ifndef XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#define XRT_MODULE_CRYPTO_RSA_PKCS1_SIGN
#endif
#ifndef XACME_MODULE_ACME_JOSE
#define XACME_MODULE_ACME_JOSE
#endif
#endif

/* acme_jose 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_JOSE)
#ifndef XACME_FEATURE_ACME_JOSE
#define XACME_FEATURE_ACME_JOSE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_P256_KEYPAIR
#define XRT_MODULE_CRYPTO_P256_KEYPAIR
#endif
#ifndef XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#define XRT_MODULE_CRYPTO_ECDSA_P256_SIGN
#endif
#endif

/* acme_http 及其直接依赖。 */
#if defined(XACME_MODULE_ALL) || defined(XACME_MODULE_ACME_HTTP)
#ifndef XACME_FEATURE_ACME_HTTP
#define XACME_FEATURE_ACME_HTTP
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#ifndef XRT_MODULE_HTTP1_HEAD
#define XRT_MODULE_HTTP1_HEAD
#endif
#ifndef XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_HTTP1_NET
#define XRT_MODULE_HTTP1_NET
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_NET_RESOLVER
#define XRT_MODULE_NET_RESOLVER
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL
#define XRT_MODULE_TLS_STREAM_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_CLIENT
#define XRT_MODULE_TLS_CLIENT
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_VERIFY
#define XRT_MODULE_TLS_VERIFY
#endif
#ifndef XRT_MODULE_X509_STORE
#define XRT_MODULE_X509_STORE
#endif
#ifndef XRT_MODULE_X509_STORE_SYSTEM
#define XRT_MODULE_X509_STORE_SYSTEM
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_NET_TCP_FUTURE
#define XRT_MODULE_NET_TCP_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_FUTURE
#define XRT_MODULE_NET_TCP_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_NEGOTIATE
#define XRT_MODULE_TLS_NEGOTIATE
#endif
#ifndef XRT_MODULE_TLS_POLICY
#define XRT_MODULE_TLS_POLICY
#endif
#ifndef XRT_MODULE_TLS_CONTEXT
#define XRT_MODULE_TLS_CONTEXT
#endif
#ifndef XRT_MODULE_TLS_RECORD
#define XRT_MODULE_TLS_RECORD
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P384
#define XRT_MODULE_TLS_KEY_EXCHANGE_P384
#endif
#endif

#endif /* XACME_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_http.h */
/* ========================================================================== */

#ifndef XRT_ACME_HTTP_H
#define XRT_ACME_HTTP_H

/*
	xacme HTTPS 传输层的裁剪闭包契约：ACME 端点访问建立在 xrt 的
	自研 TLS/网络/HTTP 栈上（一次性连接，系统或自定义信任库）。
	传输对象为内部实现细节，宿主经 xrt/acme_client.h 使用；
	本头固化依赖闭包，并提供未交付对象的延后清理入口。
*/


#if defined(XACME_FEATURE_ACME_HTTP)

#if !defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ATOMIC) || \
	!defined(XRT_FEATURE_HTTP) || \
	!defined(XRT_FEATURE_HTTP1_HEAD) || \
	!defined(XRT_FEATURE_HTTP1_BODY) || \
	!defined(XRT_FEATURE_HTTP1_NET) || \
	!defined(XRT_FEATURE_NET_ENGINE) || \
	!defined(XRT_FEATURE_NET_RESOLVER) || \
	!defined(XRT_FEATURE_NET_TCP) || \
	!defined(XRT_FEATURE_NET_TCP_DIAL) || \
	!defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE) || \
	!defined(XRT_FEATURE_NET_TCP_FUTURE) || \
	!defined(XRT_FEATURE_TLS_STREAM) || \
	!defined(XRT_FEATURE_TLS_STREAM_DIAL) || \
	!defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
	!defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
	!defined(XRT_FEATURE_TLS_CLIENT) || \
	!defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	!defined(XRT_FEATURE_TLS_VERIFY) || \
	!defined(XRT_FEATURE_TLS_NEGOTIATE) || \
	!defined(XRT_FEATURE_TLS_POLICY) || \
	!defined(XRT_FEATURE_TLS_CONTEXT) || \
	!defined(XRT_FEATURE_TLS_RECORD) || \
	!defined(XRT_FEATURE_TLS_RECORD_AES) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384) || \
	!defined(XRT_FEATURE_X509_STORE) || \
	!defined(XRT_FEATURE_X509_STORE_SYSTEM) || \
	!defined(XRT_FEATURE_FUTURE) || \
	!defined(XRT_FEATURE_FUTURE_BRIDGE) || \
	!defined(XRT_FEATURE_CANCEL) || \
	!defined(XRT_FEATURE_THREAD) || \
	!defined(XRT_FEATURE_TIME)
	#error "XACME_FEATURE_ACME_HTTP requires the xrt TLS/net/HTTP stack"
#endif

/* 传输错误域 "xrt.acme.http" 的稳定代码。 */
typedef enum xacmehttperror {
	XACME_HTTP_ERROR_ARGUMENT = 1,
	XACME_HTTP_ERROR_URL,
	XACME_HTTP_ERROR_CONNECT,
	XACME_HTTP_ERROR_SEND,
	XACME_HTTP_ERROR_PROTOCOL,
	XACME_HTTP_ERROR_TIMEOUT,
	XACME_HTTP_ERROR_TLS,
	/* 请求已开始发送，服务端是否执行了写操作无法判定。 */
	XACME_HTTP_ERROR_UNCERTAIN
} xacmehttperror;

XRT_EXTERN_C_BEGIN

/*
	重试未交付的工厂/一站式临时对象的延后退休。队列跨线程可用，
	入列不分配内存；宿主须先停止新调用、等待在途调用结束，清理
	已交付实例，再于退出或卸载库前调用至 true。false 时保留库及
	相关运行环境，稍后重试；不启动后台清理线程。
	uTimeoutMs == 0 为一次非阻塞轮询；非零为本次等待预算，退休
	错误会提前结束。true 表示队列及其他清理调用正在处理的对象全部
	释放；false 表示仍有对象。piPending 可为空，否则返回未完成数量。
	保留调用前已有诊断；无旧诊断时报告退休错误或等待超时，非阻塞
	BUSY 不制造错误。返回值和数量是清理状态的依据。
	此入口仅处理未交付对象；已交付客户端/provider 由各自清理入口负责。
	有未完成对象时，新的私有引擎构造先尝试非阻塞清理，仍未完成
	则以 XERR_STATE 拒绝；借用引擎的构造不受此限制。
*/
XRT_API bool xrtAcmeCleanupPending(int64 uTimeoutMs, size_t* piPending);

XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_jose.h */
/* ========================================================================== */

#ifndef XRT_ACME_JOSE_H
#define XRT_ACME_JOSE_H

/*
	xacme JOSE 层的裁剪闭包契约：ES256 账户密钥与 JWS 组装建立在
	xrt 的 P-256 与 SHA-256 原语上。JOSE 对象为内部实现细节，
	宿主经 xrt/acme_client.h 使用；本头只固化依赖闭包，
	供构建器做负向裁剪门禁。
*/


#if defined(XACME_FEATURE_ACME_JOSE)

#if !defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR) || \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)
	#error "XACME_FEATURE_ACME_JOSE requires base64, SHA-256 and P-256 signing"
#endif

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_csr.h */
/* ========================================================================== */

#ifndef XRT_ACME_CSR_H
#define XRT_ACME_CSR_H

/*
	xacme CSR/密钥序列化层的裁剪闭包契约：PKCS#10 组装与 PKCS#8/SEC1
	PEM 读写建立在 xrt 的 ASN.1/PEM/P-256 原语上，并复用 JOSE 密钥
	类型。CSR 对象为内部实现细节，宿主经 xrt/acme_client.h 使用；
	本头只固化依赖闭包，供构建器做负向裁剪门禁。
*/


#if defined(XACME_FEATURE_ACME_CSR)

#if !defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ASN1_DER) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_P256) || \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER) || \
	!defined(XRT_FEATURE_CRYPTO_RSA) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PKCS1) || \
	!defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)
	#error "XACME_FEATURE_ACME_CSR requires JOSE, DER, PEM and P-256 DER signing"
#endif

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_H
#define XRT_ACME_DNS_H






#if defined(XACME_FEATURE_ACME_DNS) && (\
	!defined(XRT_FEATURE_NET_ENGINE) || \
	!defined(XRT_FEATURE_NET_UDP) || \
	!defined(XRT_FEATURE_NET_UDP_SYNC) || \
	!defined(XRT_FEATURE_THREAD) || \
	!defined(XRT_FEATURE_RANDOM) || \
	!defined(XRT_FEATURE_RANDOM_DEFAULT) || \
	!defined(XRT_FEATURE_RANDOM_SECURE) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ARRAY))
	#error "XACME_FEATURE_ACME_DNS requires net engine, UDP, secure random, time, buffer and array"
#endif

/* DNS provider 模块稳定错误码（错误域 "xrt.acme.dns"）。 */
typedef enum xacmednserror {
	XACME_DNS_ERROR_ARGUMENT = 1,
	XACME_DNS_ERROR_CREDENTIAL,
	XACME_DNS_ERROR_ZONE,
	XACME_DNS_ERROR_PROTOCOL,
	XACME_DNS_ERROR_NETWORK,
	/* A write was sent but record ownership could not be confirmed. Do not replay. */
	XACME_DNS_ERROR_UNCERTAIN,
	/* Built-in provider context was released or transport cleanup has begun. */
	XACME_DNS_ERROR_STATE
} xacmednserror;



#if defined(XACME_FEATURE_ACME_DNS)

/* provider 能力位：PROPAGATE 表示自带传播确认回调。 */
#define XACME_DNS_CAP_PROPAGATE UINT32_C(0x00000001)

typedef struct xacmednsprovider xacmednsprovider;

/* 铺设一条 _acme-challenge TXT 记录；失败时设置线程错误。 */
typedef bool (*xacmednsaddproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	移除一条 TXT 记录；记录已不存在视为成功（幂等）。
	重签中断后的清理路径依赖该语义。
*/
typedef bool (*xacmednsremoveproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	可选：provider 自带传播确认；为空时由库内 TXT 探测器统一确认。
	返回 true 表示记录已在权威侧可见。
*/
typedef bool (*xacmednspropagateproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	DNS-01 provider 是值语义小结构：内建实现由各家构造函数填充，
	自定义 provider 由宿主直接填写字段，无全局注册；请求状态由
	实例持有。内建构造失败的未交付上下文若尚未退休，会转移至
	跨实例待清理队列；宿主用 xrt/acme_http.h 中的
	xrtAcmeCleanupPending 重试，并在退出/卸载前确认完成。
	内建 ProviderUnit 等待私有引擎退休；完成后 pContext 为 NULL。
	超时或失败时保留上下文供重试，此后实例只能继续 ProviderUnit。
	保留调用前诊断，因此以 pContext 是否为 NULL 判断释放结果。
	宿主须先停止新回调并等待已有调用结束，再串行执行 ProviderUnit；
	ProviderUnit 不可与回调或另一次 ProviderUnit 并发。

	Add/Remove 契约：
	- sFqdn 恒为 _acme-challenge.<domain> 全称，sTxt 为 base64url 文本；
	- 同一 Fqdn 会多条 TXT 并存，Add 必须可叠加；
	- 内建 Add/Remove 的 provider 为空时返回 false 并设置
	  XERR_ARGUMENT/XACME_DNS_ERROR_ARGUMENT；上下文已释放或清理
	  未完成时设置 XERR_STATE/XACME_DNS_ERROR_STATE，错误域均为
	  xrt.acme.dns。该状态检查先于记录操作；自定义 provider 可使用
	  空上下文，最小契约校验不检查其上下文；
	- Add 写入已发送但结果未知时必须返回明确错误；流程不会重放该次
	  Add。无法证明记录所有权时，不得按同名同值查询结果自动删除；
	  提供商层结果未知使用 xrt.acme.dns/XACME_DNS_ERROR_UNCERTAIN；
	- 回调为同步契约且必须可重入；zone 归属解析是 provider 内部职责。
*/
struct xacmednsprovider {
	const char* sId;
	uint32 iCaps;
	ptr pContext;
	xacmednsaddproc Add;
	xacmednsremoveproc Remove;
	xacmednspropagateproc Propagate;
};

#endif



XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_ACME_DNS)

/* 校验 provider 满足签发路径的最小契约（id 与 Add/Remove 非空）。 */
XRT_API bool xrtAcmeDnsProviderValidate(const xacmednsprovider* pProvider);

/*
	内建 provider 枚举；实现在组合根模块 xacme 中，随三态选择结果变化。
	仅枚举 id 字符串；构造各家 provider 仍调用各自的构造函数。
*/
XRT_API size_t xrtAcmeDnsProviderCount(void);
XRT_API const char* xrtAcmeDnsProviderId(size_t iIndex);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns_ali.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_ALI_H
#define XRT_ACME_DNS_ALI_H



#if defined(XACME_FEATURE_DNS_ALI) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_ALI requires acme dns, acme http transport, signing primitives and mutex"
#endif



#if defined(XACME_FEATURE_DNS_ALI)

/*
	阿里云 DNS（alidns）provider，走 V3 签名（ACS3-HMAC-SHA256）。
	Endpoint 默认 alidns.aliyuncs.com；构造时复制凭据和 Endpoint，
	三者各自必须短于 160 字节，超长直接拒绝。Add 接受 ASCII DNS-01
	属主和未填充的 base64url 摘要。传播确认由签发流程层统一负责
	（provider 只做 Add/Remove）。新增响应丢失、5xx、畸形或没有安全 RecordId 时，
	Add 返回 XACME_DNS_ERROR_UNCERTAIN；错误包装/解析分配失败保留内存错误。
	固定槽保留未知状态，后续同名同值 Add/Remove 均失败且不发请求；
	需外部核对该次操作并重建实例，不按同名同值认领或删除。
	已确认和未知记录共用八槽容量；同实例 Add/Remove 串行保护内部状态。
	创建响应复用实例内已登记的 ID 时，新记录按未知结果保留；原记录归属不变。
	DNS 属主按 ASCII 小写登记和匹配，TXT 摘要字节仍区分大小写。
	重复 Add 与 Remove 先按保存的 RecordId 调用 DescribeDomainRecordInfo，
	核对 ID、属主、TXT 值、类型与默认线路。重复 Add 仅在记录已启用时成功；
	已确认 ID 明确消失时可重新创建，其他错误或记录被修改均保留句柄并失败。
	Remove 接受禁用的本实例记录；明确消失时清除句柄而不再次删除。仅 400/404
	配合有效 RequestId、无 RecordId 字段的 DomainRecordNotBelongToUser 表示消失。
	需授权 DescribeDomainRecords、DescribeDomainRecordInfo、AddDomainRecord 与
	DeleteDomainRecord；外部写入者仍须协调读取和删除之间的修改。
	DescribeDomainRecords/DescribeDomainRecordInfo 为只读 RPC，传输断流允许至多
	三次调用，每次重签名；内存失败立即停止。添加和删除不因只读重试规则重放。
	区域发现仅在 400/404 明确返回 InvalidDomainName.NoExist 或 DomainNotFound
	时继续探测父区域；权限、限流、服务错误与畸形应答立即失败。成功应答须有
	有效的分页、请求标识与记录列表，列表记录必须属于请求区域。四项缓存按
	规范化的完整探测起点匹配，满载后替换旧项，不跳过更具体的子区域。
*/
typedef struct xacmednaliconfig {
	cstr sAccessKeyId;
	cstr sAccessKeySecret;
	cstr sEndpoint;
} xacmednaliconfig;

#endif



#if defined(XACME_FEATURE_DNS_ALI)

struct xnetengine;

#endif

XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_DNS_ALI)

/* 全零初始化；AccessKeyId/Secret 必填。 */
XRT_API void xrtAcmeDnsAliConfigInit(xacmednaliconfig* pConfig);

/*
	构造阿里云 DNS provider。内部上下文由 xrtMalloc 分配，
	宿主用 xrtAcmeDnsAliProviderUnit 归还；凭据缺失返回 false。
	pBorrowedEngine 为空时自建网络引擎。
*/
XRT_API bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文（Add 期间记录的 RecordId 列表等）。 */
XRT_API void xrtAcmeDnsAliProviderUnit(xacmednsprovider* pProvider);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme.h */
/* ========================================================================== */

#ifndef XRT_ACME_H
#define XRT_ACME_H



#if defined(XACME_FEATURE_ACME_CORE) && !defined(XACME_FEATURE_ACME_DNS)
	#error "XRT acme core requires XACME_FEATURE_ACME_DNS"
#endif



/* acme 核心模块稳定错误码（错误域 "xrt.acme"）。 */
typedef enum xacmeerror {
	XACME_ERROR_ARGUMENT = 1,
	XACME_ERROR_CONFIG,
	XACME_ERROR_DIRECTORY,
	XACME_ERROR_ACCOUNT,
	XACME_ERROR_ORDER,
	XACME_ERROR_CHALLENGE,
	XACME_ERROR_FINALIZE,
	XACME_ERROR_RATE_LIMIT,
	XACME_ERROR_PROTOCOL,
	XACME_ERROR_NETWORK,
	XACME_ERROR_DNS,
	XACME_ERROR_STORE
} xacmeerror;



#if defined(XACME_FEATURE_ACME_CORE)

/*
	内置 CA directory 预设。xacme 对 CA 完全中立：任何 RFC 8555
	directory 都可用（LiteSSL 等控制台发放的地址直接传入即可），
	切换 CA = 换 DirectoryUrl 与 EAB，不换构建。
*/
#define XACME_DIRECTORY_LE \
	"https://acme-v02.api.letsencrypt.org/directory"
#define XACME_DIRECTORY_LE_STAGING \
	"https://acme-staging-v02.api.letsencrypt.org/directory"
#define XACME_DIRECTORY_ZEROSSL \
	"https://acme.zerossl.com/v2/DV90"
#define XACME_DIRECTORY_GOOGLE \
	"https://dv.acme-v02.api.pki.goog/directory"
#define XACME_DIRECTORY_BUYPASS \
	"https://api.buypass.com/acme/directory"
#define XACME_DIRECTORY_BUYPASS_TEST \
	"https://api.test.buypass.com/acme/directory"



/* External Account Binding；Kid 与 Hmac 均为借用视图，Hmac 为 base64url 文本。 */
typedef struct xacmeeab {
	cstr sKid;
	cstr sHmac;
} xacmeeab;

/*
	账户配置全部借用视图，宿主保证存活至调用返回。
	DirectoryUrl 必填（建议用 XACME_DIRECTORY_* 预设）；
	AccountKeyPem 为空时由库生成 ES256 账户密钥并经 store 保存。
*/
typedef struct xacmeaccountconfig {
	cstr sDirectoryUrl;
	cstr sAccountKeyPem;
	xacmeeab Eab;
	cstr sContactEmail;
} xacmeaccountconfig;

/*
	签发产物：证书链 + 配对私钥，两段文本均由 xrtFree 释放。
	私钥为 PKCS#8 PEM（ES256），与链中叶证书配对；没有它证书不可用。
*/
typedef struct xacmeissuegrant {
	str sFullchainPem;
	str sKeyPem;
} xacmeissuegrant;

/* 释放一段签发产物（成员非空即释放并清零）。 */
XRT_API void xrtAcmeGrantUnit(xacmeissuegrant* pGrant);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_ACME_CORE)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeAccountConfigInit(xacmeaccountconfig* pConfig);

#endif



#if defined(XACME_FEATURE_ACME_CORE)

/* 释放一段签发产物（成员非空即释放并清零）；入参可为空。 */
XRT_API void xrtAcmeGrantUnit(xacmeissuegrant* pGrant);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_store.h */
/* ========================================================================== */

#ifndef XRT_ACME_STORE_H
#define XRT_ACME_STORE_H



#if defined(XACME_FEATURE_ACME_STORE) && ( \
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XRT_FEATURE_FILE) || \
	!defined(XRT_FEATURE_FILE_WHOLE) || \
	!defined(XRT_FEATURE_FILE_LOCK) || \
	!defined(XRT_FEATURE_DIR_TEMP) || \
	!defined(XRT_FEATURE_X509_PARSE) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_DIR) )
	#error "XACME_FEATURE_ACME_STORE requires acme core, file, file lock, dir temp, x509, pem, base64, time and buffer"
#endif

/* store 模块稳定错误码（错误域 "xrt.acme.store"）。 */
typedef enum xacmestoreerror {
	XACME_STORE_ERROR_ARGUMENT = 1,
	XACME_STORE_ERROR_IO,
	XACME_STORE_ERROR_NOT_FOUND,
	XACME_STORE_ERROR_PARSE
} xacmestoreerror;

/*
	磁盘布局（root 由宿主显式指定，库不猜家目录）：
	  <root>/accounts/<ca16>/account.pem   账户密钥（PKCS#8 PEM）
	  <root>/certs/<domain>/current        当前版本目录名（原子提交点）
	  <root>/certs/<domain>/current.lock   跨进程写者锁（运行期间勿删除）
	  <root>/certs/<domain>/.grant-<16hex>/key.pem
	  <root>/certs/<domain>/.grant-<16hex>/fullchain.pem
	  <root>/certs/<domain>/.grant-<16hex>/meta.txt
	通配符 *.example.com 在磁盘上使用 %2A.example.com；ListDomains 返回
	原始 *.example.com。POSIX 上旧的 *.example.com 目录仍可读，新写入使用
	%2A 映射目录；两者并存时优先读取已提交的新目录。
	旧布局的直属 key.pem/fullchain.pem/meta.txt 在 current 缺失时仍可读取。
	SaveGrant 只发布版本目录，不更新旧布局；宿主须经 API 取产物，
	或读取 current 后固定同一版本目录，不能分别读取旧版直属文件。
	旧版与未引用的完整版本不会自动删除；停用全部读写方后可用仓库的
	tools/prune_acme_store.py 预览及回收，运行期间不能删除 current.lock。
	<ca16> 为 directory URL 的 SHA-256 hex 前 16 字符，多 CA 并存互不污染。
	POSIX 私钥临时文件从创建起使用 0600；Windows 上宿主应限制 root 的 ACL。
	域名必须是 ASCII DNS 名或开头为 *. 的通配符名。
	存储层自身的参数、容量和结构错误使用 xrt.acme.store 错误域。
	文件系统、分配、PEM 与 X509 子操作失败保留其原始错误；尤其不得把
	XERR_MEMORY 包装成 IO、NOT_FOUND 或重新签发的条件。诊断分配失败
	也交付内存错误，调用方不能只按存储层域名判断失败。
*/

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_STORE)

/*
	保存账户密钥 PEM（按 CA 隔离；原子替换）。POSIX 同步目录项；
	发布后同步失败时可能返回 false 但新密钥已可见，须重新读取核对。
*/
XRT_API bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem);

/* 读取账户密钥 PEM（xrtMalloc/xrtFree）；未注册返回 NULL 且置
   XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl);

/* 保存旧布局的独立证书链与 CA 溯源；已有 current 的域名拒绝此操作。 */
XRT_API bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl);

/* 读取证书链 PEM；不存在返回 NULL 且置 XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain);

/* 读取签发时使用的 CA directory（meta.txt）；无溯源返回 NULL。 */
XRT_API str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain);

/*
	签发产物先写入独立的 0700 版本目录，三文件齐备后原子替换 current。
	失败或并发续签不会让 LoadGrant 读取混合版本；写者通过 current.lock
	串行发布，崩溃留下的未引用目录
	可由宿主在停用读取方后清理。POSIX 发布前同步版本目录及其祖先，
	发布后同步 current 所在目录；不支持目录 fsync 的文件系统会拒绝发布。
	若发布后的同步失败，函数返回 false，但新版本可能已经可见；宿主应通过
	LoadGrant 核对当前状态。Windows 的 current 使用同目录临时文件与
	重命名替换；重命名失败后也保留可能已发布的版本供核查。其持久化仍依赖
	系统和卷的刷新/替换语义。
	pGrant 借用；sDirectoryUrl 可为空（meta 溯源留空）。
*/
XRT_API bool xrtAcmeStoreSaveGrant(
	cstr sRoot, cstr sPrimaryDomain,
	const xacmeissuegrant* pGrant, cstr sDirectoryUrl);

/*
	固定一次 current 再读取同一版本的链与私钥。key.pem 或 fullchain.pem
	缺失即失败（XERR_NOT_FOUND），输出清零。两段均 xrtFree。
*/
XRT_API bool xrtAcmeStoreLoadGrant(
	cstr sRoot, cstr sPrimaryDomain, xacmeissuegrant* pOut);

/*
	枚举 <root>/certs/ 下的域名（续签守护遍历用）；%2A 通配符目录
	解码为 *.，新旧目录并存时只返回一次。
	每元素 256 字节；容量不足时返回 false 并置 XERR_RANGE。
*/
XRT_API bool xrtAcmeStoreListDomains(
	cstr sRoot, char (*sOutDomains)[256],
	size_t iCapacity, size_t* pOutCount);

/*
	续签判定：解析本地链叶证书的 notAfter，剩余寿命不足
	iRenewalDays 天时 *pbNeed=true。本地证书缺失同样 *pbNeed=true。
*/
XRT_API bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_client.h */
/* ========================================================================== */

#ifndef XRT_ACME_CLIENT_H
#define XRT_ACME_CLIENT_H


#if defined(XACME_FEATURE_ACME_FLOW) && (\
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XACME_FEATURE_ACME_CSR) || \
	!defined(XACME_FEATURE_DNS_ALI) || \
	!defined(XACME_FEATURE_ACME_STORE) || \
	!defined(XRT_FEATURE_JSON) || !defined(XRT_FEATURE_HTTP_TARGET) || \
	!defined(XRT_FEATURE_X509_PARSE) || !defined(XRT_FEATURE_X509_PROFILE) || \
	!defined(XRT_FEATURE_X509_NAME) || !defined(XRT_FEATURE_X509_VERIFY))
	#error "XACME_FEATURE_ACME_FLOW requires its core, DNS, HTTP target, JOSE, CSR, store, JSON and X.509 closures"
#endif

struct xnetengine;

/*
	线程安全契约：客户端与 provider 实例为单线程归属对象——同一
	实例的任意两个调用不得并发；跨线程使用需宿主外部串行化。
	不同实例的请求与账户状态独立，可并行使用；未交付对象的异常
	退休队列跨实例共享，经 xrtAcmeCleanupPending 同步。全部 API
	为同步阻塞调用。
*/

#if defined(XACME_FEATURE_ACME_FLOW)

/*
	客户端配置：全部借用视图，宿主保证存活至 Create 返回。
	pAccount 必填；sPropagateResolvers 为空时使用内置默认组
	（223.5.5.5 / 119.29.29.29 / 8.8.8.8，任一可见即通过），
	uPropagateTimeoutMs 为 0 时默认 120 秒。
*/
typedef struct xacmeclientconfig {
	const xacmeaccountconfig* pAccount;
	cstr sCaPem;
	struct xnetengine* pBorrowedEngine;
	int64 uTimeoutMs;
	const cstr* sPropagateResolvers;
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/*
		单次签发的总预算（毫秒；0 = 不限时）：覆盖订单/挑战/
		finalize/证书下载的全部轮询与退避，超限以 XERR_TIMEOUT
		失败。防病态 CA 把签发挂成小时级。
	*/
	int64 uIssueTimeoutMs;
	/*
		宿主提供的证书私钥 PEM（可选；EC P-256 或 RSA-2048+）：
		设置后每次签发复用同一证书密钥（含 RSA 证书场景）；
		为空则每次签发生成一次性 ES256。库不内置 RSA 密钥生成，
		RSA 密钥由宿主用 openssl 等工具预先生成。
	*/
	cstr sCertKeyPem;
} xacmeclientconfig;

#endif

/* 不透明客户端；定义在内部头，宿主只经指针使用。 */

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_FLOW)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeClientConfigInit(xacmeclientconfig* pConfig);

/*
	创建客户端：建传输、解析 directory、注册或复用账户（含
	EAB/contact）。失败返回 NULL 并设置线程错误。失败构造的资源
	回滚独立留出至少 30 秒预算，不复用已耗尽的单次请求超时。
	仍未退休的未交付对象转移至待清理队列，保留原始构造错误；
	宿主在退出/卸载前用 xrtAcmeCleanupPending 确认全部释放。
	JSON/PEM/签名/传输/存储的内存失败保留原错误；底层传输失败
	保留其 kind/domain/code。宿主按 xrtErrorKind 分类，不假定
	所有错误来自 xrt.acme.flow。响应格式或状态无效为 XERR_PROTOCOL。
*/
XRT_API struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig);

/*
	清理资源而保留客户端外壳；空指针为 true，可重复调用。
	true 表示完成，随后可 Destroy；false 表示引擎尚未退休，保留
	客户端供稍后重试。无论结果如何，实例此后仅可 Cleanup/Destroy。
	保留调用前已有错误，因此用返回值判断清理结果，不能只看线程错误。
*/
XRT_API bool xrtAcmeClientCleanup(struct xacmeclient* pClient);

/* 销毁并释放；入参可为空。清理超时或失败时保留外壳供重试。
 * 需要确定释放结果时先调用 Cleanup；true 后再调用 Destroy。 */
XRT_API void xrtAcmeClientDestroy(struct xacmeclient* pClient);

/* 账户密钥 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化复用。 */
XRT_API str xrtAcmeClientAccountPem(const struct xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；产物含证书链与
	配对私钥（pOut 两段均 xrtFree，或经 xrtAcmeGrantUnit 统一释放）。
	provider 的 Add 用于铺设 TXT；随后传播确认，最后触发挑战。
	普通传播不可达或超时不阻断签发；传播分配失败立即返回，
	保留 MEMORY 诊断且不再查询其他 resolver 或触发挑战。
	Remove 在结束后尽力调用。
	最多 16 个不同域名；每段视图须为 1-511 字节且不含 NUL。
	这些限制在发订单前检查。失败不交付部分产物，清理 TXT 时
	保留签发的原始失败原因。轮询的解析或内存失败立即返回。
	证书密钥须不同于当前账户钥。下载仅接受 PEM 证书链（最多
	16 张、每张 DER 不超过 256 KiB），检查公钥、SAN、叶证书有效期
	及所提供链的发行者、CA 标志和签名；不代替部署的根信任策略。
*/
XRT_API bool xrtAcmeClientIssue(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	xacmeissuegrant* pOut
);

/*
	Issue 的备用链变体：bPreferAlternate 时若证书响应的 Link 头带
	rel="alternate"（RFC 8555 §7.4.2），改用备用链下载；备用链获取
	或校验失败自动回退主链，不视为错误。相对引用以主证书 URL
	解析；备用链须从同一张 DER 叶证书开始。
*/
XRT_API bool xrtAcmeClientIssueEx(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	bool bPreferAlternate,
	xacmeissuegrant* pOut
);

/*
	吊销证书（RFC 8555 §7.6，账户钥签名）：sCertPem 为单张证书
	（取首个 PEM 块）；iReason 0-9（RFC 5280 CRLReason），<0 省略。
	已被吊销视为幂等成功。要求 directory 提供 revokeCert 端点。
*/
XRT_API bool xrtAcmeClientRevoke(
	struct xacmeclient* pClient,
	cstr sCertPem,
	int iReason
);

/*
	账户密钥滚动（RFC 8555 §7.3.5）：用 sNewKeyPem（PKCS#8/SEC1）
	替换当前账户密钥，账户 kid 不变。要求 directory 提供 keyChange
	端点；请求按 RFC 8555 内层新钥 JWK、外层旧钥 kid 签名。
	写入响应丢失时，使用 onlyReturnExisting 对账；未确认生效则返回
	false 并保留当前内存密钥。成功后客户端即刻使用新钥；若指定
	sStoreRoot 而重存失败，返回 false 但内存仍使用新钥，调用方可
	用同一 sNewKeyPem 重试对账与持久化。
*/
XRT_API bool xrtAcmeClientRollover(
	struct xacmeclient* pClient,
	cstr sNewKeyPem,
	cstr sStoreRoot
);

/*
	账户停用（RFC 8555 §7.3.6）：停用后该账户及其订单永久不可用；
	幂等（已停用视为成功）。
*/
XRT_API bool xrtAcmeClientDeactivate(struct xacmeclient* pClient);

#endif

#if defined(XACME_FEATURE_ACME_FLOW) && defined(XACME_FEATURE_ACME_STORE)

/*
	一站式续签（组合 store）：本地证书剩余寿命不少于 iRenewalDays
	天时 *pbRenewed=false 并直接返回现有链与私钥；否则签发、落盘
	（key.pem + fullchain.pem + CA 溯源）并返回新产物。
	pDomains[0] 同时是 store 的主域名键。
*/
XRT_API bool xrtAcmeClientIssueStored(
	struct xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	cstr sStoreRoot,
	int iRenewalDays,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns_cf.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_CF_H
#define XRT_ACME_DNS_CF_H



#if defined(XACME_FEATURE_DNS_CF) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_CF requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_CF)

/*
	Cloudflare DNS provider（API v4，Bearer API Token）。
	Token 建议只授予目标 zone 的 Zone.DNS Edit 权限；均为借用视图。
*/
typedef struct xacmednscfconfig {
	cstr sApiToken;
	cstr sEndpoint;
} xacmednscfconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_CF)

/* 全零初始化；ApiToken 必填。 */
XRT_API void xrtAcmeDnsCfConfigInit(xacmednscfconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 新记录从完整属主逐级查询最近的区域，每次重新探测；只有有效空列表
 * 才查父区域，查询错误立即停止。无托管区域返回 XACME_DNS_ERROR_ZONE。
 * 区域列表只接受 HTTP 200；401/403 返回 XERR_PERMISSION，429/5xx 返回
 * XERR_AGAIN，其余异常返回 XERR_PROTOCOL；解析 OOM 保留首个内存诊断。
 * 已拥有同名同值的重复 Add 先读取原 ID 核对身份；匹配时不新建或占槽。
 * 只有严格确认原 ID 不存在才释放并重新发现区域；其他失败保留句柄。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * Remove 先按登记时的 zone/record ID 读取并核对 TXT 属主和值；
 * 身份变化或读取失败保留句柄，不发删除。删除应答必须可核验，
 * 未知结果只读对账，确认原 ID 不存在才清空；内存失败保留诊断和
 * 句柄供下次 Remove 对账。一次 Remove 不重复发送删除。
 * GET 与 DELETE 不是原子操作；宿主须协调同一记录的外部写者。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例；Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsCf(
	const xacmednscfconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsCfProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns_tencent.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_TENCENT_H
#define XRT_ACME_DNS_TENCENT_H



#if defined(XACME_FEATURE_DNS_TENCENT) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_TENCENT requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256 签名）。
	凭据为 SecretId/SecretKey；Endpoint 默认 dnspod.tencentcloudapi.com。
*/
typedef struct xacmednstencentconfig {
	cstr sSecretId;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednstencentconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_TENCENT)

/* 全零初始化；SecretId/SecretKey 必填。 */
XRT_API void xrtAcmeDnsTencentConfigInit(xacmednstencentconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 已拥有同名同值的重复 Add 核对原 Domain/DomainId/RecordId；身份匹配
 * 且 Enabled=1 时不新建或占槽，禁用返回 XERR_STATE。无法读取原 ID
 * 不释放或重建；仅此只读失败不会标记删除待确认，可再次核对原 ID。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * 每次创建从完整 TXT 属主逐级查询 DescribeDomain，固定已验证的 DomainId。
 * 属主本身为托管域名时以 SubDomain=@ 创建和核验顶点记录。
 * Remove 在该 ID 下读取原 RecordId，核对域名 ID、属主、TXT 值、默认
 * 线路及 Enabled 类型。删除 ACK 要求 HTTP 200、非空 RequestId 且无
 * 矛盾字段；结果未知只读对账，一次 Remove 不重放删除，句柄仍保留时
 * 同名同值 Add 拒绝。RecordIdInvalid 或索引库存中的缺失不能证明
 * 记录已消失；该状态保留句柄并阻止重复 Add。创建后至少 30 秒才
 * 查询当前域名 ID 及最多 3000 条完整无过滤库存，用于诊断而非释放。
 * 官方 30 秒重查建议不是索引延迟上限。未获严格删除 ACK 时，后续
 * 仍只核对原 ID；能重新读取且身份匹配时可恢复清理。始终无法确认
 * 的缺失（含已提交删除的丢失应答）返回错误，宿主须核对云端原操作
 * 后决定何时重建实例。宿主仍须协调查询与删除之间的外部写者。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例；Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsTencent(
	const xacmednstencentconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsTencentProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns_aws.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_AWS_H
#define XRT_ACME_DNS_AWS_H



#if defined(XACME_FEATURE_DNS_AWS) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER))
	#error "XACME_FEATURE_DNS_AWS requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_AWS)

/*
	AWS Route53 provider（SigV4，XML API 2013-04-01）。
	Region 可空（Route53 为全局服务，默认 us-east-1）；
	凭据建议为仅限 Route53 的 IAM 用户/角色。
*/
typedef struct xacmednsawsconfig {
	cstr sAccessKeyId;
	cstr sSecretAccessKey;
	cstr sRegion;
	cstr sEndpoint;
} xacmednsawsconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_AWS)

/* 全零初始化；AccessKeyId/SecretAccessKey 必填。 */
XRT_API void xrtAcmeDnsAwsConfigInit(xacmednsawsconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * DNS 属主按 ASCII 小写登记、匹配和签名请求，TXT 摘要字节区分大小写。
 * 新属主/值从完整属主逐级查找唯一公有托管区；查询失败不回退到父区域。
 * 已拥有值的重复 Add 和 Remove 固定使用登记时的区域 ID；不重新选区。
 * 已存在的同值 TXT 不认领、不删除；本实例已确认写入的值可以重复 Add。
 * 完整 REST-XML 错误中的 400 InvalidChangeBatch、Throttling 和
 * PriorRequestNotComplete 最多尝试 4 次，500ms/1s/2s 退避并重新读取 TXT。
 * 限流耗尽保留 AGAIN/NETWORK；权限拒绝返回 PERMISSION/CREDENTIAL。
 * 错误 XML 畸形或含重复 Code/Error 不能证明拒绝，新增进入未知保护状态。
 * 新增请求发送后应答丢失、5xx 或畸形，返回 DNS_ERROR_UNCERTAIN，保留阻止重放的
 * 状态，后续同名同值 Add/Remove 均失败且不发请求；需外部核对并重建实例。
 * Route53 不提供逐值所有者/租约。相同 zone/name/value 的跨实例或跨进程
 * 使用，宿主必须协调完整 Add→验证→Remove 生命周期，包括外部 DNS 写者。 */
XRT_API bool xrtAcmeDnsAws(
	const xacmednsawsconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsAwsProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_dns_huawei.h */
/* ========================================================================== */

#ifndef XRT_ACME_DNS_HUAWEI_H
#define XRT_ACME_DNS_HUAWEI_H



#if defined(XACME_FEATURE_DNS_HUAWEI) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_HUAWEI requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256 签名）。
	凭据为 AK/SK；Endpoint 默认 dns.myhuaweicloud.com。
	注意 recordset 的 name 带尾点、records 值需内嵌双引号。
*/
typedef struct xacmednshuaaweiconfig {
	cstr sAccessKey;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednshuaaweiconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_HUAWEI)

/* 全零初始化；AccessKey/SecretKey 必填。 */
XRT_API void xrtAcmeDnsHuaweiConfigInit(xacmednshuaaweiconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 新记录从完整属主逐级查询最近的区域，每次重新探测；只有有效空列表
 * 才查父区域，查询错误立即停止。无托管区域返回 XACME_DNS_ERROR_ZONE。
 * 区域列表只接受 HTTP 200；401/403 返回 XERR_PERMISSION，429/5xx 返回
 * XERR_AGAIN，其余异常返回 XERR_PROTOCOL；解析 OOM 保留首个内存诊断。
 * 已拥有同名同值的重复 Add 先读取原 ID；身份完整且 ACTIVE 时不新建
 * 或占槽。创建/更新中返回 XERR_AGAIN，不可用状态返回 XERR_STATE。
 * 删除中保留待确认状态，后续 Add 不发请求；严格缺失才释放并重建。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例。Remove 先读取原 zone/recordset ID，核对 TXT 身份、
 * 唯一值、default=false 和状态；只以 404/DNS.0313 确认记录消失。
 * 202/PENDING_DELETE 仅表示受理，最多四次读取（间隔 0.5/1/2 秒）
 * 确认消失才释放句柄。轮询耗尽返回 XERR_AGAIN；读取/内存失败也
 * 保留句柄。删除中的同名同值 Add 拒绝；后续 Remove 继续对账，
 * 已核验受理的删除不重发，未知结果仅在再次核对原记录后可重试。
 * 需要 dns:recordset:get/delete 权限；宿主须协调同一记录的外部写者。
 * Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsHuawei(
	const xacmednshuaaweiconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsHuaweiProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xrt/acme_obtain.h */
/* ========================================================================== */

#ifndef XRT_ACME_OBTAIN_H
#define XRT_ACME_OBTAIN_H



#if defined(XACME_FEATURE_ACME_OBTAIN) && (\
	!defined(XACME_FEATURE_ACME_FLOW) || \
	!defined(XACME_FEATURE_ACME_STORE) || \
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XACME_FEATURE_ACME_JOSE) || \
	!defined(XACME_FEATURE_ACME_CSR) || \
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_DNS_ALI) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_FILE_WHOLE) || \
	!defined(XRT_FEATURE_X509_PARSE) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_DIR))
	#error "XACME_FEATURE_ACME_OBTAIN requires flow and store closures"
#endif

struct xnetengine;

#if defined(XACME_FEATURE_ACME_OBTAIN)

/*
	一站式配置：账户层 + 客户端层 + 续签层，全部借用视图，
	宿主保证存活至 Obtain 返回。sStoreRoot 必填；iRenewalDays
	为 0 时默认 30（剩余寿命不足该天数即续签）。
*/
typedef struct xacmeobtainconfig {
	const xacmeaccountconfig* pAccount;
	cstr sCaPem;
	struct xnetengine* pBorrowedEngine;
	int64 uTimeoutMs;
	const cstr* sPropagateResolvers;
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/* 单次签发总预算（毫秒；0 = 不限时），透传给客户端。 */
	int64 uIssueTimeoutMs;
	/* 宿主提供的证书私钥 PEM（可选，EC/RSA），透传给客户端。 */
	cstr sCertKeyPem;
	cstr sStoreRoot;
	int iRenewalDays;
} xacmeobtainconfig;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_OBTAIN)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeObtainConfigInit(xacmeobtainconfig* pConfig);

/*
	一次调用取得可用证书（链 + 配对私钥）：
	  1. store 无账户则注册并持久化（accounts/<ca16>/account.pem），
	     有则复用（同一 CA 稳定账户，不反复开户）；
	  2. 本地证书剩余寿命充足时直接返回（*pbRenewed=false）；
	  3. 不足则完整 dns-01 签发并以 current 原子发布新版本
	     （certs/<主域名>/.grant-<16hex>/{key.pem,fullchain.pem,meta.txt}）。
	pOut 两段文本均 xrtFree（或 xrtAcmeGrantUnit 统一释放）。
	失败返回 false 并设置线程错误。
	返回值只表示证书操作结果。临时客户端清理独立留出至少 30 秒
	回滚预算；仍未退休时转移至待清理队列，宿主用
	xrtAcmeCleanupPending 重试并在退出/卸载前确认全部释放。
*/
XRT_API bool xrtAcmeObtain(
	const xacmeobtainconfig* pConfig,
	const xstrview* pDomains,
	size_t iDomainCount,
	const xacmednsprovider* pDns,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xacme/include/xacme.h */
/* ========================================================================== */

/*
	xacme —— 构建在 xrt 核心之上的 ACME (RFC 8555) 客户端扩展库。

	模块选择见 <xacme/features.h>（由 tools/generate_extension_features.py
	按清单生成）：定义 XACME_MODULE_<名> 点名模块，或不定义任何宏取全量。
*/
#ifndef XACME_H
#define XACME_H



#if defined(XACME_FEATURE_ACME_DNS)
#endif

#if defined(XACME_FEATURE_ACME_CORE)
#endif

#if defined(XACME_FEATURE_ACME_JOSE)
#endif

#if defined(XACME_FEATURE_ACME_CSR)
#endif

#if defined(XACME_FEATURE_ACME_HTTP)
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
#endif

#if defined(XACME_FEATURE_ACME_OBTAIN)
#endif

#if defined(XACME_FEATURE_ACME_STORE)
#endif

#if defined(XACME_FEATURE_DNS_ALI)
#endif

#if defined(XACME_FEATURE_DNS_CF)
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)
#endif

#if defined(XACME_FEATURE_DNS_AWS)
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)
#endif

#endif

#endif

#if defined(XACME_IMPLEMENTATION) && !defined(XACME_IMPLEMENTATION_ONCE)
#define XACME_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_http.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_HTTP)
#ifndef XACME_HTTP_H
#define XACME_HTTP_H


struct xnetengine;
struct xnetresolver;
struct xtlsverifier;

#if defined(XACME_FEATURE_ACME_HTTP)

/*
	同步 HTTP(S) 客户端：一次请求一条连接，引擎与信任库可借用。
	sCaPem 为空时使用系统信任库验证对端证书。
*/
typedef struct xacmehttp {
	struct xnetengine* pEngine;
	bool bEngineOwned;
	struct xnetresolver* pResolver;
	struct xtlsverifier* pVerifier;
	int64 uTimeoutMs;
	/* Last exchange failed after a write began; independent of error allocation. */
	bool bWriteUncertain;
	/* 未交付的堆外壳可转移到退休队列；入列后仅由队列访问。 */
	struct xacmehttp* pPendingNext;
	size_t iPendingOwnerSize;
} xacmehttp;

/* 响应字段均为 xrtMalloc 零结尾文本，缺失头为 NULL；正文拒绝原始 NUL。 */
typedef struct xacmehttpresponse {
	uint16 iStatus;
	str sLocation;
	str sReplayNonce;
	str sRetryAfter;
	str sLink;
	str sContentType;
	str sBody;
	size_t iBodySize;
} xacmehttpresponse;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_HTTP)

/* pBorrowedEngine 为空时自建引擎并在 Unit 中停止销毁。 */
bool xacmeHttpInit(
	xacmehttp* pHttp,
	struct xnetengine* pBorrowedEngine,
	cstr sCaPem,
	int64 uTimeoutMs
);

/* 等待自建引擎的异步关闭退休，保留调用前诊断。
 * true 表示清理完成；false 保留引擎拥有权，不能释放或清零外层对象。
 * 释放阻塞对象后可再次 Unit；无论结果如何，句柄只能继续清理，不能发请求。 */
bool xacmeHttpUnit(xacmehttp* pHttp);

/* 消费一个 HTTP 位于首字段的堆外壳。调用前须已释放其他子对象，
 * 且 Unit 返回 false；复用内嵌链结，不分配内存，立即清除外壳的非 HTTP 数据。 */
void xacmeHttpDeferOwner(xacmehttp* pHttp, size_t iOwnerSize);

void xacmeHttpResponseUnit(xacmehttpresponse* pResponse);

/*
	执行一次 HTTP 交换（http/https 均可）。
	sMethod 为 "GET"/"POST"；POST 空 body 即 POST-as-GET。
	失败返回 false 并设置线程错误；响应不交付部分字段。
	响应头/trailer 各最多 100 字段，正文最多 4 MiB；关闭定界 HTTPS 要求 close_notify。
	URL 支持大小写等价的 http/https 与 IPv6 方括号；无路径查询使用 /?，片段不发送。
	拒绝 userinfo、非法端口、非规范数值地址、未编码控制字符与畸形百分号编码。
	DNS SNI/校验名去末尾根点；IP 字面量只校验证书身份，不发送 SNI。
*/
bool xacmeHttpExchange(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	xacmehttpresponse* pResponse
);

/* 附加请求头（借用视图），用于签名类 API。 */
typedef struct xacmehttpheader {
	cstr sName;
	cstr sValue;
} xacmehttpheader;

/* 同上，支持附加请求头；总数最多 100，包括传输层自动生成的字段。
 * 不可覆盖 Host/User-Agent/Accept/Connection/Content-Type/Content-Length/Transfer-Encoding。 */
bool xacmeHttpExchangeV(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount,
	xacmehttpresponse* pResponse
);

/*
	单次尝试；需要逐次重新签名的 provider 自行管理有限重试。
	失败时响应结构保持全零，调用方无需释放部分响应。
	写请求开始发送后失败会以 XACME_HTTP_ERROR_UNCERTAIN 标记结果未知。
*/
bool xacmeHttpExchangeOnceV(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount,
	xacmehttpresponse* pResponse
);

#endif

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_jose.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_JOSE)
#ifndef XACME_JOSE_H
#define XACME_JOSE_H


#if defined(XACME_FEATURE_ACME_JOSE)

/* jose 模块稳定错误码（错误域 "xrt.acme.jose"）。 */
typedef enum xacmejoseerror {
	XACME_JOSE_ERROR_ARGUMENT = 1,
	XACME_JOSE_ERROR_INTERNAL
} xacmejoseerror;

/* ES256 账户/签名密钥；Private 为 32 字节标量，Public 为 65 字节未压缩点。 */
typedef struct xacmees256key {
	uint8 Private[XRT_P256_PRIVATE_SIZE];
	uint8 Public[XRT_P256_PUBLIC_SIZE];
} xacmees256key;

/* JWS 保护头成员：Nonce/Url/Kid 均为借用视图；Kid 为空时嵌入完整 JWK。 */
typedef struct xacmejwsheader {
	xstrview Nonce;
	xstrview Url;
	xstrview Kid;
} xacmejwsheader;

/* base64url 无填充的 32 字节摘要文本长度（含末尾零共 44）。 */
#define XACME_JWK_THUMBPRINT_TEXT_SIZE 44

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_JOSE)

/* 用操作系统安全随机源生成 P-256 密钥对。 */
bool xacmeEs256Generate(xacmees256key* pKey);

/* 从已有 32 字节私钥派生公钥；失败时保持 Private 之外的输出不变。 */
bool xacmeEs256FromPrivate(xacmees256key* pKey);

/* 规范化 EC JWK JSON（成员字典序：crv,kty,x,y）；返回 xrtFree 释放的文本。 */
str xacmeJwkEcJson(const xacmees256key* pKey);

/* RFC 7638 指纹：SHA-256(规范化 JWK JSON) 的 base64url 无填充文本。 */
bool xacmeJwkThumbprint(xstrview sCanonicalJson, char* sOut);

/* 等价于对 xacmeJwkEcJson 的输出取指纹。 */
bool xacmeJwkEcThumbprint(const xacmees256key* pKey, char* sOut);

/*
	ES256 JWS 紧凑序列化：base64url(header).base64url(payload).base64url(sig)。
	签名为 JOSE raw r||s（64 字节，非 DER）。payload 是调用方已备好的 JSON 字节。
	Url 必填（ACME 要求）；失败返回 NULL 并设置线程错误。
*/
str xacmeJwsEs256(
	const xacmees256key* pKey,
	const xacmejwsheader* pHeader,
	xstrview sPayload
);

/*
	RFC 8555 §7.3.4 External Account Binding 的内层 JWS：
	保护头 {"alg":"HS256","kid":<sKid>,"url":<sUrl>}，payload 为账户
	JWK JSON（借用视图），签名为 HMAC-SHA256(MAC) 裸 32 字节。
	输出扁平 JSON 序列化（xrtFree 释放），直接嵌入 newAccount 载荷。
*/
str xacmeJwsEabHs256(
	cstr sKid,
	cstr sUrl,
	xstrview sJwkJson,
	const uint8* pMac,
	size_t iMacSize
);

#endif

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_csr.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_CSR)
#ifndef XACME_CSR_H
#define XACME_CSR_H



#if defined(XACME_FEATURE_ACME_CSR)

/* csr 模块稳定错误码（错误域 "xrt.acme.csr"）。 */
typedef enum xacmecsrerror {
	XACME_CSR_ERROR_ARGUMENT = 1,
	XACME_CSR_ERROR_INTERNAL
} xacmecsrerror;

/* CSR 输入全部借用视图；Domains 为 SAN 列表（可含通配符）。 */
typedef struct xacmecsrconfig {
	xstrview CommonName;
	const xstrview* Domains;
	size_t DomainCount;
} xacmecsrconfig;

/* 证书密钥算法：ES256 内部生成，或宿主提供的 RSA（PKCS#8/PKCS#1）。 */
typedef enum xacmecertkeykind {
	XACME_CERT_KEY_ES256 = 0,
	XACME_CERT_KEY_RSA
} xacmecertkeykind;

/*
	RSA 私钥的定长持有形态（大端正整数字节，无符号前导零已剥）。
	CRT 五参数齐备时签名走 CRT 快路径，否则要求完整私有指数。
*/
typedef struct xacmersakey {
	uint8 Modulus[1024];
	size_t ModulusSize;
	uint8 Exponent[16];
	size_t ExponentSize;
	uint8 PrivateExponent[1024];
	size_t PrivateExponentSize;
	uint8 Prime1[520];
	size_t Prime1Size;
	uint8 Prime2[520];
	size_t Prime2Size;
	uint8 Exponent1[520];
	size_t Exponent1Size;
	uint8 Exponent2[520];
	size_t Exponent2Size;
	uint8 Coefficient[520];
	size_t CoefficientSize;
} xacmersakey;

/* 证书密钥统一外壳；不用的分支内容未定义。 */
typedef struct xacmecertkey {
	xacmecertkeykind Kind;
	xacmees256key Ec;
	xacmersakey Rsa;
} xacmecertkey;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_CSR)

/* 生成 ES256 PKCS#10 CSR（DER）追加到 pOut；SAN 扩展经 extensionRequest。 */
bool xacmeCsrEc(
	const xacmees256key* pKey,
	const xacmecsrconfig* pConfig,
	xbuffer* pOut
);

/* PKCS#8 未加密 EC 私钥 PEM（"PRIVATE KEY"）；返回 xrtFree 释放文本。 */
str xacmeKeyPemWrite(const xacmees256key* pKey);

/* 解析 PKCS#8 或 SEC1 EC 私钥 PEM；公钥缺失时从私钥派生。 */
bool xacmeKeyPemRead(cstr sPem, size_t iSize, xacmees256key* pKey);

/*
	解析证书密钥 PEM（自动识别）：EC（PKCS#8/SEC1）或 RSA
	（PKCS#8 "PRIVATE KEY" / PKCS#1 "RSA PRIVATE KEY"）。
	输出敏感，宿主用 xacmeCertKeyUnit 擦除。
*/
bool xacmeCertKeyReadPem(cstr sPem, size_t iSize, xacmecertkey* pKey);

/* 序列化为未加密 PKCS#8 PEM（EC 或 RSA）；xrtFree 释放。 */
str xacmeCertKeyPemWrite(const xacmecertkey* pKey);

/* 擦除并清零（含 EC/RSA 两分支全量）。 */
void xacmeCertKeyUnit(xacmecertkey* pKey);

/* 按密钥算法生成 PKCS#10 CSR（DER）追加到 pOut。 */
bool xacmeCsrBuild(
	const xacmecertkey* pKey,
	const xacmecsrconfig* pConfig,
	xbuffer* pOut
);

#endif

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dnstxt.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_DNS)
#ifndef XACME_DNSTXT_H
#define XACME_DNSTXT_H


struct xnetengine;

#if defined(XACME_FEATURE_ACME_DNS)

/* dns_txt 模块稳定错误码（错误域 "xrt.acme.dns.txt"）。 */
typedef enum xacmednstxterror {
	XACME_TXT_ERROR_ARGUMENT = 1,
	XACME_TXT_ERROR_NETWORK,
	XACME_TXT_ERROR_PROTOCOL,
	XACME_TXT_ERROR_TIMEOUT
} xacmednstxterror;

/* TXT 记录值上限：ACME dns-01 的 base64url(SHA-256) 恒为 43 字节。 */
#define XACME_TXT_RECORD_MAX 256u

/* 迷你 DNS 客户端：仅实现 TXT 查询所需的 UDP 报文收发与解析。 */
typedef struct xacmedns {
	struct xnetengine* pEngine;
	bool bEngineOwned;
} xacmedns;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_DNS)

/* pBorrowedEngine 为空时自建引擎。 */
bool xacmeDnsInit(xacmedns* pDns, struct xnetengine* pBorrowedEngine);
/* 等待私有引擎退休（最多 30 秒）；false 保留拥有者供重试。
 * 借用引擎不停止；调用前诊断保留。Init 失败后也须清理非空实例。 */
bool xacmeDnsUnit(xacmedns* pDns);

/*
	一次 TXT 查询。记录值拷入调用方数组（每元素 XACME_TXT_RECORD_MAX
	字节），无记录时成功且计数为零。失败设置线程错误，非空输出计数
	清零；分配失败保留原 MEMORY 诊断，不再重发。
	sResolver 为 IP 字面量（v1 不解析解析器域名）。
*/
bool xacmeDnsTxtQuery(
	xacmedns* pDns,
	cstr sResolver,
	uint16 iPort,
	cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX],
	size_t iCapacity,
	size_t* pOutCount
);

/*
	解析完整 DNS 响应报文为 TXT 记录集合（不可信网络输入的唯一
	消化口，fuzz 目标）。QR 位缺失/结构损坏 → false；RCODE 非零
	→ true 且零记录。每条记录严格小于 XACME_TXT_RECORD_MAX。
	输出计数必须非空；失败时不发布部分记录计数。
*/
bool xacmeTxtParseResponse(
	const uint8* pData,
	size_t iSize,
	uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX],
	size_t iCapacity,
	size_t* pOutCount
);

/* 轮询直到期望 TXT 值在解析器可见；超时返回 false。 */
bool xacmeDnsTxtWait(
	xacmedns* pDns,
	cstr sResolver,
	uint16 iPort,
	cstr sFqdn,
	cstr sExpected,
	int64 uTimeoutMs
);

#endif

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dnscommon.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_ALI) || \
	defined(XACME_FEATURE_DNS_CF) || \
	defined(XACME_FEATURE_DNS_TENCENT) || \
	defined(XACME_FEATURE_DNS_AWS) || \
	defined(XACME_FEATURE_DNS_HUAWEI)
#ifndef XACME_DNSCOMMON_H
#define XACME_DNSCOMMON_H

/*
	DNS provider 公共助手：挑战名称校验、记录句柄登记、
	JSON 文本追加/取值。全部 static 实现，供各家 provider 内部复用，
	不进入公开 API。
*/



#include <stdio.h>
#include <string.h>

#define XACME_DNS_RECORD_MAX 8u
#define XACME_DNS_RECORD_TEXT_CAP 320u

#if defined(XACME_FEATURE_ACME_DNS) && defined(XACME_FEATURE_ACME_HTTP)

/* Unit is serialized by the host. Check the context before touching its lock. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static ptr xacmeDnsProviderContext(const xacmednsprovider* pProvider)
{
	if(pProvider == NULL)
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"acme DNS callback requires a provider");
		return NULL;
	}
	if(pProvider->pContext == NULL)
	{
		xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_STATE,
			"acme DNS provider context has been released");
		return NULL;
	}
	return pProvider->pContext;
}

/* A delivered HTTP transport owns all three resources. Unit revokes the
 * resolver and verifier even when engine retirement must be retried. Check
 * this under the provider lock, before reuse, reservations or empty Remove. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsProviderReady(const xacmehttp* pHttp)
{
	if(pHttp->pEngine != NULL && pHttp->pResolver != NULL && pHttp->pVerifier != NULL)
		return true;
	xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_STATE,
		"acme DNS provider cleanup must finish before the instance can be released");
	return false;
}
#endif

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsHttpsUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery)
{
	int iWritten;
	if((sOutput == NULL) || (iCapacity == 0u) ||
		(sEndpoint == NULL) || (sPathAndQuery == NULL))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "DNS HTTPS URL input invalid");
		return false;
	}
	iWritten = snprintf(sOutput, iCapacity, "https://%s%s",
		sEndpoint, sPathAndQuery);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		sOutput[0] = '\0';
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "DNS HTTPS URL too long");
		return false;
	}
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsChallengeValid(xstrview sFqdn, xstrview sTxt)
{
	size_t i;
	bool bDot = false;
	if((sFqdn.Data == NULL) || (sTxt.Data == NULL) ||
		(sFqdn.Size == 0u) || (sFqdn.Size >= 256u) ||
		(sTxt.Size == 0u) || (sTxt.Size > 200u))
		return false;
	for(i = 0u; i < sFqdn.Size; i++)
	{
		unsigned char c = (unsigned char)sFqdn.Data[i];
		if(c == '.')
		{
			if((i == 0u) || (i + 1u == sFqdn.Size) ||
				(sFqdn.Data[i - 1u] == '.'))
				return false;
			bDot = true;
		}
		else if(!((c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			(c == '-') || (c == '_')))
			return false;
	}
	if(!bDot)
		return false;
	for(i = 0u; i < sTxt.Size; i++)
	{
		unsigned char c = (unsigned char)sTxt.Data[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || (c == '-') || (c == '_')))
			return false;
	}
	return true;
}

/* 本 provider 生命周期内添加的记录句柄及其 DNS-01 属主/值。 */
typedef struct xacmednsrecords {
	char sIds[XACME_DNS_RECORD_MAX][XACME_DNS_RECORD_TEXT_CAP];
	char sFqdns[XACME_DNS_RECORD_MAX][256];
	char sTxts[XACME_DNS_RECORD_MAX][201];
	size_t iCount;
} xacmednsrecords;

typedef enum xacmednsownedresult {
	XACME_DNS_OWNED_NONE,
	XACME_DNS_OWNED_VALID,
	XACME_DNS_OWNED_ERROR
} xacmednsownedresult;

/* Only a saved, confirmed ID can establish local ownership; matching text alone cannot. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static size_t xacmeDnsRecordFindOwned(const xacmednsrecords* pRecords,
	xstrview Owner, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] != '\0' &&
			strlen(pRecords->sFqdns[i]) == Owner.Size &&
			memcmp(pRecords->sFqdns[i], Owner.Data, Owner.Size) == 0 &&
			strlen(pRecords->sTxts[i]) == Txt.Size &&
			memcmp(pRecords->sTxts[i], Txt.Data, Txt.Size) == 0) return i;
	return XACME_DNS_RECORD_MAX;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordCanAdd(const xacmednsrecords* pRecords)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] == '\0')
			return true;
	return pRecords->iCount < XACME_DNS_RECORD_MAX;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRemember(
	xacmednsrecords* pRecords, cstr sId, xstrview sFqdn, xstrview sTxt)
{
	size_t iSize = strlen(sId);
	size_t i;
	if(!xacmeDnsRecordCanAdd(pRecords) ||
		(iSize == 0u) || (iSize >= XACME_DNS_RECORD_TEXT_CAP) ||
		!xacmeDnsChallengeValid(sFqdn, sTxt))
		return false;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pRecords->sIds[i][0] == '\0')
			break;
	if(i == pRecords->iCount)
		pRecords->iCount++;
	memcpy(pRecords->sIds[i], sId, iSize + 1u);
	memcpy(pRecords->sFqdns[i], sFqdn.Data, sFqdn.Size);
	pRecords->sFqdns[i][sFqdn.Size] = '\0';
	memcpy(pRecords->sTxts[i], sTxt.Data, sTxt.Size);
	pRecords->sTxts[i][sTxt.Size] = '\0';
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRememberPair(xacmednsrecords* pRecords,
	cstr sLeft, char cSeparator, cstr sRight,
	xstrview sFqdn, xstrview sTxt)
{
	char sHandle[XACME_DNS_RECORD_TEXT_CAP];
	size_t iLeft = strlen(sLeft);
	size_t iRight = strlen(sRight);
	if((iLeft == 0u) || (iRight == 0u) ||
		(strchr(sLeft, cSeparator) != NULL) ||
		(strchr(sRight, cSeparator) != NULL) ||
		(iRight >= sizeof(sHandle) - 1u) ||
		(iLeft >= sizeof(sHandle) - iRight - 1u))
		return false;
	memcpy(sHandle, sLeft, iLeft);
	sHandle[iLeft] = cSeparator;
	memcpy(sHandle + iLeft + 1u, sRight, iRight + 1u);
	return xacmeDnsRecordRemember(pRecords, sHandle, sFqdn, sTxt);
}

typedef bool (*xacmednsrecorddeleteproc)(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt);

/* A create reservation lives independently of fallible error/JSON allocation.
 * Providers call these helpers while holding their context mutex. */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateUncertainError(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "DNS create outcome and ownership are unknown");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static xstrview xacmeDnsCanonicalOwner(xstrview Owner, char sOut[256])
{
	size_t i;
	for(i = 0u; i < Owner.Size; i++) {
		char c = Owner.Data[i];
		sOut[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
	}
	sOut[Owner.Size] = '\0';
	return (xstrview){ sOut, Owner.Size };
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateBlocked(const xacmednsrecords* pRecords,
	const bool* pUncertain, xstrview Owner, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(pUncertain[i] && strlen(pRecords->sFqdns[i]) == Owner.Size &&
			memcmp(pRecords->sFqdns[i], Owner.Data, Owner.Size) == 0 &&
			strlen(pRecords->sTxts[i]) == Txt.Size &&
			memcmp(pRecords->sTxts[i], Txt.Data, Txt.Size) == 0) return true;
	return false;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static size_t xacmeDnsCreateSlot(const xacmednsrecords* pRecords,
	const bool* pUncertain)
{
	size_t i;
	for(i = 0u; i < pRecords->iCount; i++)
		if(!pUncertain[i] && pRecords->sIds[i][0] == '\0') return i;
	return pRecords->iCount;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsCreateReserve(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot, xstrview Owner, xstrview Txt)
{
	if(iSlot == pRecords->iCount) pRecords->iCount++;
	pRecords->sIds[iSlot][0] = '\0';
	memcpy(pRecords->sFqdns[iSlot], Owner.Data, Owner.Size);
	pRecords->sFqdns[iSlot][Owner.Size] = '\0';
	memcpy(pRecords->sTxts[iSlot], Txt.Data, Txt.Size);
	pRecords->sTxts[iSlot][Txt.Size] = '\0';
	pUncertain[iSlot] = true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeDnsCreateCancel(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot)
{
	pRecords->sIds[iSlot][0] = '\0';
	pRecords->sFqdns[iSlot][0] = '\0';
	pRecords->sTxts[iSlot][0] = '\0';
	pUncertain[iSlot] = false;
	while(pRecords->iCount != 0u &&
		!pUncertain[pRecords->iCount - 1u] &&
		pRecords->sIds[pRecords->iCount - 1u][0] == '\0') pRecords->iCount--;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsCreateCommit(xacmednsrecords* pRecords, bool* pUncertain,
	size_t iSlot, cstr sLeft, char Separator, cstr sRight)
{
	char sHandle[XACME_DNS_RECORD_TEXT_CAP];
	size_t iLeft = strlen(sLeft), iRight = strlen(sRight), i;
	if(iLeft == 0u || iRight == 0u || strchr(sLeft, Separator) != NULL ||
		strchr(sRight, Separator) != NULL || iRight >= sizeof(sHandle) - 1u ||
		iLeft >= sizeof(sHandle) - iRight - 1u) return false;
	memcpy(sHandle, sLeft, iLeft); sHandle[iLeft] = Separator;
	memcpy(sHandle + iLeft + 1u, sRight, iRight + 1u);
	/* Reusing an owned ID is not proof that this create is ours. */
	for(i = 0u; i < pRecords->iCount; i++)
		if(i != iSlot && strcmp(pRecords->sIds[i], sHandle) == 0) return false;
	memcpy(pRecords->sIds[iSlot], sHandle, iLeft + iRight + 2u);
	pUncertain[iSlot] = false;
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordRemoveMatching(xacmednsrecords* pRecords,
	xstrview sFqdn, xstrview sTxt, xacmednsrecorddeleteproc Delete,
	void* pContext)
{
	size_t i;
	if((Delete == NULL) || !xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	for(i = 0u; i < pRecords->iCount; i++)
	{
		if((pRecords->sIds[i][0] == '\0') ||
			(strlen(pRecords->sFqdns[i]) != sFqdn.Size) ||
			(memcmp(pRecords->sFqdns[i], sFqdn.Data, sFqdn.Size) != 0) ||
			(strlen(pRecords->sTxts[i]) != sTxt.Size) ||
			(memcmp(pRecords->sTxts[i], sTxt.Data, sTxt.Size) != 0))
			continue;
		if(!Delete(pContext, pRecords->sIds[i], sFqdn, sTxt))
		{
			if(xrtErrorKind(xrtGetError()) == XERR_NONE)
				xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
					XACME_DNS_ERROR_PROTOCOL,
					"acme DNS-01 record deletion failed");
			return false;
		}
		pRecords->sIds[i][0] = '\0';
		pRecords->sFqdns[i][0] = '\0';
		pRecords->sTxts[i][0] = '\0';
	}
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsRecordSplit(cstr sHandle, char cSeparator,
	char* sLeft, size_t iLeftCap, char* sRight, size_t iRightCap)
{
	const char* sSeparator = strchr(sHandle, cSeparator);
	size_t iLeft;
	size_t iRight;
	if((sSeparator == NULL) || (sSeparator == sHandle) ||
		(sSeparator[1] == '\0') ||
		(strchr(sSeparator + 1u, cSeparator) != NULL))
		return false;
	iLeft = (size_t)(sSeparator - sHandle);
	iRight = strlen(sSeparator + 1u);
	if((iLeft >= iLeftCap) || (iRight >= iRightCap))
		return false;
	memcpy(sLeft, sHandle, iLeft);
	sLeft[iLeft] = '\0';
	memcpy(sRight, sSeparator + 1u, iRight + 1u);
	return true;
}

/* 把借用文本按 JSON 字符串 token（含引号）转义追加。 */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonQuote(xbuffer* pOut, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pOut, (uint8)'"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bOk;
		if((c == '"') || (c == '\\'))
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)'\\') &&
				xrtBufferAppendByte(pOut, (uint8)c);
		}
		else
		{
			/* 域名与 base64url 值不含控制字符；其余原样透传。 */
			bOk = xrtBufferAppendByte(pOut, (uint8)c);
		}
		if(!bOk)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pOut, (uint8)'"');
}

/* 取 JSON 对象字符串成员到固定缓冲（含末尾零）；失败返回 false。 */
#if defined(XRT_FEATURE_VALUE_CONTAINER)
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonText(
	const xvalue* pObject, cstr sKey, char* sOut, size_t iCapacity)
{
	xvalue* pMember;
	xstrview Text;
	size_t i;
	if((pObject == NULL) || !xrtValueIs(pObject, XVALUE_OBJECT) ||
		(sKey == NULL) || (sOut == NULL) || (iCapacity == 0u))
		return false;
	sOut[0] = '\0';
	pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	if((pMember == NULL) || !xrtValueGetString(pMember, &Text) ||
		(Text.Size >= iCapacity) ||
		((Text.Size != 0u) && (Text.Data == NULL)))
	{
		return false;
	}
	/* 下游使用 C 字符串；不能让 JSON 的转义 NUL 改变比较或路径。 */
	for(i = 0u; i < Text.Size; i++)
	{
		unsigned char c = (unsigned char)Text.Data[i];
		if((c < 0x20u) || (c == 0x7fu)) return false;
	}
	if(Text.Size != 0u) memcpy(sOut, Text.Data, Text.Size);
	sOut[Text.Size] = '\0';
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonPathId(
	const xvalue* pObject, cstr sKey, char* sOut, size_t iCapacity)
{
	size_t i;
	if(!xacmeDnsJsonText(pObject, sKey, sOut, iCapacity)) return false;
	if(sOut[0] == '\0') return false;
	if((strcmp(sOut, ".") == 0) || (strcmp(sOut, "..") == 0))
	{
		sOut[0] = '\0';
		return false;
	}
	for(i = 0u; sOut[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sOut[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || (c == '-') || (c == '_') ||
			(c == '.') || (c == '~')))
		{
			sOut[0] = '\0';
			return false;
		}
	}
	return true;
}

typedef enum xacmednszoneresult {
	XACME_DNS_ZONE_ERROR = -1,
	XACME_DNS_ZONE_MISSING = 0,
	XACME_DNS_ZONE_FOUND = 1
} xacmednszoneresult;

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonEqual(const xvalue* pObject, cstr sKey, cstr sWanted)
{
	char sText[512];
	return xacmeDnsJsonText(pObject, sKey, sText, sizeof(sText)) &&
		strcmp(sText, sWanted) == 0;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeDnsJsonSuccess(const xvalue* pRoot)
{
	bool bSuccess = false;
	xvalue* pSuccess = (pRoot != NULL &&
		xrtValueIs(pRoot, XVALUE_OBJECT)) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success")) : NULL;
	return (pSuccess != NULL) && xrtValueGetBool(pSuccess, &bSuccess) &&
		bSuccess;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static xacmednszoneresult xacmeDnsJsonZoneId(
	xstrview Body, cstr sArrayKey, cstr sWantedName, bool bRequireSuccess,
	size_t iPageLimit,
	char* sOutId, size_t iIdCapacity)
{
	xvalue* pRoot = (Body.Data != NULL) ? xrtJsonParse(Body) : NULL;
	xvalue* pZones = (pRoot != NULL && xrtValueIs(pRoot, XVALUE_OBJECT)) ?
		xrtValueObjectGet(pRoot, (xstrview){ sArrayKey, strlen(sArrayKey) }) :
		NULL;
	xacmednszoneresult Result = XACME_DNS_ZONE_MISSING;
	size_t i;
	/* A parser allocation failure is the first cause, not a zone miss or a
	 * schema error. Successful parsing must not preserve an unrelated old OOM. */
	if(pRoot == NULL && Body.Data != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_MEMORY)
	{
		if(sOutId != NULL && iIdCapacity != 0u) sOutId[0] = '\0';
		return XACME_DNS_ZONE_ERROR;
	}
	if((bRequireSuccess && !xacmeDnsJsonSuccess(pRoot)) ||
		(pZones == NULL) || !xrtValueIs(pZones, XVALUE_ARRAY))
	{
		Result = XACME_DNS_ZONE_ERROR;
		goto Done;
	}
	/* 页已填满时可能还有未读取的同名 zone，不能选择首项。 */
	if((iPageLimit != 0u) && (xrtValueCount(pZones) >= iPageLimit))
	{
		Result = XACME_DNS_ZONE_ERROR;
		goto Done;
	}
	for(i = 0u; i < xrtValueCount(pZones); i++)
	{
		xvalue* pItem = xrtValueArrayGet(pZones, i);
		char sName[280];
		if((pItem == NULL) || !xrtValueIs(pItem, XVALUE_OBJECT) ||
			!xacmeDnsJsonText(pItem, "name", sName, sizeof(sName)))
		{
			Result = XACME_DNS_ZONE_ERROR;
			break;
		}
		if(strcmp(sName, sWantedName) == 0)
		{
			if(Result == XACME_DNS_ZONE_FOUND)
			{
				Result = XACME_DNS_ZONE_ERROR;
				break;
			}
			Result = xacmeDnsJsonPathId(pItem, "id", sOutId,
				iIdCapacity) ? XACME_DNS_ZONE_FOUND : XACME_DNS_ZONE_ERROR;
			if(Result == XACME_DNS_ZONE_ERROR) break;
		}
	}
Done:
	if((sOutId != NULL) && (iIdCapacity != 0u) &&
		(Result != XACME_DNS_ZONE_FOUND)) sOutId[0] = '\0';
	xrtValueRelease(pRoot);
	if(Result == XACME_DNS_ZONE_ERROR)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"acme DNS managed zone response invalid");
	return Result;
}
#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dns_ali_internal.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_ALI)
#ifndef XACME_DNS_ALI_INTERNAL_H
#define XACME_DNS_ALI_INTERNAL_H


/* 按 ACS3 规则校验并排序当前 provider 支持的 ASCII query 参数。 */
bool xacmeDnsAliCanonicalQuery(
	cstr sQuery, char* sOut, size_t iCapacity);

/* 固定时间和 nonce 的离线签名入口；实际请求需生成独立随机 nonce。 */
bool xacmeDnsAliAuthorization(
	cstr sKeyId, cstr sSecret, cstr sEndpoint, cstr sAction,
	cstr sQuery, cstr sNonce, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sDate, size_t iDateCapacity,
	char* sCanonicalQuery, size_t iQueryCapacity);

/* 校验创建响应中的 RecordId 可安全用于后续删除 query。 */
bool xacmeDnsAliCreateResponseId(
	cstr sBody, char* sRecordId, size_t iCapacity);

typedef enum xacmednsalizoneoutcome {
	XACME_ALI_ZONE_ERROR = -1,
	XACME_ALI_ZONE_MISSING = 0,
	XACME_ALI_ZONE_FOUND = 1
} xacmednsalizoneoutcome;

/* Validate the one-record discovery page; only explicit absence permits fallback. */
xacmednsalizoneoutcome xacmeDnsAliZoneResponse(
	uint16 iStatus, cstr sBody, cstr sDomain);

typedef enum xacmednsalirecordoutcome {
	XACME_ALI_RECORD_ERROR = -1,
	XACME_ALI_RECORD_MISSING = 0,
	XACME_ALI_RECORD_FOUND = 1
} xacmednsalirecordoutcome;

/* Confirm the tracked id and DNS tuple; absence never establishes ownership. */
xacmednsalirecordoutcome xacmeDnsAliRecordResponse(
	uint16 iStatus, cstr sBody, cstr sId, cstr sFqdn, cstr sTxt, bool* pEnabled);

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_flow.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_FLOW)
#ifndef XACME_FLOW_H
#define XACME_FLOW_H




struct xacmednsprovider;

/* 传播确认内置默认 resolver 组（任一可见即通过）。 */
#define XACME_FLOW_RESOLVER_MAX 4u
#define XACME_FLOW_PROPAGATE_TIMEOUT_MS 120000u

#if defined(XACME_FEATURE_ACME_FLOW)

/* flow 模块稳定错误码（错误域 "xrt.acme.flow"）。 */
typedef enum xacmeflowerror {
	XACME_FLOW_ERROR_ARGUMENT = 1,
	XACME_FLOW_ERROR_DIRECTORY,
	XACME_FLOW_ERROR_ACCOUNT,
	XACME_FLOW_ERROR_NONCE,
	XACME_FLOW_ERROR_ORDER,
	XACME_FLOW_ERROR_CHALLENGE,
	XACME_FLOW_ERROR_FINALIZE,
	XACME_FLOW_ERROR_CERTIFICATE,
	XACME_FLOW_ERROR_STORE,
	XACME_FLOW_ERROR_PROTOCOL
} xacmeflowerror;

/*
	ACME 客户端：账户密钥 + directory 端点 + nonce 槽。
	DirectoryUrl 任意 RFC 8555 CA；AccountKeyPem 为空时生成 ES256。
*/
typedef struct xacmeclient {
	xacmehttp Http;
	xacmees256key AccountKey;
	/* 宿主提供的证书密钥（EC/RSA）；为空则每次签发生成 ES256。 */
	xacmecertkey* pCertKey;
	char sDirectoryUrl[512];
	char sKid[512];
	char sNewNonce[512];
	char sNewAccount[512];
	char sNewOrder[512];
	char sRevokeCert[512];
	char sKeyChange[512];
	char sNonce[512];
	/* 传播确认 resolver（IP 字面量）与预算；空组走默认组。 */
	char sPropagateResolvers[XACME_FLOW_RESOLVER_MAX][64];
	size_t iPropagateResolverCount;
	uint32 uPropagateTimeoutMs;
	/* 单次签发的总预算（毫秒；0 = 不限时）。Issue 入口打点，
	   轮询/传播/退避逐段检查剩余时间。 */
	int64 uIssueTimeoutMs;
	double IssueDeadline;
	bool bIssueDeadline;
} xacmeclient;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_FLOW)

/*
	初始化：建传输（pBorrowedEngine 为空则自建）、解析 directory、
	注册或复用账户（kid 来自 Location 头）。pAccount 携带 directory、
	账户密钥、EAB 与联系方式（借用视图，宿主保证存活至返回）。
	uTimeoutMs 为 0 时取传输默认（30 秒）。失败设置线程错误。
*/
bool xacmeClientInit(
	xacmeclient* pClient,
	struct xnetengine* pBorrowedEngine,
	cstr sCaPem,
	const xacmeaccountconfig* pAccount,
	int64 uTimeoutMs
);

/* false 时保留传输拥有者，只能继续清理。 */
bool xacmeClientUnit(xacmeclient* pClient);

/* 消费尚未交付的堆客户端；退休未完成时把拥有权转移到公共待清理队列。 */
void xacmeClientDiscard(xacmeclient* pClient);

/* 账户密钥的 PKCS#8 PEM 导出（xrtFree 释放），宿主可持久化后传入 Init 复用。 */
str xacmeClientAccountPem(const xacmeclient* pClient);

/*
	一次 dns-01 签发：域名可含通配符（*. 前缀）；产物含证书链与
	配对私钥（均 xrtFree）。bAlt 时若证书响应带 rel="alternate"
	备用链则优先采用（失败回退主链）。链中仅接受证书对象，核对
	CSR 公钥、SAN、叶有效期及所提供
	链的相邻签名；备用链要求同一 DER 叶证书，相对 URI 以主下载 URL
	解析。此检查不代替部署根信任与完整 PKIX 策略。失败返回 false
	并保留线程错误；provider 的 Add 在挑战触发前调用、Remove 在结束后尽力
	调用。
*/
bool xacmeClientIssue(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns,
	xacmeissuegrant* pOut,
	bool bPreferAlternate
);

/*
	账户密钥滚动（RFC 8555 §7.3.5）：用 sNewKeyPem（PKCS#8/SEC1）
	替换当前账户密钥，kid 不变。要求 directory 提供 keyChange 端点。
	sStoreRoot 非空时成功后自动把新账户钥重存进该 store（按
	sDirectoryUrl 隔离），消除滚动后 Obtain 读旧钥开新账户的漂移。
*/
bool xacmeClientRollover(
	xacmeclient* pClient,
	cstr sNewKeyPem,
	cstr sStoreRoot
);

/*
	账户停用（RFC 8555 §7.3.6）：向账户 URL 提交 deactivated。
	停用后该账户及其订单永久不可用；幂等（已停用视为成功）。
*/
bool xacmeClientDeactivate(xacmeclient* pClient);

/*
	一站式续签（组合 store）：
	本地证书剩余寿命不少于 iRenewalDays 天时 *pbRenewed=false 并
	直接返回现有链与私钥；否则签发、落盘（key.pem + fullchain.pem
	+ CA 溯源）并返回新产物。pDomains[0] 同时是 store 的主域名键。
*/
bool xacmeClientIssueStored(
	xacmeclient* pClient,
	const xstrview* pDomains,
	size_t iDomainCount,
	const struct xacmednsprovider* pDns,
	cstr sStoreRoot,
	int iRenewalDays,
	xacmeissuegrant* pOut,
	bool* pbRenewed
);

/*
	吊销证书（RFC 8555 §7.6，账户钥签名）：sCertPem 为单张证书
	（取首个 PEM 块）；iReason 0-9（RFC 5280 CRLReason），<0 省略。
	已被吊销视为成功。要求 directory 提供 revokeCert 端点。
*/
bool xacmeClientRevoke(
	xacmeclient* pClient,
	cstr sCertPem,
	int iReason
);

#endif

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dns_tencent_internal.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_TENCENT)
#ifndef XACME_DNS_TENCENT_INTERNAL_H
#define XACME_DNS_TENCENT_INTERNAL_H


/* API 3.0 在 HTTP 2xx 内仍可能返回 Response.Error。 */
bool xacmeDnsTencentResponseSuccess(xstrview sBody);

/* 固定一次 UTC 时间，生成与 X-TC-Timestamp 一致的 TC3 Authorization。 */
bool xacmeDnsTencentAuthorization(
	cstr sId, cstr sKey, cstr sEndpoint, cstr sAction, cstr sBody,
	xtime iNow, char* sAuth, size_t iAuthCapacity,
	char* sTimestamp, size_t iTimestampCapacity
);

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_sigv4.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_TENCENT) || \
	defined(XACME_FEATURE_DNS_AWS) || \
	defined(XACME_FEATURE_DNS_HUAWEI)
#ifndef XACME_SIGV4_H
#define XACME_SIGV4_H

/*
	AWS SigV4 家族签名公共件（TC3 / AWS4 / SDK-HMAC-SHA256 共用骨架）：
	三方差异只在 StringToSign 前缀与派生密钥链长度，canonical request
	形状一致（method\nuri\nquery\nheaders\nsignedheaders\npayloadhash）。
	全部 static 实现，仅内部使用。
*/


#include <stdio.h>
#include <string.h>

#define XACME_SIG_HASH_TEXT 65u

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void xacmeSigHex(const uint8* pData, size_t iSize, char* sOut)
{
	size_t i;
	for(i = 0; i < iSize; i++)
	{
		sprintf(sOut + i * 2u, "%02x", pData[i]);
	}
	sOut[iSize * 2u] = '\0';
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigSha256Hex(
	const void* pData, size_t iSize, char* sOut)
{
	uint8 Digest[XRT_SHA256_SIZE];
	if(!xrtSha256(pData, iSize, Digest))
	{
		return false;
	}
	xacmeSigHex(Digest, sizeof(Digest), sOut);
	return true;
}

#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigHmac(
	const uint8* pKey, size_t iKeySize, const void* pData, size_t iSize,
	uint8 pOut[XRT_SHA256_SIZE])
{
	return xrtHmacSha256(pKey, iKeySize, pData, iSize, pOut);
}

/*
	组装 canonical request。canonHeaders 形如
	"content-type:v\nhost:h\n"（键小写、按字典序、值裁剪首尾空白），
	sSignedHeaders 形如 "content-type;host"。
*/
#if defined(__GNUC__)
__attribute__((unused))
#endif
static bool xacmeSigCanonical(
	char* sOut, size_t iCapacity, cstr sMethod, cstr sUri, cstr sQuery,
	cstr sCanonHeaders, cstr sSignedHeaders, cstr sPayloadHashHex)
{
	int iWritten = snprintf(
		sOut, iCapacity, "%s\n%s\n%s\n%s\n%s\n%s", sMethod, sUri,
		((sQuery != NULL) ? sQuery : ""), sCanonHeaders, sSignedHeaders,
		sPayloadHashHex);
	return (iWritten > 0) && ((size_t)iWritten < iCapacity);
}

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dns_aws_internal.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_AWS)
#ifndef XACME_DNS_AWS_INTERNAL_H
#define XACME_DNS_AWS_INTERNAL_H


#define XACME_AWS_TXT_MAX_VALUES 400u

/* Values 借用 ListResourceRecordSets 响应体，保留原 XML 转义文本。 */
typedef struct xacmeawstxtset {
	bool bPresent;
	char sTtl[24];
	xstrview Values[XACME_AWS_TXT_MAX_VALUES];
	size_t iCount;
} xacmeawstxtset;

bool xacmeAwsParseTxtSet(cstr sXml, cstr sFqdn, xacmeawstxtset* pSet);
/* Only a complete Route53 change acknowledgment proves accepted submission. */
bool xacmeAwsChangeAccepted(cstr sXml);
/* Validate the complete bounded REST-XML error envelope; reset output on failure. */
bool xacmeAwsErrorCode(xstrview Xml, char sCode[64]);
bool xacmeAwsBuildTxtChange(const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, xbuffer* pOut, bool* pChanged);
/* SigV4 Route53 密钥链；终止字符串不包含 C 字符串的结尾 NUL。 */
bool xacmeAwsSigningKey(cstr sSecret, cstr sDate, cstr sRegion,
	uint8 pKey[32]);
/* 1=唯一公有 zone，0=不存在，-1=畸形或同名公有 zone 歧义。 */
int xacmeAwsSelectPublicZone(cstr sXml, cstr sZone,
	char* sOutId, size_t iIdCap);

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xacme/src/internal/xacme_dns_huawei_internal.h */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_HUAWEI)
#ifndef XACME_DNS_HUAWEI_INTERNAL_H
#define XACME_DNS_HUAWEI_INTERNAL_H


/* 拼接签名请求的 HTTPS URL；空间不足时清空输出并失败。 */
bool xacmeDnsHuaweiBuildUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery);

/* 构造 recordsets 创建请求，输出缓冲由调用方初始化并释放。 */
bool xacmeDnsHuaweiBuildCreateBody(
	xbuffer* pBody, xstrview sFqdn, xstrview sTxt);

/* 按华为云 AK/SK 规则分离 URI、排序 query 并生成签名头。 */
bool xacmeDnsHuaweiCanonicalTarget(cstr sTarget,
	char* sUri, size_t iUriCapacity, char* sQuery, size_t iQueryCapacity);
bool xacmeDnsHuaweiAuthorization(
	cstr sAk, cstr sSk, cstr sEndpoint, cstr sMethod,
	cstr sPathAndQuery, cstr sBody, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sStamp, size_t iStampCapacity);

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_http.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_HTTP)

#if defined(XACME_FEATURE_ACME_HTTP)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_HTTP_FIELD_MAX 100u
#define XACME_HTTP_IO_CHUNK 16384u

/* 这里只共享异常回滚的资源拥有者，不共享请求或账户状态。 */
static xatomic32 __xacmePendingLock = { 0u };
static xacmehttp* __xacmePendingHead;
static xacmehttp* __xacmePendingTail;
static size_t __xacmePendingCount;

static void xacmePendingLock(void)
{
	uint32 Expected = 0u;
	while(!xrtAtomic32CompareExchange(&__xacmePendingLock, &Expected, 1u,
		XMEMORY_ACQUIRE, XMEMORY_RELAXED))
	{
		Expected = 0u;
		xrtThreadYield();
	}
}

static void xacmePendingUnlock(void)
{
	xrtAtomic32Store(&__xacmePendingLock, 0u, XMEMORY_RELEASE);
}

/*
	传输级重试：IO/超时类失败最多 3 次尝试（500ms/1s 退避）。
	GET/HEAD 只在未收到响应时重试；其他方法仅在发送前失败时重试。
	HTTP 层语义（状态码、badNonce）由上层处理，不受影响。
	收到响应前的连接故障仍可能发生于服务端处理 POST 之后；
	写入端点的重放安全性须由调用方及 provider 的幂等/去重语义保证。
	全部尝试失败时保留首个根因。
*/
#define XACME_HTTP_RETRY_MAX 3
#define XACME_HTTP_MAX_RESPONSE_BODY (4u * 1024u * 1024u)

static bool xacmeHttpErrorRetryable(void)
{
	xerrkind Kind = xrtErrorKind(xrtGetError());
	return (Kind == XERR_IO) || (Kind == XERR_TIMEOUT);
}

static bool xacmeHttpMethodReadOnly(cstr sMethod)
{
	return (sMethod != NULL) &&
		((strcmp(sMethod, "GET") == 0) || (strcmp(sMethod, "HEAD") == 0));
}

static void xacmeHttpMarkWriteUncertain(xacmehttp* pHttp)
{
	const xerror* pCause = xrtGetError();
	pHttp->bWriteUncertain = true;
	xerror* pUncertain = xrtErrorWrap(
		pCause, xrtErrorKind(pCause), "xrt.acme.http",
		(int32)XACME_HTTP_ERROR_UNCERTAIN,
		"http write outcome unknown after request started");
	if(pUncertain != NULL)
	{
		xrtSetErrorTake(pUncertain);
	}
}

static bool xacmeHttpExchangeOnceImpl(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse,
	bool* pbRequestStarted, bool* pbResponseStarted);

static bool xacmeHttpExchangeRetry(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	uint32 uAttempt;
	xerror* pFirst = NULL;
	bool bReadOnlyMethod = xacmeHttpMethodReadOnly(sMethod);
	for(uAttempt = 1u; uAttempt <= XACME_HTTP_RETRY_MAX; uAttempt++)
	{
		bool bRequestStarted = false;
		bool bResponseStarted = false;
		bool bOk = xacmeHttpExchangeOnceImpl(
			pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
			iExtraCount, pResponse, &bRequestStarted, &bResponseStarted);
		if(bOk)
		{
			xrtErrorFree(pFirst);
			return true;
		}
		if(bResponseStarted || (bRequestStarted && !bReadOnlyMethod) ||
			!xacmeHttpErrorRetryable())
		{
			if(bRequestStarted && !bReadOnlyMethod)
			{
				xacmeHttpMarkWriteUncertain(pHttp);
			}
			xrtErrorFree(pFirst);
			return false;
		}
		if(pFirst == NULL)
		{
			pFirst = xrtErrorRef(xrtGetError());
		}
		if(uAttempt == XACME_HTTP_RETRY_MAX)
		{
			if(pFirst != NULL)
			{
				xrtSetErrorTake(pFirst);
			}
			return false;
		}
		if(getenv("XACME_DEBUG"))
		{
			printf("[http-retry] attempt=%u url=%.80s\n",
				(unsigned)uAttempt, sUrl);
		}
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static void xacmeHttpError(
	xerrkind Kind, xacmehttperror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.http", (int32)Code, sMessage);
}

void xacmeHttpDeferOwner(xacmehttp* pHttp, size_t iOwnerSize)
{
	/* 全部调用者拥有首字段为 Http 的堆对象，已拆除 HTTP 外的子资源。 */
	xrtSecureZero((uint8*)pHttp + sizeof(*pHttp), iOwnerSize - sizeof(*pHttp));
	pHttp->iPendingOwnerSize = iOwnerSize;
	xacmePendingLock();
	pHttp->pPendingNext = __xacmePendingHead;
	__xacmePendingHead = pHttp;
	if(__xacmePendingTail == NULL) __xacmePendingTail = pHttp;
	__xacmePendingCount++;
	xacmePendingUnlock();
}

bool xrtAcmeCleanupPending(int64 uTimeoutMs, size_t* piPending)
{
	xerror* pPrevious = xrtErrorRef(xrtGetError());
	xerror* pFirst = NULL;
	double Deadline = __xrtWaitAfter(uTimeoutMs);
	size_t iPending;
	bool bError = false;
	for(;;)
	{
		xacmehttp *pList, *pListTail, *pWait = NULL, *pWaitTail = NULL;
		xacmePendingLock();
		pList = __xacmePendingHead;
		pListTail = __xacmePendingTail;
		__xacmePendingHead = NULL;
		__xacmePendingTail = NULL;
		xacmePendingUnlock();
		/* 局部链表取得独占拥有权；计数仍包括这些正在处理的外壳。 */
		while(pList != NULL)
		{
			xacmehttp* pNext = pList->pPendingNext;
			xnetretireresult Result = xrtNetEngineTryDestroy(pList->pEngine);
			if(Result == XNET_RETIRE_READY)
			{
				size_t iSize = pList->iPendingOwnerSize;
				xrtSecureZero(pList, iSize);
				xrtFree(pList);
				xacmePendingLock();
				__xacmePendingCount--;
				xacmePendingUnlock();
			}
			else
			{
				if(Result == XNET_RETIRE_ERROR)
				{
					bError = true;
					if(pFirst == NULL) pFirst = xrtErrorRef(xrtGetError());
				}
				pList->pPendingNext = pWait;
				pWait = pList;
				if(pWaitTail == NULL) pWaitTail = pList;
			}
			pList = pNext;
			if(pList != NULL && (bError ||
				(uTimeoutMs != 0u && __xrtWaitExpired(Deadline))))
			{
				/* 预算耗尽或首个 ERROR 后，未处理的尾段也仍是本次的拥有者。 */
				if(pWaitTail != NULL) pWaitTail->pPendingNext = pList;
				else pWait = pList;
				pWaitTail = pListTail;
				pList = NULL;
			}
		}
		xacmePendingLock();
		if(pWait != NULL)
		{
			pWaitTail->pPendingNext = __xacmePendingHead;
			if(__xacmePendingTail == NULL) __xacmePendingTail = pWaitTail;
			__xacmePendingHead = pWait;
		}
		iPending = __xacmePendingCount;
		xacmePendingUnlock();
		if(iPending == 0u || bError || uTimeoutMs == 0u) break;
		if(__xrtWaitExpired(Deadline))
		{
			xacmeHttpError(XERR_TIMEOUT, XACME_HTTP_ERROR_TIMEOUT,
				"acme pending cleanup still has live objects");
			break;
		}
		xrtSleep(1u);
	}
	if(piPending != NULL) *piPending = iPending;
	if(pPrevious != NULL)
	{
		xrtErrorFree(pFirst);
		xrtSetErrorTake(pPrevious);
	}
	else if(pFirst != NULL) xrtSetErrorTake(pFirst);
	return iPending == 0u;
}

/* ------------------------------------------------------------------ */
/* 初始化                                                              */
/* ------------------------------------------------------------------ */

bool xacmeHttpInit(
	xacmehttp* pHttp, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, int64 uTimeoutMs)
{
	xtlsverifierconfig Verify;
	xx509store* pStore = NULL;

	if(pHttp == NULL)
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http init requires http");
		return false;
	}
	memset(pHttp, 0, sizeof(*pHttp));
	pHttp->uTimeoutMs = (uTimeoutMs != 0u) ?
		uTimeoutMs : INT64_C(30000);

	pHttp->pEngine = pBorrowedEngine;
	if(pBorrowedEngine == NULL)
	{
		size_t iPending = 0u;
		/* 异常尚未退休时暂停新增私有引擎，避免反复失败无限增加线程和内存。 */
		if(!xrtAcmeCleanupPending(0, &iPending))
		{
			xacmeHttpError(XERR_STATE, XACME_HTTP_ERROR_CONNECT,
				"acme pending cleanup must finish before creating a private engine");
			return false;
		}
	}

	pHttp->pResolver = xrtNetResolverCreate(NULL);
	if(pHttp->pResolver == NULL)
	{
		goto Failure;
	}

	xrtTlsVerifierConfigInit(&Verify);
	if((sCaPem != NULL) && (sCaPem[0] != '\0'))
	{
		size_t iAdded = 0u;
		pStore = xrtX509StoreCreate();
		if((pStore == NULL) ||
			!xrtX509StoreAddPem(
				pStore, sCaPem, strlen(sCaPem), &iAdded) || (iAdded == 0u))
		{
			xrtX509StoreFree(pStore);
			goto Failure;
		}
		Verify.Store = pStore;
	}
	else
	{
		pStore = xrtX509StoreSystem();
		if(pStore == NULL)
		{
			goto Failure;
		}
		Verify.Store = pStore;
	}
	pHttp->pVerifier = xrtTlsVerifierCreate(&Verify);
	/* 信任库已深复制进验证器。 */
	if(pStore != NULL)
	{
		xrtX509StoreFree(pStore);
	}
	if(pHttp->pVerifier == NULL)
	{
		goto Failure;
	}
	/* 配置分配均完成后才启动私有引擎；初始化失败不依赖请求的极短预算。 */
	if(pHttp->pEngine == NULL)
	{
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if(pHttp->pEngine == NULL) goto Failure;
		pHttp->bEngineOwned = true;
		if(!xrtNetEngineStart(pHttp->pEngine))
		{
			xerror* pStartError = xrtErrorRef(xrtGetError());
			/* 尚未发布，也没有连接对象；同步归还启动失败的引擎。 */
			if(xrtNetEngineDestroy(pHttp->pEngine))
			{
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
			}
			if(pStartError != NULL) xrtSetErrorTake(pStartError);
			goto Failure;
		}
	}
	return true;

Failure:
	{
		xerror* pCause = xrtErrorRef(xrtGetError());
		if(pCause != NULL)
		{
			xerror* pWrapped = xrtErrorWrap(pCause, XERR_STATE, "xrt.acme.http",
				XACME_HTTP_ERROR_CONNECT, "acme http init failed");
			if(pWrapped != NULL) xrtSetErrorTake(pWrapped);
			else xrtSetError(pCause); /* OOM 时仍保留底层首因。 */
			xrtErrorFree(pCause);
		}
		else xacmeHttpError(XERR_STATE, XACME_HTTP_ERROR_CONNECT, "acme http init failed");
	}
	xacmeHttpUnit(pHttp);
	return false;
}

bool xacmeHttpUnit(xacmehttp* pHttp)
{
	bool bReady = true;
	xerror* pPrevious;
	if(pHttp == NULL)
	{
		return true;
	}
	pPrevious = xrtErrorRef(xrtGetError());
	if(pHttp->pResolver != NULL)
	{
		if(xrtNetResolverDestroy(pHttp->pResolver))
			pHttp->pResolver = NULL;
		else
		{
			/* Resolver retirement can fail too. Retain its creator slot and
			 * dependent engine so the caller can retry the same owner. */
			if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
			return false;
		}
	}
	if(pHttp->bEngineOwned && (pHttp->pEngine != NULL))
	{
		double Deadline = __xrtWaitAfter(pHttp->uTimeoutMs);
		for(;;)
		{
			xnetretireresult Result = xrtNetEngineTryDestroy(pHttp->pEngine);
			if(Result == XNET_RETIRE_READY)
			{
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
				break;
			}
			if(Result == XNET_RETIRE_ERROR) { bReady = false; break; }
			if(__xrtWaitExpired(Deadline))
			{
				xacmeHttpError(XERR_TIMEOUT, XACME_HTTP_ERROR_TIMEOUT,
					"acme http engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			/* Close/Abort 是异步命令；等待内部引用退休，不丢弃创建者拥有权。 */
			xrtSleep(1u);
		}
	}
	else
	{
		pHttp->pEngine = NULL;
		pHttp->bEngineOwned = false;
	}
	if(pHttp->pVerifier != NULL)
	{
		xrtTlsVerifierRelease(pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
	if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	return bReady;
}

void xacmeHttpResponseUnit(xacmehttpresponse* pResponse)
{
	if(pResponse == NULL)
	{
		return;
	}
	xrtFree(pResponse->sLocation);
	xrtFree(pResponse->sReplayNonce);
	xrtFree(pResponse->sRetryAfter);
	xrtFree(pResponse->sLink);
	xrtFree(pResponse->sContentType);
	xrtFree(pResponse->sBody);
	memset(pResponse, 0, sizeof(*pResponse));
}

/* ------------------------------------------------------------------ */
/* URL 解析（https://host[:port]/path，ACME 全集）                      */
/* ------------------------------------------------------------------ */

typedef struct xacmeurl {
	char sHost[256];
	uint16 iPort;
	char sPath[1024];
	bool bTls;
	bool bIpLiteral;
} xacmeurl;

static bool xacmeUrlScheme(cstr sUrl, cstr sPrefix)
{
	while(*sPrefix != 0)
	{
		unsigned char c = (unsigned char)*sUrl++;
		if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
		if(c != (unsigned char)*sPrefix++) return false;
	}
	return true;
}

static bool xacmeUrlPort(cstr sPort, size_t iSize, uint16* pPort)
{
	unsigned iPort = 0u;
	size_t i;
	if(iSize == 0u) return false;
	for(i = 0u; i < iSize; i++)
	{
		unsigned iDigit;
		if(sPort[i] < '0' || sPort[i] > '9') return false;
		iDigit = (unsigned)(sPort[i] - '0');
		if(iPort > (65535u - iDigit) / 10u) return false;
		iPort = iPort * 10u + iDigit;
	}
	if(iPort == 0u) return false;
	*pPort = (uint16)iPort;
	return true;
}

static bool xacmeUrlHex(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
		(c >= 'a' && c <= 'f');
}

static bool xacmeUrlTargetChar(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("-._~!$&'()*+,;=:@/?", (int)c) != NULL);
}

/* Path/query/fragment use the same ASCII grammar here; validate before discarding a fragment. */
static bool xacmeUrlComponent(cstr sText, size_t iSize)
{
	size_t i;
	for(i = 0u; i < iSize; i++)
	{
		unsigned char c = (unsigned char)sText[i];
		if(c == '%')
		{
			if(iSize - i < 3u || !xacmeUrlHex((unsigned char)sText[i + 1u]) ||
				!xacmeUrlHex((unsigned char)sText[i + 2u])) return false;
			i += 2u;
		}
		else if(!xacmeUrlTargetChar(c)) return false;
	}
	return true;
}

/* Legacy IPv4 components may mix decimal/octal and 0x hexadecimal forms. */
static bool xacmeUrlNumericHost(cstr sHost, size_t iSize)
{
	size_t i = 0u;
	while(i < iSize)
	{
		size_t iStart = i;
		bool bHex = iSize - i >= 2u && sHost[i] == '0' &&
			(sHost[i + 1u] == 'x' || sHost[i + 1u] == 'X');
		if(bHex) i += 2u;
		iStart = i;
		while(i < iSize && sHost[i] != '.')
		{
			unsigned char c = (unsigned char)sHost[i];
			if(bHex ? !xacmeUrlHex(c) : (c < '0' || c > '9')) return false;
			i++;
		}
		if(i == iStart) return false;
		if(i < iSize) i++; /* A root dot does not turn an address into a DNS identity. */
	}
	return true;
}

static bool xacmeUrlParse(cstr sUrl, xacmeurl* pOut)
{
	const char *sHost, *sTail, *sPort;
	size_t iAuthority, iHost, iTarget, iPrefix, i;
	bool bNumeric = true;

	memset(pOut, 0, sizeof(*pOut));
	if(sUrl == NULL) return false;
	if(xacmeUrlScheme(sUrl, "https://"))
	{
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if(xacmeUrlScheme(sUrl, "http://"))
	{
		sHost = sUrl + 7;
	}
	else return false;
	pOut->iPort = pOut->bTls ? 443u : 80u;
	/* Authority 在 /、? 或 # 前结束；完整检查 userinfo，不能被端口截断掩盖。 */
	iAuthority = strcspn(sHost, "/?#");
	if(iAuthority == 0u || memchr(sHost, '@', iAuthority) != NULL) return false;
	sTail = sHost + iAuthority;
	iPrefix = *sTail == '/' ? 0u : 1u;
	iTarget = strcspn(sTail, "#");
	if(iTarget > sizeof(pOut->sPath) - 1u - iPrefix) return false;
	if(!xacmeUrlComponent(sTail, iTarget)) return false;
	if(sTail[iTarget] == '#' &&
		!xacmeUrlComponent(sTail + iTarget + 1u, strlen(sTail + iTarget + 1u))) return false;
	if(iPrefix != 0u) pOut->sPath[0] = '/';
	memcpy(pOut->sPath + iPrefix, sTail, iTarget);
	pOut->sPath[iPrefix + iTarget] = 0;
	if(sHost[0] == '[')
	{
		const char* sClose = (const char*)memchr(sHost, ']', iAuthority);
		xnetaddr Address;
		bool bValid;
		if(sClose == NULL) return false;
		iHost = (size_t)(sClose - sHost) + 1u;
		if(iHost <= 2u || iHost >= sizeof(pOut->sHost)) return false;
		for(i = 1u; i + 1u < iHost; i++)
		{
			unsigned char c = (unsigned char)sHost[i];
			if(!xacmeUrlHex(c) && c != ':' && c != '.') return false;
		}
		memcpy(pOut->sHost, sHost, iHost);
		pOut->sHost[iHost] = 0;
		if(iHost < iAuthority)
		{
			if(sHost[iHost] != ':' || !xacmeUrlPort(sHost + iHost + 1u,
				iAuthority - iHost - 1u, &pOut->iPort)) return false;
		}
		/* Host 保留方括号；数值解析与证书校验不带括号，不支持 scope/IPvFuture。 */
		pOut->sHost[iHost - 1u] = 0;
		bValid = xrtNetAddrParse(&Address, pOut->sHost + 1u, pOut->iPort) &&
			Address.Family == XNET_FAMILY_IPV6;
		pOut->sHost[iHost - 1u] = ']';
		if(!bValid) return false;
		pOut->bIpLiteral = true;
		return true;
	}
	sPort = (const char*)memchr(sHost, ':', iAuthority);
	iHost = iAuthority;
	if(sPort != NULL)
	{
		if(!xacmeUrlPort(sPort + 1u, iAuthority - (size_t)(sPort - sHost) - 1u,
			&pOut->iPort)) return false;
		iHost = (size_t)(sPort - sHost);
	}
	if(iHost == 0u || iHost >= sizeof(pOut->sHost) || sHost[0] == '.' ||
		(iHost > 1u && sHost[iHost - 1u] == '.' && sHost[iHost - 2u] == '.')) return false;
	for(i = 0u; i < iHost; i++)
	{
		unsigned char c = (unsigned char)sHost[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')) return false;
		bNumeric = bNumeric && ((c >= '0' && c <= '9') || c == '.');
	}
	memcpy(pOut->sHost, sHost, iHost);
	if(bNumeric)
	{
		xnetaddr Address;
		/* 拒绝缩写、前导零和纯整数，避免宿主解析器把 DNS 名转成旧式 IP。 */
		if(!xrtNetAddrParse(&Address, pOut->sHost, pOut->iPort) ||
			Address.Family != XNET_FAMILY_IPV4) return false;
		pOut->bIpLiteral = true;
	}
	else if(xacmeUrlNumericHost(sHost, iHost)) return false;
	return true;
}

static void xacmeUrlTlsNames(const xacmeurl* pUrl,
	xtlsclientconfig* pTls, xtlsdialconfig* pDial)
{
	const char* sName = pUrl->sHost;
	size_t iSize = strlen(sName);
	if(pUrl->bIpLiteral && sName[0] == '[') { sName++; iSize -= 2u; }
	else if(!pUrl->bIpLiteral && iSize > 0u && sName[iSize - 1u] == '.') iSize--;
	pTls->VerifyName = (xstrview){ sName, iSize };
	pTls->ServerName = pUrl->bIpLiteral ? (xstrview){ NULL, 0u } : pTls->VerifyName;
	/* 空 SNI 不能由 Dial 从 IP 主机自动补回。 */
	pDial->ServerNameFromHost = false;
}

/* ------------------------------------------------------------------ */
/* 流封装：任意线程安全的 future 化 IO                                   */
/* ------------------------------------------------------------------ */

typedef struct xacmestream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xacmestream;

static bool xacmeFutureResolved(xfuture* pFuture, uint64 uUs)
{
	return uUs != 0u &&
		xrtFutureWaitFor(pFuture, uUs) == XWAIT_OK &&
		xrtFutureState(pFuture) == XFUTURE_RESOLVED;
}

static bool xacmeFutureWait(xfuture* pFuture, uint64 uUs)
{
	bool bResolved = xacmeFutureResolved(pFuture, uUs);
	xrtFutureDestroy(pFuture);
	return bResolved;
}

/* 发送全部字节；TLS 用 SendAsync（任意线程），明文用复制语义 Send。 */
static bool xacmeStreamSendAll(
	xacmestream* pStream, const void* pData, size_t iSize, uint64 uUs)
{
	size_t iOffset = 0u;
	double Deadline = __xrtWaitAfter(uUs);
	while(iOffset < iSize)
	{
		size_t iChunk = iSize - iOffset;
		if(__xrtWaitExpired(Deadline))
		{
			return false;
		}
		if(iChunk > XACME_HTTP_IO_CHUNK)
		{
			iChunk = XACME_HTTP_IO_CHUNK;
		}
		if(pStream->pTls != NULL)
		{
			xfuture* pFuture = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if((pFuture == NULL) ||
				!xacmeFutureWait(pFuture, __xrtWaitRemaining(Deadline)))
			{
				return false;
			}
		}
		else
		{
			xnetresult eResult;
			xfuture* pFuture;
			while((eResult = xrtNetStreamSend(
				pStream->pTcp, (const uint8*)pData + iOffset, iChunk))
				== XNET_RESULT_AGAIN)
			{
				if(__xrtWaitExpired(Deadline))
				{
					return false;
				}
				pFuture = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if((pFuture == NULL) ||
					!xacmeFutureWait(pFuture, __xrtWaitRemaining(Deadline)))
				{
					return false;
				}
			}
			if(eResult != XNET_RESULT_OK)
			{
				return false;
			}
		}
		iOffset += iChunk;
	}
	return true;
}

/* 返回 1=读到数据，0=流结束，-1=超时，-2=I/O 失败。 */
static int xacmeStreamRecv(
	xacmestream* pStream, uint8* pBuffer, size_t iCapacity,
	size_t* pRead, double Deadline)
{
	xfuture* pFuture;
	xnetbytes* pBytes;
	xwaitresult eWait;
	xbytesview View;
	int64 uRemaining = __xrtWaitRemaining(Deadline);
	if(uRemaining == 0u) return -1;
	if(pStream->pTls != NULL)
		pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
	else
		pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
	if(pFuture == NULL) return -2;
	eWait = xrtFutureWaitFor(pFuture, uRemaining);
	if(eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_CLOSED)
	{
		xrtFutureDestroy(pFuture);
		/* CLOSED 也可来自终止路径，必须确认正常读端结束。 */
		if(pStream->pTls != NULL)
		{
			bool bEnd;
			pFuture = xrtTlsStreamWaitAsync(pStream->pTls, XTLS_STREAM_WAIT_END);
			if(pFuture == NULL) return -2;
			eWait = xrtFutureWaitFor(pFuture, __xrtWaitRemaining(Deadline));
			bEnd = eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_RESOLVED;
			xrtFutureDestroy(pFuture);
			return bEnd ? 0 : (eWait == XWAIT_TIMEOUT ? -1 : -2);
		}
		else
		{
			xnetstreamstats Stats;
			return xrtNetStreamStats(pStream->pTcp, &Stats) &&
				Stats.ReadEnded && xrtNetStreamError(pStream->pTcp) == NULL ? 0 : -2;
		}
	}
	if(eWait != XWAIT_OK || xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	{
		xrtFutureDestroy(pFuture);
		return eWait == XWAIT_TIMEOUT ? -1 : -2;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	if(pBytes == NULL)
	{
		xrtFutureDestroy(pFuture);
		return -2;
	}
	View = xrtNetBytesView(pBytes);
	if(View.Size == 0u || View.Size > iCapacity)
	{
		xrtFutureDestroy(pFuture);
		return -2;
	}
	memcpy(pBuffer, View.Data, View.Size);
	*pRead = View.Size;
	xrtFutureDestroy(pFuture);
	return 1;
}

static void xacmeStreamClose(xacmestream* pStream, bool bAbort)
{
	if(pStream->pTls != NULL)
	{
		if(bAbort) (void)xrtTlsStreamAbort(pStream->pTls);
		else (void)xrtTlsStreamClose(pStream->pTls);
		xrtTlsStreamDestroy(pStream->pTls);
		pStream->pTls = NULL;
	}
	if(pStream->pTcp != NULL)
	{
		if(bAbort) (void)xrtNetStreamAbort(pStream->pTcp);
		else (void)xrtNetStreamClose(pStream->pTcp);
		xrtNetStreamDestroy(pStream->pTcp);
		pStream->pTcp = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 交换                                                               */
/* ------------------------------------------------------------------ */

static bool xacmeHeaderTake(const xhttp1head* pHead, cstr sName, str* psValue)
{
	{
		const xhttpfield* pField = xrtHttpFieldGet(
			pHead->Fields, pHead->FieldCount,
			(xstrview){ sName, strlen(sName) });
		str sValue;
		*psValue = NULL;
		if((pField == NULL) || (pField->Value.Data == NULL))
		{
			return true;
		}
		sValue = (str)xrtMalloc(pField->Value.Size + 1u);
		if(sValue == NULL)
		{
			return false;
		}
		memcpy(sValue, pField->Value.Data, pField->Value.Size);
		sValue[pField->Value.Size] = '\0';
		*psValue = sValue;
		return true;
	}
}

/* Link is a list field: preserve every field line in wire order (RFC 9110
 * section 5.3). Other copied fields retain their individual semantics. */
static bool xacmeHeaderTakeLinks(const xhttp1head* pHead, str* psValue)
{
	size_t i, iSize = 1u, iOffset = 0u, iCount = 0u;
	str sValue;
	*psValue = NULL;
	for(i = 0u; i < pHead->FieldCount; i++)
	{
		const xhttpfield* pField = &pHead->Fields[i];
		if(!xrtHttpFieldNameEqual(pField->Name, XRT_STR_LITERAL("Link")) ||
			(pField->Value.Data == NULL)) continue;
		if(iCount != 0u)
		{
			if(iSize > SIZE_MAX - 2u) goto TooLarge;
			iSize += 2u;
		}
		if(pField->Value.Size > SIZE_MAX - iSize) goto TooLarge;
		iSize += pField->Value.Size;
		iCount++;
	}
	if(iCount == 0u) return true;
	sValue = (str)xrtMalloc(iSize);
	if(sValue == NULL) return false;
	for(i = 0u; i < pHead->FieldCount; i++)
	{
		const xhttpfield* pField = &pHead->Fields[i];
		if(!xrtHttpFieldNameEqual(pField->Name, XRT_STR_LITERAL("Link")) ||
			(pField->Value.Data == NULL)) continue;
		memcpy(sValue + iOffset, pField->Value.Data, pField->Value.Size);
		iOffset += pField->Value.Size;
		if(--iCount != 0u)
		{
			memcpy(sValue + iOffset, ", ", 2u);
			iOffset += 2u;
		}
	}
	sValue[iOffset] = '\0';
	*psValue = sValue;
	return true;

TooLarge:
	xacmeHttpError(XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
		"acme http Link response header too large");
	return false;
}

static bool xacmeHttpHeaderOwned(cstr sName)
{
	static const xhttpfield Owned[] = {
		{ { "Host", 4u }, { NULL, 0u } },
		{ { "User-Agent", 10u }, { NULL, 0u } },
		{ { "Accept", 6u }, { NULL, 0u } },
		{ { "Connection", 10u }, { NULL, 0u } },
		{ { "Content-Type", 12u }, { NULL, 0u } },
		{ { "Content-Length", 14u }, { NULL, 0u } },
		{ { "Transfer-Encoding", 17u }, { NULL, 0u } }
	};
	return xrtHttpFieldGet(Owned, sizeof(Owned) / sizeof(Owned[0]),
		(xstrview){ sName, strlen(sName) }) != NULL;
}

bool xacmeHttpExchange(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, xacmehttpresponse* pResponse)
{
	return xacmeHttpExchangeV(
		pHttp, sMethod, sUrl, sContentType, sBody, NULL, 0u, pResponse);
}

bool xacmeHttpExchangeV(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	if(pHttp != NULL) pHttp->bWriteUncertain = false;
	return xacmeHttpExchangeRetry(
		pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
		iExtraCount, pResponse);
}

bool xacmeHttpExchangeOnceV(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	bool bRequestStarted = false;
	if(pHttp != NULL) pHttp->bWriteUncertain = false;
	bool bOk = xacmeHttpExchangeOnceImpl(
		pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
		iExtraCount, pResponse, &bRequestStarted, NULL);
	if(!bOk && bRequestStarted && !xacmeHttpMethodReadOnly(sMethod))
	{
		xacmeHttpMarkWriteUncertain(pHttp);
	}
	return bOk;
}

static bool xacmeHttpExchangeOnceImpl(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse,
	bool* pbRequestStarted, bool* pbResponseStarted)
{
	xacmeurl Url;
	xacmestream Stream = { NULL, NULL };
	xbuffer Request;
	xbuffer Received;
	xbuffer BodyBuffer;
	xhttpfield Fields[XACME_HTTP_FIELD_MAX];
	xhttp1head Head;
	xhttp1limits Limits;
	xhttp1errorinfo ProtocolError;
	xhttp1bodyplan Plan;
	xhttp1body Body;
	xhttp1bodylimits BodyLimits;
	xhttp1bodystatus eBody;
	char sHostHeader[280];
	char sLength[24];
	char sPortText[8];
	uint8 Chunk[XACME_HTTP_IO_CHUNK];
	size_t iRequestSize = 0u;
	size_t iField = 0u;
	size_t iUsed = 0u;
	size_t iConsumed = 0u;
	xfuture* pFuture = NULL;
	double ResponseDeadline;
	bool bOk = false;
	bool bHeadDone = false;
	bool bStreamEnd = false;
	bool bContentType = (sContentType != NULL && sContentType[0] != '\0');
	bool bContentLength;
	size_t iReserved;
	if(pbRequestStarted != NULL)
	{
		*pbRequestStarted = false;
	}
	if(pbResponseStarted != NULL)
	{
		*pbResponseStarted = false;
	}

	if((pHttp == NULL) || (sMethod == NULL) || (sUrl == NULL) ||
		(pResponse == NULL))
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http exchange requires http, method, url and response");
		return false;
	}
	memset(pResponse, 0, sizeof(*pResponse));
	bContentLength = (sBody.Data != NULL || strcmp(sMethod, "POST") == 0);
	if(pHttp->pEngine == NULL || pHttp->pResolver == NULL || pHttp->pVerifier == NULL)
	{
		xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http transport is not initialized or has been cleaned up");
		return false;
	}
	iReserved = 4u + (size_t)bContentType + (size_t)bContentLength;
	if((sBody.Data == NULL && sBody.Size != 0u) ||
		(iExtraCount != 0u && pExtraHeaders == NULL) ||
		iExtraCount > XACME_HTTP_FIELD_MAX - iReserved)
	{
		xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http body or extra header count invalid");
		return false;
	}
	{
		size_t i;
		for(i = 0u; i < iExtraCount; i++)
		{
			if(pExtraHeaders[i].sName == NULL || pExtraHeaders[i].sValue == NULL ||
				xacmeHttpHeaderOwned(pExtraHeaders[i].sName))
			{
				xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
					"acme http extra header invalid or managed by transport");
				return false;
			}
		}
	}
	if(!xacmeUrlParse(sUrl, &Url))
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_URL,
			"acme http exchange url is not http(s) absolute");
		return false;
	}
	if(((Url.bTls ? 443u : 80u) != Url.iPort))
	{
		snprintf(sPortText, sizeof(sPortText), ":%u", (unsigned)Url.iPort);
	}
	else
	{
		sPortText[0] = '\0';
	}
	snprintf(
		sHostHeader, sizeof(sHostHeader), "%s%s", Url.sHost, sPortText);

	/* ---- 请求组装 ---- */
	xrtBufferInit(&Request);
	xrtBufferInit(&Received);
	xrtBufferInit(&BodyBuffer);
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Host"),
		(xstrview){ sHostHeader, strlen(sHostHeader) } };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("User-Agent"), XRT_STR_LITERAL("xacme") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("*/*") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close") };
	{
		size_t i;
		for(i = 0; i < iExtraCount; i++)
		{
			Fields[iField++] = (xhttpfield){
				(xstrview){
					pExtraHeaders[i].sName,
					strlen(pExtraHeaders[i].sName) },
				(xstrview){
					pExtraHeaders[i].sValue,
					strlen(pExtraHeaders[i].sValue) } };
		}
	}
	if(bContentType)
	{
		Fields[iField++] = (xhttpfield){
			(xstrview){ "Content-Type", 12u },
			(xstrview){ sContentType, strlen(sContentType) } };
	}
	if(bContentLength)
	{
		snprintf(sLength, sizeof(sLength), "%llu",
			(unsigned long long)((sBody.Data == NULL) ? 0u : sBody.Size));
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){ sLength, strlen(sLength) } };
	}
	if(!xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField, NULL, 0u, &iRequestSize))
	{
		goto Done;
	}
	if(!xrtBufferResize(&Request, iRequestSize) ||
		!xrtHttp1RequestWrite(
			(xstrview){ sMethod, strlen(sMethod) },
			(xstrview){ Url.sPath, strlen(Url.sPath) },
			XHTTP_VERSION_1_1, Fields, iField,
			Request.Data, iRequestSize, &iRequestSize))
	{
		goto Done;
	}
	if((sBody.Data != NULL) && (sBody.Size > 0u) &&
		!xrtBufferAppend(
			&Request, (xbytesview){ (const uint8*)sBody.Data, sBody.Size }))
	{
		goto Done;
	}

	/* ---- 连接 ---- */
	if(Url.bTls)
	{
		xtlsclientconfig Tls;
		xtlsdialconfig Dial;
		xrtTlsClientConfigInit(&Tls);
		Tls.Verifier = pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
		xacmeUrlTlsNames(&Url, &Tls, &Dial);
		Dial.Timeout = pHttp->uTimeoutMs;
		pFuture = xrtTlsDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Tls, &Dial, NULL, NULL);
	}
	else
	{
		xnetdialconfig Dial;
		xrtNetDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutMs;
		pFuture = xrtNetDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Dial, NULL, NULL);
	}
	if(pFuture == NULL)
	{
		/* 底层拨号错误已在线程错误里；仅补充域信息。 */
		goto Done;
	}
	if(xrtFutureWaitFor(pFuture, pHttp->uTimeoutMs) != XWAIT_OK ||
		xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	{
		const xerror* pFutureError = xrtFutureError(pFuture);
		if(pFutureError != NULL)
		{
			/* 包装底层根因，保留完整因链。 */
			xerror* pWrap = xrtErrorWrap(
				pFutureError, XERR_IO, "xrt.acme.http",
				(int32)XACME_HTTP_ERROR_CONNECT,
				"acme http connect failed");
			if(pWrap != NULL)
			{
				xrtSetErrorTake(pWrap);
				goto Done;
			}
		}
		{
			xacmeHttpError(
				XERR_TIMEOUT, XACME_HTTP_ERROR_CONNECT,
				"acme http connect timeout");
		}
		goto Done;
	}
	if(Url.bTls)
	{
		Stream.pTls = xrtTlsStreamRef((xtlsstream*)xrtFutureValue(pFuture));
		if(Stream.pTls == NULL)
		{
			goto ConnectFail;
		}
	}
	else
	{
		Stream.pTcp = xrtNetStreamRef((xnetstream*)xrtFutureValue(pFuture));
		if(Stream.pTcp == NULL)
		{
			goto ConnectFail;
		}
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;

	/* ---- 发送 ---- */
	if(pbRequestStarted != NULL)
	{
		*pbRequestStarted = true;
	}
	if(!xacmeStreamSendAll(
		&Stream, Request.Data, Request.Size, pHttp->uTimeoutMs))
	{
		xacmeHttpError(
			XERR_IO, XACME_HTTP_ERROR_SEND,
			"acme http send failed");
		goto Done;
	}
	ResponseDeadline = __xrtWaitAfter(pHttp->uTimeoutMs);

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFields = XACME_HTTP_FIELD_MAX;
	memset(&Head, 0, sizeof(Head));
	Head.Fields = Fields;
	Head.FieldCapacity = XACME_HTTP_FIELD_MAX;
	while(!bHeadDone)
	{
		xhttp1status eStatus = xrtHttp1ResponseParse(
			xrtBufferView(&Received), &Head, &Limits, &ProtocolError);
		if(eStatus == XHTTP1_READY)
		{
			bHeadDone = true;
			break;
		}
		if(eStatus != XHTTP1_MORE)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response head invalid");
			goto Done;
		}
		{
			int iGot = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iUsed,
				ResponseDeadline);
			if(iGot < 0)
			{
				xacmeHttpError(
					(iGot == -1) ? XERR_TIMEOUT : XERR_IO,
					XACME_HTTP_ERROR_PROTOCOL,
					(iGot == -1) ? "acme http response head timeout" :
					"acme http response head read failed");
				goto Done;
			}
			if(iGot == 0)
			{
				/* 连接在收到任何响应字节前关闭 = 传输层故障
				   （可重试）；已收到部分头才算协议截断。 */
				xacmeHttpError(
					(Received.Size == 0u) ? XERR_IO : XERR_PROTOCOL,
					XACME_HTTP_ERROR_PROTOCOL,
					(Received.Size == 0u) ?
						"acme http connection closed before response" :
						"acme http response head truncated");
				goto Done;
			}
			if(pbResponseStarted != NULL)
			{
				*pbResponseStarted = true;
			}
			if(!xrtBufferAppend(&Received, (xbytesview){ Chunk, iUsed }))
			{
				goto Done;
			}
		}
	}

	/* ---- 响应字段 ---- */
	pResponse->iStatus = Head.Status;
	if(!xacmeHeaderTake(&Head, "Location", &pResponse->sLocation) ||
		!xacmeHeaderTake(&Head, "Replay-Nonce", &pResponse->sReplayNonce) ||
		!xacmeHeaderTake(&Head, "Retry-After", &pResponse->sRetryAfter) ||
		!xacmeHeaderTakeLinks(&Head, &pResponse->sLink) ||
		!xacmeHeaderTake(&Head, "Content-Type", &pResponse->sContentType))
	{
		goto Done;
	}

	/* ---- 接收体 ---- */
	if(!xrtHttp1ResponseBodyPlan(
		&Head, (xstrview){ sMethod, strlen(sMethod) }, &Plan))
	{
		xacmeHttpError(
			XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
			"acme http response body plan failed");
		goto Done;
	}
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = XACME_HTTP_MAX_RESPONSE_BODY;
	BodyLimits.MaxTrailers = XACME_HTTP_FIELD_MAX;
	/* Header 已复制，Body Plan 不再借用字段，可复用存储解析 trailer。 */
	if(!xrtHttp1BodyInit(&Body, &Plan, Fields, XACME_HTTP_FIELD_MAX, &BodyLimits))
	{
		xacmeHttpError(
			XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
			"acme http response body invalid");
		goto Done;
	}
	iUsed = Head.Bytes;
	for(;;)
	{
		xbytesview Data;
		eBody = xrtHttp1BodyRead(
			&Body,
			(xbytesview){
				(uint8*)Received.Data + iUsed, Received.Size - iUsed },
			bStreamEnd, &iConsumed, &Data, &ProtocolError);
		iUsed += iConsumed;
		if(eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response body invalid");
			goto Done;
		}
		if(eBody == XHTTP1_BODY_DATA)
		{
			/* DNS 与 ACME 消费零结尾文本，不允许按 NUL 截断有效前缀。 */
			if(Data.Size != 0u && memchr(Data.Data, 0, Data.Size) != NULL)
			{
				xacmeHttpError(
					XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
					"acme http response body invalid");
				goto Done;
			}
			if(!xrtBufferAppend(&BodyBuffer, Data))
			{
				goto Done;
			}
			continue;
		}
		if(eBody == XHTTP1_BODY_DONE)
		{
			break;
		}
		if(bStreamEnd)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response body truncated");
			goto Done;
		}
		{
			size_t iGot = 0u;
			/* Data 已复制；只保留未完成 trailer 等尚未消费的字节。 */
			if(iUsed != 0u)
			{
				if(!xrtBufferRemove(&Received, 0u, iUsed)) goto Done;
				iUsed = 0u;
			}
			int iResult = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iGot,
				ResponseDeadline);
			if(iResult < 0)
			{
				xacmeHttpError(
					(iResult == -1) ? XERR_TIMEOUT : XERR_IO,
					XACME_HTTP_ERROR_PROTOCOL,
					(iResult == -1) ? "acme http response body timeout" :
					"acme http response body read failed");
				goto Done;
			}
			if(iResult == 0)
			{
				bStreamEnd = true;
				continue;
			}
			if(!xrtBufferAppend(&Received, (xbytesview){ Chunk, iGot }))
			{
				goto Done;
			}
		}
	}
	/* 体数据已独立收拢在 BodyBuffer；拷出为零结尾文本。 */
	pResponse->sBody = (str)xrtMalloc(BodyBuffer.Size + 1u);
	if(pResponse->sBody == NULL)
	{
		goto Done;
	}
	if(BodyBuffer.Size > 0u)
	{
		memcpy(pResponse->sBody, BodyBuffer.Data, BodyBuffer.Size);
	}
	pResponse->sBody[BodyBuffer.Size] = '\0';
	pResponse->iBodySize = BodyBuffer.Size;
	if(getenv("XACME_DEBUG"))
	{
		printf("[ex-dbg] %s %s -> status=%u bodyLen=%zu ct=%s\n",
			sMethod, sUrl, (unsigned)pResponse->iStatus, pResponse->iBodySize,
			(pResponse->sContentType != NULL) ? pResponse->sContentType : "-");
	}
	bOk = true;
	goto Done;

ConnectFail:
	xacmeHttpError(
		XERR_IO, XACME_HTTP_ERROR_CONNECT,
		"acme http connect failed");
Done:
	if(pFuture != NULL)
	{
		xrtFutureDestroy(pFuture);
	}
	xacmeStreamClose(&Stream, !bOk);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	if(!bOk)
	{
		/* 响应头可能已分配；失败时不把部分响应留给调用方。 */
		xacmeHttpResponseUnit(pResponse);
	}
	return bOk;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_jose.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_JOSE)

#if defined(XACME_FEATURE_ACME_JOSE)


#include <string.h>

/* JOSE 全部编码统一 base64url 无填充。 */
static const xbase64config __xacmeB64Url = {
	NULL,
	XBASE64_URL | XBASE64_NO_PADDING
};

/*
	扁平 JWS 组装（RFC 7515 §7.2.2）：三段 base64url 文本。
	返回含末尾零的 xrtMalloc 文本；失败返回 NULL。
*/
static str xacmeJwsFlatAssemble(
	cstr sHeaderB64, cstr sPayloadB64, cstr sSignatureB64)
{
	xbuffer Flat;
	str sResult = NULL;
	bool bOk;
	xrtBufferInit(&Flat);
	bOk = xrtBufferAppend(
			&Flat, XRT_BYTES_LITERAL("{\"protected\":\"")) &&
		xrtBufferAppend(
			&Flat,
			(xbytesview){ (const uint8*)sHeaderB64, strlen(sHeaderB64) }) &&
		xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\",\"payload\":\"")) &&
		xrtBufferAppend(
			&Flat,
			(xbytesview){ (const uint8*)sPayloadB64, strlen(sPayloadB64) }) &&
		xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\",\"signature\":\"")) &&
		xrtBufferAppend(
			&Flat,
			(xbytesview){ (const uint8*)sSignatureB64, strlen(sSignatureB64) }) &&
		xrtBufferAppend(&Flat, XRT_BYTES_LITERAL("\"}")) &&
		xrtBufferAppendByte(&Flat, 0u);
	if(bOk && (Flat.Data != NULL))
	{
		sResult = (str)xrtMalloc(Flat.Size);
		if(sResult != NULL)
		{
			memcpy(sResult, Flat.Data, Flat.Size);
		}
	}
	xrtBufferUnit(&Flat);
	return sResult;
}

/* 把借用文本按 JSON 字符串 token（含两侧引号）转义追加；控制字符用 \u。 */
static bool xacmeJsonQuoteAppend(xbuffer* pBuffer, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pBuffer, '"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bEscaped = false;
		char sEscape[6];
		switch(c)
		{
		case '"':
			bEscaped = xrtBufferAppend(
				pBuffer, XRT_BYTES_LITERAL("\\\""));
			break;
		case '\\':
			bEscaped = xrtBufferAppend(
				pBuffer, XRT_BYTES_LITERAL("\\\\"));
			break;
		default:
			if((unsigned char)c < 0x20u)
			{
				sEscape[0] = '\\';
				sEscape[1] = 'u';
				sEscape[2] = '0';
				sEscape[3] = '0';
				sEscape[4] = "0123456789ABCDEF"
					[((unsigned char)c >> 4u) & 0x0Fu];
				sEscape[5] = "0123456789ABCDEF"
					[(unsigned char)c & 0x0Fu];
				bEscaped = xrtBufferAppend(
					pBuffer,
					(xbytesview){ (const uint8*)sEscape, 6u }
				);
			}
			else
			{
				bEscaped = (xrtBufferAppendByte(pBuffer, (uint8)c));
			}
			break;
		}
		if(!bEscaped)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pBuffer, '"');
}

/* 32 字节标量 → 43 字符 base64url 文本 + 末尾零，写入 44 字节输出。 */
static bool xacmeScalarText(const uint8* pScalar, char* sOut)
{
	size_t iSize = 0;
	if(!xrtBase64Encode(
		pScalar, 32u, sOut, 44u, &iSize, &__xacmeB64Url) || iSize != 43u)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_INTERNAL,
			"acme jose scalar base64url size mismatch"
		);
		return false;
	}
	sOut[43] = '\0';
	return true;
}

bool xacmeEs256Generate(xacmees256key* pKey)
{
	if(pKey == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT, "acme jose key required");
		return false;
	}
	if(!xrtP256KeyPair(pKey->Private, pKey->Public))
	{
		return false;
	}
	return true;
}

bool xacmeEs256FromPrivate(xacmees256key* pKey)
{
	if(pKey == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT, "acme jose key required");
		return false;
	}
	return xrtP256Public(pKey->Private, pKey->Public);
}

str xacmeJwkEcJson(const xacmees256key* pKey)
{
	char sX[44];
	char sY[44];
	str sJson;
	size_t iSize;
	if(pKey == NULL || pKey->Public[0] != 0x04u)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose jwk requires key with uncompressed public point"
		);
		return NULL;
	}
	if(!xacmeScalarText(&pKey->Public[1], sX) ||
		!xacmeScalarText(&pKey->Public[33], sY))
	{
		return NULL;
	}
	/* {"crv":"P-256","kty":"EC","x":"","y":""} 固定形状 = 46 + 86。 */
	iSize = 46u + 43u + 43u;
	sJson = (str)xrtMalloc(iSize + 1u);
	if(sJson == NULL)
	{
		return NULL;
	}
	memcpy(sJson, "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"", 31u);
	memcpy(sJson + 31u, sX, 43u);
	memcpy(sJson + 74u, "\",\"y\":\"", 7u);
	memcpy(sJson + 81u, sY, 43u);
	memcpy(sJson + 124u, "\"}", 2u);
	sJson[126] = '\0';
	return sJson;
}

bool xacmeJwkThumbprint(xstrview sCanonicalJson, char* sOut)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t iSize = 0;
	if(sCanonicalJson.Data == NULL || sCanonicalJson.Size == 0 || sOut == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose thumbprint requires json and output"
		);
		return false;
	}
	if(!xrtSha256(sCanonicalJson.Data, sCanonicalJson.Size, Digest))
	{
		return false;
	}
	if(!xrtBase64Encode(
		Digest, sizeof(Digest), sOut, 44u, &iSize, &__xacmeB64Url) ||
		iSize != 43u)
	{
		return false;
	}
	sOut[43] = '\0';
	return true;
}

bool xacmeJwkEcThumbprint(const xacmees256key* pKey, char* sOut)
{
	str sJson = xacmeJwkEcJson(pKey);
	bool bOk;
	if(sJson == NULL)
	{
		return false;
	}
	bOk = xacmeJwkThumbprint(
		(xstrview){ sJson, 126u }, sOut);
	xrtFree(sJson);
	return bOk;
}

str xacmeJwsEs256(
	const xacmees256key* pKey, const xacmejwsheader* pHeader,
	xstrview sPayload)
{
	xbuffer Header;
	xbuffer Token;
	str sResult = NULL;
	str sHeaderB64;
	str sPayloadB64;
	str sSignatureB64;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_ECDSA_P256_SIGNATURE_SIZE];
	bool bOk;

	if(pKey == NULL || pHeader == NULL || pHeader->Url.Data == NULL ||
		pHeader->Url.Size == 0 || sPayload.Data == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose jws requires key, header url and payload"
		);
		return NULL;
	}

	xrtBufferInit(&Header);
	xrtBufferInit(&Token);
	sHeaderB64 = NULL;
	sPayloadB64 = NULL;
	sSignatureB64 = NULL;

	bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL("{\"alg\":\"ES256\""));
	if(bOk && (pHeader->Nonce.Data != NULL) && (pHeader->Nonce.Size > 0u))
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"nonce\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Nonce);
	}
	if(bOk)
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"url\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Url);
	}
	if(bOk && (pHeader->Kid.Data != NULL) && (pHeader->Kid.Size > 0u))
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"kid\":"))
			&& xacmeJsonQuoteAppend(&Header, pHeader->Kid);
	}
	else if(bOk)
	{
		str sJwk = xacmeJwkEcJson(pKey);
		if(sJwk == NULL)
		{
			bOk = false;
		}
		else
		{
			bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"jwk\":"))
				&& xrtBufferAppend(
					&Header, (xbytesview){ (const uint8*)sJwk, 126u });
			xrtFree(sJwk);
		}
	}
	if(bOk)
	{
		bOk = xrtBufferAppend(&Header, XRT_BYTES_LITERAL("}"));
	}
	if(!bOk)
	{
		goto Done;
	}

	sHeaderB64 = xrtBase64EncodeNew(
		Header.Data == NULL ? "" : (cstr)Header.Data,
		xrtBufferView(&Header).Size,
		&__xacmeB64Url);
	sPayloadB64 = xrtBase64EncodeNew(
		sPayload.Data, sPayload.Size, &__xacmeB64Url);
	if((sHeaderB64 == NULL) || (sPayloadB64 == NULL))
	{
		goto Done;
	}

	/* 签名输入是 ASCII 的 header.payload 拼接。 */
	bOk = xrtBufferAppend(
		&Token, (xbytesview){ (const uint8*)sHeaderB64, strlen(sHeaderB64) })
		&& xrtBufferAppendByte(&Token, (uint8)'.')
		&& xrtBufferAppend(
			&Token,
			(xbytesview){ (const uint8*)sPayloadB64, strlen(sPayloadB64) });
	if(!bOk || !xrtSha256(Token.Data, xrtBufferView(&Token).Size, Digest))
	{
		goto Done;
	}
	if(!xrtEcdsaP256Sign(
		XCRYPTO_HASH_SHA256, Digest, pKey->Private, Signature))
	{
		goto Done;
	}
	sSignatureB64 = xrtBase64EncodeNew(
		Signature, sizeof(Signature), &__xacmeB64Url);
	if(sSignatureB64 == NULL)
	{
		goto Done;
	}

	/* ACME 使用 JWS 扁平 JSON 序列化（RFC 8555 §6.2）。 */
	sResult = xacmeJwsFlatAssemble(sHeaderB64, sPayloadB64, sSignatureB64);

Done:
	xrtFree(sHeaderB64);
	xrtFree(sPayloadB64);
	xrtFree(sSignatureB64);
	xrtBufferUnit(&Header);
	xrtBufferUnit(&Token);
	if(sResult == NULL)
	{
		xrtSetErrorInfo(
			XERR_MEMORY,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_INTERNAL,
			"acme jose jws assembly failed"
		);
	}
	return sResult;
}

str xacmeJwsEabHs256(
	cstr sKid, cstr sUrl, xstrview sJwkJson,
	const uint8* pMac, size_t iMacSize)
{
	xbuffer Header;
	xbuffer SigningInput;
	str sResult = NULL;
	str sHeaderB64 = NULL;
	str sPayloadB64 = NULL;
	str sSignatureB64 = NULL;
	uint8 Mac[XRT_SHA256_SIZE];
	bool bOk;

	if((sKid == NULL) || (sKid[0] == '\0') || (sUrl == NULL) ||
		(sUrl[0] == '\0') || (sJwkJson.Data == NULL) ||
		(sJwkJson.Size == 0u) || (pMac == NULL) || (iMacSize == 0u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_ARGUMENT,
			"acme jose eab requires kid, url, jwk and mac key"
		);
		return NULL;
	}

	xrtBufferInit(&Header);
	xrtBufferInit(&SigningInput);
	bOk = xrtBufferAppend(
			&Header, XRT_BYTES_LITERAL("{\"alg\":\"HS256\",\"kid\":")) &&
		xacmeJsonQuoteAppend(
			&Header, (xstrview){ sKid, strlen(sKid) }) &&
		xrtBufferAppend(&Header, XRT_BYTES_LITERAL(",\"url\":")) &&
		xacmeJsonQuoteAppend(
			&Header, (xstrview){ sUrl, strlen(sUrl) }) &&
		xrtBufferAppend(&Header, XRT_BYTES_LITERAL("}"));
	if(bOk)
	{
		sHeaderB64 = xrtBase64EncodeNew(
			(cstr)Header.Data, xrtBufferView(&Header).Size, &__xacmeB64Url);
		sPayloadB64 = xrtBase64EncodeNew(
			sJwkJson.Data, sJwkJson.Size, &__xacmeB64Url);
	}
	if((sHeaderB64 == NULL) || (sPayloadB64 == NULL))
	{
		goto Done;
	}
	/* 签名输入是 ASCII 的 header.payload 拼接（与 ES256 相同）。 */
	bOk = xrtBufferAppend(
			&SigningInput,
			(xbytesview){ (const uint8*)sHeaderB64, strlen(sHeaderB64) }) &&
		xrtBufferAppendByte(&SigningInput, (uint8)'.') &&
		xrtBufferAppend(
			&SigningInput,
			(xbytesview){ (const uint8*)sPayloadB64, strlen(sPayloadB64) }) &&
		xrtHmacSha256(
			pMac, iMacSize, SigningInput.Data,
			xrtBufferView(&SigningInput).Size, Mac);
	if(!bOk)
	{
		goto Done;
	}
	sSignatureB64 = xrtBase64EncodeNew(Mac, sizeof(Mac), &__xacmeB64Url);
	if(sSignatureB64 == NULL)
	{
		goto Done;
	}
	sResult = xacmeJwsFlatAssemble(sHeaderB64, sPayloadB64, sSignatureB64);

Done:
	xrtFree(sHeaderB64);
	xrtFree(sPayloadB64);
	xrtFree(sSignatureB64);
	xrtBufferUnit(&Header);
	xrtBufferUnit(&SigningInput);
	if(sResult == NULL)
	{
		xrtSetErrorInfo(
			XERR_MEMORY,
			"xrt.acme.jose",
			XACME_JOSE_ERROR_INTERNAL,
			"acme jose eab assembly failed"
		);
	}
	return sResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_csr.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_CSR)

#if defined(XACME_FEATURE_ACME_CSR)


#include <string.h>

/*
	DER 组装纪律：裸内容只积累在一个缓冲；包装永远是把 TLV
	追加进另一个缓冲（目标为空或已有兄弟 TLV）。禁止把缓冲
	自身的视图再包装回自身——xrtDerAppend 是追加而非替换。
*/

/* 把 AlgIdentifier 内容（两个 OID）包成 SEQ 追加到 pOut。 */
static bool xacmeEcAlgId(xbuffer* pOut)
{
	xbuffer Raw;
	bool bOk;
	xrtBufferInit(&Raw);
	bOk = xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.10045.2.1"))
		&& xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.10045.3.1.7"))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Raw));
	xrtBufferUnit(&Raw);
	return bOk;
}

/* ---------- CSR（PKCS#10，RFC 2986） ---------- */

/* subject: SEQ{ SET{ SEQ{ OID 2.5.4.3, UTF8String } } } */
static bool xacmeCsrSubject(xbuffer* pOut, xstrview sCommonName)
{
	xbuffer Ava;
	xbuffer RdnSeq;
	xbuffer RdnSet;
	bool bOk;
	if((sCommonName.Data == NULL) || (sCommonName.Size == 0u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme csr requires common name"
		);
		return false;
	}
	xrtBufferInit(&Ava);
	xrtBufferInit(&RdnSeq);
	xrtBufferInit(&RdnSet);
	bOk = xrtDerAppendOid(&Ava, XRT_STR_LITERAL("2.5.4.3"))
		&& xrtDerAppend(
			&Ava, XASN1_UNIVERSAL, XASN1_UTF8_STRING, false,
			(xbytesview){
				(const uint8*)sCommonName.Data, sCommonName.Size })
		&& xrtDerAppend(
			&RdnSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Ava))
		&& xrtDerAppend(
			&RdnSet, XASN1_UNIVERSAL, XASN1_SET, true,
			xrtBufferView(&RdnSeq))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&RdnSet));
	xrtBufferUnit(&Ava);
	xrtBufferUnit(&RdnSeq);
	xrtBufferUnit(&RdnSet);
	return bOk;
}

/* SubjectPublicKeyInfo：SEQ{ AlgId, BIT STRING 点 } */
static bool xacmeCsrSpki(xbuffer* pOut, const xacmees256key* pKey)
{
	xbuffer Spki;
	bool bOk;
	xrtBufferInit(&Spki);
	bOk = xacmeEcAlgId(&Spki)
		&& xrtDerAppendBitString(
			&Spki,
			(xbytesview){ pKey->Public, XRT_P256_PUBLIC_SIZE },
			0u)
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Spki));
	xrtBufferUnit(&Spki);
	return bOk;
}

/* attributes [0] IMPLICIT：extensionRequest 携带 subjectAltName。 */
static bool xacmeCsrAttributes(xbuffer* pOut, const xacmecsrconfig* pConfig)
{
	xbuffer Names;
	xbuffer ExtValue;
	xbuffer Ext;
	xbuffer ExtSeq;
	xbuffer ExtList;
	xbuffer ExtListSeq;
	xbuffer AttrValues;
	xbuffer Attr;
	xbuffer AttrSeq;
	bool bOk = true;
	size_t i;

	if((pConfig->Domains == NULL) || (pConfig->DomainCount == 0u))
	{
		return true; /* 无 SAN：整个 attributes 字段省略。 */
	}
	xrtBufferInit(&Names);
	xrtBufferInit(&ExtValue);
	xrtBufferInit(&Ext);
	xrtBufferInit(&ExtSeq);
	xrtBufferInit(&ExtList);
	xrtBufferInit(&ExtListSeq);
	xrtBufferInit(&AttrValues);
	xrtBufferInit(&Attr);
	xrtBufferInit(&AttrSeq);

	for(i = 0; bOk && (i < pConfig->DomainCount); i++)
	{
		if((pConfig->Domains[i].Data == NULL) ||
			(pConfig->Domains[i].Size == 0u))
		{
			xrtSetErrorInfo(
				XERR_ARGUMENT,
				"xrt.acme.csr",
				XACME_CSR_ERROR_ARGUMENT,
				"acme csr domain entry is empty"
			);
			bOk = false;
			break;
		}
		bOk = xrtDerAppend(
			&Names, XASN1_CONTEXT, 2u, false,
			(xbytesview){
				(const uint8*)pConfig->Domains[i].Data,
				pConfig->Domains[i].Size });
	}
		/* GeneralNames SEQ */
		bOk = bOk
			&& xrtDerAppend(
				&ExtValue, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Names))
			/* Extension 裸内容 = OID san + OCTET STRING{GeneralNames} */
			&& xrtDerAppendOid(&Ext, XRT_STR_LITERAL("2.5.29.17"))
			&& xrtDerAppend(
				&Ext, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				xrtBufferView(&ExtValue))
			/* Extension SEQ */
			&& xrtDerAppend(
				&ExtSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Ext))
			/* Extensions SEQUENCE OF Extension（RFC 2986 语义，
			   Go 严格解析要求此层）。 */
			&& xrtBufferAppend(&ExtList, xrtBufferView(&ExtSeq))
			&& xrtDerAppend(
				&ExtListSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&ExtList))
			/* values SET 携带 Extensions SEQUENCE。 */
			&& xrtDerAppend(
				&AttrValues, XASN1_UNIVERSAL, XASN1_SET, true,
				xrtBufferView(&ExtListSeq))
			/* Attribute 裸内容 = OID extReq + values SET TLV。 */
			&& xrtDerAppendOid(&Attr, XRT_STR_LITERAL("1.2.840.113549.1.9.14"))
			&& xrtBufferAppend(&Attr, xrtBufferView(&AttrValues))
			/* [0] IMPLICIT SET OF Attribute = [0]{ Attribute SEQ }。 */
			&& xrtDerAppend(
				&AttrSeq, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Attr))
			&& xrtDerAppend(
				pOut, XASN1_CONTEXT, 0u, true, xrtBufferView(&AttrSeq));

	xrtBufferUnit(&Names);
	xrtBufferUnit(&ExtValue);
	xrtBufferUnit(&Ext);
	xrtBufferUnit(&ExtSeq);
	xrtBufferUnit(&ExtList);
	xrtBufferUnit(&ExtListSeq);
	xrtBufferUnit(&AttrValues);
	xrtBufferUnit(&Attr);
	xrtBufferUnit(&AttrSeq);
	return bOk;
}

bool xacmeCsrEc(
	const xacmees256key* pKey, const xacmecsrconfig* pConfig, xbuffer* pOut)
{
	xbuffer Cri;
	xbuffer CriTlv;
	xbuffer SigAlgRaw;
	xbuffer SigAlg;
	xbuffer Outer;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_ECDSA_P256_DER_MAX_SIZE];
	size_t iSignatureSize = 0;
	bool bOk;

	if((pKey == NULL) || (pConfig == NULL) || (pOut == NULL) ||
		(pKey->Public[0] != 0x04u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme csr requires key, config and output"
		);
		return false;
	}

	xrtBufferInit(&Cri);
	xrtBufferInit(&CriTlv);
	xrtBufferInit(&SigAlgRaw);
	xrtBufferInit(&SigAlg);
	xrtBufferInit(&Outer);
	bOk = xrtDerAppendUInt64(&Cri, 0u) /* version INTEGER 0 */
		&& xacmeCsrSubject(&Cri, pConfig->CommonName)
		&& xacmeCsrSpki(&Cri, pKey)
		&& xacmeCsrAttributes(&Cri, pConfig)
		/* 签名对象是含 SEQ 头的完整 CRI DER（RFC 2986）。 */
		&& xrtDerAppend(
			&CriTlv, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Cri))
		&& xrtSha256(CriTlv.Data, CriTlv.Size, Digest)
		&& xrtEcdsaP256SignDer(
			XCRYPTO_HASH_SHA256, Digest, pKey->Private, Signature,
			sizeof(Signature), &iSignatureSize)
		&& xrtDerAppendOid(
			&SigAlgRaw, XRT_STR_LITERAL("1.2.840.10045.4.3.2"))
		&& xrtDerAppend(
			&SigAlg, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&SigAlgRaw))
		/* CertificationRequest = SEQ{ CRI, sigAlg, BIT STRING sig } */
		&& xrtBufferAppend(&Outer, xrtBufferView(&CriTlv))
		&& xrtBufferAppend(&Outer, xrtBufferView(&SigAlg))
		&& xrtDerAppendBitString(
			&Outer,
			(xbytesview){ Signature, iSignatureSize },
			0u)
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Outer));

	xrtBufferUnit(&Cri);
	xrtBufferUnit(&CriTlv);
	xrtBufferUnit(&SigAlgRaw);
	xrtBufferUnit(&SigAlg);
	xrtBufferUnit(&Outer);
	if(!bOk && xrtGetError() == NULL)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xrt.acme.csr",
			XACME_CSR_ERROR_INTERNAL,
			"acme csr assembly failed"
		);
	}
	return bOk;
}

/* ---------- 证书密钥抽象（ES256 / RSA） ---------- */

/* 大端无符号整数 → DER INTEGER（剥前导零，高位置位补 0x00）。 */
static bool xacmeDerAppendBigUint(
	xbuffer* pOut, const uint8* pData, size_t iSize)
{
	xbuffer Content;
	bool bOk = true;
	while((iSize > 1u) && (pData[0] == 0u))
	{
		pData++;
		iSize--;
	}
	xrtBufferInit(&Content);
	if(iSize == 0u)
	{
		bOk = xrtBufferAppendByte(&Content, 0u);
	}
	else
	{
		if((pData[0] & 0x80u) != 0u)
		{
			bOk = xrtBufferAppendByte(&Content, 0u);
		}
		if(bOk)
		{
			bOk = xrtBufferAppend(
				&Content, (xbytesview){ pData, iSize });
		}
	}
	if(bOk)
	{
		bOk = xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_INTEGER, false,
			xrtBufferView(&Content));
	}
	xrtBufferUnit(&Content);
	return bOk;
}

/* DER INTEGER（正数）剥符号前导零后拷入定长缓冲。 */
static bool xacmeDerIntToBytes(
	const xdervalue* pValue, uint8* pOut, size_t iCap, size_t* pOutSize)
{
	const uint8* pData = pValue->Value.Data;
	size_t iSize = pValue->Value.Size;
	if((pData == NULL) || (iSize == 0u))
	{
		return false;
	}
	if((pData[0] & 0x80u) != 0u)
	{
		return false; /* 负数：密钥分量不可能。 */
	}
	while((iSize > 1u) && (pData[0] == 0u))
	{
		pData++;
		iSize--;
	}
	if(iSize > iCap)
	{
		return false;
	}
	memcpy(pOut, pData, iSize);
	*pOutSize = iSize;
	return true;
}

/* PKCS#1（RSAPrivateKey）：n/e/d 必填，CRT 五参可选成组。 */
static bool xacmeRsaParsePkcs1(xdercursor* pCursor, xacmersakey* pKey)
{
	xdervalue Value;
	uint8 Version[8];
	size_t iVersionSize = 0u;
	struct
	{
		uint8* pData;
		size_t* pSize;
		size_t iCap;
		bool bGot;
	} Tails[5];
	size_t i;
	Tails[0].pData = pKey->Prime1;
	Tails[0].pSize = &pKey->Prime1Size;
	Tails[0].iCap = sizeof(pKey->Prime1);
	Tails[0].bGot = false;
	Tails[1].pData = pKey->Prime2;
	Tails[1].pSize = &pKey->Prime2Size;
	Tails[1].iCap = sizeof(pKey->Prime2);
	Tails[1].bGot = false;
	Tails[2].pData = pKey->Exponent1;
	Tails[2].pSize = &pKey->Exponent1Size;
	Tails[2].iCap = sizeof(pKey->Exponent1);
	Tails[2].bGot = false;
	Tails[3].pData = pKey->Exponent2;
	Tails[3].pSize = &pKey->Exponent2Size;
	Tails[3].iCap = sizeof(pKey->Exponent2);
	Tails[3].bGot = false;
	Tails[4].pData = pKey->Coefficient;
	Tails[4].pSize = &pKey->CoefficientSize;
	Tails[4].iCap = sizeof(pKey->Coefficient);
	Tails[4].bGot = false;
	if(!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, Version, sizeof(Version), &iVersionSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->Modulus, sizeof(pKey->Modulus),
			&pKey->ModulusSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->Exponent, sizeof(pKey->Exponent),
			&pKey->ExponentSize) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) ||
		!xacmeDerIntToBytes(
			&Value, pKey->PrivateExponent, sizeof(pKey->PrivateExponent),
			&pKey->PrivateExponentSize))
	{
		return false;
	}
	for(i = 0; i < 5u; i++)
	{
		if(xrtDerRead(pCursor, &Value) != XDER_VALUE)
		{
			break;
		}
		if(!xrtDerIs(
				&Value, XASN1_UNIVERSAL, XASN1_INTEGER, false))
		{
			break; /* 允许尾部出现其他可选元素。 */
		}
		if(!xacmeDerIntToBytes(
				&Value, Tails[i].pData, Tails[i].iCap, Tails[i].pSize))
		{
			return false;
		}
		Tails[i].bGot = true;
	}
	for(i = 0; i < 5u; i++)
	{
		if(!Tails[i].bGot)
		{
			*Tails[i].pSize = 0u;
		}
	}
	return (pKey->ModulusSize != 0u) && (pKey->ExponentSize != 0u) &&
		(pKey->PrivateExponentSize != 0u);
}

/* RSA PKCS#1 写出（CRT 完整时带五参数）。 */
static bool xacmeRsaWritePkcs1(const xacmersakey* pKey, xbuffer* pOut)
{
	xbuffer Body;
	bool bOk;
	xrtBufferInit(&Body);
	bOk = xrtDerAppendUInt64(&Body, 0u) &&
		xacmeDerAppendBigUint(
			&Body, pKey->Modulus, pKey->ModulusSize) &&
		xacmeDerAppendBigUint(
			&Body, pKey->Exponent, pKey->ExponentSize) &&
		xacmeDerAppendBigUint(
			&Body, pKey->PrivateExponent, pKey->PrivateExponentSize);
	if(bOk && (pKey->Prime1Size != 0u) && (pKey->Prime2Size != 0u) &&
		(pKey->Exponent1Size != 0u) && (pKey->Exponent2Size != 0u) &&
		(pKey->CoefficientSize != 0u))
	{
		bOk = xacmeDerAppendBigUint(
				&Body, pKey->Prime1, pKey->Prime1Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Prime2, pKey->Prime2Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Exponent1, pKey->Exponent1Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Exponent2, pKey->Exponent2Size) &&
			xacmeDerAppendBigUint(
				&Body, pKey->Coefficient, pKey->CoefficientSize);
	}
	if(bOk)
	{
		bOk = xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Body));
	}
	xrtBufferUnit(&Body);
	return bOk;
}

/* rsaEncryption AlgIdentifier（NULL 参数）。 */
static bool xacmeRsaAlgId(xbuffer* pOut)
{
	xbuffer Raw;
	bool bOk;
	xrtBufferInit(&Raw);
	bOk = xrtDerAppendOid(&Raw, XRT_STR_LITERAL("1.2.840.113549.1.1.1")) &&
		xrtDerAppendNull(&Raw) &&
		xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Raw));
	xrtBufferUnit(&Raw);
	return bOk;
}

/* RSA SPKI：SEQ{ rsaEncryption, BIT STRING{ SEQ{ n, e } } }。 */
static bool xacmeCsrSpkiRsa(xbuffer* pOut, const xacmersakey* pKey)
{
	xbuffer Alg;
	xbuffer NeRaw;
	xbuffer Ne;
	xbuffer Spki;
	bool bOk;
	xrtBufferInit(&Alg);
	xrtBufferInit(&NeRaw);
	xrtBufferInit(&Ne);
	xrtBufferInit(&Spki);
	bOk = xacmeRsaAlgId(&Alg) &&
		xacmeDerAppendBigUint(
			&NeRaw, pKey->Modulus, pKey->ModulusSize) &&
		xacmeDerAppendBigUint(
			&NeRaw, pKey->Exponent, pKey->ExponentSize) &&
		xrtDerAppend(
			&Ne, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&NeRaw)) &&
		xrtBufferAppend(&Spki, xrtBufferView(&Alg)) &&
		xrtDerAppendBitString(
			&Spki, xrtBufferView(&Ne), 0u) &&
		xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Spki));
	xrtBufferUnit(&Alg);
	xrtBufferUnit(&NeRaw);
	xrtBufferUnit(&Ne);
	xrtBufferUnit(&Spki);
	return bOk;
}

/* RSA 私钥视图（借用 xacmersakey 定长缓冲）。 */
static void xacmeRsaView(
	const xacmersakey* pKey, xrsaprivatekey* pView)
{
	memset(pView, 0, sizeof(*pView));
	pView->Public.Modulus = pKey->Modulus;
	pView->Public.ModulusSize = pKey->ModulusSize;
	pView->Public.Exponent = pKey->Exponent;
	pView->Public.ExponentSize = pKey->ExponentSize;
	pView->PrivateExponent = pKey->PrivateExponent;
	pView->PrivateExponentSize = pKey->PrivateExponentSize;
	if((pKey->Prime1Size != 0u) && (pKey->Prime2Size != 0u) &&
		(pKey->Exponent1Size != 0u) && (pKey->Exponent2Size != 0u) &&
		(pKey->CoefficientSize != 0u))
	{
		pView->Prime1 = pKey->Prime1;
		pView->Prime1Size = pKey->Prime1Size;
		pView->Prime2 = pKey->Prime2;
		pView->Prime2Size = pKey->Prime2Size;
		pView->Exponent1 = pKey->Exponent1;
		pView->Exponent1Size = pKey->Exponent1Size;
		pView->Exponent2 = pKey->Exponent2;
		pView->Exponent2Size = pKey->Exponent2Size;
		pView->Coefficient = pKey->Coefficient;
		pView->CoefficientSize = pKey->CoefficientSize;
	}
}

void xacmeCertKeyUnit(xacmecertkey* pKey)
{
	if(pKey == NULL)
	{
		return;
	}
	xrtSecureZero(pKey, sizeof(*pKey));
	pKey->Kind = XACME_CERT_KEY_ES256;
}

bool xacmeCertKeyReadPem(cstr sPem, size_t iSize, xacmecertkey* pKey)
{
	xpemblock Block;
	bool bHavePkcs8;
	bool bHavePkcs1Rsa;
	bool bHaveSec1 = false;

	if((sPem == NULL) || (iSize == 0u) || (pKey == NULL))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key read requires pem and key");
		return false;
	}
	xacmeCertKeyUnit(pKey);
	bHavePkcs8 = xrtPemFind(sPem, iSize, "PRIVATE KEY", &Block);
	if(!bHavePkcs8 && xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	bHavePkcs1Rsa = false;
	if(!bHavePkcs8)
	{
		xrtClearError();
		bHavePkcs1Rsa = xrtPemFind(sPem, iSize, "RSA PRIVATE KEY", &Block);
		if(!bHavePkcs1Rsa && xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
		if(!bHavePkcs1Rsa)
		{
			xrtClearError();
			bHaveSec1 = xrtPemFind(sPem, iSize, "EC PRIVATE KEY", &Block);
			if(!bHaveSec1 && xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
		}
	}

	/* EC：PKCS#8（非 RSA）或 SEC1。 */
	if(bHavePkcs8 || bHaveSec1)
	{
		xrtClearError();
		if(xacmeKeyPemRead(sPem, iSize, &pKey->Ec))
		{
			pKey->Kind = XACME_CERT_KEY_ES256;
			return true;
		}
		if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
		xrtClearError();
	}

	if(bHavePkcs1Rsa)
	{
		size_t iDerSize = 0u;
		bytes pDer = xrtPemDecodeNew(&Block, &iDerSize);
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value) &&
			xrtDerEnter(&Value, &Cursor) &&
			xacmeRsaParsePkcs1(&Cursor, &pKey->Rsa);
		xrtFree(pDer);
		if(bOk)
		{
			pKey->Kind = XACME_CERT_KEY_RSA;
			return true;
		}
		xacmeCertKeyUnit(pKey);
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key rsa pkcs1 invalid");
		return false;
	}

	if(bHavePkcs8)
	{
		size_t iDerSize = 0u;
		bytes pDer = xrtPemDecodeNew(&Block, &iDerSize);
		xdercursor Cursor;
		xdervalue Value;
		xdervalue Algorithm;
		xdercursor AlgCursor;
		xdervalue Oid;
		xdercursor Inner;
		xdervalue InnerSeq;
		bool bIsRsa = false;
		bool bOk;
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value) &&
			xrtDerEnter(&Value, &Cursor) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value) &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				&Algorithm) &&
			xrtDerEnter(&Algorithm, &AlgCursor) &&
			(xrtDerRead(&AlgCursor, &Oid) == XDER_VALUE) &&
			xrtDerIs(&Oid, XASN1_UNIVERSAL, XASN1_OBJECT_IDENTIFIER, false);
		if(bOk)
		{
			/* rsaEncryption OID 的 DER 内容固定 9 字节。 */
			static const uint8 uRsaOid[9] = {
				0x2a, 0x86, 0x48, 0x86, 0xf7,
				0x0d, 0x01, 0x01, 0x01 };
			bIsRsa = (Oid.Value.Size == sizeof(uRsaOid)) &&
				(memcmp(Oid.Value.Data, uRsaOid, sizeof(uRsaOid)) == 0);
		}
		if(bOk && bIsRsa &&
			xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				&Value))
		{
			bOk = xrtDerInit(
					&Inner, Value.Value.Data, Value.Value.Size) &&
				xrtDerExpect(
					&Inner, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
					&InnerSeq) &&
				xrtDerEnter(&InnerSeq, &Inner) &&
				xacmeRsaParsePkcs1(&Inner, &pKey->Rsa);
			xrtFree(pDer);
			if(bOk)
			{
				pKey->Kind = XACME_CERT_KEY_RSA;
				return true;
			}
			xacmeCertKeyUnit(pKey);
			xrtSetErrorInfo(
				XERR_ARGUMENT, "xrt.acme.csr",
				XACME_CSR_ERROR_ARGUMENT,
				"acme cert key rsa pkcs8 invalid");
			return false;
		}
		xrtFree(pDer);
	}

	xacmeCertKeyUnit(pKey);
	xrtSetErrorInfo(
		XERR_NOT_FOUND, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
		"acme cert key pem unrecognized");
	return false;
}

str xacmeCertKeyPemWrite(const xacmecertkey* pKey)
{
	if((pKey == NULL) ||
		((pKey->Kind == XACME_CERT_KEY_ES256) &&
			(pKey->Ec.Public[0] != 0x04u)) ||
		((pKey->Kind == XACME_CERT_KEY_RSA) &&
			((pKey->Rsa.ModulusSize == 0u) ||
				(pKey->Rsa.PrivateExponentSize == 0u))))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme cert key write requires valid key");
		return NULL;
	}
	if(pKey->Kind == XACME_CERT_KEY_ES256)
	{
		return xacmeKeyPemWrite(&pKey->Ec);
	}
	{
		xbuffer Alg;
		xbuffer Pkcs1;
		xbuffer Body;
		xbuffer Pkcs8;
		str sPem = NULL;
		bool bOk;
		xrtBufferInit(&Alg);
		xrtBufferInit(&Pkcs1);
		xrtBufferInit(&Body);
		xrtBufferInit(&Pkcs8);
		bOk = xacmeRsaAlgId(&Alg) &&
			xacmeRsaWritePkcs1(&pKey->Rsa, &Pkcs1) &&
			xrtDerAppendUInt64(&Body, 0u) &&
			xrtBufferAppend(&Body, xrtBufferView(&Alg)) &&
			xrtDerAppend(
				&Body, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				xrtBufferView(&Pkcs1)) &&
			xrtDerAppend(
				&Pkcs8, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Body));
		if(bOk && (Pkcs8.Data != NULL))
		{
			sPem = xrtPemEncodeNew(
				"PRIVATE KEY", Pkcs8.Data, Pkcs8.Size);
		}
		xrtBufferUnit(&Alg);
		xrtBufferUnit(&Pkcs1);
		xrtBufferUnit(&Body);
		xrtBufferUnit(&Pkcs8);
		return sPem;
	}
}

/* 通用 PKCS#10 组装：骨架共享，SPKI/签名算法/签名按密钥算法分派。 */
bool xacmeCsrBuild(
	const xacmecertkey* pKey, const xacmecsrconfig* pConfig, xbuffer* pOut)
{
	xbuffer Cri;
	xbuffer CriTlv;
	xbuffer SigAlgRaw;
	xbuffer SigAlg;
	xbuffer Outer;
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Signature[XRT_RSA_MAX_MODULUS_SIZE];
	size_t iSignatureSize = 0u;
	bool bOk;

	if((pKey == NULL) || (pConfig == NULL) || (pOut == NULL) ||
		(pConfig->CommonName.Data == NULL) ||
		(pConfig->CommonName.Size == 0u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.csr", XACME_CSR_ERROR_ARGUMENT,
			"acme csr build requires key, config and output");
		return false;
	}
	xrtBufferInit(&Cri);
	xrtBufferInit(&CriTlv);
	xrtBufferInit(&SigAlgRaw);
	xrtBufferInit(&SigAlg);
	xrtBufferInit(&Outer);
	bOk = xrtDerAppendUInt64(&Cri, 0u) &&
		xacmeCsrSubject(&Cri, pConfig->CommonName) &&
		((pKey->Kind == XACME_CERT_KEY_ES256) ?
			xacmeCsrSpki(&Cri, &pKey->Ec) :
			xacmeCsrSpkiRsa(&Cri, &pKey->Rsa)) &&
		xacmeCsrAttributes(&Cri, pConfig) &&
		xrtDerAppend(
			&CriTlv, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Cri)) &&
		xrtSha256(CriTlv.Data, xrtBufferView(&CriTlv).Size, Digest);
	if(bOk && (pKey->Kind == XACME_CERT_KEY_ES256))
	{
		bOk = xrtEcdsaP256SignDer(
			XCRYPTO_HASH_SHA256, Digest, pKey->Ec.Private, Signature,
			sizeof(Signature), &iSignatureSize);
	}
	else if(bOk)
	{
		xrsaprivatekey View;
		xacmeRsaView(&pKey->Rsa, &View);
		iSignatureSize = pKey->Rsa.ModulusSize;
		bOk = (iSignatureSize <= sizeof(Signature)) &&
			xrtRsaPkcs1Sign(
				&View, XCRYPTO_HASH_SHA256, Digest, Signature);
	}
	if(bOk)
	{
		bOk = xrtDerAppendOid(
				&SigAlgRaw,
				(pKey->Kind == XACME_CERT_KEY_ES256) ?
					XRT_STR_LITERAL("1.2.840.10045.4.3.2") :
					XRT_STR_LITERAL("1.2.840.113549.1.1.11"));
		if(bOk && (pKey->Kind == XACME_CERT_KEY_RSA))
		{
			bOk = xrtDerAppendNull(&SigAlgRaw);
		}
		bOk = bOk &&
			xrtDerAppend(
				&SigAlg, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&SigAlgRaw)) &&
			xrtBufferAppend(&Outer, xrtBufferView(&CriTlv)) &&
			xrtBufferAppend(&Outer, xrtBufferView(&SigAlg)) &&
			xrtDerAppendBitString(
				&Outer, (xbytesview){ Signature, iSignatureSize }, 0u) &&
			xrtDerAppend(
				pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
				xrtBufferView(&Outer));
	}
	xrtBufferUnit(&Cri);
	xrtBufferUnit(&CriTlv);
	xrtBufferUnit(&SigAlgRaw);
	xrtBufferUnit(&SigAlg);
	xrtBufferUnit(&Outer);
	if(!bOk && xrtGetError() == NULL)
	{
		xrtSetErrorInfo(
			XERR_INTERNAL, "xrt.acme.csr", XACME_CSR_ERROR_INTERNAL,
			"acme csr build assembly failed");
	}
	return bOk;
}

/* ---------- 私钥 PEM 序列化 ---------- */
/* ---------- 私钥 PEM 序列化 ---------- */

/* SEC1：SEQ{ INT 1, OCTET d, [0]{曲线 OID}, [1]{BIT STRING pub} } */
static bool xacmeSec1Der(const xacmees256key* pKey, xbuffer* pOut)
{
	xbuffer Curve;
	xbuffer Pub;
	xbuffer Sec1;
	bool bOk;
	xrtBufferInit(&Curve);
	xrtBufferInit(&Pub);
	xrtBufferInit(&Sec1);
	bOk = xrtDerAppendOid(&Curve, XRT_STR_LITERAL("1.2.840.10045.3.1.7"))
		&& xrtDerAppendUInt64(&Sec1, 1u)
		&& xrtDerAppend(
			&Sec1, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
			(xbytesview){ pKey->Private, XRT_P256_PRIVATE_SIZE })
		&& xrtDerAppend(
			&Sec1, XASN1_CONTEXT, 0u, true, xrtBufferView(&Curve))
		&& xrtDerAppendBitString(
			&Pub,
			(xbytesview){ pKey->Public, XRT_P256_PUBLIC_SIZE },
			0u)
		&& xrtDerAppend(
			&Sec1, XASN1_CONTEXT, 1u, true, xrtBufferView(&Pub))
		&& xrtDerAppend(
			pOut, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Sec1));
	xrtBufferUnit(&Curve);
	xrtBufferUnit(&Pub);
	xrtBufferUnit(&Sec1);
	return bOk;
}

str xacmeKeyPemWrite(const xacmees256key* pKey)
{
	xbuffer AlgId;
	xbuffer Sec1;
	xbuffer Body;
	xbuffer Pkcs8;
	str sPem = NULL;
	bool bOk;

	if((pKey == NULL) || (pKey->Public[0] != 0x04u))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme key pem write requires key with public point"
		);
		return NULL;
	}
	xrtBufferInit(&AlgId);
	xrtBufferInit(&Sec1);
	xrtBufferInit(&Body);
	xrtBufferInit(&Pkcs8);
	bOk = xrtDerAppendUInt64(&Body, 0u) /* PKCS#8 version 0 */
		&& xacmeEcAlgId(&AlgId)
		&& xrtBufferAppend(&Body, xrtBufferView(&AlgId))
		&& xacmeSec1Der(pKey, &Sec1)
		&& xrtDerAppend(
			&Body, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
			xrtBufferView(&Sec1))
		&& xrtDerAppend(
			&Pkcs8, XASN1_UNIVERSAL, XASN1_SEQUENCE, true,
			xrtBufferView(&Body));
	if(bOk)
	{
		sPem = xrtPemEncodeNew("PRIVATE KEY", Pkcs8.Data, Pkcs8.Size);
	}
	xrtBufferUnit(&AlgId);
	xrtBufferUnit(&Sec1);
	xrtBufferUnit(&Body);
	xrtBufferUnit(&Pkcs8);
	return sPem;
}

/* 解析 SEC1 内容（已进入最外层 SEQ）：INT ver, OCTET d, [0]?, [1]? */
static bool xacmeSec1Parse(xdercursor* pCursor, xacmees256key* pKey)
{
	xdervalue Version;
	xdervalue Private;
	xdervalue Field;
	bool bHasPublic = false;

	if(!xrtDerExpect(
		pCursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Version) ||
		!xrtDerExpect(
			pCursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false, &Private))
	{
		return false;
	}
	if(Private.Value.Size != XRT_P256_PRIVATE_SIZE)
	{
		return false;
	}
	memcpy(pKey->Private, Private.Value.Data, XRT_P256_PRIVATE_SIZE);
	while(xrtDerRead(pCursor, &Field) == XDER_VALUE)
	{
		if((Field.Tag.Class == XASN1_CONTEXT) &&
			(Field.Tag.Number == 1u))
		{
			xdercursor Pub;
			xdervalue Point;
			xbytesview Key;
			uint8 iUnusedBits = 0;
			if(!xrtDerEnter(&Field, &Pub) ||
				(xrtDerRead(&Pub, &Point) != XDER_VALUE) ||
				!xrtDerIs(
					&Point, XASN1_UNIVERSAL, XASN1_BIT_STRING, false) ||
				!xrtDerBitString(&Point, &Key, &iUnusedBits) ||
				(iUnusedBits != 0u) ||
				(Key.Size != XRT_P256_PUBLIC_SIZE))
			{
				return false;
			}
			memcpy(pKey->Public, Key.Data, XRT_P256_PUBLIC_SIZE);
			bHasPublic = true;
		}
	}
	if(!bHasPublic)
	{
		return xacmeEs256FromPrivate(pKey);
	}
	return true;
}

bool xacmeKeyPemRead(cstr sPem, size_t iSize, xacmees256key* pKey)
{
	xpemblock Block;

	if((sPem == NULL) || (iSize == 0u) || (pKey == NULL))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.csr",
			XACME_CSR_ERROR_ARGUMENT,
			"acme key pem read requires pem and key"
		);
		return false;
	}

	/* PKCS#8 "PRIVATE KEY"。 */
	if(xrtPemFind(sPem, iSize, "PRIVATE KEY", &Block))
	{
		size_t iDerSize = 0;
		bytes pDer;
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize))
		{
			return false;
		}
		pDer = xrtPemDecodeNew(&Block, &iDerSize);
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_INTEGER, false, &Value)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_OCTET_STRING, false,
				&Value)
			/* privateKey 是 primitive OCTET STRING，内容即完整 SEC1。 */
			&& xrtDerInit(
				&Cursor, Value.Value.Data, Value.Value.Size)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xacmeSec1Parse(&Cursor, pKey);
		xrtFree(pDer);
		return bOk;
	}

	/* 传统 SEC1 "EC PRIVATE KEY"。 */
	if(xrtPemFind(sPem, iSize, "EC PRIVATE KEY", &Block))
	{
		size_t iDerSize = 0;
		bytes pDer;
		xdercursor Cursor;
		xdervalue Value;
		bool bOk;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize))
		{
			return false;
		}
		pDer = xrtPemDecodeNew(&Block, &iDerSize);
		if(pDer == NULL)
		{
			return false;
		}
		bOk = xrtDerInit(&Cursor, pDer, iDerSize)
			&& xrtDerExpect(
				&Cursor, XASN1_UNIVERSAL, XASN1_SEQUENCE, true, &Value)
			&& xrtDerEnter(&Value, &Cursor)
			&& xacmeSec1Parse(&Cursor, pKey);
		xrtFree(pDer);
		return bOk;
	}

	xrtSetErrorInfo(
		XERR_NOT_FOUND,
		"xrt.acme.csr",
		XACME_CSR_ERROR_ARGUMENT,
		"acme key pem read found no ec private key block"
	);
	return false;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_dns.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_DNS)

#if defined(XACME_FEATURE_ACME_DNS)

bool xrtAcmeDnsProviderValidate(const xacmednsprovider* pProvider)
{
	if(pProvider == NULL || pProvider->sId == NULL ||
		pProvider->Add == NULL || pProvider->Remove == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns provider requires id, add and remove"
		);
		return false;
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_registry.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_DNS)
/*
	组合根：内建 DNS provider 注册表实现在这里而不是 acme_dns 模块，
	使 provider 可以独立编译链接而不强制引入全部内建实现。
	注册表内容随 features.h 三态选择结果变化。
*/

#if defined(XACME_FEATURE_ACME_DNS)

typedef struct xacmednsentry {
	const char* sId;
} xacmednsentry;

static const xacmednsentry __xacmeDnsBuiltins[] = {
#if defined(XACME_FEATURE_DNS_ALI)
	{ "ali" },
#endif
#if defined(XACME_FEATURE_DNS_CF)
	{ "cf" },
#endif
#if defined(XACME_FEATURE_DNS_TENCENT)
	{ "tencent" },
#endif
#if defined(XACME_FEATURE_DNS_AWS)
	{ "aws" },
#endif
#if defined(XACME_FEATURE_DNS_HUAWEI)
	{ "huawei" },
#endif
};

size_t xrtAcmeDnsProviderCount(void)
{
	return sizeof(__xacmeDnsBuiltins) / sizeof(__xacmeDnsBuiltins[0]);
}

const char* xrtAcmeDnsProviderId(size_t iIndex)
{
	if(iIndex >= xrtAcmeDnsProviderCount())
	{
		xrtSetErrorInfo(
			XERR_RANGE,
			"xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns provider index out of range"
		);
		return NULL;
	}
	return __xacmeDnsBuiltins[iIndex].sId;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dnstxt.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_DNS)

#if defined(XACME_FEATURE_ACME_DNS)


#include <string.h>

static void xacmeTxtError(
	xerrkind Kind, xacmednstxterror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns.txt", (int32)Code, sMessage);
}

/* Resource exhaustion is terminal and retains the actual allocator cause.
 * Other failures keep the DNS probe's stable protocol/network diagnostics. */
static void xacmeTxtFailure(
	xerrkind Kind, xacmednstxterror Code, cstr sMessage)
{
	if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeTxtError(Kind, Code, sMessage);
}

bool xacmeDnsInit(xacmedns* pDns, struct xnetengine* pBorrowedEngine)
{
	if(pDns == NULL)
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns init requires dns");
		return false;
	}
	memset(pDns, 0, sizeof(*pDns));
	if(pBorrowedEngine != NULL)
	{
		pDns->pEngine = pBorrowedEngine;
		return true;
	}
	{
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pDns->pEngine = xrtNetEngineCreate(&Engine);
		if(pDns->pEngine == NULL) return false;
		pDns->bEngineOwned = true;
		if(!xrtNetEngineStart(pDns->pEngine))
		{
			(void)xacmeDnsUnit(pDns);
			return false;
		}
	}
	return true;
}

bool xacmeDnsUnit(xacmedns* pDns)
{
	xerror* pPrevious;
	bool bReady = true;
	if(pDns == NULL)
	{
		return true;
	}
	pPrevious = xrtErrorRef(xrtGetError());
	if(pDns->bEngineOwned && (pDns->pEngine != NULL))
	{
		double Deadline = __xrtWaitAfter(INT64_C(30000));
		for(;;)
		{
			xnetretireresult Result = xrtNetEngineTryDestroy(pDns->pEngine);
			if(Result == XNET_RETIRE_READY) break;
			if(Result == XNET_RETIRE_ERROR) { bReady = false; break; }
			if(__xrtWaitExpired(Deadline))
			{
				xacmeTxtError(XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
					"acme dns engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			xrtSleep(1u);
		}
	}
	if(bReady) memset(pDns, 0, sizeof(*pDns));
	if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	return bReady;
}

/* 组装 QNAME：点分文本 → DNS 标签序列（含末尾根零）。 */
static bool xacmeTxtEncodeName(xbuffer* pOut, cstr sFqdn)
{
	const char* s = sFqdn;
	if((sFqdn == NULL) || (sFqdn[0] == '\0'))
	{
		xacmeTxtError(XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt query fqdn invalid");
		return false;
	}
	while(*s != '\0')
	{
		const char* sDot = strchr(s, '.');
		size_t iLabel =
			(sDot != NULL) ? (size_t)(sDot - s) : strlen(s);
		if((iLabel == 0u) || (iLabel > 63u))
		{
			xacmeTxtError(XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
				"acme dns txt query fqdn invalid");
			return false;
		}
		if(!xrtBufferAppendByte(pOut, (uint8)iLabel) ||
			!xrtBufferAppend(
				pOut, (xbytesview){ (const uint8*)s, iLabel }))
		{
			return false;
		}
		s += iLabel + ((sDot != NULL) ? 1u : 0u);
	}
	return xrtBufferAppendByte(pOut, 0u);
}

/* 跳过响应中的（可能压缩的）name；返回消费字节数，0 表示非法。 */
static size_t xacmeTxtSkipName(const uint8* p, size_t iSize, size_t iAt)
{
	size_t i = iAt;
	size_t iGuard = 0u;
	for(;;)
	{
		uint8 iLen;
		if(i >= iSize)
		{
			return 0u;
		}
		iLen = p[i];
		if((iLen & 0xC0u) == 0xC0u)
		{
			return (i + 2u <= iSize) ? (i + 2u - iAt) : 0u;
		}
		if((iLen & 0xC0u) != 0u)
		{
			return 0u;
		}
		i += 1u + iLen;
		if(iLen == 0u)
		{
			return i - iAt;
		}
		if((++iGuard) > 128u)
		{
			return 0u;
		}
	}
}

static bool xacmeTxtReadU16(
	const uint8* p, size_t iSize, size_t* pAt, uint16* pOut)
{
	if((*pAt + 2u) > iSize)
	{
		return false;
	}
	*pOut = (uint16)(((uint16)p[*pAt] << 8u) | p[*pAt + 1]);
	*pAt += 2u;
	return true;
}

/*
	解析完整 DNS 响应报文为 TXT 记录集合（不可信网络输入的唯一
	消化口，fuzz 目标）。QR 位缺失/结构损坏 → false；RCODE 非零
	（NXDOMAIN 等）→ true 且零记录。记录值拼接 character-string，
	每条以零结尾且严格小于 XACME_TXT_RECORD_MAX。
*/
bool xacmeTxtParseResponse(
	const uint8* p, size_t iSize, uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount)
{
	size_t iAt;
	uint16 iQuestions;
	uint16 iAnswers;
	uint16 iFlags;
	size_t iCount = 0u;

	if(pOutCount != NULL) *pOutCount = 0u;
	if((p == NULL) || (iSize < 12u) || (sOutRecords == NULL) ||
		(pOutCount == NULL) || (iCapacity == 0u))
	{
		return false;
	}
	if((p[0] != (uint8)(uExpectId >> 8u)) ||
		(p[1] != (uint8)uExpectId))
	{
		return false;
	}
	iFlags = (uint16)(((uint16)p[2] << 8u) | p[3]);
	if((iFlags & 0x8000u) == 0u)
	{
		return false;
	}
	if((iFlags & 0x000Fu) != 0u)
	{
		return true; /* NXDOMAIN 等：无记录，成功返回零。 */
	}
	iAt = 4u;
	if(!xacmeTxtReadU16(p, iSize, &iAt, &iQuestions) ||
		!xacmeTxtReadU16(p, iSize, &iAt, &iAnswers))
	{
		return false;
	}
	iAt = 12u;
	for(; iQuestions > 0u; iQuestions--)
	{
		size_t iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if((iSkip == 0u) || ((iAt + iSkip + 4u) > iSize))
		{
			return false;
		}
		iAt += iSkip + 4u;
	}
	for(; iAnswers > 0u; iAnswers--)
	{
		uint16 iType;
		uint16 iClass;
		uint16 iRdLength;
		size_t iRdAt;
		size_t iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if(iSkip == 0u)
		{
			return false;
		}
		iAt += iSkip;
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iType) ||
			!xacmeTxtReadU16(p, iSize, &iAt, &iClass) ||
			(iAt + 4u > iSize))
		{
			return false;
		}
		iAt += 4u; /* TTL */
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iRdLength) ||
			((iAt + iRdLength) > iSize))
		{
			return false;
		}
		iRdAt = iAt;
		iAt += iRdLength;
		if((iType != 0x0010u) || (iClass != 0x0001u) ||
			(iCount >= iCapacity))
		{
			continue;
		}
		/* TXT rdata = 若干 character-string，拼接为一条记录值。 */
		{
			size_t iUsed = 0u;
			char* sRecord = sOutRecords[iCount];
			while(iRdAt < iAt)
			{
				uint8 iStrLen = p[iRdAt];
				iRdAt += 1u;
				if((iStrLen == 0u) || ((iRdAt + iStrLen) > iAt) ||
					((iUsed + iStrLen) >= XACME_TXT_RECORD_MAX))
				{
					return false;
				}
				memcpy(sRecord + iUsed, p + iRdAt, iStrLen);
				iUsed += iStrLen;
				iRdAt += iStrLen;
			}
			sRecord[iUsed] = '\0';
			iCount++;
		}
	}
	*pOutCount = iCount;
	return true;
}

bool xacmeDnsTxtQuery(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount)
{
	xbuffer Query;
	xnetaddr Peer;
	xnetudp* pUdp = NULL;
	xnetudppacket* pPacket = NULL;
	uint16 iId;
	const uint8* p;
	size_t iSize;
	bool bOk = false;

	if(pOutCount != NULL) *pOutCount = 0u;
	if((pDns == NULL) || (pDns->pEngine == NULL) || (sResolver == NULL) || (sFqdn == NULL) ||
		(sOutRecords == NULL) || (pOutCount == NULL) || (iCapacity == 0u))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt query requires dns, resolver, fqdn and outputs");
		return false;
	}
	xrtBufferInit(&Query);
	/* 探测仅咨询性（失败不阻断），但事务 ID 仍用密码学随机，
	   降低在路径攻击者伪造应答提前放行传播门的概率。 */
	{
		uint8 uSecure[2];
		if(!xrtSecureRandom(uSecure, sizeof(uSecure)))
		{
			xacmeTxtFailure(
				XERR_INTERNAL, XACME_TXT_ERROR_NETWORK,
				"acme dns txt secure random failed");
			goto Done;
		}
		iId = (uint16)(((uint16)uSecure[0] << 8u) | uSecure[1]);
	}
	{
		uint8 Head[12] = {
			(uint8)(iId >> 8u), (uint8)iId,
			0x01, 0x00, /* RD=1 */
			0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
		if(!xrtBufferAppend(&Query, (xbytesview){ Head, 12u }) ||
			!xacmeTxtEncodeName(&Query, sFqdn))
		{
			goto Done;
		}
		{
			uint8 Tail[4] = { 0x00, 0x10, 0x00, 0x01 };
			if(!xrtBufferAppend(&Query, (xbytesview){ Tail, 4u }))
			{
				goto Done;
			}
		}
	}

	if(!xrtNetAddrParse(&Peer, sResolver, (iPort != 0u) ? iPort : 53u))
	{
		xacmeTxtFailure(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt resolver address invalid");
		goto Done;
	}
	pUdp = xrtNetUdpConnect(pDns->pEngine, &Peer, 0u, NULL, NULL, NULL);
	if(pUdp == NULL)
	{
		xacmeTxtFailure(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp open failed");
		goto Done;
	}
	if(xrtNetUdpSend(pUdp, Query.Data, Query.Size) != XNET_RESULT_OK)
	{
		xacmeTxtFailure(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp send failed");
		goto Done;
	}
	{
		int iAttempt;
		bool bGot = false;
		for(iAttempt = 0; (iAttempt < 2) && !bGot; iAttempt++)
		{
			pPacket = xrtNetUdpReceiveWait(pUdp, INT64_C(2000), NULL);
			if(pPacket == NULL)
			{
				if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) goto Done;
				if(iAttempt == 1)
				{
					xacmeTxtError(
						XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
						"acme dns txt udp receive timeout");
					goto Done;
				}
				if(xrtNetUdpSend(pUdp, Query.Data, Query.Size) != XNET_RESULT_OK)
				{
					xacmeTxtFailure(XERR_IO, XACME_TXT_ERROR_NETWORK,
						"acme dns txt udp resend failed");
					goto Done;
				}
				continue;
			}
			if((xrtNetUdpPacketSize(pPacket) >= 12u) &&
				(xrtNetUdpPacketData(pPacket)[0] ==
					(uint8)(iId >> 8u)) &&
				(xrtNetUdpPacketData(pPacket)[1] == (uint8)iId))
			{
				bGot = true;
			}
			else
			{
				xrtNetUdpPacketDestroy(pPacket);
				pPacket = NULL;
			}
		}
		if(!bGot)
		{
			xacmeTxtError(
				XERR_PROTOCOL, XACME_TXT_ERROR_PROTOCOL,
				"acme dns txt response id mismatch");
			goto Done;
		}
	}

	p = xrtNetUdpPacketData(pPacket);
	iSize = xrtNetUdpPacketSize(pPacket);
	if(!xacmeTxtParseResponse(p, iSize, iId, sOutRecords, iCapacity,
			pOutCount))
	{
		goto Protocol;
	}
	bOk = true;

Done:
	{
		xerror* pPrevious = xrtErrorRef(xrtGetError());
		if(!bOk) *pOutCount = 0u;
		if(pPacket != NULL)
		{
			xrtNetUdpPacketDestroy(pPacket);
		}
		if(pUdp != NULL)
		{
			/* Destroy 只释放调用方引用；不关闭的 UDP 会继续占用引擎。 */
			if(!bOk || !xrtNetUdpClose(pUdp)) (void)xrtNetUdpAbort(pUdp);
			xrtNetUdpDestroy(pUdp);
		}
		xrtBufferUnit(&Query);
		if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	}
	return bOk;

Protocol:
	xacmeTxtError(
		XERR_PROTOCOL, XACME_TXT_ERROR_PROTOCOL,
		"acme dns txt response malformed");
	goto Done;
}

bool xacmeDnsTxtWait(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	cstr sExpected, int64 uTimeoutMs)
{
	double uDeadline = __xrtWaitAfter(uTimeoutMs);
	if((sExpected == NULL) || (sExpected[0] == '\0'))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt wait requires expected value");
		return false;
	}
	if (!__xrtWaitValid(uDeadline)) return false;
	while(!__xrtWaitExpired(uDeadline))
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		size_t iCount = 0u;
		size_t i;
		if(!xacmeDnsTxtQuery(
			pDns, sResolver, iPort, sFqdn, sRecords, 4u, &iCount))
		{
			return false;
		}
		for(i = 0; i < iCount; i++)
		{
			if(strcmp(sRecords[i], sExpected) == 0)
			{
				return true;
			}
		}
		xrtSleep(1000u);
	}
	xacmeTxtError(
		XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
		"acme dns txt wait exhausted");
	return false;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_ali.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_ALI)

#if defined(XACME_FEATURE_DNS_ALI)



#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define XACME_ALI_VERSION "2015-01-09"
#define XACME_ALI_ZONE_MAX 4u
#define XACME_ALI_EMPTY_SHA256 \
	"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

typedef struct xacmednsalicontext {
	xacmehttp Http;
	xmutex Lock;
	char sKeyId[160];
	char sSecret[160];
	char sEndpoint[160];
	/* Exact discovery starts map to verified zones; a parent cannot hide a child. */
	char sZoneStarts[XACME_ALI_ZONE_MAX][256];
	char sZones[XACME_ALI_ZONE_MAX][256];
	size_t iZoneCount;
	size_t iZoneNext;
	/* 本 provider 生命周期内添加的 RecordId 与 DNS-01 值。 */
	xacmednsrecords Records;
	/* An empty id in an uncertain slot is reserved, never free or owned. */
	bool bUncertain[XACME_DNS_RECORD_MAX];
} xacmednsalicontext;

static bool xacmeAliFormat(char* sOut, size_t iCapacity, cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		if((sOut != NULL) && (iCapacity > 0u))
		{
			sOut[0] = '\0';
		}
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "alidns request exceeds buffer capacity");
		return false;
	}
	return true;
}

/* DNS-01 的属主与摘要只进入已签名的 query，拒绝分隔符注入。 */
static bool xacmeAliQueryInputValid(xstrview Fqdn, xstrview Txt)
{
	return xacmeDnsChallengeValid(Fqdn, Txt);
}

static bool xacmeAliUncertainError(void)
{
	xerror* pError;
	/* Parsing or diagnostic allocation failure must keep its original error. */
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "alidns create outcome and ownership are unknown");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeAliPairMatches(xacmednsalicontext* pCtx, size_t i,
	xstrview Fqdn, xstrview Txt)
{
	return strlen(pCtx->Records.sFqdns[i]) == Fqdn.Size &&
		memcmp(pCtx->Records.sFqdns[i], Fqdn.Data, Fqdn.Size) == 0 &&
		strlen(pCtx->Records.sTxts[i]) == Txt.Size &&
		memcmp(pCtx->Records.sTxts[i], Txt.Data, Txt.Size) == 0;
}

static bool xacmeAliPairUncertain(xacmednsalicontext* pCtx, xstrview Fqdn, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(pCtx->bUncertain[i] && xacmeAliPairMatches(pCtx, i, Fqdn, Txt)) return true;
	return false;
}

static size_t xacmeAliOwnedSlot(xacmednsalicontext* pCtx, xstrview Fqdn, xstrview Txt)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(!pCtx->bUncertain[i] && pCtx->Records.sIds[i][0] != '\0' &&
			xacmeAliPairMatches(pCtx, i, Fqdn, Txt)) return i;
	return XACME_DNS_RECORD_MAX;
}

static size_t xacmeAliFreeSlot(xacmednsalicontext* pCtx)
{
	size_t i;
	for(i = 0u; i < pCtx->Records.iCount; i++)
		if(!pCtx->bUncertain[i] && pCtx->Records.sIds[i][0] == '\0') return i;
	return pCtx->Records.iCount;
}

/* Inputs and capacity are checked before the write; recording cannot allocate. */
static void xacmeAliStoreRecord(xacmednsalicontext* pCtx, size_t i, cstr sId,
	xstrview Fqdn, xstrview Txt)
{
	if(i == pCtx->Records.iCount) pCtx->Records.iCount++;
	pCtx->bUncertain[i] = sId == NULL;
	if(sId != NULL) strcpy(pCtx->Records.sIds[i], sId);
	else pCtx->Records.sIds[i][0] = '\0';
	memcpy(pCtx->Records.sFqdns[i], Fqdn.Data, Fqdn.Size);
	pCtx->Records.sFqdns[i][Fqdn.Size] = '\0';
	memcpy(pCtx->Records.sTxts[i], Txt.Data, Txt.Size);
	pCtx->Records.sTxts[i][Txt.Size] = '\0';
}

static void xacmeAliHex(const uint8* pData, size_t iSize, char* sOut)
{
	size_t i;
	for(i = 0; i < iSize; i++)
	{
		sprintf(sOut + i * 2u, "%02x", pData[i]);
	}
	sOut[iSize * 2u] = '\0';
}

static bool xacmeAliUnreserved(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ||
		((c >= 'a') && (c <= 'z')) ||
		((c >= '0') && (c <= '9')) ||
		(c == '-') || (c == '_') || (c == '.') || (c == '~');
}

static int xacmeAliQueryKeyCompare(cstr sA, cstr sB)
{
	const char* pAEnd = strchr(sA, '=');
	const char* pBEnd = strchr(sB, '=');
	size_t iALen = (size_t)(pAEnd - sA);
	size_t iBLen = (size_t)(pBEnd - sB);
	size_t iMin = (iALen < iBLen) ? iALen : iBLen;
	int iOrder = memcmp(sA, sB, iMin);
	if(iOrder != 0)
	{
		return iOrder;
	}
	return (iALen < iBLen) ? -1 : ((iALen > iBLen) ? 1 : 0);
}

bool xacmeDnsAliCanonicalQuery(
	cstr sQuery, char* sOut, size_t iCapacity)
{
	char sParts[1024];
	char* pParts[16];
	size_t iSize;
	size_t iCount = 0u;
	size_t iUsed = 0u;
	size_t i;
	if((sQuery == NULL) || (sOut == NULL) || (iCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "alidns query arguments are invalid");
		return false;
	}
	sOut[0] = '\0';
	iSize = strlen(sQuery);
	if(iSize == 0u)
	{
		return true;
	}
	if(iSize >= sizeof(sParts))
	{
		goto Invalid;
	}
	for(i = 0u; i < iSize; ++i)
	{
		if(!xacmeAliUnreserved(sQuery[i]) &&
			(sQuery[i] != '=') && (sQuery[i] != '&'))
		{
			goto Invalid;
		}
	}
	memcpy(sParts, sQuery, iSize + 1u);
	pParts[iCount++] = sParts;
	for(i = 0u; i < iSize; ++i)
	{
		if(sParts[i] == '&')
		{
			if((iCount >= (sizeof(pParts) / sizeof(pParts[0]))) ||
				(i == 0u) || (sParts[i - 1u] == '&') ||
				(sParts[i + 1u] == '\0'))
			{
				goto Invalid;
			}
			sParts[i] = '\0';
			pParts[iCount++] = sParts + i + 1u;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		char* pEquals = strchr(pParts[i], '=');
		if((pEquals == NULL) || (pEquals == pParts[i]) ||
			(strchr(pEquals + 1u, '=') != NULL))
		{
			goto Invalid;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t j;
		for(j = i + 1u; j < iCount; ++j)
		{
			int iOrder = xacmeAliQueryKeyCompare(pParts[i], pParts[j]);
			if(iOrder == 0)
			{
				goto Invalid;
			}
			if(iOrder > 0)
			{
				char* pSwap = pParts[i];
				pParts[i] = pParts[j];
				pParts[j] = pSwap;
			}
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t iPartSize = strlen(pParts[i]);
		if(iUsed + iPartSize + ((i > 0u) ? 1u : 0u) + 1u >
			iCapacity)
		{
			goto Invalid;
		}
		if(i > 0u)
		{
			sOut[iUsed++] = '&';
		}
		memcpy(sOut + iUsed, pParts[i], iPartSize);
		iUsed += iPartSize;
		sOut[iUsed] = '\0';
	}
	return true;
Invalid:
	sOut[0] = '\0';
	xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT, "alidns query is invalid or too long");
	return false;
}

/* 从完整 FQDN 拆出 RR 与 zone 候选（去最左标签逐级）。 */
static bool xacmeAliSplit(
	cstr sFqdn, char* sRr, size_t iRrCap, char* sZone, size_t iZoneCap)
{
	const char* sDot;
	size_t iLen = strlen(sFqdn);
	if((iLen == 0u) || (iLen >= 512u))
	{
		return false;
	}
	sDot = strchr(sFqdn, '.');
	if((sDot == NULL) || (sDot == sFqdn) ||
		((size_t)(sDot - sFqdn) >= iRrCap))
	{
		return false;
	}
	memcpy(sRr, sFqdn, (size_t)(sDot - sFqdn));
	sRr[sDot - sFqdn] = '\0';
	if((iLen - (size_t)(sDot - sFqdn) - 1u) >= iZoneCap)
	{
		return false;
	}
	strcpy(sZone, sDot + 1);
	return true;
}

/* 固定时间与 nonce 的 ACS3 签名入口，测试可对照独立实现。 */
bool xacmeDnsAliAuthorization(
	cstr sKeyId, cstr sSecret, cstr sEndpoint, cstr sAction,
	cstr sQuery, cstr sNonce, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sDate, size_t iDateCapacity,
	char* sCanonicalQuery, size_t iQueryCapacity)
{
	char sCanonical[2048];
	char sHeaders[512];
	char sStringToSign[160];
	char sHex[72];
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Mac[XRT_SHA256_SIZE];
	xdatetime Now;
	size_t i;
	static const char* sSignedHeaders =
		"content-type;host;x-acs-action;x-acs-content-sha256;"
		"x-acs-date;x-acs-signature-nonce;x-acs-version";

	if((sKeyId == NULL) || (sSecret == NULL) || (sEndpoint == NULL) ||
		(sAction == NULL) || (sQuery == NULL) || (sNonce == NULL) ||
		(sAuth == NULL) || (sDate == NULL) || (sCanonicalQuery == NULL) ||
		(iAuthCapacity == 0u) || (iDateCapacity == 0u) ||
		(iQueryCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "alidns signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sDate[0] = '\0';
	sCanonicalQuery[0] = '\0';
	if(strlen(sNonce) != 32u)
	{
		goto InvalidNonce;
	}
	for(i = 0u; i < 32u; ++i)
	{
		if(!(((sNonce[i] >= '0') && (sNonce[i] <= '9')) ||
			((sNonce[i] >= 'a') && (sNonce[i] <= 'f'))))
		{
			goto InvalidNonce;
		}
	}
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeAliFormat(sDate, iDateCapacity,
			"%04ld-%02d-%02dT%02d:%02d:%02dZ",
			(long)Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute,
			Now.Second) ||
		!xacmeDnsAliCanonicalQuery(sQuery, sCanonicalQuery,
			iQueryCapacity) ||
		!xacmeAliFormat(sHeaders, sizeof(sHeaders),
			"content-type:application/json\n"
			"host:%s\n"
			"x-acs-action:%s\n"
			"x-acs-content-sha256:%s\n"
			"x-acs-date:%s\n"
			"x-acs-signature-nonce:%s\n"
			"x-acs-version:%s\n",
			sEndpoint, sAction, XACME_ALI_EMPTY_SHA256, sDate,
			sNonce, XACME_ALI_VERSION) ||
		!xacmeAliFormat(sCanonical, sizeof(sCanonical),
			"POST\n/\n%s\n%s\n%s\n%s", sCanonicalQuery,
			sHeaders, sSignedHeaders, XACME_ALI_EMPTY_SHA256))
	{
		return false;
	}
	if(!xrtSha256(sCanonical, strlen(sCanonical), Digest))
	{
		return false;
	}
	xacmeAliHex(Digest, sizeof(Digest), sHex);
	if(!xacmeAliFormat(sStringToSign, sizeof(sStringToSign),
		"ACS3-HMAC-SHA256\n%s", sHex))
		return false;
	if(!xrtHmacSha256(
			sSecret, strlen(sSecret),
			sStringToSign, strlen(sStringToSign), Mac))
	{
		xrtSecureZero(Mac, sizeof(Mac));
		return false;
	}
	xacmeAliHex(Mac, sizeof(Mac), sHex);
	xrtSecureZero(Mac, sizeof(Mac));
	return xacmeAliFormat(sAuth, iAuthCapacity,
		"ACS3-HMAC-SHA256 Credential=%s,SignedHeaders=%s,Signature=%s",
		sKeyId, sSignedHeaders, sHex);
InvalidNonce:
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT, "alidns signature nonce is invalid");
	return false;
}

/*
	执行一次 alidns V3 调用（POST + query、空 body）。
	成功返回响应体（xrtMalloc，含状态）；HTTP 非 2xx 时也返回 true
	并把状态/体交给调用方判断（如 zone 试探要看 4xx）。
*/
static bool xacmeAliCall(
	xacmednsalicontext* pCtx, cstr sAction, cstr sQuery,
	uint16* pOutStatus, str* pOutBody)
{
	uint8 uNonce[16];
	char sNonce[33];
	char sDate[24];
	char sCanonicalQuery[1024];
	char sAuth[640];
	char sUrl[1200];
	xacmehttpheader Extra[6];
	xacmehttpresponse R;
	uint32 uAttempt;
	bool bReadOnly = strcmp(sAction, "DescribeDomainRecords") == 0 ||
		strcmp(sAction, "DescribeDomainRecordInfo") == 0;
	/* This RPC has not sent anything yet; older unknown writes live in the ledger. */
	pCtx->Http.bWriteUncertain = false;
	for(uAttempt = 1u; uAttempt <= 3u; ++uAttempt)
	{
		xerrkind Kind;
		if(!xrtSecureRandom(uNonce, sizeof(uNonce)))
		{
			return false;
		}
		xacmeAliHex(uNonce, sizeof(uNonce), sNonce);
		xrtSecureZero(uNonce, sizeof(uNonce));
		if(!xacmeDnsAliAuthorization(pCtx->sKeyId, pCtx->sSecret,
				pCtx->sEndpoint, sAction, sQuery, sNonce, xrtNow(),
				sAuth, sizeof(sAuth), sDate, sizeof(sDate),
				sCanonicalQuery, sizeof(sCanonicalQuery)))
		{
			return false;
		}
		if(sCanonicalQuery[0] != 0u)
		{
			if(!xacmeAliFormat(sUrl, sizeof(sUrl), "https://%s/?%s",
				pCtx->sEndpoint, sCanonicalQuery))
			{
				return false;
			}
		}
		else if(!xacmeAliFormat(sUrl, sizeof(sUrl), "https://%s/",
			pCtx->sEndpoint))
		{
			return false;
		}

		Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
		Extra[1] = (xacmehttpheader){ "x-acs-action", sAction };
		Extra[2] = (xacmehttpheader){ "x-acs-content-sha256",
			XACME_ALI_EMPTY_SHA256 };
		Extra[3] = (xacmehttpheader){ "x-acs-date", sDate };
		Extra[4] = (xacmehttpheader){ "x-acs-signature-nonce", sNonce };
		Extra[5] = (xacmehttpheader){ "x-acs-version",
			XACME_ALI_VERSION };

		if(xacmeHttpExchangeOnceV(
				&pCtx->Http, "POST", sUrl, "application/json",
				(xstrview){ "", 0u }, Extra, 6u, &R))
		{
			*pOutStatus = R.iStatus;
			*pOutBody = R.sBody;
			R.sBody = NULL;
			xacmeHttpResponseUnit(&R);
			return true;
		}
		xacmeHttpResponseUnit(&R);
		if(bReadOnly) {
			const xerror* pError = xrtGetError();
			cstr sDomain = xrtErrorDomain(pError);
			/* RPC reads use POST on the wire but cannot create or delete records. */
			if(xrtErrorCode(pError) == XACME_HTTP_ERROR_UNCERTAIN && sDomain != NULL &&
				strcmp(sDomain, "xrt.acme.http") == 0) {
				const xerror* pCause = xrtErrorCause(pError);
				if(pCause != NULL) xrtSetError(pCause);
				else xrtSetErrorKind(XERR_IO);
			}
			pCtx->Http.bWriteUncertain = false;
		}
		Kind = xrtErrorKind(xrtGetError());
		if((uAttempt == 3u) ||
			pCtx->Http.bWriteUncertain ||
			((Kind != XERR_IO) && (Kind != XERR_TIMEOUT)))
		{
			return false;
		}
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static bool xacmeAliRecordIdCopy(
	xstrview Text, char* sRecordId, size_t iCapacity)
{
	size_t i;
	if((sRecordId == NULL) || (iCapacity == 0u))
	{
		return false;
	}
	sRecordId[0] = '\0';
	if((Text.Data == NULL) || (Text.Size == 0u) ||
		(Text.Size >= iCapacity))
	{
		return false;
	}
	for(i = 0u; i < Text.Size; ++i)
	{
		if(!xacmeAliUnreserved(Text.Data[i]))
		{
			return false;
		}
	}
	memcpy(sRecordId, Text.Data, Text.Size);
	sRecordId[Text.Size] = '\0';
	return true;
}

bool xacmeDnsAliCreateResponseId(
	cstr sBody, char* sRecordId, size_t iCapacity)
{
	xvalue* pRoot;
	xvalue* pId;
	xstrview Text;
	bool bOk = false;
	if((sBody == NULL) || (sRecordId == NULL) || (iCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns create response arguments are invalid");
		return false;
	}
	sRecordId[0] = '\0';
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	/* An error envelope never establishes ownership, even if it carries an id. */
	pId = (pRoot != NULL) && xrtValueIs(pRoot, XVALUE_OBJECT) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code")) == NULL ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("RecordId")) : NULL;
	if((pId != NULL) && xrtValueGetString(pId, &Text))
	{
		bOk = xacmeAliRecordIdCopy(Text, sRecordId, iCapacity);
	}
	if(!bOk && pRoot != NULL)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"alidns create response lacks a usable record id");
	xrtValueRelease(pRoot);
	return bOk;
}

/* 在响应 JSON 里取安全的 RecordId 并存档。 */
static bool xacmeAliSaveRecordId(xacmednsalicontext* pCtx, size_t iSlot, cstr sBody,
	xstrview sFqdn, xstrview sTxt)
{
	char sId[XACME_DNS_RECORD_TEXT_CAP];
	bool bTracked = xacmeDnsAliCreateResponseId(sBody, sId, sizeof(sId));
	if(bTracked) {
		for(size_t i = 0u; i < pCtx->Records.iCount; i++) {
			if(strcmp(pCtx->Records.sIds[i], sId) != 0) continue;
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"alidns create response reuses a tracked record id");
			bTracked = false;
			break;
		}
	}
	xacmeAliStoreRecord(pCtx, iSlot, bTracked ? sId : NULL, sFqdn, sTxt);
	return bTracked ? true : xacmeAliUncertainError();
}

static bool xacmeAliTextEquals(xstrview Text, cstr sExpected)
{
	return Text.Data != NULL && Text.Size == strlen(sExpected) &&
		memcmp(Text.Data, sExpected, Text.Size) == 0;
}

static unsigned char xacmeAliLower(unsigned char c)
{
	return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

/* Both entry points validate the bounded ASCII view before copying it. */
static xstrview xacmeAliCanonicalOwner(xstrview Fqdn, char* sOut)
{
	size_t i;
	for(i = 0u; i < Fqdn.Size; i++) sOut[i] = (char)xacmeAliLower((unsigned char)Fqdn.Data[i]);
	sOut[Fqdn.Size] = '\0';
	return (xstrview){ sOut, Fqdn.Size };
}

static bool xacmeAliDomainEquals(cstr sA, cstr sB)
{
	for(; *sA != '\0' && *sB != '\0'; sA++, sB++)
		if(xacmeAliLower((unsigned char)*sA) != xacmeAliLower((unsigned char)*sB)) return false;
	return *sA == *sB;
}

static bool xacmeAliJsonUInt(const xvalue* pObject, cstr sKey, uint64* pOut)
{
	xvalue* pValue = xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) });
	int64 iSigned;
	if(pValue == NULL) return false;
	if(xrtValueIs(pValue, XVALUE_UINT)) return xrtValueGetUInt(pValue, pOut);
	if(!xrtValueIs(pValue, XVALUE_INT) || !xrtValueGetInt(pValue, &iSigned) || iSigned < 0) return false;
	*pOut = (uint64)iSigned;
	return true;
}

static void xacmeAliResponseFailure(uint16 iStatus, xstrview Code)
{
	xerrkind Kind = XERR_PROTOCOL;
	int32 iCode = XACME_DNS_ERROR_PROTOCOL;
	cstr sMessage = "alidns response is invalid";
	if(iStatus >= 500u) {
		Kind = XERR_IO; iCode = XACME_DNS_ERROR_NETWORK;
		sMessage = "alidns server failure";
	} else if(iStatus == 401u || iStatus == 403u ||
		xacmeAliTextEquals(Code, "Forbidden") || xacmeAliTextEquals(Code, "Forbidden.RAM")) {
		Kind = XERR_PERMISSION; iCode = XACME_DNS_ERROR_CREDENTIAL;
		sMessage = "alidns access denied";
	} else if(iStatus == 429u || xacmeAliTextEquals(Code, "Throttling") ||
		xacmeAliTextEquals(Code, "Throttling.User")) {
		Kind = XERR_AGAIN; iCode = XACME_DNS_ERROR_NETWORK;
		sMessage = "alidns request throttled";
	}
	xrtSetErrorInfo(Kind, "xrt.acme.dns", iCode, sMessage);
}

static xacmednsalizoneoutcome xacmeAliZoneFailure(uint16 iStatus, xstrview Code)
{
	xacmeAliResponseFailure(iStatus, Code);
	return XACME_ALI_ZONE_ERROR;
}

xacmednsalirecordoutcome xacmeDnsAliRecordResponse(
	uint16 iStatus, cstr sBody, cstr sId, cstr sFqdn, cstr sTxt, bool* pEnabled)
{
	xvalue* pRoot;
	xvalue* pCode;
	xstrview Code = { NULL, 0u };
	char sText[320], sRr[256], sDomain[256], sOwner[512];
	bool bEnabled = false;
	xacmednsalirecordoutcome Result = XACME_ALI_RECORD_ERROR;
	if(pEnabled != NULL) *pEnabled = false;
	if(sBody == NULL || sId == NULL || sId[0] == '\0' || sFqdn == NULL || sTxt == NULL) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns record response arguments are invalid");
		return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	if(pRoot == NULL) {
		if(xrtErrorKind(xrtGetError()) != XERR_MEMORY) xacmeAliResponseFailure(iStatus, Code);
		return Result;
	}
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) goto Invalid;
	pCode = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code"));
	if(pCode != NULL) {
		if(!xrtValueGetString(pCode, &Code)) goto Invalid;
		if((iStatus == 400u || iStatus == 404u) &&
			(xacmeAliTextEquals(Code, "DomainRecordNotBelongToUser") ||
			 xacmeAliTextEquals(Code, "InvalidRR.NoExist")) &&
			xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) && sText[0] != '\0' &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("RecordId")) == NULL) {
			Result = XACME_ALI_RECORD_MISSING;
			goto Done;
		}
		goto Invalid;
	}
	if(iStatus < 200u || iStatus >= 300u ||
		!xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) || sText[0] == '\0' ||
		!xacmeDnsJsonPathId(pRoot, "RecordId", sText, sizeof(sText)) || strcmp(sId, sText) != 0 ||
		!xacmeDnsJsonText(pRoot, "Type", sText, sizeof(sText)) || strcmp(sText, "TXT") != 0 ||
		!xacmeDnsJsonText(pRoot, "Line", sText, sizeof(sText)) || strcmp(sText, "default") != 0 ||
		!xacmeDnsJsonText(pRoot, "Value", sText, sizeof(sText)) || strcmp(sTxt, sText) != 0 ||
		!xacmeDnsJsonText(pRoot, "RR", sRr, sizeof(sRr)) || sRr[0] == '\0' ||
		!xacmeDnsJsonText(pRoot, "DomainName", sDomain, sizeof(sDomain)) || sDomain[0] == '\0' ||
		!xacmeAliFormat(sOwner, sizeof(sOwner), "%s.%s", sRr, sDomain) ||
		!xacmeAliDomainEquals(sOwner, sFqdn) ||
		!xacmeDnsJsonText(pRoot, "Status", sText, sizeof(sText))) goto Invalid;
	/* The live service also returns uppercase states; accept only the two
	 * complete spellings of each documented state. */
	if(strcmp(sText, "Enable") == 0 || strcmp(sText, "ENABLE") == 0) bEnabled = true;
	else if(strcmp(sText, "Disable") != 0 && strcmp(sText, "DISABLE") != 0) goto Invalid;
	Result = XACME_ALI_RECORD_FOUND;
	if(pEnabled != NULL) *pEnabled = bEnabled;
	goto Done;
Invalid:
	xacmeAliResponseFailure(iStatus, Code);
Done:
	xrtValueRelease(pRoot);
	return Result;
}

static xacmednsalirecordoutcome xacmeAliReadRecord(
	xacmednsalicontext* pCtx, size_t iSlot, bool* pEnabled)
{
	char sQuery[384];
	str sBody = NULL;
	uint16 iStatus = 0u;
	xacmednsalirecordoutcome Result;
	if(!xacmeAliFormat(sQuery, sizeof(sQuery), "RecordId=%s", pCtx->Records.sIds[iSlot]))
		return XACME_ALI_RECORD_ERROR;
	if(!xacmeAliCall(pCtx, "DescribeDomainRecordInfo", sQuery, &iStatus, &sBody))
		return XACME_ALI_RECORD_ERROR;
	Result = xacmeDnsAliRecordResponse(iStatus, sBody, pCtx->Records.sIds[iSlot],
		pCtx->Records.sFqdns[iSlot], pCtx->Records.sTxts[iSlot], pEnabled);
	xrtFree(sBody);
	return Result;
}

xacmednsalizoneoutcome xacmeDnsAliZoneResponse(uint16 iStatus, cstr sBody, cstr sDomain)
{
	xvalue* pRoot;
	xvalue* pCode;
	xvalue* pRecords;
	xvalue* pList;
	xstrview Code = { NULL, 0u };
	uint64 iTotal, iPage, iSize;
	char sText[320];
	xacmednsalizoneoutcome Result;
	if(sBody == NULL || sDomain == NULL || sDomain[0] == '\0') {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns zone response arguments are invalid");
		return XACME_ALI_ZONE_ERROR;
	}
	pRoot = xrtJsonParse((xstrview){ sBody, strlen(sBody) });
	if(pRoot == NULL) {
		if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return XACME_ALI_ZONE_ERROR;
		return xacmeAliZoneFailure(iStatus, Code);
	}
	Result = XACME_ALI_ZONE_ERROR;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) goto Invalid;
	pCode = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Code"));
	if(pCode != NULL) {
		if(!xrtValueGetString(pCode, &Code)) goto Invalid;
		if((iStatus == 400u || iStatus == 404u) &&
			(xacmeAliTextEquals(Code, "InvalidDomainName.NoExist") ||
			 xacmeAliTextEquals(Code, "DomainNotFound"))) {
			Result = XACME_ALI_ZONE_MISSING;
			goto Done;
		}
		goto Invalid;
	}
	if(iStatus < 200u || iStatus >= 300u) goto Invalid;
	pRecords = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("DomainRecords"));
	pList = pRecords != NULL && xrtValueIs(pRecords, XVALUE_OBJECT) ?
		xrtValueObjectGet(pRecords, XRT_STR_LITERAL("Record")) : NULL;
	if(!xacmeAliJsonUInt(pRoot, "TotalCount", &iTotal) ||
		!xacmeAliJsonUInt(pRoot, "PageNumber", &iPage) || iPage != 1u ||
		!xacmeAliJsonUInt(pRoot, "PageSize", &iSize) || iSize != 1u ||
		!xacmeDnsJsonText(pRoot, "RequestId", sText, sizeof(sText)) || sText[0] == '\0' ||
		pList == NULL || !xrtValueIs(pList, XVALUE_ARRAY) ||
		xrtValueCount(pList) != (iTotal == 0u ? 0u : 1u)) goto Invalid;
	if(iTotal != 0u) {
		xvalue* pItem = xrtValueArrayGet(pList, 0u);
		if(!xacmeDnsJsonText(pItem, "DomainName", sText, sizeof(sText)) ||
			!xacmeAliDomainEquals(sText, sDomain) ||
			!xacmeDnsJsonPathId(pItem, "RecordId", sText, sizeof(sText))) goto Invalid;
	}
	Result = XACME_ALI_ZONE_FOUND;
	goto Done;
Invalid:
	Result = xacmeAliZoneFailure(iStatus, Code);
Done:
	xrtValueRelease(pRoot);
	return Result;
}

/* Return the verified result even when all bounded cache entries are occupied. */
static bool xacmeAliFindZone(xacmednsalicontext* pCtx, cstr sZoneStart, char* sOutZone)
{
	char sZone[256];
	char sStart[256];
	uint16 iStatus = 0u;
	str sBody = NULL;
	char sBodyText[512];
	size_t i;
	for(i = 0u; sZoneStart[i] != '\0'; i++) sStart[i] = (char)xacmeAliLower((unsigned char)sZoneStart[i]);
	sStart[i] = '\0';
	for(i = 0u; i < pCtx->iZoneCount; i++)
		if(strcmp(sStart, pCtx->sZoneStarts[i]) == 0) {
			strcpy(sOutZone, pCtx->sZones[i]); return true;
		}
	strcpy(sZone, sStart);
	for(;;)
	{
		xacmednsalizoneoutcome Result;
		if(!xacmeAliFormat(
			sBodyText, sizeof(sBodyText),
			"DomainName=%s&PageNumber=1&PageSize=1", sZone))
			return false;
		if(!xacmeAliCall(
			pCtx, "DescribeDomainRecords", sBodyText, &iStatus, &sBody))
		{
			return false;
		}
		if(getenv("XACME_DEBUG"))
		{
			printf("[ali-dbg] zone try %s -> %u body=%.140s\n", sZone,
				(unsigned)iStatus, (sBody != NULL) ? sBody : "");
		}
		Result = xacmeDnsAliZoneResponse(iStatus, sBody, sZone);
		xrtFree(sBody);
		sBody = NULL;
		if(Result == XACME_ALI_ZONE_ERROR) return false;
		if(Result == XACME_ALI_ZONE_FOUND)
		{
			size_t iSlot = pCtx->iZoneNext;
			strcpy(sOutZone, sZone);
			strcpy(pCtx->sZoneStarts[iSlot], sStart);
			strcpy(pCtx->sZones[iSlot], sZone);
			pCtx->iZoneNext = (iSlot + 1u) % XACME_ALI_ZONE_MAX;
			if(pCtx->iZoneCount < XACME_ALI_ZONE_MAX) pCtx->iZoneCount++;
			return true;
		}
		{
			char* sDot = strchr(sZone, '.');
			if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL))
			{
				xrtSetErrorInfo(XERR_NOT_FOUND, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
					"alidns zone discovery found no managed domain");
				return false;
			}
			memmove(sZone, sDot + 1, strlen(sDot + 1) + 1u);
		}
	}
}

static bool xacmeAliAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sRr[256];
	char sZone[256];
	char sBody[1024];
	char sTxtText[208];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSlot;
	if(!xacmeAliQueryInputValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"alidns DNS-01 owner or digest contains invalid characters");
		return false;
	}
	sFqdn = xacmeAliCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeAliPairUncertain(pCtx, sFqdn, sTxt)) return xacmeAliUncertainError();
	iSlot = xacmeAliOwnedSlot(pCtx, sFqdn, sTxt);
	if(iSlot < XACME_DNS_RECORD_MAX) {
		bool bEnabled = false;
		xacmednsalirecordoutcome Result = xacmeAliReadRecord(pCtx, iSlot, &bEnabled);
		if(Result == XACME_ALI_RECORD_ERROR) return false;
		if(Result == XACME_ALI_RECORD_FOUND) {
			if(bEnabled) return true;
			xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"alidns owned record is disabled");
			return false;
		}
		/* Only explicit absence of the tracked id frees it for a new create. */
		pCtx->Records.sIds[iSlot][0] = '\0';
		pCtx->Records.sFqdns[iSlot][0] = '\0';
		pCtx->Records.sTxts[iSlot][0] = '\0';
	}
	iSlot = xacmeAliFreeSlot(pCtx);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"alidns record tracking capacity exhausted");
		return false;
	}
	if(!xacmeAliSplit(
		sFqdnText, sRr, sizeof(sRr), sZone, sizeof(sZone)))
	{
		return false;
	}
	if(!xacmeAliFindZone(pCtx, sZone, sZone))
	{
		return false;
	}
	{
		size_t iZoneLen;
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		/* RR = FQDN 去掉 ".zone" 后缀的完整前缀（zone 试探可能
		   剥掉多段，不能只用最左段）。 */
		iZoneLen = strlen(sZone);
		if((iFqdnLen <= iZoneLen + 1u) ||
			(sFqdnText[iFqdnLen - iZoneLen - 1u] != '.'))
		{
			return false;
		}
		iRrLen = iFqdnLen - iZoneLen - 1u;
		if(iRrLen >= sizeof(sRr))
		{
			return false;
		}
		memcpy(sRr, sFqdnText, iRrLen);
		sRr[iRrLen] = '\0';
		memcpy(sTxtText, sTxt.Data, sTxt.Size);
		sTxtText[sTxt.Size] = '\0';
		if(!xacmeAliFormat(
			sBody, sizeof(sBody),
			"DomainName=%s&RR=%s&Type=TXT&Value=%s",
			sZone, sRr, sTxtText))
			return false;
	}
	/* 同名同值并不证明记录所有权；只删除本实例保存的 RecordId。 */
	if(!xacmeAliCall(
		pCtx, "AddDomainRecord", sBody, &iStatus, &sResp))
	{
		if(pCtx->Http.bWriteUncertain)
		{
			xacmeAliStoreRecord(pCtx, iSlot, NULL, sFqdn, sTxt);
			return xacmeAliUncertainError();
		}
		return false;
	}
	if((iStatus < 200u) || (iStatus >= 300u))
	{
		if(getenv("XACME_DEBUG"))
		{
			printf("[ali-dbg] add -> %u body=%.200s\n", (unsigned)iStatus,
				(sResp != NULL) ? sResp : "");
		}
		xrtFree(sResp);
		if(iStatus >= 500u)
		{
			xacmeAliStoreRecord(pCtx, iSlot, NULL, sFqdn, sTxt);
			xrtSetErrorInfo(XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
				"alidns server failure after create request");
			return xacmeAliUncertainError();
		}
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"alidns record creation was rejected");
		return false;
	}
	{
		bool bTracked = xacmeAliSaveRecordId(pCtx, iSlot, sResp, sFqdn, sTxt);
		xrtFree(sResp);
		return bTracked;
	}
}

static bool xacmeAliDeleteRecord(xacmednsalicontext* pCtx, size_t iSlot)
{
	cstr sId = pCtx->Records.sIds[iSlot];
	char sBody[384];
	char sSafeId[XACME_DNS_RECORD_TEXT_CAP];
	char sReturnedId[XACME_DNS_RECORD_TEXT_CAP];
	uint16 iStatus = 0u;
	str sResp = NULL;
	bool bOk;
	if((sId == NULL) ||
		!xacmeAliRecordIdCopy(
			(xstrview){ sId, strlen(sId) }, sSafeId, sizeof(sSafeId)))
	{
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL, "alidns tracked record id is invalid");
		return false;
	}
	{
		xacmednsalirecordoutcome Result = xacmeAliReadRecord(pCtx, iSlot, NULL);
		if(Result != XACME_ALI_RECORD_FOUND) return Result == XACME_ALI_RECORD_MISSING;
	}
	if(!xacmeAliFormat(sBody, sizeof(sBody), "RecordId=%s", sSafeId))
		return false;
	if(!xacmeAliCall(pCtx, "DeleteDomainRecord", sBody,
			&iStatus, &sResp))
	{
		xrtFree(sResp);
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u) {
		bOk = xacmeDnsAliCreateResponseId(sResp, sReturnedId, sizeof(sReturnedId)) &&
			strcmp(sSafeId, sReturnedId) == 0;
	} else {
		/* A precise account-scoped absence also handles a concurrent delete. */
		bOk = xacmeDnsAliRecordResponse(iStatus, sResp, sSafeId,
			pCtx->Records.sFqdns[iSlot], pCtx->Records.sTxts[iSlot], NULL) == XACME_ALI_RECORD_MISSING;
	}
	xrtFree(sResp);
	if(!bOk && iStatus >= 200u && iStatus < 300u && xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL, "alidns record deletion failed");
	return bOk;
}

static bool xacmeAliRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsalicontext* pCtx =
		(xacmednsalicontext*)pProvider->pContext;
	char sFqdnText[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"alidns DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAliCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeAliPairUncertain(pCtx, sFqdn, sTxt)) return xacmeAliUncertainError();
	/* Keep the caller's tuple bound to its slot even in an older duplicate-id ledger. */
	for(size_t i = 0u; i < pCtx->Records.iCount; i++) {
		if(pCtx->Records.sIds[i][0] == '\0' || !xacmeAliPairMatches(pCtx, i, sFqdn, sTxt)) continue;
		if(!xacmeAliDeleteRecord(pCtx, i)) return false;
		pCtx->Records.sIds[i][0] = '\0';
		pCtx->Records.sFqdns[i][0] = '\0';
		pCtx->Records.sTxts[i][0] = '\0';
	}
	return true;
}

static bool xacmeAliAdd(xacmednsprovider* pProvider, xstrview Fqdn, xstrview Txt)
{
	xacmednsalicontext* pCtx = (xacmednsalicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAliAddLocked(pProvider, Fqdn, Txt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeAliRemove(xacmednsprovider* pProvider, xstrview Fqdn, xstrview Txt)
{
	xacmednsalicontext* pCtx = (xacmednsalicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAliRemoveLocked(pProvider, Fqdn, Txt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

void xrtAcmeDnsAliConfigInit(xacmednaliconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKeyId = NULL;
	pConfig->sAccessKeySecret = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
{
	xacmednsalicontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKeyId == NULL) ||
		(pConfig->sAccessKeySecret == NULL) ||
		(pConfig->sAccessKeyId[0] == '\0') ||
		(pConfig->sAccessKeySecret[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_ali requires access key id and secret");
		return false;
	}
	if(strlen(pConfig->sAccessKeyId) >= sizeof(pCtx->sKeyId) ||
		strlen(pConfig->sAccessKeySecret) >= sizeof(pCtx->sSecret) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"alidns credentials or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednsalicontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtFree(pCtx);
		return false;
	}
	strcpy(pCtx->sKeyId, pConfig->sAccessKeyId);
	strcpy(pCtx->sSecret, pConfig->sAccessKeySecret);
	strcpy(pCtx->sEndpoint, (pConfig->sEndpoint != NULL) ?
		pConfig->sEndpoint : "alidns.aliyuncs.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		bool bReady = xacmeHttpUnit(&pCtx->Http);
		(void)xrtMutexUnit(&pCtx->Lock);
		if(bReady)
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
		return false;
	}
	pProvider->sId = "ali";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeAliAdd;
	pProvider->Remove = xacmeAliRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsAliProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednsalicontext* pCtx =
			(xacmednsalicontext*)pProvider->pContext;
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_core.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_CORE)

#include <string.h>

#if defined(XACME_FEATURE_ACME_CORE)

void xrtAcmeAccountConfigInit(xacmeaccountconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme",
			XACME_ERROR_ARGUMENT,
			"acme account config init requires config"
		);
		return;
	}
	pConfig->sDirectoryUrl = NULL;
	pConfig->sAccountKeyPem = NULL;
	pConfig->Eab.sKid = NULL;
	pConfig->Eab.sHmac = NULL;
	pConfig->sContactEmail = NULL;
}

void xrtAcmeGrantUnit(xacmeissuegrant* pGrant)
{
	if(pGrant == NULL)
	{
		return;
	}
	xrtFree(pGrant->sFullchainPem);
	if(pGrant->sKeyPem != NULL)
		xrtSecureZero(pGrant->sKeyPem, strlen(pGrant->sKeyPem));
	xrtFree(pGrant->sKeyPem);
	memset(pGrant, 0, sizeof(*pGrant));
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_store.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_STORE)

#if defined(XACME_FEATURE_ACME_STORE)


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <errno.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif

#define XACME_STORE_GENERATION_PREFIX ".grant-"
#define XACME_STORE_GENERATION_SIZE 23u
#define XACME_STORE_POINTER_LIMIT 64u
#define XACME_STORE_CERT_LIMIT (16u * 1024u * 1024u)
#define XACME_STORE_KEY_LIMIT (1024u * 1024u)
#define XACME_STORE_META_LIMIT 1200u

static void xacmeStoreError(xerrkind Kind, xacmestoreerror Code, cstr s)
{
	xrtSetErrorInfo(Kind, "xrt.acme.store", (int32)Code, s);
}

static bool xacmeStoreFormat(char* sOut, size_t iCapacity, cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store path or metadata exceeds capacity");
		return false;
	}
	return true;
}

/* 存储目录名只接受 ASCII DNS 名与开头的通配符，不接受路径语法。 */
static bool xacmeStoreDomainValid(cstr sDomain)
{
	size_t i, iSize, iLabel = 0u;
	if(sDomain == NULL)
		return false;
	iSize = strlen(sDomain);
	if((iSize == 0u) || (iSize > 253u))
		return false;
	i = ((iSize > 2u) && (sDomain[0] == '*') &&
		(sDomain[1] == '.')) ? 2u : 0u;
	for(; i < iSize; i++)
	{
		unsigned char c = (unsigned char)sDomain[i];
		bool bAlphaNum = ((c >= 'A') && (c <= 'Z')) ||
			((c >= 'a') && (c <= 'z')) ||
			((c >= '0') && (c <= '9'));
		if(c == '.')
		{
			if((iLabel == 0u) || (sDomain[i - 1u] == '-'))
				return false;
			iLabel = 0u;
		}
		else if(bAlphaNum || ((c == '-') && (iLabel != 0u)))
		{
			if(++iLabel > 63u)
				return false;
		}
		else
		{
			return false;
		}
	}
	return (iLabel != 0u) && (sDomain[iSize - 1u] != '-');
}

static bool xacmeStoreCertPath(char* sOut, size_t iCapacity,
	cstr sRoot, cstr sDomain, cstr sFile)
{
	char sMapped[256];
	cstr sDiskDomain = sDomain;
	if((sRoot == NULL) || (sRoot[0] == 0) ||
		!xacmeStoreDomainValid(sDomain))
	{
		xacmeStoreError(XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store requires a root and a valid DNS domain");
		return false;
	}
	/* '*' 不能用于 Windows 路径；百分号不会与合法 DNS 标签碰撞。 */
	if(sDomain[0] == '*')
	{
		if(!xacmeStoreFormat(sMapped, sizeof(sMapped),
			"%%2A%s", sDomain + 1u))
			return false;
		sDiskDomain = sMapped;
	}
	if(sFile == NULL)
		return xacmeStoreFormat(sOut, iCapacity,
			"%s/certs/%s", sRoot, sDiskDomain);
	return xacmeStoreFormat(sOut, iCapacity, "%s/certs/%s/%s",
		sRoot, sDiskDomain, sFile);
}

static bool xacmeStoreGenerationNameValid(cstr sName, size_t iSize)
{
	size_t i;
	if((iSize != XACME_STORE_GENERATION_SIZE) ||
		(memcmp(sName, XACME_STORE_GENERATION_PREFIX, 7u) != 0))
		return false;
	for(i = 7u; i < iSize; i++)
	{
		if(!((sName[i] >= '0' && sName[i] <= '9') ||
			(sName[i] >= 'a' && sName[i] <= 'f')))
			return false;
	}
	return true;
}

/* 并发重命名在 Windows 与部分映射文件系统上可短暂报告未找到。 */
static bool xacmeStoreRetryPointerRead(size_t iAttempt)
{
	xerrkind Kind = xrtErrorKind(xrtGetError());
	if(iAttempt < 7u &&
		(Kind == XERR_NOT_FOUND || Kind == XERR_PERMISSION ||
		Kind == XERR_AGAIN))
	{
		xrtClearError();
		xrtSleep(1u);
		return true;
	}
	return false;
}

/* current 缺失时读取直属布局；存在时只接受受限的版本目录名。 */
static bool xacmeStoreResolveAtBase(char* sOut, size_t iCapacity,
	cstr sBase, bool* pbVersioned)
{
	char sPointer[1024];
	size_t iSize = 0u;
	size_t iAttempt;
	bytes pName;
	xfileinfo Info;
	if(!xacmeStoreFormat(sPointer, sizeof(sPointer), "%s/current", sBase))
		return false;
	for(iAttempt = 0u; ; iAttempt++)
	{
		if(!xrtPathStat(sPointer, false, &Info))
		{
			if(xacmeStoreRetryPointerRead(iAttempt))
				continue;
			if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
				return false;
			xrtClearError();
			*pbVersioned = false;
			return xacmeStoreFormat(sOut, iCapacity, "%s", sBase);
		}
		if(Info.Type != XFILE_TYPE_FILE)
		{
			xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
				"acme store current must be a regular file");
			return false;
		}
		pName = xrtFileReadAllLimit(sPointer,
			XACME_STORE_POINTER_LIMIT, &iSize);
		if(pName != NULL)
			break;
		if(!xacmeStoreRetryPointerRead(iAttempt))
			return false;
	}
	if((iSize != XACME_STORE_GENERATION_SIZE + 1u) ||
		(pName[XACME_STORE_GENERATION_SIZE] != '\n') ||
		!xacmeStoreGenerationNameValid((cstr)pName,
			XACME_STORE_GENERATION_SIZE))
	{
		xrtFree(pName);
		xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store current generation is invalid");
		return false;
	}
	*pbVersioned = true;
	if(!xacmeStoreFormat(sOut, iCapacity, "%s/%.*s", sBase,
		(int)XACME_STORE_GENERATION_SIZE, (cstr)pName))
	{
		xrtFree(pName);
		return false;
	}
	xrtFree(pName);
	return true;
}

/* 新的 %2A 目录优先；POSIX 旧版 '*.domain' 目录仍可读取。 */
static bool xacmeStoreResolveBase(char* sOut, size_t iCapacity,
	cstr sRoot, cstr sDomain, bool* pbVersioned)
{
	char sMapped[1024];
	if(!xacmeStoreCertPath(sMapped, sizeof(sMapped),
		sRoot, sDomain, NULL) ||
		!xacmeStoreResolveAtBase(sOut, iCapacity, sMapped, pbVersioned))
		return false;
	#if !defined(_WIN32) && !defined(_WIN64)
		if((sDomain[0] == '*') && !*pbVersioned)
		{
			char sCert[1024];
			char sLegacy[1024];
			xfileinfo Info;
			if(!xacmeStoreFormat(sCert, sizeof(sCert),
				"%s/fullchain.pem", sMapped))
				return false;
			if(xrtPathStat(sCert, false, &Info))
			{
				if(Info.Type != XFILE_TYPE_FILE)
				{
					xacmeStoreError(XERR_PROTOCOL,
						XACME_STORE_ERROR_PARSE,
						"acme store mapped certificate is not a regular file");
					return false;
				}
				return true;
			}
			if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
				return false;
			xrtClearError();
			if(!xacmeStoreFormat(sLegacy, sizeof(sLegacy),
				"%s/certs/%s", sRoot, sDomain))
				return false;
			if(!xrtPathStat(sLegacy, false, &Info))
			{
				if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
					return false;
				xrtClearError();
				return true;
			}
			if(Info.Type != XFILE_TYPE_DIRECTORY)
			{
				xacmeStoreError(XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
					"acme store legacy wildcard path is not a directory");
				return false;
			}
			return xacmeStoreResolveAtBase(sOut, iCapacity,
				sLegacy, pbVersioned);
		}
	#endif
	return true;
}

/* directory URL → 16 字符十六进制目录名。 */
static bool xacmeStoreCaDir(
	cstr sDirectoryUrl, char* sOut /* >= 17 */)
{
	uint8 Digest[XRT_SHA256_SIZE];
	size_t i;
	if(!xrtSha256(sDirectoryUrl, strlen(sDirectoryUrl), Digest))
	{
		return false;
	}
	for(i = 0; i < 8u; i++)
	{
		sprintf(sOut + i * 2u, "%02x", Digest[i]);
	}
	sOut[16] = '\0';
	return true;
}

static bool xacmeStoreWriteAtomicText(cstr sPath, cstr sText)
{
	if(!xrtFileWriteAtomic(
			sPath, (xbytesview){ (const uint8*)sText, strlen(sText) }))
	{
		return false;
	}
	return true;
}

/* POSIX rename 只保证可见性；目录项还需要显式落盘。 */
static bool xacmeStoreSyncDirectory(cstr sPath)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		int iFlags = O_RDONLY;
		int iFd;
		int iResult;
		#ifdef O_DIRECTORY
			iFlags |= O_DIRECTORY;
		#endif
		#ifdef O_CLOEXEC
			iFlags |= O_CLOEXEC;
		#endif
		do {
			iFd = open(sPath, iFlags);
		} while((iFd < 0) && (errno == EINTR));
		if(iFd < 0)
		{
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store could not open directory for sync");
			return false;
		}
		do {
			iResult = fsync(iFd);
		} while((iResult != 0) && (errno == EINTR));
		if(close(iFd) != 0)
			iResult = -1;
		if(iResult != 0)
		{
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store directory sync failed");
			return false;
		}
	#else
		(void)sPath;
	#endif
	return true;
}

/* 首次签发时 root/certs/domain 可能均为新目录，逐级持久化目录项。 */
static bool xacmeStoreSyncAncestors(cstr sDirectory)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		char sPath[1024];
		if(!xacmeStoreFormat(sPath, sizeof(sPath), "%s", sDirectory))
			return false;
		for(;;)
		{
			char* sSlash;
			if(!xacmeStoreSyncDirectory(sPath))
				return false;
			if((strcmp(sPath, ".") == 0) || (strcmp(sPath, "/") == 0))
				break;
			sSlash = strrchr(sPath, '/');
			if(sSlash == NULL)
			{
				strcpy(sPath, ".");
			}
			else if(sSlash == sPath)
			{
				sPath[1] = '\0';
			}
			else
			{
				*sSlash = '\0';
			}
		}
	#else
		(void)sDirectory;
	#endif
	return true;
}

/* 临时文件从创建起就是 POSIX 0600，写完后才发布私钥。 */
static bool xacmeStoreWriteAtomicKey(cstr sPath, cstr sText)
{
	str sDirectory = xrtPathParent(sPath);
	str sTemporary = NULL;
	xfile File;
	bool bOk;
	if(sDirectory == NULL)
	{
		return false;
	}
	File = xrtFileTemp(sDirectory, ".xacme-key-", ".tmp", &sTemporary);
	xrtFree(sDirectory);
	if(File == NULL)
	{
		return false;
	}
	bOk = xrtWriteFull(File, sText, strlen(sText), NULL) && xrtFlush(File);
	if(!xrtClose(File))
	{
		bOk = false;
	}
	if(bOk)
	{
		bOk = xrtPathRename(sTemporary, sPath, true);
	}
	if(!bOk)
	{
		(void)xrtFileDelete(sTemporary);
	}
	xrtFree(sTemporary);
	return bOk;
}

/* current 只含版本名。Windows 避开 ReplaceFile 的 1176 中间态；
	MoveFileEx 的失败若已尝试发布则保留版本供调用方核对。 */
static bool xacmeStoreWritePointer(cstr sPath, cstr sText,
	bool* pbPublishAttempted)
{
	*pbPublishAttempted = false;
	#if defined(_WIN32) || defined(_WIN64)
		str sDirectory = xrtPathParent(sPath);
		str sTemporary = NULL;
		xfile File;
		bool bOk;
		size_t iAttempt;
		uint64 iDelay = 1u;
		xerror* pRenameError = NULL;
		if(sDirectory == NULL)
			return false;
		File = xrtFileTemp(sDirectory,
			".xacme-current-", ".tmp", &sTemporary);
		xrtFree(sDirectory);
		if(File == NULL)
			return false;
		bOk = xrtWriteFull(File, sText, strlen(sText), NULL) &&
			xrtFlush(File);
		if(!xrtClose(File))
			bOk = false;
		if(bOk)
		{
			*pbPublishAttempted = true;
			/* 读者或文件扫描器的短暂占用可能超过一轮线程调度。
			 * 仅重试原生占用错误，累计等待最多 447 毫秒。 */
			for(iAttempt = 0u; iAttempt < 32u; iAttempt++)
			{
				int64 iSystemCode;
				if(xrtPathRename(sTemporary, sPath, true))
				{
					xrtErrorFree(pRenameError);
					xrtFree(sTemporary);
					return true;
				}
				if(pRenameError == NULL)
					pRenameError = xrtErrorRef(xrtGetError());
				iSystemCode = xrtErrorSystemCode(xrtGetError());
				if(iAttempt == 31u ||
					(xrtErrorKind(xrtGetError()) != XERR_IO &&
						xrtErrorKind(xrtGetError()) != XERR_PERMISSION &&
						xrtErrorKind(xrtGetError()) != XERR_AGAIN) ||
					(iSystemCode != ERROR_ACCESS_DENIED &&
						iSystemCode != ERROR_SHARING_VIOLATION &&
						iSystemCode != ERROR_LOCK_VIOLATION))
					break;
				xrtClearError();
				xrtSleep(iDelay);
				if(iDelay < 16u)
					iDelay *= 2u;
			}
		}
		{
			xerror* pCause = xrtTakeError();
			if(pRenameError != NULL)
			{
				xrtErrorFree(pCause);
				pCause = pRenameError;
			}
			(void)xrtFileDelete(sTemporary);
			xrtClearError();
			if(pCause != NULL)
				xrtSetErrorTake(pCause);
		}
		xrtFree(sTemporary);
		return false;
	#else
		return xacmeStoreWriteAtomicText(sPath, sText);
	#endif
}

static void xacmeStoreReleasePointerLock(xfile File)
{
	xerror* pCause = xrtTakeError();
	(void)xrtFileUnlock(File);
	(void)xrtClose(File);
	xrtClearError();
	if(pCause != NULL)
		xrtSetErrorTake(pCause);
}

static str xacmeStoreReadText(cstr sPath, size_t iLimit,
	bool bSecret, cstr sMissing)
{
	size_t iSize = 0u;
	bytes pBytes = xrtFileReadAllLimit(sPath, iLimit, &iSize);
	str sText;
	if(pBytes == NULL)
	{
		if((sMissing != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND))
			xacmeStoreError(XERR_NOT_FOUND,
				XACME_STORE_ERROR_NOT_FOUND, sMissing);
		return NULL;
	}
	sText = (str)xrtMalloc(iSize + 1u);
	if(sText != NULL)
	{
		memcpy(sText, pBytes, iSize);
		sText[iSize] = '\0';
	}
	if(bSecret)
		xrtSecureZero(pBytes, iSize);
	xrtFree(pBytes);
	return sText;
}

bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem)
{
	char sCa[17];
	char sDirectory[1024];
	char sPath[1024];
	if((sRoot == NULL) || (sRoot[0] == 0) ||
		(sDirectoryUrl == NULL) || (sAccountPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save account requires root, url and pem");
		return false;
	}
	if(strlen(sAccountPem) > XACME_STORE_KEY_LIMIT)
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store account exceeds read limit");
		return false;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return false;
	}
	if(!xacmeStoreFormat(sDirectory, sizeof(sDirectory),
		"%s/accounts/%s", sRoot, sCa))
		return false;
	if(!xrtDirCreateAll(sDirectory))
	{
		return false;
	}
	if(!xacmeStoreSyncAncestors(sDirectory))
		return false;
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/accounts/%s/account.pem", sRoot, sCa))
		return false;
	if(!xacmeStoreWriteAtomicKey(sPath, sAccountPem))
	{
		return false;
	}
	/* 文件已可见；失败时交由调用方重新读取以核对提交状态。 */
	return xacmeStoreSyncDirectory(sDirectory);
}

str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl)
{
	char sCa[17];
	char sPath[1024];
	if((sRoot == NULL) || (sRoot[0] == 0) || (sDirectoryUrl == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load account requires root and url");
		return NULL;
	}
	if(!xacmeStoreCaDir(sDirectoryUrl, sCa))
	{
		return NULL;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/accounts/%s/account.pem", sRoot, sCa))
		return NULL;
	return xacmeStoreReadText(sPath, XACME_STORE_KEY_LIMIT, true,
		"acme store account not found");
}

bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl)
{
	char sPath[1024];
	char sPointer[1024];
	char sMeta[1200];
	xfileinfo Info;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (sChainPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save cert requires root, domain and chain");
		return false;
	}
	if(strlen(sChainPem) > XACME_STORE_CERT_LIMIT)
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store certificate exceeds read limit");
		return false;
	}
	if(!xacmeStoreCertPath(sPath, sizeof(sPath),
		sRoot, sPrimaryDomain, NULL))
		return false;
	if(!xacmeStoreFormat(sPointer, sizeof(sPointer), "%s/current", sPath))
		return false;
	if(xrtPathStat(sPointer, false, &Info))
	{
		xacmeStoreError(XERR_STATE, XACME_STORE_ERROR_ARGUMENT,
			"acme store standalone cert cannot replace a committed grant");
		return false;
	}
	if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
		return false;
	xrtClearError();
	if(!xrtDirCreateAll(sPath))
	{
		return false;
	}
	if(!xacmeStoreCertPath(sPath, sizeof(sPath),
		sRoot, sPrimaryDomain, "fullchain.pem"))
		return false;
	if(!xacmeStoreWriteAtomicText(sPath, sChainPem))
	{
		return false;
	}
	if(!xacmeStoreFormat(sMeta, sizeof(sMeta), "directory=%s\n",
		(sDirectoryUrl != NULL) ? sDirectoryUrl : "") ||
		!xacmeStoreCertPath(sPath, sizeof(sPath),
			sRoot, sPrimaryDomain, "meta.txt"))
		return false;
	if(!xacmeStoreWriteAtomicText(sPath, sMeta))
	{
		return false;
	}
	return true;
}

str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	char sBase[1024];
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load cert requires root and domain");
		return NULL;
	}
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/fullchain.pem", sBase))
		return NULL;
	(void)bVersioned;
	return xacmeStoreReadText(sPath, XACME_STORE_CERT_LIMIT, false,
		"acme store cert not found");
}

str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain)
{
	char sPath[1024];
	char sBase[1024];
	size_t iSize = 0u;
	bytes pBytes;
	str sCa = NULL;
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL))
	{
		return NULL;
	}
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath), "%s/meta.txt", sBase))
		return NULL;
	(void)bVersioned;
	pBytes = xrtFileReadAllLimit(sPath, XACME_STORE_META_LIMIT, &iSize);
	if((pBytes == NULL) || (iSize <= 11u) ||
		(memcmp(pBytes, "directory=", 10u) != 0))
	{
		xrtFree(pBytes);
		return NULL;
	}
	/* 去掉末尾换行。 */
	while((iSize > 0u) &&
		((pBytes[iSize - 1u] == '\n') || (pBytes[iSize - 1u] == '\r')))
	{
		iSize--;
	}
	sCa = (str)xrtMalloc(iSize - 10u + 1u);
	if(sCa != NULL)
	{
		memcpy(sCa, pBytes + 10u, iSize - 10u);
		sCa[iSize - 10u] = '\0';
	}
	xrtFree(pBytes);
	return sCa;
}

static void xacmeStoreDiscardGeneration(cstr sGeneration)
{
	static const cstr sFiles[] = {
		"fullchain.pem", "key.pem", "meta.txt"
	};
	xerror* pCause = xrtTakeError();
	char sPath[1024];
	size_t i;
	for(i = 0u; i < sizeof(sFiles) / sizeof(sFiles[0]); i++)
	{
		if(xacmeStoreFormat(sPath, sizeof(sPath), "%s/%s",
			sGeneration, sFiles[i]))
			(void)xrtFileDelete(sPath);
	}
	(void)xrtDirRemove(sGeneration);
	xrtClearError();
	if(pCause != NULL)
		xrtSetErrorTake(pCause);
}

bool xrtAcmeStoreSaveGrant(
	cstr sRoot, cstr sPrimaryDomain, const xacmeissuegrant* pGrant,
	cstr sDirectoryUrl)
{
	char sBase[1024];
	char sProbe[1024];
	char sPath[1024];
	char sPointer[1024];
	char sLockPath[1024];
	char sPointerText[XACME_STORE_GENERATION_SIZE + 2u];
	char sMeta[1200];
	str sGeneration;
	xfile Lock;
	const char* sName;
	bool bPublishAttempted;
	size_t iSize;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pGrant == NULL) ||
		(pGrant->sFullchainPem == NULL) || (pGrant->sKeyPem == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store save grant requires root, domain, chain and key");
		return false;
	}
	if((strlen(pGrant->sFullchainPem) > XACME_STORE_CERT_LIMIT) ||
		(strlen(pGrant->sKeyPem) > XACME_STORE_KEY_LIMIT))
	{
		xacmeStoreError(XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
			"acme store grant exceeds read limits");
		return false;
	}
	if(!xacmeStoreCertPath(sBase, sizeof(sBase),
		sRoot, sPrimaryDomain, NULL) ||
		!xacmeStoreFormat(sProbe, sizeof(sProbe),
			"%s/.grant-0000000000000000/fullchain.pem", sBase) ||
		!xacmeStoreFormat(sPointer, sizeof(sPointer),
			"%s/current", sBase) ||
		!xacmeStoreFormat(sLockPath, sizeof(sLockPath),
			"%s/current.lock", sBase) ||
		!xacmeStoreFormat(sMeta, sizeof(sMeta), "directory=%s\n",
			(sDirectoryUrl != NULL) ? sDirectoryUrl : ""))
		return false;
	if(!xrtDirCreateAll(sBase))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant directory creation failed");
		return false;
	}
	sGeneration = xrtDirTemp(sBase, XACME_STORE_GENERATION_PREFIX, "");
	if(sGeneration == NULL)
		return false;
	iSize = strlen(sGeneration);
	sName = (iSize > XACME_STORE_GENERATION_SIZE) ?
		sGeneration + iSize - XACME_STORE_GENERATION_SIZE : NULL;
	if((sName == NULL) ||
		(sName[-1] != '/' && sName[-1] != '\\') ||
		!xacmeStoreGenerationNameValid(sName,
			XACME_STORE_GENERATION_SIZE))
	{
		xacmeStoreError(XERR_INTERNAL, XACME_STORE_ERROR_PARSE,
			"acme store temporary generation name is invalid");
		goto Fail;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath),
		"%s/fullchain.pem", sGeneration) ||
		!xacmeStoreWriteAtomicText(sPath, pGrant->sFullchainPem) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/key.pem", sGeneration) ||
		!xacmeStoreWriteAtomicKey(sPath, pGrant->sKeyPem) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/meta.txt", sGeneration) ||
		!xacmeStoreWriteAtomicText(sPath, sMeta))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant staging failed");
		goto Fail;
	}
	if(!xacmeStoreSyncDirectory(sGeneration) ||
		!xacmeStoreSyncAncestors(sBase))
		goto Fail;
	/* 同域的跨进程写者串行发布；读者始终无锁读取固定版本。 */
	Lock = xrtOpen(sLockPath, XFILE_READ | XFILE_WRITE | XFILE_CREATE);
	if(Lock == NULL)
		goto Fail;
	if(!xrtFileLock(Lock, XFILE_LOCK_EXCLUSIVE, true))
	{
		xerror* pCause = xrtTakeError();
		(void)xrtClose(Lock);
		xrtClearError();
		if(pCause != NULL)
			xrtSetErrorTake(pCause);
		goto Fail;
	}
	memcpy(sPointerText, sName, XACME_STORE_GENERATION_SIZE);
	sPointerText[XACME_STORE_GENERATION_SIZE] = '\n';
	sPointerText[XACME_STORE_GENERATION_SIZE + 1u] = '\0';
	if(!xacmeStoreWritePointer(sPointer, sPointerText,
		&bPublishAttempted))
	{
		if(xrtGetError() == NULL)
			xacmeStoreError(XERR_IO, XACME_STORE_ERROR_IO,
				"acme store grant commit failed");
		xacmeStoreReleasePointerLock(Lock);
		/* Windows 重命名失败的可见性可能不确定，不能删除它可能引用的版本。 */
		if(bPublishAttempted)
		{
			xrtFree(sGeneration);
			return false;
		}
		goto Fail;
	}
	if(!xrtFileUnlock(Lock))
	{
		xerror* pCause = xrtTakeError();
		(void)xrtClose(Lock);
		xrtClearError();
		if(pCause != NULL)
			xrtSetErrorTake(pCause);
		xrtFree(sGeneration);
		return false;
	}
	if(!xrtClose(Lock))
	{
		xrtFree(sGeneration);
		return false;
	}
	/* 指针已可见；即使目录 fsync 失败也不得删除它所指向的版本。 */
	if(!xacmeStoreSyncDirectory(sBase))
	{
		xrtFree(sGeneration);
		return false;
	}
	xrtFree(sGeneration);
	return true;
Fail:
	xacmeStoreDiscardGeneration(sGeneration);
	xrtFree(sGeneration);
	return false;
}

bool xrtAcmeStoreLoadGrant(
	cstr sRoot, cstr sPrimaryDomain, xacmeissuegrant* pOut)
{
	char sPath[1024];
	char sBase[1024];
	bool bVersioned;
	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pOut == NULL))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store load grant requires root, domain and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	if(!xacmeStoreResolveBase(sBase, sizeof(sBase), sRoot,
		sPrimaryDomain, &bVersioned) ||
		!xacmeStoreFormat(sPath, sizeof(sPath),
			"%s/fullchain.pem", sBase))
		return false;
	(void)bVersioned;
	pOut->sFullchainPem = xacmeStoreReadText(sPath,
		XACME_STORE_CERT_LIMIT, false, "acme store cert not found");
	if(pOut->sFullchainPem == NULL)
	{
		return false;
	}
	if(!xacmeStoreFormat(sPath, sizeof(sPath), "%s/key.pem", sBase))
	{
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	pOut->sKeyPem = xacmeStoreReadText(sPath,
		XACME_STORE_KEY_LIMIT, true, "acme store key not found");
	if(pOut->sKeyPem == NULL)
	{
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	return true;
}

bool xrtAcmeStoreListDomains(
	cstr sRoot, char (*sOutDomains)[256],
	size_t iCapacity, size_t* pOutCount)
{
	char sPath[1024];
	xdir Dir;
	if((sRoot == NULL) || (sOutDomains == NULL) || (pOutCount == NULL) ||
		(iCapacity == 0u))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store list requires root, output and capacity");
		return false;
	}
	*pOutCount = 0u;
	if((sRoot[0] == 0) ||
		!xacmeStoreFormat(sPath, sizeof(sPath), "%s/certs", sRoot))
		return false;
	Dir = xrtDirOpen(sPath, XDIR_STAT);
	if(!Dir)
	{
		if(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND)
			xacmeStoreError(XERR_NOT_FOUND, XACME_STORE_ERROR_NOT_FOUND,
				"acme store certs dir not found");
		return false;
	}
	for(;;)
	{
		xdirentry Entry;
		xdirnext eNext = xrtDirNext(Dir, &Entry);
		char sDomain[256];
		size_t iDomainSize;
		size_t i;
		if(eNext == XDIR_NEXT_END)
		{
			break;
		}
		if(eNext != XDIR_NEXT_ITEM)
		{
			xerror* pCause = xrtTakeError();
			(void)xrtDirClose(Dir);
			xrtClearError();
			if(pCause != NULL) xrtSetErrorTake(pCause);
			return false;
		}
		if((Entry.Name.Size == 0u) || (Entry.Name.Size >= 256u) ||
			(Entry.Info.Type != XFILE_TYPE_DIRECTORY))
		{
			continue;
		}
		if((Entry.Name.Size >= 4u) &&
			(memcmp(Entry.Name.Data, "%2A.", 4u) == 0))
		{
			iDomainSize = Entry.Name.Size - 2u;
			sDomain[0] = '*';
			memcpy(sDomain + 1u,
				Entry.Name.Data + 3u, Entry.Name.Size - 3u);
		}
		else
		{
			iDomainSize = Entry.Name.Size;
			memcpy(sDomain, Entry.Name.Data, iDomainSize);
		}
		sDomain[iDomainSize] = '\0';
		if((strlen(sDomain) != iDomainSize) ||
			!xacmeStoreDomainValid(sDomain))
			continue;
		for(i = 0u; i < *pOutCount; i++)
			if(strcmp(sOutDomains[i], sDomain) == 0)
				break;
		if(i < *pOutCount)
			continue;
		if(*pOutCount >= iCapacity)
		{
			xrtDirClose(Dir);
			xacmeStoreError(
				XERR_RANGE, XACME_STORE_ERROR_ARGUMENT,
				"acme store list capacity exhausted");
			return false;
		}
		memcpy(sOutDomains[*pOutCount], sDomain, iDomainSize + 1u);
		(*pOutCount)++;
	}
	xrtDirClose(Dir);
	return true;
}

bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed)
{
	str sChain;
	xpemcursor Pem;
	xpemblock Block;
	size_t iDerSize = 0u;
	bytes pDer;
	xx509cert Cert;
	xtime Deadline;
	bool bNeed = true;
	bool bOk = false;

	if((sRoot == NULL) || (sPrimaryDomain == NULL) || (pbNeed == NULL) ||
		(iRenewalDays < 0))
	{
		xacmeStoreError(
			XERR_ARGUMENT, XACME_STORE_ERROR_ARGUMENT,
			"acme store need renew requires root, domain and days");
		return false;
	}
	*pbNeed = true;

	sChain = xrtAcmeStoreLoadCert(sRoot, sPrimaryDomain);
	if(sChain == NULL)
	{
		if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
		{
			/* 读故障不是"缺证书"：如实失败，避免误触重签。 */
			return false;
		}
		return true; /* 缺证书即需要签发。 */
	}
	if(!xrtPemInit(&Pem, sChain, strlen(sChain))) goto Done;
	xpemresult Next = xrtPemRead(&Pem, &Block);
	if(Next == XPEM_ERROR) goto Done;
	if(Next != XPEM_BLOCK)
	{
		xacmeStoreError(
			XERR_PROTOCOL, XACME_STORE_ERROR_PARSE,
			"acme store chain has no pem block");
		goto Done;
	}
	pDer = xrtPemDecodeNew(&Block, &iDerSize);
	if(pDer == NULL)
	{
		goto Done;
	}
	if(!xrtX509Parse(pDer, iDerSize, &Cert))
	{
		xrtFree(pDer);
		goto Done;
	}
	xrtFree(pDer);
	if(!xrtTimeAdd(
			xrtNow(), (int64)iRenewalDays, XTIME_UNIT_DAY, &Deadline))
	{
		goto Done;
	}
	/* notAfter 早于续签线 → 需要续。 */
	bNeed = (Cert.NotAfter < Deadline);
	bOk = true;

Done:
	xrtFree(sChain);
	if(bOk)
	{
		*pbNeed = bNeed;
	}
	return bOk;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_flow.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_FLOW)

#if defined(XACME_FEATURE_ACME_FLOW)



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_FLOW_POLL_MAX 180u
#define XACME_FLOW_URL_MAX ((size_t)sizeof(((xacmeclient*)0)->sKid))

typedef struct xacmeflowurl {
	char sData[512];
	size_t iSize;
} xacmeflowurl;

static void xacmeFlowError(
	xerrkind Kind, xacmeflowerror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.flow", (int32)Code, sMessage);
}

static bool xacmeFlowCopyText(char* sDest, size_t iCapacity, cstr sSource)
{
	if((sSource == NULL) || (strlen(sSource) >= iCapacity))
	{
		return false;
	}
	strcpy(sDest, sSource);
	return true;
}

/* Keep only the CSR public key after the temporary private key is erased. */
typedef struct xacmeflowcertpublic {
	xacmecertkeykind Kind;
	uint8 Data[1040];
	size_t ModulusSize;
	size_t ExponentSize;
} xacmeflowcertpublic;

static void xacmeFlowCertPublic(
	const xacmecertkey* pKey, xacmeflowcertpublic* pPublic)
{
	pPublic->Kind = pKey->Kind;
	if(pKey->Kind == XACME_CERT_KEY_ES256)
		memcpy(pPublic->Data, pKey->Ec.Public, sizeof(pKey->Ec.Public));
	else
	{
		pPublic->ModulusSize = pKey->Rsa.ModulusSize;
		pPublic->ExponentSize = pKey->Rsa.ExponentSize;
		memcpy(pPublic->Data, pKey->Rsa.Modulus, pPublic->ModulusSize);
		memcpy(pPublic->Data + pPublic->ModulusSize,
			pKey->Rsa.Exponent, pPublic->ExponentSize);
	}
}

static bool xacmeFlowCertWhitespace(cstr sText, size_t iSize)
{
	size_t i;
	for(i = 0u; i < iSize; i++)
		if((sText[i] != ' ') && (sText[i] != '\t') &&
			(sText[i] != '\r') && (sText[i] != '\n')) return false;
	return true;
}

static bool xacmeFlowCertType(cstr sType)
{
	cstr sEnd;
	if(sType == NULL) return false;
	while((*sType == ' ') || (*sType == '\t')) sType++;
	sEnd = sType;
	while((*sEnd != '\0') && (*sEnd != ';')) sEnd++;
	while((sEnd > sType) && ((sEnd[-1] == ' ') || (sEnd[-1] == '\t')))
		sEnd--;
	return xrtHttpFieldNameEqual((xstrview){ sType, (size_t)(sEnd - sType) },
		XRT_STR_LITERAL("application/pem-certificate-chain"));
}

static bool xacmeFlowCertLeaf(
	const xx509cert* pCert, const xacmeflowcertpublic* pPublic,
	const xacmeflowurl* pDomains, size_t iDomainCount)
{
	xx509pubkey Key;
	xx509gencursor Names;
	xx509genname Name;
	xx509basicconstraints Constraints;
	xx509result Result;
	uint32 uMatched = 0u;
	xtime Now = xrtNow();
	if(!xrtX509PublicKey(pCert, &Key)) return false;
	if(pPublic->Kind == XACME_CERT_KEY_ES256)
	{
		if((Key.Type != X509_KEY_EC) || (Key.Curve != X509_CURVE_P256) ||
			(Key.Key.Size != 65u) || (memcmp(Key.Key.Data, pPublic->Data, 65u) != 0))
			goto Invalid;
	}
	else if(((Key.Type != X509_KEY_RSA) && (Key.Type != X509_KEY_RSA_PSS)) ||
		(Key.Modulus.Size != pPublic->ModulusSize) ||
		(Key.Exponent.Size != pPublic->ExponentSize) ||
		(memcmp(Key.Modulus.Data, pPublic->Data, Key.Modulus.Size) != 0) ||
		(memcmp(Key.Exponent.Data, pPublic->Data + pPublic->ModulusSize,
			Key.Exponent.Size) != 0)) goto Invalid;
	if((Now < pCert->NotBefore) || (Now > pCert->NotAfter)) goto Invalid;
	Result = xrtX509BasicConstraints(pCert, &Constraints);
	if(Result == X509_ERROR) return false;
	if((Result == X509_VALUE) && Constraints.CA) goto Invalid;
	Result = xrtX509SubjectAltName(pCert, &Names);
	if(Result == X509_ERROR) return false;
	if(Result != X509_VALUE) goto Invalid;
	while((Result = xrtX509GeneralNameRead(&Names, &Name)) == X509_VALUE)
	{
		size_t i;
		bool bFound = false;
		if(Name.Type != X509_NAME_DNS) goto Invalid;
		for(i = 0u; i < iDomainCount; i++)
			if(xrtHttpFieldNameEqual(
				(xstrview){ (cstr)Name.Value.Data, Name.Value.Size },
				(xstrview){ pDomains[i].sData, pDomains[i].iSize }))
			{
				uMatched |= UINT32_C(1) << i;
				bFound = true;
			}
		if(!bFound) goto Invalid;
	}
	if(Result == X509_ERROR) return false;
	if(uMatched == ((UINT32_C(1) << iDomainCount) - 1u)) return true;
Invalid:
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
		"acme certificate does not match CSR key, identifiers or validity");
	return false;
}

/* This checks the supplied chain, not trust in a deployment's root store.
 * RFC 8555 certificates contain only CERTIFICATE objects, leaf first. Bounds
 * are independent of the HTTP body cap; at most three DER buffers are live. */
static bool xacmeFlowCertificate(
	const xacmehttpresponse* pR, const xacmeflowcertpublic* pPublic,
	const xacmeflowurl* pDomains, size_t iDomainCount, xbytesview Reference,
	bytes* ppLeaf, size_t* piLeafSize)
{
	xpemcursor Cursor;
	xpemblock Block;
	xpemresult Result;
	xx509cert Previous, Current;
	bytes pLeaf = NULL, pPrevious = NULL, pCurrent = NULL;
	size_t iLeafSize = 0u, iCount = 0u;
	bool bOk = false;
	if((pR->iStatus != 200u) || (pR->sBody == NULL) ||
		(pR->iBodySize == 0u) || !xacmeFlowCertType(pR->sContentType))
		goto Invalid;
	if(!xrtPemInit(&Cursor, pR->sBody, pR->iBodySize)) goto ChildFailure;
	for(;;)
	{
		size_t iOffset = Cursor.Offset, iDerSize = 0u;
		Result = xrtPemRead(&Cursor, &Block);
		if(Result == XPEM_ERROR) goto ChildFailure;
		if(Result == XPEM_DONE)
		{
			if((iCount == 0u) || !xacmeFlowCertWhitespace(
				pR->sBody + iOffset, pR->iBodySize - iOffset)) goto Invalid;
			break;
		}
		if((iCount >= 16u) || (Block.Label.Size != 11u) ||
			(memcmp(Block.Label.Data, "CERTIFICATE", 11u) != 0) ||
			!xacmeFlowCertWhitespace(pR->sBody + iOffset,
				(size_t)(Block.Raw.Data - (pR->sBody + iOffset)))) goto Invalid;
		if(!xrtPemDecode(&Block, NULL, 0u, &iDerSize)) goto ChildFailure;
		if((iDerSize == 0u) || (iDerSize > 256u * 1024u)) goto Invalid;
		pCurrent = xrtPemDecodeNew(&Block, &iDerSize);
		if((pCurrent == NULL) || !xrtX509Parse(pCurrent, iDerSize, &Current))
			goto ChildFailure;
		if(iCount == 0u)
		{
			if(!xacmeFlowCertLeaf(&Current, pPublic, pDomains, iDomainCount))
				goto ChildFailure;
			if((Reference.Data != NULL) && ((Reference.Size != iDerSize) ||
				(memcmp(Reference.Data, pCurrent, iDerSize) != 0))) goto Invalid;
			pLeaf = pCurrent;
			iLeafSize = iDerSize;
		}
		else
		{
			xx509basicconstraints Constraints;
			uint16 uUsage;
			xx509result ConstraintsResult = xrtX509BasicConstraints(&Current, &Constraints);
			xx509result UsageResult, NameResult;
			if(ConstraintsResult == X509_ERROR) goto ChildFailure;
			UsageResult = xrtX509KeyUsage(&Current, &uUsage);
			if(UsageResult == X509_ERROR) goto ChildFailure;
			NameResult = xrtX509NameEqual(Previous.Issuer, Current.Subject);
			if(NameResult == X509_ERROR) goto ChildFailure;
			if((ConstraintsResult != X509_VALUE) || !Constraints.CA ||
				((UsageResult == X509_VALUE) && !(uUsage & X509_USAGE_CERT_SIGN)) ||
				(NameResult != X509_VALUE)) goto Invalid;
			if(!xrtX509CertificateVerify(&Previous, &Current)) goto ChildFailure;
		}
		if(pPrevious != pLeaf) xrtFree(pPrevious);
		pPrevious = pCurrent;
		Previous = Current;
		pCurrent = NULL;
		iCount++;
	}
	bOk = true;
	goto Done;
ChildFailure:
	if((xrtErrorKind(xrtGetError()) == XERR_MEMORY) ||
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED)) goto Done;
Invalid:
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
		"acme certificate response has invalid type, objects, identity or chain");
Done:
	xrtFree(pCurrent);
	if(pPrevious != pLeaf) xrtFree(pPrevious);
	if(bOk && (ppLeaf != NULL))
	{
		*ppLeaf = pLeaf;
		*piLeafSize = iLeafSize;
		pLeaf = NULL;
	}
	xrtFree(pLeaf);
	return bOk;
}

/* ---------------- JSON 辅助 ---------------- */

static bool xacmeFlowJsonQuoteAppend(xbuffer* pOut, xstrview sText)
{
	size_t i;
	if(!xrtBufferAppendByte(pOut, '"'))
	{
		return false;
	}
	for(i = 0; i < sText.Size; i++)
	{
		char c = sText.Data[i];
		bool bOk;
		if((c == '"') || (c == '\\'))
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)'\\') &&
				xrtBufferAppendByte(pOut, (uint8)c);
		}
		else if((unsigned char)c < 0x20u)
		{
			char sEscape[6];
			sEscape[0] = '\\';
			sEscape[1] = 'u';
			sEscape[2] = '0';
			sEscape[3] = '0';
			sEscape[4] = "0123456789ABCDEF"[((unsigned char)c >> 4u) & 0xFu];
			sEscape[5] = "0123456789ABCDEF"[(unsigned char)c & 0xFu];
			bOk = xrtBufferAppend(
				pOut, (xbytesview){ (const uint8*)sEscape, 6u });
		}
		else
		{
			bOk = xrtBufferAppendByte(pOut, (uint8)c);
		}
		if(!bOk)
		{
			return false;
		}
	}
	return xrtBufferAppendByte(pOut, '"');
}

/* 取对象字符串成员到固定缓冲。 */
static bool xacmeJsonValueText(
	const xvalue* pObject, cstr sKey, xacmeflowurl* pOut)
{
	xvalue* pMember = xrtValueObjectGet(
		pObject, (xstrview){ sKey, strlen(sKey) });
	xstrview Text;
	if((pMember == NULL) ||
		!xrtValueGetString(pMember, &Text) || (Text.Size == 0u) ||
		(Text.Size >= sizeof(pOut->sData)) ||
		(memchr(Text.Data, '\0', Text.Size) != NULL))
	{
		return false;
	}
	memcpy(pOut->sData, Text.Data, Text.Size);
	pOut->sData[Text.Size] = '\0';
	pOut->iSize = Text.Size;
	return true;
}

/* A malformed peer response is a protocol error. Allocation failures retain
 * the parser's original cause, including its domain and object identity. */
static xvalue* xacmeFlowResponseObject(
	const xacmehttpresponse* pR, xacmeflowerror Code, cstr sMessage)
{
	xvalue* pRoot = NULL;
	if((pR->sBody != NULL) && (pR->iBodySize != 0u))
	{
		pRoot = xrtJsonParse((xstrview){ pR->sBody, pR->iBodySize });
		if((pRoot == NULL) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY))
			return NULL;
	}
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		xacmeFlowError(XERR_PROTOCOL, Code, sMessage);
		return NULL;
	}
	return pRoot;
}

static bool xacmeFlowProblemType(
	const xacmehttpresponse* pR, xacmeflowurl* pType)
{
	xvalue* pRoot = xacmeFlowResponseObject(pR, XACME_FLOW_ERROR_PROTOCOL,
		"acme problem response must be a JSON object");
	bool bOk;
	if(pRoot == NULL) return false;
	bOk = xacmeJsonValueText(pRoot, "type", pType);
	xrtValueRelease(pRoot);
	if(!bOk)
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL,
			"acme problem response type invalid");
	return bOk;
}

static bool xacmeFlowLinkTokenChar(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

/* Registered relation names are case insensitive. A quoted rel value may
 * contain several space-separated relations and quoted-pair escapes. */
static bool xacmeFlowLinkHasAlternate(const char* p, const char* pEnd)
{
	static const char sRelation[] = "alternate";
	size_t iSize = 0u;
	bool bMatch = true;
	while(p < pEnd)
	{
		unsigned char c = (unsigned char)*p++;
		if(c == '\\' && p < pEnd) c = (unsigned char)*p++;
		if(c == ' ' || c == '\t')
		{
			if(bMatch && iSize == sizeof(sRelation) - 1u) return true;
			iSize = 0u;
			bMatch = true;
			continue;
		}
		if(c >= 'A' && c <= 'Z') c = (unsigned char)(c + 'a' - 'A');
		if(iSize >= sizeof(sRelation) - 1u || c != sRelation[iSize])
			bMatch = false;
		iSize++;
	}
	return bMatch && iSize == sizeof(sRelation) - 1u;
}

/* Parse each RFC 8288 link-value before considering its target. Commas in
 * URI references and quoted strings do not separate links. We do not apply
 * anchor overrides, so links containing anchor are ignored as required by
 * section 3.2. Only the first rel parameter of a link has meaning. */
static bool xacmeFlowLinkAlternate(cstr sLink, char* sOut, size_t iCap)
{
	const char* p = sLink;
	if(sLink == NULL || sOut == NULL || iCap == 0u) return false;
	for(;;)
	{
		const char *pTarget, *pTargetEnd;
		bool bAlternate = false, bRelSeen = false, bAnchor = false;
		while(*p == ' ' || *p == '\t' || *p == ',') p++;
		if(*p != '<') return false;
		pTarget = ++p;
		while(*p != '>' && *p != '\0')
		{
			if((unsigned char)*p <= 0x20u || *p == '<') return false;
			p++;
		}
		if(*p != '>') return false;
		pTargetEnd = p++;
		for(;;)
		{
			const char *pName, *pValue = NULL, *pValueEnd = NULL;
			bool bRel;
			xstrview Name;
			while(*p == ' ' || *p == '\t') p++;
			if(*p == ',' || *p == '\0') break;
			if(*p++ != ';') return false;
			while(*p == ' ' || *p == '\t') p++;
			pName = p;
			while(xacmeFlowLinkTokenChar((unsigned char)*p)) p++;
			if(p == pName) return false;
			Name = (xstrview){ pName, (size_t)(p - pName) };
			bRel = xrtHttpFieldNameEqual(Name, XRT_STR_LITERAL("rel"));
			if(xrtHttpFieldNameEqual(Name, XRT_STR_LITERAL("anchor"))) bAnchor = true;
			while(*p == ' ' || *p == '\t') p++;
			if(*p == '=')
			{
				p++;
				while(*p == ' ' || *p == '\t') p++;
				if(*p == '"')
				{
					pValue = ++p;
					while(*p != '"')
					{
						if(*p == '\0' || ((unsigned char)*p < 0x20u && *p != '\t') ||
							(unsigned char)*p == 0x7fu) return false;
						if(*p == '\\')
						{
							p++;
							if(*p == '\0' || ((unsigned char)*p < 0x20u && *p != '\t') ||
								(unsigned char)*p == 0x7fu) return false;
						}
						p++;
					}
					pValueEnd = p++;
				}
				else
				{
					pValue = p;
					while(xacmeFlowLinkTokenChar((unsigned char)*p)) p++;
					if(p == pValue) return false;
					pValueEnd = p;
				}
			}
			if(bRel && !bRelSeen)
			{
				if(pValue == NULL) return false;
				bAlternate = xacmeFlowLinkHasAlternate(pValue, pValueEnd);
				bRelSeen = true;
			}
		}
		if(bAlternate && !bAnchor)
		{
			size_t iLen = (size_t)(pTargetEnd - pTarget);
			if(iLen == 0u || iLen >= iCap) return false;
			memcpy(sOut, pTarget, iLen);
			sOut[iLen] = '\0';
			return true;
		}
		if(*p == '\0') return false;
		p++;
	}
}

static bool xacmeFlowUriAppend(char* sOut, size_t iCapacity,
	size_t* piSize, xstrview Part)
{
	if(Part.Size >= iCapacity - *piSize) return false;
	if(Part.Size != 0u) memcpy(sOut + *piSize, Part.Data, Part.Size);
	*piSize += Part.Size;
	sOut[*piSize] = '\0';
	return true;
}

/* RFC 3986 section 5.2.4. Only literal dot segments are removed: percent
 * escapes and the query retain their exact spelling for the protected URL. */
static size_t xacmeFlowUriDots(cstr sPath, size_t iSize, char* sOut)
{
	size_t i = 0u, n = 0u;
	while(i < iSize)
	{
		size_t r = iSize - i;
		if((r >= 3u) && (memcmp(sPath + i, "../", 3u) == 0)) i += 3u;
		else if((r >= 2u) && (memcmp(sPath + i, "./", 2u) == 0)) i += 2u;
		else if((r >= 3u) && (memcmp(sPath + i, "/./", 3u) == 0)) i += 2u;
		else if((r == 2u) && (memcmp(sPath + i, "/.", 2u) == 0))
		{
			i += 2u;
			sOut[n++] = '/';
		}
		else if(((r >= 4u) && (memcmp(sPath + i, "/../", 4u) == 0)) ||
			((r == 3u) && (memcmp(sPath + i, "/..", 3u) == 0)))
		{
			i += 3u;
			while((n > 0u) && (sOut[n - 1u] != '/')) n--;
			if(n > 0u) n--;
			if(i == iSize) sOut[n++] = '/';
		}
		else if(((r == 1u) && (sPath[i] == '.')) ||
			((r == 2u) && (memcmp(sPath + i, "..", 2u) == 0))) i = iSize;
		else
		{
			if(sPath[i] == '/') sOut[n++] = sPath[i++];
			while((i < iSize) && (sPath[i] != '/')) sOut[n++] = sPath[i++];
		}
	}
	sOut[n] = '\0';
	return n;
}

static bool xacmeFlowUriText(cstr sText)
{
	size_t i;
	for(i = 0u; sText[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sText[i];
		if(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) ||
			((c >= '0') && (c <= '9'))) continue;
		if((c == '%') && sText[i + 1u] && sText[i + 2u])
		{
			size_t k;
			for(k = 1u; k <= 2u; k++)
			{
				unsigned char h = (unsigned char)sText[i + k];
				if(!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
					(h >= 'A' && h <= 'F'))) return false;
			}
			i += 2u;
		}
		else if((c == '%') || (c > 0x7fu) ||
			(strchr("-._~:/?#[]@!$&'()*+,;=", c) == NULL)) return false;
	}
	return true;
}

/* Resolve against the certificate retrieval URL, not the directory URL.
 * Fragments identify no different HTTP resource and are omitted from both
 * the request and its JWS URL. Failure leaves the caller's output untouched. */
static bool xacmeFlowAlternateUrl(cstr sBase, cstr sReference,
	char* sOut, size_t iCapacity)
{
	xhttptarget Base, Ref, Checked;
	xstrview Scheme, Authority, Path, Query;
	char sRef[1024] = {0}, sMerged[1024] = {0}, sPath[1024], sUrl[512] = {0};
	size_t iRefSize, iSize = 0u, iPathSize;
	bool bQuery, bNormalize = true;
	cstr sQuestion;
	size_t iFirstDelimiter;
	if((sBase == NULL) || (sReference == NULL) || (sOut == NULL) ||
		(iCapacity == 0u) || (strlen(sBase) >= sizeof(sUrl)) ||
		(strlen(sReference) >= sizeof(sUrl)) || !xacmeFlowUriText(sReference) ||
		!xrtHttpTargetParse(XRT_STR_LITERAL("POST"),
			(xstrview){ sBase, strlen(sBase) }, &Base) ||
		(Base.Form != XHTTP_TARGET_ABSOLUTE) ||
		!(Base.Flags & XHTTP_TARGET_HAS_AUTHORITY) ||
		(Base.Authority.Size == 0u)) return false;
	iRefSize = strcspn(sReference, "#");
	iFirstDelimiter = strcspn(sReference, ":/?#");
	Scheme = Base.Scheme;
	Authority = Base.Authority;
	if((iFirstDelimiter < iRefSize && sReference[iFirstDelimiter] == ':') ||
		((iRefSize >= 2u) && (memcmp(sReference, "//", 2u) == 0)))
	{
		if((sReference[0] != '/') &&
			((iRefSize - iFirstDelimiter < 3u) ||
			(memcmp(sReference + iFirstDelimiter, "://", 3u) != 0))) return false;
		if(sReference[0] == '/')
		{
			if(!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize, Scheme) ||
				!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize, XRT_STR_LITERAL(":")))
				return false;
		}
		if(!xacmeFlowUriAppend(sRef, sizeof(sRef), &iSize,
			(xstrview){ sReference, iRefSize }) ||
			!xrtHttpTargetParse(XRT_STR_LITERAL("POST"),
				(xstrview){ sRef, iSize }, &Ref) ||
			(Ref.Form != XHTTP_TARGET_ABSOLUTE) ||
			!(Ref.Flags & XHTTP_TARGET_HAS_AUTHORITY) ||
			(Ref.Authority.Size == 0u)) return false;
		Scheme = Ref.Scheme;
		Authority = Ref.Authority;
		Path = Ref.Path;
		Query = Ref.Query;
		bQuery = (Ref.Flags & XHTTP_TARGET_HAS_QUERY) != 0u;
	}
	else
	{
		sQuestion = (cstr)memchr(sReference, '?', iRefSize);
		Path = (xstrview){ sReference, sQuestion ? (size_t)(sQuestion - sReference) : iRefSize };
		bQuery = (sQuestion != NULL);
		Query = bQuery ? (xstrview){ sQuestion + 1u,
			iRefSize - (size_t)(sQuestion + 1u - sReference) } : (xstrview){ NULL, 0u };
		if(Path.Size == 0u)
		{
			Path = Base.Path;
			bNormalize = false;
			if(!bQuery)
			{
				Query = Base.Query;
				bQuery = (Base.Flags & XHTTP_TARGET_HAS_QUERY) != 0u;
			}
		}
		else if(Path.Data[0] != '/')
		{
			size_t iPrefix = Base.Path.Size;
			while((iPrefix > 0u) && (Base.Path.Data[iPrefix - 1u] != '/')) iPrefix--;
			iSize = 0u;
			if(!xacmeFlowUriAppend(sMerged, sizeof(sMerged), &iSize,
				Base.Path.Size == 0u ? XRT_STR_LITERAL("/") :
				(xstrview){ Base.Path.Data, iPrefix }) ||
				!xacmeFlowUriAppend(sMerged, sizeof(sMerged), &iSize, Path)) return false;
			Path = (xstrview){ sMerged, iSize };
		}
	}
	if(!xrtHttpFieldNameEqual(Scheme, XRT_STR_LITERAL("http")) &&
		!xrtHttpFieldNameEqual(Scheme, XRT_STR_LITERAL("https"))) return false;
	if(!xrtHttpHostValid(Authority)) return false;
	iPathSize = bNormalize ? xacmeFlowUriDots(Path.Data, Path.Size, sPath) : Path.Size;
	if(bNormalize) Path = (xstrview){ sPath, iPathSize };
	iSize = 0u;
	if(!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Scheme) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, XRT_STR_LITERAL("://")) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Authority) ||
		!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Path) ||
		(bQuery && (!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, XRT_STR_LITERAL("?")) ||
			!xacmeFlowUriAppend(sUrl, sizeof(sUrl), &iSize, Query))) ||
		!xrtHttpTargetParse(XRT_STR_LITERAL("POST"), (xstrview){ sUrl, iSize }, &Checked) ||
		(iSize >= iCapacity)) return false;
	memcpy(sOut, sUrl, iSize + 1u);
	return true;
}

/* Issue 总预算：超限返回 true（已到截止）。 */
static bool xacmeFlowDeadlineHit(const xacmeclient* pClient)
{
	return pClient->bIssueDeadline &&
		(xrtTimer() >= pClient->IssueDeadline);
}

/* ---------------- nonce 与 POST ---------------- */

static void xacmeFlowTakeNonce(xacmeclient* pClient, xacmehttpresponse* pR)
{
	cstr sNonce = pR->sReplayNonce;
	size_t i;
	if((sNonce == NULL) || (sNonce[0] == '\0') ||
		(strlen(sNonce) >= sizeof(pClient->sNonce))) return;
	for(i = 0u; sNonce[i] != '\0'; i++)
	{
		unsigned char c = (unsigned char)sNonce[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_')) return;
	}
	strcpy(pClient->sNonce, sNonce);
}

static bool xacmeFlowNewNonce(xacmeclient* pClient)
{
	xacmehttpresponse R;
	if(pClient->sNonce[0] != '\0')
	{
		return true;
	}
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pClient->sNewNonce, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		return false;
	}
	if((R.iStatus < 200u) || (R.iStatus >= 300u))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
			"acme flow new-nonce response status invalid");
		return false;
	}
	xacmeFlowTakeNonce(pClient, &R);
	xacmeHttpResponseUnit(&R);
	if(pClient->sNonce[0] == '\0')
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
			"acme flow new-nonce response missing Replay-Nonce");
		return false;
	}
	return true;
}

/*
	执行一次 ACME POST（bKid=false 时保护头嵌 JWK，用于 newAccount）。
	响应由调用方 Unit；本函数顺带收割 nonce。
*/
static bool xacmeFlowPost(
	xacmeclient* pClient, cstr sUrl, xstrview sPayload, bool bKid,
	xacmehttpresponse* pR, uint32 uNonceRetry)
{
	xacmejwsheader H;
	str sToken = NULL;
	bool bOk;

	memset(pR, 0, sizeof(*pR));
	if(!xacmeFlowNewNonce(pClient))
	{
		return false;
	}
	H.Nonce = (xstrview){ pClient->sNonce, strlen(pClient->sNonce) };
	H.Url = (xstrview){ sUrl, strlen(sUrl) };
	if(bKid)
	{
		H.Kid = (xstrview){ pClient->sKid, strlen(pClient->sKid) };
	}
	else
	{
		H.Kid.Data = NULL;
		H.Kid.Size = 0u;
	}
	sToken = xacmeJwsEs256(&pClient->AccountKey, &H, sPayload);
	if(sToken == NULL)
	{
		return false;
	}
	pClient->sNonce[0] = '\0';
	bOk = xacmeHttpExchange(
		&pClient->Http, "POST", sUrl, "application/jose+json",
		(xstrview){ sToken, strlen(sToken) }, pR);
	xrtFree(sToken);
	if(!bOk)
	{
		return false;
	}
	xacmeFlowTakeNonce(pClient, pR);
	/* RFC 8555 §6.5: identify the problem structurally and use the fresh
	 * nonce supplied with that response. Three POST attempts in total. */
	if(pR->iStatus == 400u)
	{
		xacmeflowurl Type;
		if(!xacmeFlowProblemType(pR, &Type))
		{
			xacmeHttpResponseUnit(pR);
			return false;
		}
		if(strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0)
		{
			if((pClient->sNonce[0] == '\0') || (uNonceRetry >= 2u))
			{
				xacmeHttpResponseUnit(pR);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
					"acme badNonce response missing fresh nonce or retries exhausted");
				return false;
			}
			xacmeHttpResponseUnit(pR);
			return xacmeFlowPost(
				pClient, sUrl, sPayload, bKid, pR, uNonceRetry + 1u);
		}
	}
	return true;
}

/* POST-as-GET：空负载。 */
static bool xacmeFlowPostAsGet(
	xacmeclient* pClient, cstr sUrl, xacmehttpresponse* pR)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		const xerror* pError;
		cstr sDomain;
		if(xacmeFlowPost(
			pClient, sUrl, (xstrview){ "", 0u }, true, pR, 0u))
		{
			xrtClearError();
			return true;
		}
		pError = xrtGetError();
		sDomain = xrtErrorDomain(pError);
		if((uAttempt == 3u) || xacmeFlowDeadlineHit(pClient) ||
			(xrtErrorCode(pError) != XACME_HTTP_ERROR_UNCERTAIN) ||
			(sDomain == NULL) ||
			(strcmp(sDomain, "xrt.acme.http") != 0))
		{
			return false;
		}
		/* 只读 POST-as-GET 可用新 nonce/JWS 安全重试。 */
		xrtClearError();
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static uint32 xacmeFlowSleepMs(xacmehttpresponse* pR)
{
	long v;
	if((pR->sRetryAfter == NULL) || (pR->sRetryAfter[0] == '\0'))
	{
		return 500u;
	}
	v = atol(pR->sRetryAfter);
	if((v <= 0) || (v > 5))
	{
		v = 1;
	}
	return (uint32)(v * 1000u);
}

/*
	轮询某个 URL 的 JSON status 字段：pending/processing 继续等，
	其他状态（valid/ready/invalid...）即返回该状态的堆副本。
	pFinalizeOut 非空时顺带收割 finalize 字段一次。
*/
static str xacmeFlowWaitStatus(
	xacmeclient* pClient, cstr sUrl, xacmeflowurl* pFinalizeOut)
{
	size_t i;
	for(i = 0; i < XACME_FLOW_POLL_MAX; i++)
	{
		xacmehttpresponse R;
		xvalue* pRoot;
		xacmeflowurl Status;
		uint32 uMs;
		bool bTerminal;
		if(xacmeFlowDeadlineHit(pClient))
		{
			xacmeFlowError(XERR_TIMEOUT, XACME_FLOW_ERROR_PROTOCOL,
				"acme flow issue budget exhausted");
			return NULL;
		}
		if(!xacmeFlowPostAsGet(pClient, sUrl, &R)) return NULL;
		if((R.iStatus < 200u) || (R.iStatus >= 300u))
		{
			char sDetail[280];
			snprintf(sDetail, sizeof(sDetail),
				"acme flow poll error status=%u body=%.180s",
				(unsigned)R.iStatus, R.sBody ? R.sBody : "");
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL, sDetail);
			return NULL;
		}
		pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_PROTOCOL,
			"acme flow poll json invalid");
		if(pRoot == NULL)
		{
			xacmeHttpResponseUnit(&R);
			return NULL;
		}
		if(!xacmeJsonValueText(pRoot, "status", &Status))
		{
			xrtValueRelease(pRoot);
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL,
				"acme flow poll status invalid");
			return NULL;
		}
		if(pFinalizeOut != NULL)
		{
			if(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("finalize")) != NULL)
			{
				if(!xacmeJsonValueText(pRoot, "finalize", pFinalizeOut))
				{
					xrtValueRelease(pRoot);
					xacmeHttpResponseUnit(&R);
					xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE,
						"acme flow order finalize url invalid");
					return NULL;
				}
			}
			if(strcmp(Status.sData, "ready") == 0 && pFinalizeOut->sData[0] == 0)
			{
				xrtValueRelease(pRoot);
				xacmeHttpResponseUnit(&R);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE,
					"acme flow ready order missing finalize url");
				return NULL;
			}
		}
		bTerminal = strcmp(Status.sData, "pending") != 0 &&
			strcmp(Status.sData, "processing") != 0;
		xrtValueRelease(pRoot);
		if(bTerminal)
		{
			str sStatus = (str)xrtMalloc(Status.iSize + 1u);
			if(sStatus != NULL) memcpy(sStatus, Status.sData, Status.iSize + 1u);
			xacmeHttpResponseUnit(&R);
			return sStatus;
		}
		uMs = xacmeFlowSleepMs(&R);
		xacmeHttpResponseUnit(&R);
		xrtSleep(uMs);
	}
	xacmeFlowError(XERR_TIMEOUT, XACME_FLOW_ERROR_PROTOCOL,
		"acme flow status poll exhausted");
	return NULL;
}

/* ---------------- DNS 铺设与清理（带重试） ---------------- */

static bool xacmeFlowHttpUncertain(const xerror* pError)
{
	cstr sDomain = xrtErrorDomain(pError);
	return (xrtErrorCode(pError) == XACME_HTTP_ERROR_UNCERTAIN) &&
		(sDomain != NULL) && (strcmp(sDomain, "xrt.acme.http") == 0);
}

static bool xacmeFlowDnsAddTerminal(const xerror* pError)
{
	cstr sDomain = xrtErrorDomain(pError);
	return xrtErrorKind(pError) == XERR_MEMORY || xacmeFlowHttpUncertain(pError) ||
		(xrtErrorCode(pError) == XACME_DNS_ERROR_UNCERTAIN && sDomain != NULL &&
		 strcmp(sDomain, "xrt.acme.dns") == 0);
}

/*
	provider Add/Remove 各最多 3 次尝试（1s/2s 退避）。
	Add 在已发送请求的结果未知时不可重试：重复记录可能失去可清理的
	RecordId；Remove 按 provider 契约幂等。失败保留首个根因。
	提供商层结果未知及内存错误同样不重放 Add。
*/
static bool xacmeFlowDnsAdd(
	const xacmednsprovider* pDns, cstr sFqdn, cstr sTxt)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		xerror* pFirst;
		if(pDns->Add((xacmednsprovider*)pDns,
				(xstrview){ sFqdn, strlen(sFqdn) },
				(xstrview){ sTxt, strlen(sTxt) }))
		{
			return true;
		}
		pFirst = xrtErrorRef(xrtGetError());
		/* OOM can prevent publishing an uncertainty wrapper; never replay an
		 * Add when its failure could not even be represented completely. */
		if(xacmeFlowDnsAddTerminal(pFirst))
		{
			xrtSetErrorTake(pFirst);
			return false;
		}
		if(uAttempt < 3u)
		{
			if(getenv("XACME_DEBUG"))
			{
				printf("[dns-retry] add attempt=%u fqdn=%s\n",
					(unsigned)uAttempt, sFqdn);
			}
			xrtSleep(1000u * uAttempt);
		}
		xrtSetErrorTake(pFirst);
	}
	return false;
}

static void xacmeFlowDnsRemove(
	const xacmednsprovider* pDns, cstr sFqdn, cstr sTxt)
{
	uint32 uAttempt;
	for(uAttempt = 1u; uAttempt <= 3u; uAttempt++)
	{
		if(pDns->Remove((xacmednsprovider*)pDns,
				(xstrview){ sFqdn, strlen(sFqdn) },
				(xstrview){ sTxt, strlen(sTxt) }))
		{
			return;
		}
		if(uAttempt < 3u)
		{
			xrtSleep(1000u * uAttempt);
		}
		xrtClearError(); /* 清理是尽力而为，不污染主错误。 */
	}
}

/* ---------------- 传播确认 ---------------- */

/*
	解析 resolver 字符串："host"、"host:port" 或 "[v6]:port"。
	无端口段或段非法时回退 53；输出去掉括号的 host 到定长缓冲。
*/
static uint16 xacmeFlowResolverPort(
	cstr sResolver, char* sOutHost, size_t iHostCap)
{
	const char* sColon = strrchr(sResolver, ':');
	size_t iHostLen;
	if((sColon != NULL) && (sColon != sResolver))
	{
		long v = atol(sColon + 1);
		iHostLen = (size_t)(sColon - sResolver);
		if((sResolver[0] == '[') && (iHostLen > 1u) &&
			(sResolver[iHostLen - 1u] == ']'))
		{
			sResolver++;
			iHostLen -= 2u;
		}
		if((v > 0) && (v <= 65535) && (iHostLen < iHostCap))
		{
			memcpy(sOutHost, sResolver, iHostLen);
			sOutHost[iHostLen] = '\0';
			return (uint16)v;
		}
	}
	snprintf(sOutHost, iHostCap, "%s", sResolver);
	return 53u;
}

/* 任一配置 resolver 已返回期望 TXT 值即视为可见。 */
static bool xacmeFlowTxtVisible(
	xacmedns* pDns, const xacmeclient* pClient,
	cstr sFqdn, cstr sExpected)
{
	size_t i;
	for(i = 0; i < pClient->iPropagateResolverCount; i++)
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		char sHost[64];
		uint16 iPort = xacmeFlowResolverPort(
			pClient->sPropagateResolvers[i], sHost, sizeof(sHost));
		size_t iCount = 0u;
		size_t j;
		if(!xacmeDnsTxtQuery(
				pDns, sHost, iPort, sFqdn, sRecords, 4u, &iCount))
		{
			if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
			xrtClearError(); /* Advisory reachability failure must not mask a later OOM. */
			continue; /* 单个 resolver 不可达不算失败。 */
		}
		for(j = 0; j < iCount; j++)
		{
			if(strcmp(sRecords[j], sExpected) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

/*
	挑战触发前的传播确认门（尽力而为）：
	- provider 带 XACME_DNS_CAP_PROPAGATE 时委托 provider 自证；
	- 否则对公共 resolver 组轮询 TXT（任一可见即通过）；
	- 超时不阻断签发——CA 只查权威侧，公共递归滞后不必然失败，
	  仅在 XACME_DEBUG 下输出提示。
*/
static void xacmeFlowWaitPropagate(
	xacmeclient* pClient, const xacmednsprovider* pDns,
	cstr sFqdn, cstr sTxt)
{
	xacmedns Probe;
	double uDeadline;
	if(((pDns->iCaps & XACME_DNS_CAP_PROPAGATE) != 0u) &&
		(pDns->Propagate != NULL))
	{
		(void)pDns->Propagate((xacmednsprovider*)pDns,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sTxt, strlen(sTxt) });
		return;
	}
	if(!xacmeDnsInit(&Probe, pClient->Http.pEngine))
	{
		return;
	}
	uDeadline = __xrtWaitAfter(pClient->uPropagateTimeoutMs);
	if(pClient->bIssueDeadline &&
		(pClient->IssueDeadline < uDeadline))
	{
		uDeadline = pClient->IssueDeadline;
	}
	while(xrtTimer() < uDeadline)
	{
		if(xacmeFlowTxtVisible(&Probe, pClient, sFqdn, sTxt))
		{
			xacmeDnsUnit(&Probe);
			return;
		}
		if(xrtErrorKind(xrtGetError()) == XERR_MEMORY)
		{
			xacmeDnsUnit(&Probe);
			return;
		}
		xrtSleep(2000u);
	}
	xacmeDnsUnit(&Probe);
	if(getenv("XACME_DEBUG"))
	{
		printf("[dbg] propagate confirm timeout fqdn=%s\n", sFqdn);
	}
}

/* ---------------- 初始化与签发 ---------------- */

/*
	重新拉取授权对象，提取挑战 error.detail（约 160 字符）进 sOut。
	失败时留空串——诊断增强，不改变失败语义。
*/
static void xacmeFlowChallengeDetail(
	xacmeclient* pClient, cstr sAuthzUrl, char* sOut, size_t iCapacity)
{
	xacmehttpresponse R;
	xvalue* pRoot;
	xvalue* pChallenges;
	size_t j;

	sOut[0] = '\0';
	if(!xacmeFlowPostAsGet(pClient, sAuthzUrl, &R))
	{
		return;
	}
	pRoot = (R.sBody != NULL) ?
		xrtJsonParse((xstrview){ R.sBody, R.iBodySize }) : NULL;
	xacmeHttpResponseUnit(&R);
	if((pRoot == NULL) || !xrtValueIs(pRoot, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		return;
	}
	pChallenges = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("challenges"));
	for(j = 0; (pChallenges != NULL) &&
		xrtValueIs(pChallenges, XVALUE_ARRAY) &&
		(j < xrtValueCount(pChallenges)); j++)
	{
		xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
		xvalue* pError;
		xvalue* pDetail;
		xstrview Text;
		if((pChallenge == NULL) || !xrtValueIs(pChallenge, XVALUE_OBJECT))
		{
			continue;
		}
		pError = xrtValueObjectGet(pChallenge, XRT_STR_LITERAL("error"));
		if((pError == NULL) || !xrtValueIs(pError, XVALUE_OBJECT))
		{
			continue;
		}
		pDetail = xrtValueObjectGet(pError, XRT_STR_LITERAL("detail"));
		if((pDetail != NULL) && xrtValueGetString(pDetail, &Text) &&
			(Text.Size > 0u))
		{
			size_t iCopy = (Text.Size < (iCapacity - 1u)) ?
				Text.Size : (iCapacity - 1u);
			memcpy(sOut, Text.Data, iCopy);
			sOut[iCopy] = '\0';
			break;
		}
	}
	xrtValueRelease(pRoot);
}

bool xacmeClientInit(
	xacmeclient* pClient, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, const xacmeaccountconfig* pAccount, int64 uTimeoutMs)
{
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	xacmeflowurl NewNonce;
	xacmeflowurl NewAccount;
	xacmeflowurl NewOrder;
	xbuffer Payload;
	bool bOk = false;

	if((pClient == NULL) || (pAccount == NULL) ||
		(pAccount->sDirectoryUrl == NULL) ||
		(pAccount->sDirectoryUrl[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client init requires client and account config");
		return false;
	}
	memset(pClient, 0, sizeof(*pClient));
	if(!xacmeFlowCopyText(
			pClient->sDirectoryUrl, sizeof(pClient->sDirectoryUrl),
			pAccount->sDirectoryUrl))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client directory url too long");
		return false;
	}
	if(!xacmeHttpInit(&pClient->Http, pBorrowedEngine, sCaPem, uTimeoutMs))
	{
		goto Done;
	}
	if((pAccount->sAccountKeyPem != NULL) &&
		(pAccount->sAccountKeyPem[0] != '\0'))
	{
		if(!xacmeKeyPemRead(
			pAccount->sAccountKeyPem, strlen(pAccount->sAccountKeyPem),
			&pClient->AccountKey))
		{
			goto Done;
		}
	}
	else if(!xacmeEs256Generate(&pClient->AccountKey))
	{
		goto Done;
	}

	/* directory */
	if(!xacmeHttpExchange(
		&pClient->Http, "GET", pAccount->sDirectoryUrl, NULL,
		(xstrview){ NULL, 0u }, &R))
	{
		goto Done;
	}
	if((R.iStatus != 200u) || (R.sBody == NULL))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory response invalid");
		goto Done;
	}
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_DIRECTORY,
		"acme client directory json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;
	if(
		!xacmeJsonValueText(pRoot, "newNonce", &NewNonce) ||
		!xacmeJsonValueText(pRoot, "newAccount", &NewAccount) ||
		!xacmeJsonValueText(pRoot, "newOrder", &NewOrder))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory json missing endpoints");
		goto Done;
	}
	if(!xacmeFlowCopyText(
			pClient->sNewNonce, sizeof(pClient->sNewNonce), NewNonce.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewAccount, sizeof(pClient->sNewAccount),
			NewAccount.sData) ||
		!xacmeFlowCopyText(
			pClient->sNewOrder, sizeof(pClient->sNewOrder), NewOrder.sData))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
			"acme client directory url too long");
		goto Done;
	}
	/* revokeCert/keyChange 可选：缺失时对应路径显式报错。 */
	{
		xacmeflowurl Optional;
		if((xrtValueObjectGet(pRoot, XRT_STR_LITERAL("revokeCert")) != NULL &&
			 !xacmeJsonValueText(pRoot, "revokeCert", &Optional)) ||
			(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("keyChange")) != NULL &&
			 !xacmeJsonValueText(pRoot, "keyChange", &Optional)))
		{
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_DIRECTORY,
				"acme client optional directory endpoint invalid");
			goto Done;
		}
		if(xacmeJsonValueText(pRoot, "revokeCert", &Optional))
		{
			(void)xacmeFlowCopyText(
				pClient->sRevokeCert, sizeof(pClient->sRevokeCert),
				Optional.sData);
		}
		if(xacmeJsonValueText(pRoot, "keyChange", &Optional))
		{
			(void)xacmeFlowCopyText(
				pClient->sKeyChange, sizeof(pClient->sKeyChange),
				Optional.sData);
		}
	}

	/* 注册或复用账户：201=新建，200=已存在。载荷按需携带
	   contact 与 externalAccountBinding（RFC 8555 §7.3/§7.3.4）。 */
	xrtBufferInit(&Payload);
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"termsOfServiceAgreed\":true")))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if((pAccount->sContactEmail != NULL) &&
		(pAccount->sContactEmail[0] != '\0'))
	{
		xbuffer Mailto;
		bool bContactOk;
		xrtBufferInit(&Mailto);
		bContactOk = xrtBufferAppend(&Mailto, XRT_BYTES_LITERAL("mailto:")) &&
			xrtBufferAppend(&Mailto, (xbytesview){
				(const uint8*)pAccount->sContactEmail,
				strlen(pAccount->sContactEmail) }) &&
			xrtBufferAppend(&Payload, XRT_BYTES_LITERAL(",\"contact\":[")) &&
			xacmeFlowJsonQuoteAppend(&Payload,
				(xstrview){ (cstr)Mailto.Data, Mailto.Size }) &&
			xrtBufferAppendByte(&Payload, (uint8)']');
		xrtBufferUnit(&Mailto);
		if(!bContactOk)
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
	}
	if((pAccount->Eab.sKid != NULL) && (pAccount->Eab.sKid[0] != '\0'))
	{
		/* base64url 文本 MAC key（兼容带/不带填充）。 */
		static const xbase64config B64UrlPad = {
			NULL, XBASE64_URL | XBASE64_OPTIONAL_PADDING
		};
		uint8 Mac[64];
		size_t iMacSize = 0u;
		str sJwk = NULL;
		str sEab = NULL;
		if((pAccount->Eab.sHmac == NULL) || (pAccount->Eab.sHmac[0] == '\0'))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab kid without hmac");
			goto Done;
		}
		if(!xrtBase64Decode(
				pAccount->Eab.sHmac, strlen(pAccount->Eab.sHmac), Mac,
				sizeof(Mac), &iMacSize, &B64UrlPad) ||
			(iMacSize == 0u))
		{
			xrtBufferUnit(&Payload);
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ACCOUNT,
				"acme client eab hmac invalid base64url");
			goto Done;
		}
		sJwk = xacmeJwkEcJson(&pClient->AccountKey);
		if(sJwk != NULL)
		{
			sEab = xacmeJwsEabHs256(
				pAccount->Eab.sKid, pClient->sNewAccount,
				(xstrview){ sJwk, strlen(sJwk) }, Mac, iMacSize);
		}
		xrtFree(sJwk);
		if(sEab == NULL)
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL(",\"externalAccountBinding\":")) ||
			!xrtBufferAppend(
				&Payload,
				(xbytesview){ (const uint8*)sEab, strlen(sEab) }))
		{
			xrtFree(sEab);
			xrtBufferUnit(&Payload);
			goto Done;
		}
		xrtFree(sEab);
	}
	if(!xrtBufferAppendByte(&Payload, (uint8)'}'))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewAccount,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, false, &R, 0u))
	{
		xrtBufferUnit(&Payload);
		goto Done;
	}
	xrtBufferUnit(&Payload);
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[160];
		snprintf(sDetail, sizeof(sDetail),
			"acme new-account status=%u body=%.100s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
		goto Done;
	}
	if((R.sLocation == NULL) ||
		(R.sLocation[0] == '\0') ||
		!xacmeFlowCopyText(
			pClient->sKid, sizeof(pClient->sKid), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
			"acme client new-account missing location");
		goto Done;
	}
	xrtValueRelease(pRoot);
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
		"acme client account json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;
	{
		xacmeflowurl Status;
		if(!xacmeJsonValueText(pRoot, "status", &Status) ||
			strcmp(Status.sData, "valid") != 0)
		{
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme client account is not valid");
			goto Done;
		}
	}
	bOk = true;

Done:
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	if(!bOk)
	{
		/* 先保留首个根因，再拆传输（Unit 可能覆盖线程错误）。 */
		xerror* pFirst = xrtErrorRef(xrtGetError());
		/* 失败构造尚未交付拥有者，回滚不复用已经耗尽的请求预算。 */
		if(pClient->Http.uTimeoutMs < INT64_C(30000))
			pClient->Http.uTimeoutMs = INT64_C(30000);
		xacmeHttpUnit(&pClient->Http);
		if(pFirst != NULL)
		{
			xrtSetErrorTake(pFirst);
		}
	}
	return bOk;
}

bool xacmeClientUnit(xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return true;
	}
	if(!xacmeHttpUnit(&pClient->Http)) return false;
	xrtSecureZero(pClient, sizeof(*pClient));
	return true;
}

str xacmeClientAccountPem(const xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return NULL;
	}
	return xacmeKeyPemWrite(&pClient->AccountKey);
}

bool xacmeClientIssue(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, xacmeissuegrant* pOut,
	bool bAlt)
{
	xbuffer Payload;
	xacmehttpresponse R;
	xvalue* pRoot = NULL;
	bool bResult = false;
	xerror* pUncertainError = NULL;
	xacmeflowurl Finalize;
	xacmeflowurl OrderUrl;
	Finalize.sData[0] = 0;
	xacmeflowurl Certificate;
	size_t i;
	bool bOk = false;
	xacmeflowcertpublic CertPublic = {0};
	bytes pLeafDer = NULL;
	size_t iLeafDerSize = 0u;

	if(pOut != NULL) memset(pOut, 0, sizeof(*pOut));
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pDns == NULL) || (pDns->Add == NULL) || (pDns->Remove == NULL) ||
		(pOut == NULL) ||
		!xrtAcmeDnsProviderValidate(pDns))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue requires client, domains, dns provider and output");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	/* RFC 8555 section 11.1 forbids reusing a known account key in a CSR.
	 * Check at issue time too: account rollover may change this relationship. */
	if((pClient->pCertKey != NULL) &&
		(pClient->pCertKey->Kind == XACME_CERT_KEY_ES256) &&
		(memcmp(pClient->pCertKey->Ec.Public, pClient->AccountKey.Public,
			sizeof(pClient->AccountKey.Public)) == 0))
	{
		xacmeFlowError(XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme certificate key must differ from the account key");
		return false;
	}
	/* 总预算打点：uIssueTimeoutMs 非零时本次 Issue 全程受限。 */
	pClient->bIssueDeadline = (pClient->uIssueTimeoutMs != 0u);
	pClient->IssueDeadline = __xrtWaitAfter(pClient->uIssueTimeoutMs);

	/* 1. 新订单；identifier 用完整域名（通配符原样：*.example.com 是
	   独立 identifier，CA 依此返回通配符授权）。去重按完整字符串——
	   RFC 8555 要求订单 identifier 与 CSR SAN 严格一致。 */
	xacmeflowurl Bases[16];
	size_t iBaseCount = 0u;
	/* TXT 统一延后清理：同名多授权（裸域 + 通配符）场景下，先删后加
	   会让 LE 次级验证器的递归缓存仍见旧值而判 Incorrect TXT；多值
	   并存是 LE 明确允许的形态，全部验证完成后再统一 Remove。 */
	struct
	{
		str sFqdn;
		str sTxt;
	} TxtPending[16];
	size_t iTxtPending = 0u;
	struct
	{
		xacmeflowurl Authz;
		xacmeflowurl Challenge;
		str sFqdn;
		str sTxt;
	} Work[16];
	size_t iWorkCount = 0u;
	xrtBufferInit(&Payload);
	for(i = 0; i < iDomainCount; i++)
	{
		xstrview Domain = pDomains[i];
		size_t j;
		bool bDup = false;
		if((Domain.Data == NULL) || (Domain.Size == 0u) ||
			(Domain.Size >= sizeof(Bases[0].sData)) ||
			(memchr(Domain.Data, '\0', Domain.Size) != NULL))
		{
			xacmeFlowError(
				XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
				"acme issue domain list invalid");
			goto Done;
		}
		for(j = 0; j < iBaseCount; j++)
		{
			if((Bases[j].iSize == Domain.Size) &&
				(memcmp(Bases[j].sData, Domain.Data, Domain.Size) == 0))
			{
				bDup = true;
				break;
			}
		}
		if(bDup)
		{
			continue;
		}
		if(iBaseCount >= sizeof(Bases) / sizeof(Bases[0]))
		{
			xacmeFlowError(XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
				"acme issue has too many distinct domains");
			goto Done;
		}
		memcpy(Bases[iBaseCount].sData, Domain.Data, Domain.Size);
		Bases[iBaseCount].sData[Domain.Size] = 0;
		Bases[iBaseCount].iSize = Domain.Size;
		iBaseCount++;
	}
	if(!xrtBufferAppend(
		&Payload, XRT_BYTES_LITERAL("{\"identifiers\":[")))
	{
		goto Done;
	}
	for(i = 0; i < iBaseCount; i++)
	{
		if((i > 0u) && !xrtBufferAppendByte(&Payload, (uint8)','))
		{
			goto Done;
		}
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL("{\"type\":\"dns\",\"value\":")) ||
			!xacmeFlowJsonQuoteAppend(
				&Payload, (xstrview){ Bases[i].sData, Bases[i].iSize }) ||
			!xrtBufferAppendByte(&Payload, (uint8)'}'))
		{
			goto Done;
		}
	}
	if(!xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("]}")))
	{
		goto Done;
	}
	if(!xacmeFlowPost(
		pClient, pClient->sNewOrder,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		goto Done;
	}
	if((R.iStatus != 201u) || (R.sLocation == NULL) || (R.sLocation[0] == 0) ||
		!xacmeFlowCopyText(
			OrderUrl.sData, sizeof(OrderUrl.sData), R.sLocation))
	{
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue new-order response invalid");
		goto Done;
	}
	OrderUrl.iSize = strlen(OrderUrl.sData);
	pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ORDER,
		"acme issue new-order json invalid");
	xacmeHttpResponseUnit(&R);
	if(pRoot == NULL) goto Done;

/* 2. 逐授权域处理 dns-01（两段式：先铺全部 TXT 再统一触发）。
 *
 * 同名多授权（裸域 + 通配符共用一个 TXT 属主）场景下，逐个
 * “加 TXT→触发→轮询→删 TXT” 会让 LE 次级验证器的递归缓存仍持有
 * 只含旧值的 RRset（TTL 未过不再回源），次级视角只见旧值判
 * Incorrect TXT。两段式保证触发任何挑战时全部 TXT 值已并存，
 * 首次回源查询即拿到完整集合；TXT 统一延后到 Done 清理。 */
{
	xvalue* pAuthz = xrtValueObjectGet(
		pRoot, XRT_STR_LITERAL("authorizations"));
	size_t iCount = (pAuthz != NULL) ? xrtValueCount(pAuthz) : 0u;
	char sThumb[44];
	size_t k;
	if((pAuthz == NULL) || !xrtValueIs(pAuthz, XVALUE_ARRAY) ||
		(iCount != iBaseCount))
	{
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
			"acme issue order authorizations invalid");
		goto Done;
	}
	if(!xacmeJwkEcThumbprint(&pClient->AccountKey, sThumb)) goto Done;
	/* Pass A：逐授权取 dns-01 挑战，铺 TXT 并确认传播。 */
	for(i = 0; i < iCount; i++)
	{
		xvalue* pUrl = xrtValueArrayGet(pAuthz, i);
		xstrview UrlText;
		xacmeflowurl Authz;
		xacmehttpresponse A;
		xvalue* pAuthRoot = NULL;
		xacmeflowurl Token;
		xvalue* pChallenges;
		size_t j;
		bool bFound = false;
		if((pUrl == NULL) ||
			!xrtValueGetString(pUrl, &UrlText) ||
			(UrlText.Size == 0u) || (UrlText.Size >= sizeof(Authz.sData)) ||
			(memchr(UrlText.Data, '\0', UrlText.Size) != NULL))
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
				"acme issue authorization url invalid");
			goto Done;
		}
		memcpy(Authz.sData, UrlText.Data, UrlText.Size);
		Authz.sData[UrlText.Size] = '\0';
		Authz.iSize = UrlText.Size;
		if(!xacmeFlowPostAsGet(pClient, Authz.sData, &A))
		{
			goto Done;
		}
		if(A.iStatus != 200u)
		{
			xacmeHttpResponseUnit(&A);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
				"acme authorization response status invalid");
			goto Done;
		}
		pAuthRoot = xacmeFlowResponseObject(&A, XACME_FLOW_ERROR_CHALLENGE,
			"acme authorization json invalid");
		if(pAuthRoot == NULL)
		{
			xacmeHttpResponseUnit(&A);
			goto Done;
		}
		{
			xacmeflowurl AuthzStatus;
			bool bValid;
			if(!xacmeJsonValueText(pAuthRoot, "status", &AuthzStatus))
			{
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme authorization status invalid");
				goto Done;
			}
			bValid = strcmp(AuthzStatus.sData, "valid") == 0;
			if(!bValid && strcmp(AuthzStatus.sData, "pending") != 0)
			{
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme authorization is not pending or valid");
				goto Done;
			}
			pChallenges = xrtValueObjectGet(
				pAuthRoot, XRT_STR_LITERAL("challenges"));
			if(getenv("XACME_DEBUG"))
			{
				xacmeflowurl AuthzIdent;
				xvalue* pDbgIdent = xrtValueObjectGet(
					pAuthRoot, XRT_STR_LITERAL("identifier"));
				printf("[dbg] authz[%u] status=%s ident=%s challenges=%zu\n",
					(unsigned)i,
					(xacmeJsonValueText(pAuthRoot, "status", &AuthzStatus)) ?
						AuthzStatus.sData : "?",
					((pDbgIdent != NULL) &&
						xacmeJsonValueText(
							pDbgIdent, "value", &AuthzIdent)) ?
						AuthzIdent.sData : "?",
					((pChallenges != NULL) &&
							xrtValueIs(pChallenges, XVALUE_ARRAY)) ?
							xrtValueCount(pChallenges) : 0u);
			}
			if(bValid)
			{
				/* 已有效的授权（如复用）视为已解决。 */
				xrtValueRelease(pAuthRoot);
				xacmeHttpResponseUnit(&A);
				continue;
			}
			for(j = 0; (pChallenges != NULL) &&
				xrtValueIs(pChallenges, XVALUE_ARRAY) &&
				(j < xrtValueCount(pChallenges)); j++)
			{
				xvalue* pChallenge = xrtValueArrayGet(pChallenges, j);
				xacmeflowurl Type;
				xacmeflowurl ChallengeUrl;
				if((pChallenge == NULL) ||
					!xrtValueIs(pChallenge, XVALUE_OBJECT) ||
					!xacmeJsonValueText(pChallenge, "type", &Type) ||
					(strcmp(Type.sData, "dns-01") != 0))
				{
					if(getenv("XACME_DEBUG"))
					{
						printf("[dbg] authz[%u] chal[%u] skip type=%s\n",
							(unsigned)i, (unsigned)j,
							xacmeJsonValueText(pChallenge, "type", &Type) ?
								Type.sData : "?");
					}
					continue;
				}
				if(!xacmeJsonValueText(
						pChallenge, "url", &ChallengeUrl) ||
					!xacmeJsonValueText(pChallenge, "token", &Token))
				{
					if(getenv("XACME_DEBUG"))
					{
						printf("[dbg] dns-01 missing url/token\n");
					}
					continue;
				}
				/* keyAuthz = token '.' thumbprint；TXT = b64url(sha256) */
				{
					uint8 Digest[XRT_SHA256_SIZE];
					char sKeyAuthz[512];
					size_t iKeyAuthz = Token.iSize + 1u + 43u;
					static const xbase64config B64Url = {
						NULL,
						XBASE64_URL | XBASE64_NO_PADDING
					};
					str sTxt;
					str sFqdn = NULL;
					if(iKeyAuthz >= sizeof(sKeyAuthz))
					{
						if(getenv("XACME_DEBUG")) printf("[dbg] keyauthz too long\n");
						continue;
					}
					memcpy(sKeyAuthz, Token.sData, Token.iSize);
					sKeyAuthz[Token.iSize] = '.';
					memcpy(sKeyAuthz + Token.iSize + 1u, sThumb, 43u);
					if(!xrtSha256(sKeyAuthz, iKeyAuthz, Digest) ||
						((sTxt = xrtBase64EncodeNew(
							Digest, sizeof(Digest), &B64Url)) == NULL))
					{
						xrtValueRelease(pAuthRoot);
						xacmeHttpResponseUnit(&A);
						goto Done;
					}
					/* TXT 属主 = _acme-challenge.<授权 identifier.value>
					   （通配符授权返回的 value 已是基础域；协议正源，
					   不依赖订单 identifier 的位置映射）。 */
					{
						xbuffer Fqdn;
						xvalue* pIdent = xrtValueObjectGet(
							pAuthRoot, XRT_STR_LITERAL("identifier"));
						xvalue* pIdentValue = (pIdent != NULL) ?
							xrtValueObjectGet(
								pIdent, XRT_STR_LITERAL("value")) : NULL;
						xstrview Domain = { NULL, 0 };
						bool bFqdnOk;
						if((pIdentValue != NULL) &&
							xrtValueGetString(pIdentValue, &Domain) &&
							(Domain.Size != 0u) &&
							(Domain.Size < sizeof(Bases[0].sData)) &&
							(memchr(Domain.Data, '\0', Domain.Size) == NULL))
						{
							/* 协议正源路径 */
						}
						else
						{
							xrtFree(sTxt);
							xrtValueRelease(pAuthRoot);
							xacmeHttpResponseUnit(&A);
							xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
								"acme authorization identifier invalid");
							goto Done;
						}
						xrtBufferInit(&Fqdn);
						bFqdnOk = xrtBufferAppend(
							&Fqdn,
							XRT_BYTES_LITERAL("_acme-challenge.")) &&
							xrtBufferAppend(
								&Fqdn,
								(xbytesview){
									(const uint8*)Domain.Data,
									Domain.Size }) &&
							xrtBufferAppendByte(&Fqdn, 0u);
						sFqdn = bFqdnOk ? (str)Fqdn.Data : NULL;
						if(!bFqdnOk)
						{
							xrtBufferUnit(&Fqdn);
						}
					}
					bool bCanAdd = (sFqdn != NULL) &&
						(iWorkCount <
							(sizeof(Work) / sizeof(Work[0])));
					if(!bCanAdd ||
						!xacmeFlowDnsAdd(pDns, sFqdn, sTxt))
					{
						if(xacmeFlowDnsAddTerminal(xrtGetError()))
						{
							pUncertainError = xrtErrorRef(xrtGetError());
						}
						if(getenv("XACME_DEBUG")) printf("[dbg] dns add failed fqdn=%s err=%d\n", sFqdn ? sFqdn : "null", (int)xrtErrorKind(xrtGetError()));
						xrtFree(sFqdn);
						xrtFree(sTxt);
						if(pUncertainError != NULL)
							break;
						continue;
					}
					/* 传播确认：全部 TXT 就位后才进入 Pass B 统一触发。 */
					xacmeFlowWaitPropagate(pClient, pDns, sFqdn, sTxt);
					memcpy(&Work[iWorkCount].Authz, &Authz,
						sizeof(xacmeflowurl));
					memcpy(&Work[iWorkCount].Challenge, &ChallengeUrl,
						sizeof(xacmeflowurl));
					Work[iWorkCount].sFqdn = sFqdn;
					Work[iWorkCount].sTxt = sTxt;
					iWorkCount++;
					if(xrtErrorKind(xrtGetError()) == XERR_MEMORY)
					{
						xrtValueRelease(pAuthRoot);
						xacmeHttpResponseUnit(&A);
						goto Done;
					}
					bFound = true;
					break;
				}
			}
			xrtValueRelease(pAuthRoot);
			xacmeHttpResponseUnit(&A);
			if(!bFound)
			{
				if(pUncertainError == NULL)
					xacmeFlowError(
						XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
						"acme issue no dns-01 challenge solved");
				goto Done;
			}
		}
		}
		/* Pass B：全部 TXT 并存的前提下统一触发并轮询。 */
		for(k = 0; k < iWorkCount; k++)
		{
			xacmehttpresponse T;
			str sStatus = NULL;
			bool bValidNow = false;
			if(!xacmeFlowPost(
				pClient, Work[k].Challenge.sData,
				(xstrview){ "{}", 2u }, true, &T, 0u))
			{
				goto Done;
			}
			if(T.iStatus != 200u)
			{
				xacmeHttpResponseUnit(&T);
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE,
					"acme challenge response status invalid");
				goto Done;
			}
			xacmeHttpResponseUnit(&T);
			sStatus = xacmeFlowWaitStatus(pClient, Work[k].Authz.sData, NULL);
			if(sStatus == NULL) goto Done;
			bValidNow =
				(strcmp(sStatus, "valid") == 0);
			if(!bValidNow)
			{
				char sChallengeError[200];
				char sDetail[320];
				xerror* pE = xrtErrorRef(xrtGetError());
				xacmeFlowChallengeDetail(
					pClient, Work[k].Authz.sData, sChallengeError,
					sizeof(sChallengeError));
				if(getenv("XACME_DEBUG"))
				{
					printf("[dbg] authz not valid: "
						"status=%s err=%d %s challenge=%.160s\n",
						(sStatus != NULL) ? sStatus : "null",
						(int)xrtErrorKind(pE),
						(xrtErrorMessage(pE) != NULL) ?
							xrtErrorMessage(pE) : "-",
						sChallengeError);
				}
				snprintf(sDetail, sizeof(sDetail),
					"acme issue authorization status=%.32s "
					"challenge=%.180s",
					(sStatus != NULL) ? sStatus : "null",
					sChallengeError);
				xrtErrorFree(pE);
				xrtFree(sStatus);
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_CHALLENGE, sDetail);
				goto Done;
			}
			xrtFree(sStatus);
		}
		/* 工作表移交 TXT 统一清理（表满退化为立即删）。 */
		for(k = 0; k < iWorkCount; k++)
		{
			if(iTxtPending <
				(sizeof(TxtPending) / sizeof(TxtPending[0])))
			{
				TxtPending[iTxtPending].sFqdn = Work[k].sFqdn;
				TxtPending[iTxtPending].sTxt = Work[k].sTxt;
				iTxtPending++;
				Work[k].sFqdn = NULL;
				Work[k].sTxt = NULL;
			}
			else
			{
				xacmeFlowDnsRemove(
					pDns, Work[k].sFqdn, Work[k].sTxt);
				xrtFree(Work[k].sFqdn);
				xrtFree(Work[k].sTxt);
				Work[k].sFqdn = NULL;
				Work[k].sTxt = NULL;
			}
		}
	}
	xrtValueRelease(pRoot);
	pRoot = NULL;

	/* 3. 订单 ready → finalize。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, &Finalize);
		if(sStatus == NULL) goto Done;
		bool bReady = (sStatus != NULL) &&
			(strcmp(sStatus, "ready") == 0);
		xrtFree(sStatus);
		if(!bReady)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_ORDER,
				"acme issue order not ready");
			goto Done;
		}
	}
	/* CSR（首个域名为 CN；SAN 全量，含通配符原样）。 */
	{
		xacmecsrconfig Csr;
		xstrview CsrDomains[16];
		xbuffer CsrDer;
		str sCsrB64;
		xacmecertkey StackKey = {0};
		xacmecertkey* pUseKey = pClient->pCertKey;
		static const xbase64config B64Url = {
			NULL, XBASE64_URL | XBASE64_NO_PADDING };
		bool bCsrOk;
		for(i = 0u; i < iBaseCount; i++)
			CsrDomains[i] = (xstrview){ Bases[i].sData, Bases[i].iSize };
		Csr.CommonName = CsrDomains[0];
		Csr.Domains = CsrDomains;
		Csr.DomainCount = iBaseCount;
		xrtBufferInit(&CsrDer);
		if(pUseKey == NULL)
		{
			/* 无宿主密钥：生成一次性 ES256（证书密钥独立于账户
			   密钥，CA 普遍拒绝复用账户钥）。 */
			StackKey.Kind = XACME_CERT_KEY_ES256;
			bCsrOk = xacmeEs256Generate(&StackKey.Ec);
			pUseKey = &StackKey;
		}
		else
		{
			bCsrOk = true;
		}
		if(bCsrOk)
		{
			xacmeFlowCertPublic(pUseKey, &CertPublic);
			bCsrOk = xacmeCsrBuild(pUseKey, &Csr, &CsrDer);
		}
		if(bCsrOk)
		{
			/* 私钥随产物导出（没有它证书不可用）。 */
			pOut->sKeyPem = xacmeCertKeyPemWrite(pUseKey);
			bCsrOk = (pOut->sKeyPem != NULL);
		}
		/* 栈上密钥副本立即擦除。 */
		xacmeCertKeyUnit(&StackKey);
		sCsrB64 = bCsrOk ? xrtBase64EncodeNew(
			CsrDer.Data, CsrDer.Size, &B64Url) : NULL;
		xrtBufferUnit(&CsrDer);
		if(sCsrB64 == NULL)
		{
			goto Done;
		}
		xrtBufferClear(&Payload);
		bOk = xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("{\"csr\":\"")) &&
			xrtBufferAppend(
				&Payload,
				(xbytesview){
					(const uint8*)sCsrB64, strlen(sCsrB64) }) &&
				xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("\"}"));
		xrtFree(sCsrB64);
		if(!bOk)
		{
			goto Done;
		}
		bOk = false;
	}
	if(!xacmeFlowPost(
		pClient, Finalize.sData,
		(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u))
	{
		goto Done;
	}
	if((R.iStatus != 200u) && (R.iStatus != 201u))
	{
		char sDetail[300];
		snprintf(sDetail, sizeof(sDetail),
			"acme finalize status=%u body=%.200s",
			(unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
		goto Done;
	}
	xacmeHttpResponseUnit(&R);

	/* 4. 订单 valid → 证书 URL → 下载链。 */
	{
		str sStatus = xacmeFlowWaitStatus(pClient, OrderUrl.sData, NULL);
		if(sStatus == NULL) goto Done;
		bool bValid = (sStatus != NULL) && (strcmp(sStatus, "valid") == 0);
		char sDetail[320];
		snprintf(sDetail, sizeof(sDetail),
			"acme order not valid after finalize (state=%s)",
			(sStatus != NULL) ? sStatus : "null");
		xrtFree(sStatus);
		if(!bValid)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_FINALIZE, sDetail);
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, OrderUrl.sData, &R))
	{
		goto Done;
	}
	{
		if(R.iStatus != 200u)
		{
			xacmeHttpResponseUnit(&R);
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
				"acme certificate order response status invalid");
			goto Done;
		}
		xvalue* pOrderRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_CERTIFICATE,
			"acme certificate order json invalid");
		if(pOrderRoot == NULL)
		{
			xacmeHttpResponseUnit(&R);
			goto Done;
		}
		bool bCertUrl =
			xacmeJsonValueText(pOrderRoot, "certificate", &Certificate);
		if(pOrderRoot != NULL)
		{
			xrtValueRelease(pOrderRoot);
		}
		xacmeHttpResponseUnit(&R);
		if(!bCertUrl)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_CERTIFICATE,
				"acme issue order missing certificate url");
			goto Done;
		}
	}
	if(!xacmeFlowPostAsGet(pClient, Certificate.sData, &R))
	{
		goto Done;
	}
	if(!xacmeFlowCertificate(&R, &CertPublic, Bases, iBaseCount,
		(xbytesview){ NULL, 0u }, &pLeafDer, &iLeafDerSize))
	{
		xacmeHttpResponseUnit(&R);
		goto Done;
	}
	pOut->sFullchainPem = R.sBody;
	R.sBody = NULL;
	/* 备用链：Link 头携带 rel="alternate" 的第二下载地址。 */
	if(bAlt && (R.sLink != NULL))
	{
		char sAlternate[512];
		char sReference[512];
		if(xacmeFlowLinkAlternate(R.sLink, sReference, sizeof(sReference)) &&
			xacmeFlowAlternateUrl(Certificate.sData, sReference,
				sAlternate, sizeof(sAlternate)))
		{
			xacmehttpresponse Alt;
			if(xacmeFlowPostAsGet(pClient, sAlternate, &Alt) &&
				xacmeFlowCertificate(&Alt, &CertPublic, Bases, iBaseCount,
					(xbytesview){ pLeafDer, iLeafDerSize }, NULL, NULL))
			{
				xrtFree(pOut->sFullchainPem);
				pOut->sFullchainPem = Alt.sBody;
				Alt.sBody = NULL;
			}
			/* 备用链失败不阻断：主链始终有效。 */
			xacmeHttpResponseUnit(&Alt);
			xrtClearError();
		}
		else xrtClearError();
	}
	xacmeHttpResponseUnit(&R);
	bResult = true;

Done:
	if(pUncertainError == NULL && !bResult)
		pUncertainError = xrtErrorRef(xrtGetError());
	xrtFree(pLeafDer);
	/* TXT 统一清理：全部授权验证完成（或中途失败）后统一 Remove，
	   期间多值并存规避次级验证器缓存竞态。 */
	{
		size_t k;
		for(k = 0; k < iWorkCount; k++)
		{
			if(Work[k].sFqdn == NULL)
				continue;
			xacmeFlowDnsRemove(pDns, Work[k].sFqdn, Work[k].sTxt);
			xrtFree(Work[k].sFqdn);
			xrtFree(Work[k].sTxt);
		}

		for(k = 0; k < iTxtPending; k++)
		{
			xacmeFlowDnsRemove(
				pDns, TxtPending[k].sFqdn, TxtPending[k].sTxt);
			xrtFree(TxtPending[k].sFqdn);
			xrtFree(TxtPending[k].sTxt);
		}
	}
	if(pRoot != NULL)
	{
		xrtValueRelease(pRoot);
	}
	xrtBufferUnit(&Payload);
	/* Cleanup must not replace the primary failure with a DNS removal error. */
	if(pUncertainError != NULL)
		xrtSetErrorTake(pUncertainError);
	if(!bResult)
	{
		/* The common grant destructor also erases the exported private key. */
		xrtAcmeGrantUnit(pOut);
	}
	return bResult;
}

#endif

#if defined(XACME_FEATURE_ACME_STORE)
bool xacmeClientIssueStored(
	xacmeclient* pClient, const xstrview* pDomains, size_t iDomainCount,
	const struct xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	bool bNeed = true;
	char sPrimary[256];
	if((pClient == NULL) || (pDomains == NULL) || (iDomainCount == 0u) ||
		(pbRenewed == NULL) || (pOut == NULL) ||
		(pDomains[0].Data == NULL) || (pDomains[0].Size == 0u) ||
		(pDomains[0].Size >= sizeof(sPrimary)) ||
		(memchr(pDomains[0].Data, '\0', pDomains[0].Size) != NULL))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme issue stored requires client, domains and outputs");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	*pbRenewed = false;
	memcpy(sPrimary, pDomains[0].Data, pDomains[0].Size);
	sPrimary[pDomains[0].Size] = 0;
	if((sStoreRoot == NULL) || (sStoreRoot[0] == 0u))
	{
		sStoreRoot = ".";
	}
	if(!xrtAcmeStoreNeedRenew(
			sStoreRoot, sPrimary, iRenewalDays, &bNeed))
	{
		return false;
	}
	if(!bNeed)
	{
		return xrtAcmeStoreLoadGrant(sStoreRoot, sPrimary, pOut);
	}
	if(!xacmeClientIssue(
			pClient, pDomains, iDomainCount, pDns,
			pOut, false))
	{
		return false;
	}
	if(!xrtAcmeStoreSaveGrant(
			sStoreRoot, sPrimary, pOut, pClient->sDirectoryUrl))
	{
		/* 落盘失败不作废已签证书；报告错误由调用方权衡。 */
		xrtAcmeGrantUnit(pOut);
		return false;
	}
	*pbRenewed = true;
	return true;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
bool xacmeClientRevoke(xacmeclient* pClient, cstr sCertPem, int iReason)
{
	xacmehttpresponse R;
	xpemblock Block;
	size_t iDerSize = 0u;
	bytes pDer = NULL;
	str sCertB64 = NULL;
	xbuffer Payload;
	static const xbase64config B64Url = {
		NULL, XBASE64_URL | XBASE64_NO_PADDING };
	bool bOk = false;

	if((pClient == NULL) || (sCertPem == NULL) || (sCertPem[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme revoke requires client and certificate");
		return false;
	}
	if(pClient->sRevokeCert[0] == '\0')
	{
		xacmeFlowError(
			XERR_UNSUPPORTED, XACME_FLOW_ERROR_PROTOCOL,
			"acme revoke requires directory revokeCert endpoint");
		return false;
	}
	if(!xrtPemFind(sCertPem, strlen(sCertPem), "CERTIFICATE", &Block) ||
		((pDer = xrtPemDecodeNew(&Block, &iDerSize)) == NULL))
	{
		return false;
	}
	sCertB64 = xrtBase64EncodeNew(pDer, iDerSize, &B64Url);
	xrtFree(pDer);
	if(sCertB64 == NULL)
	{
		return false;
	}
	xrtBufferInit(&Payload);
	bOk = xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("{\"certificate\":\"")) &&
		xrtBufferAppend(&Payload,
			(xbytesview){ (const uint8*)sCertB64, strlen(sCertB64) }) &&
		xrtBufferAppend(&Payload, XRT_BYTES_LITERAL("\""));
	if(bOk && (iReason >= 0))
	{
		char sReason[24];
		snprintf(sReason, sizeof(sReason), ",\"reason\":%d", iReason);
		bOk = xrtBufferAppend(&Payload,
			(xbytesview){ (const uint8*)sReason, strlen(sReason) });
	}
	if(bOk)
	{
		bOk = xrtBufferAppendByte(&Payload, (uint8)'}');
	}
	if(bOk)
	{
		bOk = xacmeFlowPost(pClient, pClient->sRevokeCert,
			(xstrview){ (cstr)Payload.Data, Payload.Size }, true, &R, 0u);
	}
	xrtBufferUnit(&Payload);
	xrtFree(sCertB64);
	if(!bOk)
	{
		return false;
	}
	/* 200 = 已吊销；400 + alreadyRevoked 视为幂等成功。 */
	if(R.iStatus == 200u)
	{
		xacmeHttpResponseUnit(&R);
		return true;
	}
	if(R.iStatus == 400u)
	{
		xacmeflowurl Type;
		if(!xacmeFlowProblemType(&R, &Type))
		{
			xacmeHttpResponseUnit(&R);
			return false;
		}
		if(strcmp(Type.sData, "urn:ietf:params:acme:error:alreadyRevoked") == 0)
		{
			xacmeHttpResponseUnit(&R);
			return true;
		}
	}
	{
		char sDetail[240];
		snprintf(sDetail, sizeof(sDetail),
			"acme revoke status=%u body=%.160s", (unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_PROTOCOL, sDetail);
	}
	return false;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
typedef enum xacmerolloverlookup {
	XACME_ROLLOVER_LOOKUP_UNKNOWN = 0,
	XACME_ROLLOVER_LOOKUP_MATCH,
	XACME_ROLLOVER_LOOKUP_ABSENT,
	XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT
} xacmerolloverlookup;

/* RFC 8555 §7.3.1：只查询新钥是否已绑定当前账户，不创建账户。 */
static xacmerolloverlookup xacmeFlowRolloverLookup(
	xacmeclient* pClient, const xacmees256key* pNewKey)
{
	static const char* sPayload = "{\"onlyReturnExisting\":true}";
	uint32 uAttempt;
	for(uAttempt = 0u; uAttempt < 3u; uAttempt++)
	{
		xacmejwsheader H;
		xacmehttpresponse R;
		str sToken;
		xacmerolloverlookup Result = XACME_ROLLOVER_LOOKUP_UNKNOWN;
		bool bBadNonce;
		if(!xacmeFlowNewNonce(pClient))
			return Result;
		H.Nonce = (xstrview){
			pClient->sNonce, strlen(pClient->sNonce) };
		H.Url = (xstrview){
			pClient->sNewAccount, strlen(pClient->sNewAccount) };
		H.Kid = (xstrview){ NULL, 0u };
		sToken = xacmeJwsEs256(pNewKey, &H,
			(xstrview){ sPayload, strlen(sPayload) });
		if(sToken == NULL)
			return Result;
		pClient->sNonce[0] = '\0';
		if(!xacmeHttpExchange(&pClient->Http, "POST",
				pClient->sNewAccount, "application/jose+json",
				(xstrview){ sToken, strlen(sToken) }, &R))
		{
			bool bRetry = xacmeFlowHttpUncertain(xrtGetError());
			xrtFree(sToken);
			if(bRetry && (uAttempt + 1u < 3u))
			{
				xrtClearError();
				continue;
			}
			return Result;
		}
		xrtFree(sToken);
		xacmeFlowTakeNonce(pClient, &R);
		bBadNonce = false;
		if(R.iStatus == 400u)
		{
			xacmeflowurl Type;
			if(!xacmeFlowProblemType(&R, &Type))
			{
				xacmeHttpResponseUnit(&R);
				return Result;
			}
			bBadNonce = strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0;
			if(strcmp(Type.sData, "urn:ietf:params:acme:error:accountDoesNotExist") == 0)
				Result = XACME_ROLLOVER_LOOKUP_ABSENT;
		}
		else if((R.iStatus == 200u) && (R.sLocation != NULL) && R.sLocation[0])
		{
			xvalue* pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover lookup account json invalid");
			xacmeflowurl Status;
			if(pRoot == NULL)
			{
				xacmeHttpResponseUnit(&R);
				return Result;
			}
			if(xacmeJsonValueText(pRoot, "status", &Status) && strcmp(Status.sData, "valid") == 0)
				Result = strcmp(R.sLocation, pClient->sKid) == 0 ?
					XACME_ROLLOVER_LOOKUP_MATCH : XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT;
			xrtValueRelease(pRoot);
		}
		xacmeHttpResponseUnit(&R);
		if(bBadNonce)
		{
			if(uAttempt + 1u == 3u || pClient->sNonce[0] == 0)
			{
				xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
					"acme rollover key lookup nonce retries exhausted");
				return XACME_ROLLOVER_LOOKUP_UNKNOWN;
			}
			continue;
		}
		if(Result != XACME_ROLLOVER_LOOKUP_UNKNOWN)
			xrtClearError();
		else
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover key lookup response inconclusive");
		return Result;
	}
	xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
		"acme rollover key lookup retry exhausted");
	return XACME_ROLLOVER_LOOKUP_UNKNOWN;
}

bool xacmeClientRollover(
	xacmeclient* pClient, cstr sNewKeyPem, cstr sStoreRoot)
{
	xacmees256key NewKey;
	str sOldJwk = NULL;
	str sInner = NULL;
	str sOuter = NULL;
	xacmerolloverlookup Lookup;
	bool bOk = false;

	if((pClient == NULL) || (sNewKeyPem == NULL) ||
		(sNewKeyPem[0] == '\0'))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme rollover requires client and new key pem");
		return false;
	}
	if(pClient->sKeyChange[0] == '\0')
	{
		xacmeFlowError(
			XERR_UNSUPPORTED, XACME_FLOW_ERROR_PROTOCOL,
			"acme rollover requires directory keyChange endpoint");
		return false;
	}
	if(!xacmeKeyPemRead(sNewKeyPem, strlen(sNewKeyPem), &NewKey))
	{
		return false;
	}
	/* 进程重启后重试同一新钥时，先确认服务器是否已经换钥。 */
	Lookup = xacmeFlowRolloverLookup(pClient, &NewKey);
	if(Lookup == XACME_ROLLOVER_LOOKUP_MATCH)
		goto ApplyNewKey;
	if(Lookup != XACME_ROLLOVER_LOOKUP_ABSENT)
	{
		if(Lookup == XACME_ROLLOVER_LOOKUP_OTHER_ACCOUNT)
			xacmeFlowError(XERR_STATE, XACME_FLOW_ERROR_ACCOUNT,
				"acme rollover new key belongs to another account");
		goto Done;
	}
	sOldJwk = xacmeJwkEcJson(&pClient->AccountKey);
	if(sOldJwk == NULL)
	{
		goto Done;
	}
	/* RFC 8555 §7.3.5：内层由新钥签名并嵌新 JWK。 */
	{
		xbuffer Payload;
		xacmejwsheader Inner;
		xrtBufferInit(&Payload);
		if(!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL("{\"account\":")) ||
			!xacmeFlowJsonQuoteAppend(
				&Payload,
				(xstrview){ pClient->sKid, strlen(pClient->sKid) }) ||
			!xrtBufferAppend(
				&Payload, XRT_BYTES_LITERAL(",\"oldKey\":")) ||
			!xrtBufferAppend(
				&Payload,
				(xbytesview){
					(const uint8*)sOldJwk, strlen(sOldJwk) }) ||
			!xrtBufferAppendByte(&Payload, (uint8)'}'))
		{
			xrtBufferUnit(&Payload);
			goto Done;
		}
		Inner.Nonce.Data = NULL;
		Inner.Nonce.Size = 0u;
		Inner.Url = (xstrview){
			pClient->sKeyChange, strlen(pClient->sKeyChange) };
		Inner.Kid = (xstrview){ NULL, 0u };
		sInner = xacmeJwsEs256(
			&NewKey, &Inner,
			(xstrview){ (cstr)Payload.Data, Payload.Size });
		xrtBufferUnit(&Payload);
	}
	if(sInner == NULL)
	{
		goto Done;
	}
	/* 外层由旧账户钥签名，保护头携带 kid + nonce；badNonce 时
	   只重建外层 JWS，内层无 nonce 可以复用。 */
	{
		uint32 uNonceRetry;
		bool bPosted = false;
		for(uNonceRetry = 0u; uNonceRetry < 3u; uNonceRetry++)
		{
			xacmehttpresponse R;
			xacmejwsheader Outer;
			if(!xacmeFlowNewNonce(pClient))
			{
				goto Done;
			}
			Outer.Nonce = (xstrview){
				pClient->sNonce, strlen(pClient->sNonce) };
			Outer.Url = (xstrview){
				pClient->sKeyChange, strlen(pClient->sKeyChange) };
			Outer.Kid = (xstrview){
				pClient->sKid, strlen(pClient->sKid) };
			xrtFree(sOuter);
			sOuter = xacmeJwsEs256(
				&pClient->AccountKey, &Outer,
				(xstrview){ sInner, strlen(sInner) });
			if(sOuter == NULL)
			{
				goto Done;
			}
			pClient->sNonce[0] = 0;
			if(!xacmeHttpExchange(
					&pClient->Http, "POST", pClient->sKeyChange,
					"application/jose+json",
					(xstrview){ sOuter, strlen(sOuter) }, &R))
			{
				if(xacmeFlowHttpUncertain(xrtGetError()))
				{
					xerror* pWriteError = xrtErrorRef(xrtGetError());
					Lookup = xacmeFlowRolloverLookup(pClient, &NewKey);
					if(Lookup == XACME_ROLLOVER_LOOKUP_MATCH)
					{
						xrtErrorFree(pWriteError);
						xrtClearError();
						goto ApplyNewKey;
					}
					xrtSetErrorTake(pWriteError);
				}
				goto Done;
			}
			xacmeFlowTakeNonce(pClient, &R);
			if(R.iStatus == 400u)
			{
				xacmeflowurl Type;
				if(!xacmeFlowProblemType(&R, &Type))
				{
					xacmeHttpResponseUnit(&R);
					goto Done;
				}
				if(strcmp(Type.sData, "urn:ietf:params:acme:error:badNonce") == 0)
				{
					xacmeHttpResponseUnit(&R);
					if(pClient->sNonce[0] == 0)
					{
						xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
							"acme rollover badNonce response missing fresh nonce");
						goto Done;
					}
					continue;
				}
			}
			if(R.iStatus != 200u)
			{
				char sDetail[220];
				snprintf(sDetail, sizeof(sDetail),
					"acme rollover status=%u body=%.150s",
					(unsigned)R.iStatus,
					(R.sBody != NULL) ? R.sBody : "");
				xacmeHttpResponseUnit(&R);
				xacmeFlowError(
					XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
				goto Done;
			}
			xacmeHttpResponseUnit(&R);
			bPosted = true;
			break;
		}
		if(!bPosted)
		{
			xacmeFlowError(
				XERR_PROTOCOL, XACME_FLOW_ERROR_NONCE,
				"acme rollover nonce retries exhausted");
			goto Done;
		}
	}
ApplyNewKey:
	/* 200 响应或只读账户查询证明新钥生效后，才替换内存与存储。 */
	xrtClearError();
	xrtSecureZero(&pClient->AccountKey, sizeof(pClient->AccountKey));
	pClient->AccountKey = NewKey;
	memset(&NewKey, 0, sizeof(NewKey));
	bOk = true;
	/* store 重存失败不能撤销远端换钥；内存继续使用新钥，返回失败
	   让调用方知道持久化尚未完成，并可用同一新钥重试对账。 */
	if((sStoreRoot != NULL) && (sStoreRoot[0] != 0))
	{
#if defined(XACME_FEATURE_ACME_STORE)
		str sNewAccountPem = xacmeKeyPemWrite(&pClient->AccountKey);
		bool bSaved = (sNewAccountPem != NULL) && xrtAcmeStoreSaveAccount(
			sStoreRoot, pClient->sDirectoryUrl, sNewAccountPem);
		xrtFree(sNewAccountPem);
		if(!bSaved)
		{
			bOk = false;
		}
#else
		xacmeFlowError(XERR_UNSUPPORTED, XACME_FLOW_ERROR_STORE,
			"acme rollover store support is not enabled");
		bOk = false;
#endif
	}

Done:
	/* 失败时擦除新钥副本。 */
	xrtSecureZero(&NewKey, sizeof(NewKey));
	xrtFree(sOldJwk);
	xrtFree(sInner);
	xrtFree(sOuter);
	return bOk;
}
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
bool xacmeClientDeactivate(xacmeclient* pClient)
{
	xacmehttpresponse R;
	if(pClient == NULL)
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme deactivate requires client");
		return false;
	}
	if(pClient->sKid[0] == 0)
	{
		xacmeFlowError(
			XERR_STATE, XACME_FLOW_ERROR_ACCOUNT,
			"acme deactivate requires active account");
		return false;
	}
	if(!xacmeFlowPost(
			pClient, pClient->sKid,
			XRT_STR_LITERAL("{\"status\":\"deactivated\"}"), true, &R,
			0u))
	{
		return false;
	}
	/* 200 = 已停用或刚停用；幂等成功。 */
	if(R.iStatus == 200u)
	{
		xvalue* pRoot = xacmeFlowResponseObject(&R, XACME_FLOW_ERROR_ACCOUNT,
			"acme deactivate account json invalid");
		xacmeflowurl Status;
		bool bDeactivated;
		xacmeHttpResponseUnit(&R);
		if(pRoot == NULL) return false;
		bDeactivated = xacmeJsonValueText(pRoot, "status", &Status) &&
			strcmp(Status.sData, "deactivated") == 0;
		xrtValueRelease(pRoot);
		if(!bDeactivated)
			xacmeFlowError(XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT,
				"acme deactivate account is not deactivated");
		return bDeactivated;
	}
	{
		char sDetail[220];
		snprintf(sDetail, sizeof(sDetail),
			"acme deactivate status=%u body=%.150s", (unsigned)R.iStatus,
			(R.sBody != NULL) ? R.sBody : "");
		xacmeHttpResponseUnit(&R);
		xacmeFlowError(
			XERR_PROTOCOL, XACME_FLOW_ERROR_ACCOUNT, sDetail);
	}
	return false;
}
#endif

/* ---------------- 公开客户端 API（xrt/acme_client.h） ---------------- */

#if defined(XACME_FEATURE_ACME_FLOW)

void xrtAcmeClientConfigInit(xacmeclientconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client config init requires config");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}

struct xacmeclient* xrtAcmeClientCreate(
	const xacmeclientconfig* pConfig)
{
	static const char* sDefaults[XACME_FLOW_RESOLVER_MAX] = {
		"223.5.5.5", "119.29.29.29", "8.8.8.8", NULL
	};
	const cstr* sResolvers = NULL;
	size_t iResolverCount = 0u;
	xacmeclient* pClient;
	size_t i;

	if((pConfig == NULL) || (pConfig->pAccount == NULL))
	{
		xacmeFlowError(
			XERR_ARGUMENT, XACME_FLOW_ERROR_ARGUMENT,
			"acme client create requires config with account");
		return NULL;
	}
	if((pConfig->sPropagateResolvers != NULL) &&
		(pConfig->iPropagateResolverCount != 0u))
	{
		sResolvers = pConfig->sPropagateResolvers;
		iResolverCount = pConfig->iPropagateResolverCount;
	}
	else
	{
		sResolvers = sDefaults;
		iResolverCount = 3u;
	}
	/* Init 可在参数校验时返回；失败回滚也必须看到一个有效的空对象。 */
	pClient = (xacmeclient*)xrtCalloc(1u, sizeof(*pClient));
	if(pClient == NULL)
	{
		return NULL;
	}
	if(!xacmeClientInit(
			pClient, pConfig->pBorrowedEngine, pConfig->sCaPem,
			pConfig->pAccount, pConfig->uTimeoutMs))
	{
		xacmeClientDiscard(pClient);
		return NULL;
	}
	if((pConfig->sCertKeyPem != NULL) && (pConfig->sCertKeyPem[0] != 0))
	{
		pClient->pCertKey = (xacmecertkey*)xrtMalloc(
			sizeof(*pClient->pCertKey));
		if((pClient->pCertKey == NULL) ||
			!xacmeCertKeyReadPem(
				pConfig->sCertKeyPem, strlen(pConfig->sCertKeyPem),
				pClient->pCertKey))
		{
			xacmeCertKeyUnit(pClient->pCertKey);
			xrtFree(pClient->pCertKey);
			pClient->pCertKey = NULL;
			if(pClient->Http.uTimeoutMs < INT64_C(30000))
				pClient->Http.uTimeoutMs = INT64_C(30000);
			xacmeClientDiscard(pClient);
			return NULL;
		}
	}
	for(i = 0; i < iResolverCount; i++)
	{
		if((sResolvers[i] == NULL) ||
			(strlen(sResolvers[i]) >=
				sizeof(pClient->sPropagateResolvers[0])))
		{
			continue;
		}
		strcpy(pClient->sPropagateResolvers[
			pClient->iPropagateResolverCount], sResolvers[i]);
		pClient->iPropagateResolverCount++;
		if(pClient->iPropagateResolverCount >=
			XACME_FLOW_RESOLVER_MAX)
		{
			break;
		}
	}
	pClient->uPropagateTimeoutMs = (pConfig->uPropagateTimeoutMs != 0u) ?
		pConfig->uPropagateTimeoutMs : XACME_FLOW_PROPAGATE_TIMEOUT_MS;
	pClient->uIssueTimeoutMs = pConfig->uIssueTimeoutMs;
	return pClient;
}

bool xrtAcmeClientCleanup(struct xacmeclient* pClient)
{
	if(pClient == NULL)
	{
		return true;
	}
	if(pClient->pCertKey != NULL)
	{
		xacmeCertKeyUnit(pClient->pCertKey);
		xrtFree(pClient->pCertKey);
		pClient->pCertKey = NULL;
	}
	return xacmeClientUnit(pClient);
}

void xacmeClientDiscard(xacmeclient* pClient)
{
	if(pClient == NULL) return;
	if(xrtAcmeClientCleanup(pClient)) xrtFree(pClient);
	else xacmeHttpDeferOwner(&pClient->Http, sizeof(*pClient));
}

void xrtAcmeClientDestroy(struct xacmeclient* pClient)
{
	if(xrtAcmeClientCleanup(pClient)) xrtFree(pClient);
}

str xrtAcmeClientAccountPem(const struct xacmeclient* pClient)
{
	return xacmeClientAccountPem(pClient);
}

bool xrtAcmeClientIssue(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	xacmeissuegrant* pOut)
{
	return xrtAcmeClientIssueEx(
		pClient, pDomains, iDomainCount, pDns, false, pOut);
}

bool xrtAcmeClientIssueEx(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	bool bPreferAlternate, xacmeissuegrant* pOut)
{
	return xacmeClientIssue(
		pClient, pDomains, iDomainCount, pDns, pOut, bPreferAlternate);
}

bool xrtAcmeClientRollover(
	struct xacmeclient* pClient, cstr sNewKeyPem, cstr sStoreRoot)
{
	return xacmeClientRollover(pClient, sNewKeyPem, sStoreRoot);
}

bool xrtAcmeClientDeactivate(struct xacmeclient* pClient)
{
	return xacmeClientDeactivate(pClient);
}

bool xrtAcmeClientRevoke(
	struct xacmeclient* pClient, cstr sCertPem, int iReason)
{
	return xacmeClientRevoke(pClient, sCertPem, iReason);
}

#endif

#if defined(XACME_FEATURE_ACME_FLOW) && defined(XACME_FEATURE_ACME_STORE)

bool xrtAcmeClientIssueStored(
	struct xacmeclient* pClient, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns, cstr sStoreRoot,
	int iRenewalDays, xacmeissuegrant* pOut, bool* pbRenewed)
{
	return xacmeClientIssueStored(
		pClient, pDomains, iDomainCount, pDns, sStoreRoot, iRenewalDays,
		pOut, pbRenewed);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_cf.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_CF)

#if defined(XACME_FEATURE_DNS_CF)



#include <stdlib.h>

/*
	Cloudflare DNS provider（API v4）：
	  - 认证：Authorization: Bearer <API Token>；
	  - zone 发现：GET /zones?name=<候选>（精确匹配，逐级上探）；
	  - 加 TXT：POST /zones/<id>/dns_records（content 不带引号）；
	  - 删 TXT：DELETE /zones/<id>/dns_records/<record id>。
*/

typedef struct xacmednscfcontext {
	xacmehttp Http;
	xmutex Lock;
	char sToken[200];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
} xacmednscfcontext;

static void xacmeCfError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

/* 执行一次 API 调用；pBody 为空表示无请求体。 */
static bool xacmeCfCall(
	xacmednscfcontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sAuth[240];
	char sUrl[512];
	xacmehttpheader Extra[1];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	snprintf(sAuth, sizeof(sAuth), "Bearer %s", pCtx->sToken);
	if(!xacmeDnsHttpsUrl(sUrl, sizeof(sUrl), pCtx->sEndpoint,
			sPathAndQuery))
	{
		return false;
	}
	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl, "application/json",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 1u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	if(pOutSize != NULL) *pOutSize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 在 zones 列表里按 name 精确查 zone id。 */
static xacmednszoneresult xacmeCfZoneId(
	xacmednscfcontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	xacmednszoneresult Result;

	snprintf(sPath, sizeof(sPath),
		"/client/v4/zones?name=%s&per_page=5", sZone);
	if(!xacmeCfCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody, &iBodySize))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(iStatus == 200u)
		Result = xacmeDnsJsonZoneId(
			(xstrview){ sBody, iBodySize },
			"result", sZone, true, 5u, sOutId, iIdCap);
	else
	{
		Result = XACME_DNS_ZONE_ERROR;
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION :
			(iStatus == 429u || iStatus >= 500u) ? XERR_AGAIN : XERR_PROTOCOL,
			"acme dns_cf zone query rejected or unavailable");
	}
	xrtFree(sBody);
	return Result;
}

/* 每次从完整属主查找最近的托管区域；父区域不能证明子区域不存在。 */
static bool xacmeCfFindZone(
	xacmednscfcontext* pCtx, cstr sFqdn, char* sOutZone, size_t iZoneCap,
	char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	xacmednszoneresult Lookup;
	snprintf(sCandidate, sizeof(sCandidate), "%s", sFqdn);
	for(;;)
	{
		char* sDot;
		Lookup = xacmeCfZoneId(pCtx, sCandidate, sOutId, iIdCap);
		if(Lookup == XACME_DNS_ZONE_FOUND)
		{
			snprintf(sOutZone, iZoneCap, "%s", sCandidate);
			return true;
		}
		if(Lookup == XACME_DNS_ZONE_ERROR) return false;
		sDot = strchr(sCandidate, '.');
		if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL)) break;
		memmove(sCandidate, sDot + 1, strlen(sDot + 1) + 1u);
	}
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
		"acme dns_cf managed zone not found");
	return false;
}

static bool xacmeCfCreateRejected(const xvalue* pRoot, uint16 iStatus)
{
	if(pRoot == NULL || !xrtValueIs(pRoot, XVALUE_OBJECT)) return false;
	xvalue* pSuccess = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success"));
	xvalue* pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	xvalue* pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
	bool bSuccess = true;
	size_t i;
	if(iStatus < 400u || iStatus >= 500u || !xrtValueGetBool(pSuccess, &bSuccess) || bSuccess ||
		(pResult != NULL && !xrtValueIs(pResult, XVALUE_NULL)) ||
		!xrtValueIs(pErrors, XVALUE_ARRAY) || xrtValueCount(pErrors) == 0u) return false;
	for(i = 0u; i < xrtValueCount(pErrors); i++) {
		xvalue* pError = xrtValueArrayGet(pErrors, i);
		char sMessage[512]; int64 iCode = 0;
		if(!xrtValueIs(pError, XVALUE_OBJECT) ||
			!xrtValueGetInt(xrtValueObjectGet(pError, XRT_STR_LITERAL("code")), &iCode) || iCode <= 0 ||
			!xacmeDnsJsonText(pError, "message", sMessage, sizeof(sMessage)) || sMessage[0] == '\0') return false;
	}
	return true;
}

static bool xacmeCfAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZone[256];
	char sZoneId[64];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot = NULL;
	char sRecordId[64];
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xacmeCfError(XERR_ARGUMENT,
			"acme dns_cf owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xacmeCfError(XERR_RANGE,
			"acme dns_cf record tracking capacity exhausted");
		return false;
	}
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	if(!xacmeCfFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}

	xrtBufferInit(&Body);
	if(xrtBufferAppend(&Body, XRT_BYTES_LITERAL("{\"type\":\"TXT\",\"name\":")) &&
		xacmeDnsJsonQuote(&Body, sFqdn) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"content\":")) &&
		xacmeDnsJsonQuote(&Body, sTxt) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"ttl\":60}")) &&
		xrtBufferAppendByte(&Body, 0u))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/client/v4/zones/%s/dns_records",
			sZoneId);
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		bSent = true;
		bOk = xacmeCfCall(pCtx, "POST", sPath, (cstr)Body.Data, &iStatus,
			&sResp, &iRespSize);
	}
	xrtBufferUnit(&Body);
	if(!bOk)
	{
		xrtFree(sResp);
		if(!bSent) return false;
		if(!pCtx->Http.bWriteUncertain) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			return false;
		}
		return xacmeDnsCreateUncertainError();
	}
	/* 提取 result.id 供 Remove（记录句柄 = "zoneid/recordid"）。 */
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	}
	if(xacmeCfCreateRejected(pRoot, iStatus)) {
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
		xrtValueRelease(pRoot); xrtFree(sResp);
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_cf create was rejected");
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u && xacmeDnsJsonSuccess(pRoot))
	{
		xvalue* pResult = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
		xvalue* pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
		if((pResult != NULL) && xrtValueIs(pResult, XVALUE_OBJECT) &&
			xrtValueIs(pErrors, XVALUE_ARRAY) && xrtValueCount(pErrors) == 0u &&
			xacmeDnsJsonEqual(pResult, "name", sFqdnText) &&
			xacmeDnsJsonEqual(pResult, "type", "TXT") &&
			xacmeDnsJsonEqual(pResult, "content", sTxtText) &&
			xacmeDnsJsonPathId(pResult, "id", sRecordId, sizeof(sRecordId)))
		{
			bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
				iSlot, sZoneId, '/', sRecordId);
		}
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

/* GET requires the full envelope; DELETE's documented response also permits
 * just result.id. Present success/errors fields must never contradict it. */
static bool xacmeCfRecordResponseOk(const xvalue* pRoot, bool bFull)
{
	xvalue* pSuccess;
	xvalue* pErrors;
	bool bSuccess = false;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT)) return false;
	pSuccess = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("success"));
	pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	if((bFull || pSuccess != NULL) &&
		(!xrtValueGetBool(pSuccess, &bSuccess) || !bSuccess)) return false;
	return !(bFull || pErrors != NULL) ||
		(xrtValueIs(pErrors, XVALUE_ARRAY) && xrtValueCount(pErrors) == 0u);
}

/* A bare 404 can be an authentication, zone or proxy failure. Only the DNS
 * record-not-found error in a non-contradictory envelope proves absence. */
static bool xacmeCfRecordMissing(const xvalue* pRoot, uint16 iStatus)
{
	xvalue* pErrors;
	size_t i;
	if(iStatus != 404u || !xacmeCfCreateRejected(pRoot, iStatus)) return false;
	pErrors = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("errors"));
	for(i = 0u; i < xrtValueCount(pErrors); i++) {
		int64 iCode = 0;
		if(!xrtValueGetInt(xrtValueObjectGet(xrtValueArrayGet(pErrors, i),
			XRT_STR_LITERAL("code")), &iCode) || iCode != 81044) return false;
	}
	return true;
}

typedef enum xacmecfrecordresult {
	XACME_CF_RECORD_ERROR,
	XACME_CF_RECORD_FOUND,
	XACME_CF_RECORD_MISSING
} xacmecfrecordresult;

static xacmecfrecordresult xacmeCfReadRecord(xacmednscfcontext* pCtx,
	cstr sPath, cstr sRecordId, xstrview sFqdn, xstrview sTxt)
{
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xacmecfrecordresult Result = XACME_CF_RECORD_ERROR;
	char sOwner[256], sValue[208];
	/* These views have already passed challenge validation. */
	memcpy(sOwner, sFqdn.Data, sFqdn.Size); sOwner[sFqdn.Size] = '\0';
	memcpy(sValue, sTxt.Data, sTxt.Size); sValue[sTxt.Size] = '\0';
	xrtClearError();
	if(!xacmeCfCall(pCtx, "GET", sPath, NULL, &iStatus, &sResp, &iRespSize)) {
		xrtFree(sResp); return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	if(xacmeCfRecordMissing(pRoot, iStatus)) Result = XACME_CF_RECORD_MISSING;
	else if(iStatus == 200u && xacmeCfRecordResponseOk(pRoot, true)) {
		xvalue* pRecord = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result"));
		if(xacmeDnsJsonEqual(pRecord, "id", sRecordId) &&
			xacmeDnsJsonEqual(pRecord, "name", sOwner) &&
			xacmeDnsJsonEqual(pRecord, "type", "TXT") &&
			xacmeDnsJsonEqual(pRecord, "content", sValue)) Result = XACME_CF_RECORD_FOUND;
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(Result != XACME_CF_RECORD_ERROR) xrtClearError();
	else if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeCfError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_cf record read failed or no longer matches its tracked identity");
	return Result;
}

static xacmednsownedresult xacmeCfReuseOwned(xacmednscfcontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256];
	/* Invalid arguments are diagnosed by the ordinary create path. */
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	for(;;) {
		char sZoneId[64], sRecordId[64], sPath[200];
		size_t iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
		xacmecfrecordresult Result;
		if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
		if(!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '/', sZoneId,
				sizeof(sZoneId), sRecordId, sizeof(sRecordId))) {
			xacmeCfError(XERR_PROTOCOL, "acme dns_cf tracked record handle is invalid");
			return XACME_DNS_OWNED_ERROR;
		}
		snprintf(sPath, sizeof(sPath), "/client/v4/zones/%s/dns_records/%s", sZoneId, sRecordId);
		Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, Owner, Txt);
		if(Result == XACME_CF_RECORD_FOUND) return XACME_DNS_OWNED_VALID;
		if(Result != XACME_CF_RECORD_MISSING) return XACME_DNS_OWNED_ERROR;
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
	}
}

static bool xacmeCfDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "DNS deletion outcome is unknown; tracked record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeCfDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pContext;
	char sZoneId[64];
	char sRecordId[64];
	char sPath[200];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	xvalue* pRoot = NULL;
	xacmecfrecordresult Result;
	if(!xacmeDnsRecordSplit(sId, '/', sZoneId, sizeof(sZoneId),
			sRecordId, sizeof(sRecordId)))
	{
		xacmeCfError(XERR_PROTOCOL, "acme dns_cf record handle is invalid");
		return false;
	}
	snprintf(sPath, sizeof(sPath),
		"/client/v4/zones/%s/dns_records/%s", sZoneId, sRecordId);
	Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, sFqdn, sTxt);
	if(Result != XACME_CF_RECORD_FOUND) return Result == XACME_CF_RECORD_MISSING;
	bOk = xacmeCfCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp, &iRespSize);
	if(!bOk && !pCtx->Http.bWriteUncertain) { xrtFree(sResp); return false; }
	if(bOk) {
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
		bOk = iStatus == 200u && xacmeCfRecordResponseOk(pRoot, false) &&
			xacmeDnsJsonEqual(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("result")), "id", sRecordId);
		if(!bOk && (iStatus == 401u || iStatus == 403u) && xacmeCfCreateRejected(pRoot, iStatus) &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			xrtValueRelease(pRoot); xrtFree(sResp);
			xacmeCfError(XERR_PERMISSION, "acme dns_cf record deletion was rejected"); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) return true;
	(void)xacmeCfDeleteUncertain();
	/* Allocation failures retain both the diagnosis and handle. A subsequent
 * Remove can reconcile; no write is repeated within this call. */
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		Result = xacmeCfReadRecord(pCtx, sPath, sRecordId, sFqdn, sTxt);
		if(Result == XACME_CF_RECORD_FOUND) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return Result == XACME_CF_RECORD_MISSING;
}

static bool xacmeCfRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xacmeCfError(XERR_ARGUMENT, "acme dns_cf owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeCfDeleteRecord, pCtx);
}

static bool xacmeCfAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeCfReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeCfAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeCfRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednscfcontext* pCtx = (xacmednscfcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeCfRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

void xrtAcmeDnsCfConfigInit(xacmednscfconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sApiToken = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsCf(
	const xacmednscfconfig* pConfig, struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider)
{
	xacmednscfcontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sApiToken == NULL) || (pConfig->sApiToken[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_cf requires api token");
		return false;
	}
	if(strlen(pConfig->sApiToken) >= sizeof(pCtx->sToken) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_cf token or endpoint exceeds capacity");
		return false;
	}
	pCtx = (xacmednscfcontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sToken, sizeof(pCtx->sToken), "%s", pConfig->sApiToken);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"api.cloudflare.com");
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		return false;
	}
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		(void)xrtMutexUnit(&pCtx->Lock);
		if(xacmeHttpUnit(&pCtx->Http))
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
		return false;
	}
	pProvider->sId = "cf";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeCfAdd;
	pProvider->Remove = xacmeCfRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsCfProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednscfcontext* pCtx = (xacmednscfcontext*)pProvider->pContext;
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_tencent.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_TENCENT)

#if defined(XACME_FEATURE_DNS_TENCENT)



#include <stdarg.h>
#include <stdlib.h>

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256）：
	  - canonical：content-type/host/x-tc-action 三头（小写字典序）；
	  - StringToSign = "TC3-HMAC-SHA256\n<Unix 秒>\n"
	    "<UTC 日期>/dnspod/tc3_request\n" 后接 sha256hex(canonical)；
	  - 密钥链：HMAC("TC3"+SK, date) → "dnspod" → "tc3_request"；
	  - zone 发现：DescribeDomain，绑定不可变 DomainId；
	  - 加 TXT：CreateRecord（RecordLine 必填 "默认"）；
	  - 删 TXT：DescribeRecord 核对后 DeleteRecord（DomainId + RecordId）。
	RecordLine 的 "默认" 是 API 要求的 UTF-8 字面值。
*/

#define XACME_TENCENT_SERVICE "dnspod"
#define XACME_TENCENT_VERSION "2021-03-23"

typedef struct xacmednstencentcontext {
	xacmehttp Http;
	xmutex Lock;
	char sId[160];
	char sKey[160];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
	int64 iDomainIds[XACME_DNS_RECORD_MAX];
	double uCreatedAt[XACME_DNS_RECORD_MAX];
	bool bDeletePending[XACME_DNS_RECORD_MAX];
} xacmednstencentcontext;

static bool xacmeTencentDeleteUncertain(void);

/* 签名输入绝不能静默截断，否则线上的 Authorization 必然无效。 */
static bool xacmeTencentFormat(char* sOut, size_t iCapacity,
	cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent signature input exceeds capacity");
		return false;
	}
	return true;
}

/* TC3 签名与时间头共用同一个时刻，便于离线固定向量验证。 */
bool xacmeDnsTencentAuthorization(
	cstr sId, cstr sKey, cstr sEndpoint, cstr sAction, cstr sBody,
	xtime iNow, char* sAuth, size_t iAuthCapacity,
	char* sTimestamp, size_t iTimestampCapacity)
{
	static const char* sSignedHeaders = "content-type;host;x-tc-action";
	char sDateText[24];      /* YYYY-MM-DD */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[320];
	char sStringToSign[200];
	char sHex[XACME_SIG_HASH_TEXT];
	char sKeySeed[180];
	char sLowerAction[64];
	size_t iAction;
	xdatetime Now;
	uint8 kDate[XRT_SHA256_SIZE];
	uint8 kService[XRT_SHA256_SIZE];
	uint8 kSigning[XRT_SHA256_SIZE];
	uint8 Signature[XRT_SHA256_SIZE];
	bool bOk = false;

	if((sId == NULL) || (sKey == NULL) || (sEndpoint == NULL) ||
		(sAction == NULL) || (sBody == NULL) || (sAuth == NULL) ||
		(sTimestamp == NULL) || (iAuthCapacity == 0u) ||
		(iTimestampCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sTimestamp[0] = '\0';
	if(!xacmeTencentFormat(sLowerAction, sizeof(sLowerAction), "%s", sAction)) return false;
	/* TC3 canonical header values are lowercase, including X-TC-Action. */
	for(iAction = 0u; sLowerAction[iAction] != '\0'; iAction++)
		if(sLowerAction[iAction] >= 'A' && sLowerAction[iAction] <= 'Z')
			sLowerAction[iAction] = (char)(sLowerAction[iAction] + ('a' - 'A'));
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeTencentFormat(sDateText, sizeof(sDateText),
			"%04ld-%02d-%02d", (long)Now.Year, Now.Month, Now.Day) ||
		!xacmeTencentFormat(sTimestamp, iTimestampCapacity, "%lld",
			(long long)xrtTimeUnix(iNow)))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sBody, strlen(sBody), sPayloadHash))
	{
		return false;
	}
	if(!xacmeTencentFormat(sHeaders, sizeof(sHeaders),
		"content-type:application/json; charset=utf-8\nhost:%s\n"
		"x-tc-action:%s\n",
		sEndpoint, sLowerAction))
	{
		return false;
	}
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), "POST", "/",
			"", sHeaders, sSignedHeaders, sPayloadHash))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex))
	{
		return false;
	}
	if(!xacmeTencentFormat(sStringToSign, sizeof(sStringToSign),
		"TC3-HMAC-SHA256\n%s\n%s/" XACME_TENCENT_SERVICE
		"/tc3_request\n%s",
		sTimestamp, sDateText, sHex))
	{
		return false;
	}
	if(!xacmeTencentFormat(sKeySeed, sizeof(sKeySeed), "TC3%s", sKey))
	{
		xrtSecureZero(sKeySeed, sizeof(sKeySeed));
		return false;
	}
	bOk = xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
		sDateText, strlen(sDateText), kDate) &&
		xacmeSigHmac(kDate, sizeof(kDate), XACME_TENCENT_SERVICE,
			strlen(XACME_TENCENT_SERVICE), kService) &&
		xacmeSigHmac(kService, sizeof(kService), "tc3_request",
			sizeof("tc3_request") - 1u,
			kSigning) &&
		xacmeSigHmac(kSigning, sizeof(kSigning), sStringToSign,
			strlen(sStringToSign), Signature);
	xrtSecureZero(sKeySeed, sizeof(sKeySeed));
	xrtSecureZero(kDate, sizeof(kDate));
	xrtSecureZero(kService, sizeof(kService));
	xrtSecureZero(kSigning, sizeof(kSigning));
	if(!bOk)
	{
		xrtSecureZero(Signature, sizeof(Signature));
		return false;
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	return xacmeTencentFormat(sAuth, iAuthCapacity,
		"TC3-HMAC-SHA256 Credential=%s/%s/" XACME_TENCENT_SERVICE
		"/tc3_request, SignedHeaders=%s, Signature=%s",
		sId, sDateText, sSignedHeaders, sHex);
}

/* 执行一次 TC3 调用（POST + JSON body）。 */
static bool xacmeTencentCall(
	xacmednstencentcontext* pCtx, cstr sAction, cstr sBody,
	uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sAuth[640];
	char sTimestamp[24];
	char sUrl[240];
	xacmehttpheader Extra[4];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	if(!xacmeDnsTencentAuthorization(
		pCtx->sId, pCtx->sKey, pCtx->sEndpoint, sAction, sBody,
		xrtNow(), sAuth, sizeof(sAuth), sTimestamp, sizeof(sTimestamp)) ||
		!xacmeTencentFormat(sUrl, sizeof(sUrl), "https://%s/",
			pCtx->sEndpoint))
	{
		return false;
	}

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "X-TC-Action", sAction };
	Extra[2] = (xacmehttpheader){ "X-TC-Version", XACME_TENCENT_VERSION };
	Extra[3] = (xacmehttpheader){ "X-TC-Timestamp", sTimestamp };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, "POST", sUrl, "application/json; charset=utf-8",
			(xstrview){ sBody, strlen(sBody) }, Extra, 4u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	if(pOutSize != NULL) *pOutSize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* 取响应 JSON 的 Response 成员（对象）。 */
static xvalue* xacmeTencentResponse(xstrview Body)
{
	/* The JSON parser also accepts a C-string terminator. HTTP bodies are
	 * length-delimited: an embedded NUL must not hide trailing bytes. */
	if(Body.Data == NULL || memchr(Body.Data, 0, Body.Size) != NULL) return NULL;
	xvalue* pRoot = (Body.Data != NULL) ? xrtJsonParse(Body) : NULL;
	xvalue* pResponse = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(!xrtValueIs(pRoot, XVALUE_OBJECT) || xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Error")) != NULL ||
		(pResponse == NULL) || !xrtValueIs(pResponse, XVALUE_OBJECT))
	{
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot; /* 调用方经 Root 再取 Response 并释放 Root。 */
}

bool xacmeDnsTencentResponseSuccess(xstrview sBody)
{
	xvalue* pRoot;
	xvalue* pResponse;
	bool bOk;
	if((sBody.Data == NULL) || (sBody.Size == 0u))
		return false;
	pRoot = xacmeTencentResponse(sBody);
	pResponse = (pRoot != NULL) ?
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	bOk = (pResponse != NULL) && xrtValueIs(pResponse, XVALUE_OBJECT) &&
		(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL);
	if(bOk) {
		char sRequest[128];
		bOk = xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0';
	}
	xrtValueRelease(pRoot);
	return bOk;
}

static bool xacmeTencentErrorFamily(cstr sCode, cstr sFamily)
{
	size_t iSize = strlen(sFamily);
	return strncmp(sCode, sFamily, iSize) == 0 &&
		(sCode[iSize] == '\0' || sCode[iSize] == '.');
}

static bool xacmeTencentCreateRejected(const xvalue* pResponse, uint16 iStatus)
{
	xvalue* pError;
	char sCode[128], sMessage[512], sRequest[128];
	if(pResponse == NULL || !(iStatus == 200u || (iStatus >= 400u && iStatus < 500u)) ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) != NULL ||
		!xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) || sRequest[0] == '\0') return false;
	pError = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error"));
	if(!xrtValueIs(pError, XVALUE_OBJECT) ||
		!xacmeDnsJsonText(pError, "Code", sCode, sizeof(sCode)) ||
		!xacmeDnsJsonText(pError, "Message", sMessage, sizeof(sMessage)) || sMessage[0] == '\0') return false;
	/* Internal/unknown failures do not establish that the mutation was rejected. */
	return xacmeTencentErrorFamily(sCode, "AuthFailure") ||
		xacmeTencentErrorFamily(sCode, "InvalidParameter") ||
		xacmeTencentErrorFamily(sCode, "ResourceNotFound") ||
		strcmp(sCode, "UnauthorizedOperation") == 0 ||
		xacmeTencentErrorFamily(sCode, "OperationDenied") ||
		xacmeTencentErrorFamily(sCode, "LimitExceeded") ||
		strcmp(sCode, "RequestLimitExceeded") == 0;
}

static void xacmeTencentReadError(uint16 iStatus, const xvalue* pResponse)
{
	char sCode[128] = { 0 };
	xerrkind Kind = XERR_PROTOCOL;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return;
	if(pResponse != NULL)
		(void)xacmeDnsJsonText(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")), "Code", sCode, sizeof(sCode));
	if(iStatus == 401u || iStatus == 403u || xacmeTencentErrorFamily(sCode, "AuthFailure") ||
		xacmeTencentErrorFamily(sCode, "OperationDenied") || strcmp(sCode, "UnauthorizedOperation") == 0) Kind = XERR_PERMISSION;
	else if(iStatus >= 500u || xacmeTencentErrorFamily(sCode, "InternalError") ||
		xacmeTencentErrorFamily(sCode, "RequestLimitExceeded")) Kind = XERR_AGAIN;
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		"acme dns_tencent request failed or response does not match the tracked identity");
}

/* The API uses POST even for reads. Retry only these explicitly selected RPCs;
 * every attempt signs again. Mutation RPCs never enter this helper. */
static bool xacmeTencentReadCall(xacmednstencentcontext* pCtx, cstr sAction,
	cstr sBody, uint16* pStatus, str* pBody, size_t* pSize)
{
	uint32 i;
	for(i = 0u; i < 3u; i++) {
		xerrkind Kind;
		xrtClearError();
		if(xacmeTencentCall(pCtx, sAction, sBody, pStatus, pBody, pSize)) return true;
		xrtFree(*pBody); *pBody = NULL;
		Kind = xrtErrorKind(xrtGetError());
		if(!pCtx->Http.bWriteUncertain ||
			(Kind != XERR_IO && Kind != XERR_TIMEOUT && Kind != XERR_CLOSED)) return false;
		if(i != 2u) xrtSleep(250u << i);
	}
	return false;
}

static bool xacmeTencentResponseOk(const xvalue* pResponse, uint16 iStatus)
{
	char sRequest[128];
	return iStatus == 200u && xrtValueIs(pResponse, XVALUE_OBJECT) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL &&
		xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0';
}

static bool xacmeTencentErrorCode(const xvalue* pResponse, uint16 iStatus, cstr sExpected)
{
	const xvalue* pError;
	char sRequest[128], sMessage[512];
	if(!(iStatus == 200u || (iStatus >= 400u && iStatus < 500u)) || !xrtValueIs(pResponse, XVALUE_OBJECT) ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) != NULL ||
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) != NULL ||
		!xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) || sRequest[0] == '\0') return false;
	pError = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error"));
	return xrtValueIs(pError, XVALUE_OBJECT) && xacmeDnsJsonEqual(pError, "Code", sExpected) &&
		xacmeDnsJsonText(pError, "Message", sMessage, sizeof(sMessage)) && sMessage[0] != '\0';
}

static bool xacmeTencentDomainBody(xbuffer* pBody, cstr sZone, int64 iDomainId)
{
	char sNumber[64];
	if(!xrtBufferAppend(pBody, XRT_BYTES_LITERAL("{\"Domain\":")) ||
		!xacmeDnsJsonQuote(pBody, (xstrview){ sZone, strlen(sZone) })) return false;
	if(iDomainId != 0) {
		snprintf(sNumber, sizeof(sNumber), ",\"DomainId\":%lld", (long long)iDomainId);
		if(!xrtBufferAppend(pBody, (xbytesview){ (const uint8*)sNumber, strlen(sNumber) })) return false;
	}
	return true;
}

/* Returns 1 for a matching domain, 0 for a narrowly classified missing
 * candidate, and -1 for errors. Existing owned IDs must never fall back. */
static int xacmeTencentReadDomain(xacmednstencentcontext* pCtx, cstr sZone, int64* pDomainId)
{
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	int64 iId = 0;
	int Result = -1;
	xrtBufferInit(&Body);
	if(!xacmeTencentDomainBody(&Body, sZone, *pDomainId) ||
		!xrtBufferAppendByte(&Body, '}') || !xrtBufferAppendByte(&Body, 0u) ||
		!xacmeTencentReadCall(pCtx, "DescribeDomain", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus)) {
		xvalue* pDomain = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo"));
		if(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xacmeDnsJsonEqual(pDomain, "Domain", sZone) &&
			xrtValueGetInt(xrtValueObjectGet(pDomain, XRT_STR_LITERAL("DomainId")), &iId) &&
			iId > 0 && (*pDomainId == 0 || iId == *pDomainId)) { *pDomainId = iId; Result = 1; }
	} else if(*pDomainId == 0 &&
		(xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameterValue.DomainNotExists") ||
		 xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.DomainInvalid"))) Result = 0;
	if(Result < 0) xacmeTencentReadError(iStatus, pResponse);
	else xrtClearError();
Done:
	xrtValueRelease(pRoot); xrtFree(sResp); xrtBufferUnit(&Body);
	return Result;
}

static bool xacmeTencentAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sRr[256];
	char sZone[256];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xvalue* pResponse;
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;
	int64 iDomainId = 0;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, sFqdn, sTxt))
		return xacmeTencentDeleteUncertain();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent record tracking capacity exhausted");
		return false;
	}
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';
	/* Start from the complete TXT owner on every new Add. A parent cache
	 * cannot establish that a more specific hosted domain still does not exist. */
	memcpy(sZone, sFqdnText, sFqdn.Size + 1u);
	for(;;) {
		int Found = xacmeTencentReadDomain(pCtx, sZone, &iDomainId);
		char* sDot;
		if(Found == 1) break;
		if(Found < 0) return false;
		sDot = strchr(sZone, '.');
		if(sDot == NULL || strchr(sDot + 1u, '.') == NULL) {
			xrtSetErrorInfo(XERR_NOT_FOUND, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent no managed domain found"); return false;
		}
		memmove(sZone, sDot + 1u, strlen(sDot + 1u) + 1u);
	}

	/* DNSPod uses @ when the TXT owner is itself the hosted domain. */
	{
		size_t iZoneLen = strlen(sZone);
		size_t iFqdnLen = strlen(sFqdnText);
		size_t iRrLen;
		if(iFqdnLen == iZoneLen && strcmp(sFqdnText, sZone) == 0)
		{
			memcpy(sRr, "@", 2u);
		}
		else if((iFqdnLen <= iZoneLen + 1u) ||
			(strcmp(sFqdnText + iFqdnLen - iZoneLen, sZone) != 0) ||
			(sFqdnText[iFqdnLen - iZoneLen - 1u] != '.'))
		{
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent hosted domain does not contain the TXT owner");
			return false;
		}
		else
		{
			iRrLen = iFqdnLen - iZoneLen - 1u;
			memcpy(sRr, sFqdnText, iRrLen);
			sRr[iRrLen] = '\0';
		}
	}
	xrtBufferInit(&Body);
	bOk = xacmeTencentDomainBody(&Body, sZone, iDomainId) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"SubDomain\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sRr, strlen(sRr) }) &&
		xrtBufferAppend(&Body,
			XRT_BYTES_LITERAL(",\"RecordType\":\"TXT\","
				"\"RecordLine\":\"默认\",\"Value\":")) &&
		xacmeDnsJsonQuote(&Body, (xstrview){ sTxtText, strlen(sTxtText) }) &&
		xrtBufferAppend(&Body, XRT_BYTES_LITERAL("}")) &&
		xrtBufferAppendByte(&Body, 0u);
	if(bOk)
	{
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		pCtx->iDomainIds[iSlot] = iDomainId;
		bSent = true;
		bOk = xacmeTencentCall(pCtx, "CreateRecord", (cstr)Body.Data,
			&iStatus, &sResp, &iRespSize);
	}
	xrtBufferUnit(&Body);
	if(!bOk)
	{
		xrtFree(sResp);
		if(!bSent) return false;
		if(!pCtx->Http.bWriteUncertain) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			return false;
		}
		return xacmeDnsCreateUncertainError();
	}
	/* RecordId 记档（可能为数值，按文本取）。 */
	pRoot = xacmeTencentResponse((xstrview){ sResp, iRespSize });
	if(pRoot != NULL)
	{
		xvalue* pMember;
		char sRequest[128];
		pResponse = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response"));
		if(xacmeTencentCreateRejected(pResponse, iStatus)) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			xacmeTencentReadError(iStatus, pResponse);
			xrtValueRelease(pRoot); xrtFree(sResp);
			return false;
		}
		pMember = (pResponse != NULL) ?
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) : NULL;
		if(iStatus == 200u && pMember != NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("Error")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
			xacmeDnsJsonText(pResponse, "RequestId", sRequest, sizeof(sRequest)) && sRequest[0] != '\0')
		{
			int64 iId = 0;
			if(xrtValueGetInt(pMember, &iId))
			{
				char sIdText[32];
				if(iId > 0)
				{
					snprintf(sIdText, sizeof(sIdText), "%lld",
						(long long)iId);
					bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
						iSlot, sZone, '|', sIdText);
					if(bTracked) pCtx->uCreatedAt[iSlot] = xrtTimer();
				}
			}
		}
		xrtValueRelease(pRoot);
	}
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

typedef enum xacmetencentrecordresult {
	XACME_TENCENT_RECORD_ERROR,
	XACME_TENCENT_RECORD_FOUND
} xacmetencentrecordresult;

static bool xacmeTencentRecordBody(xbuffer* pBody, cstr sZone, int64 iDomainId, int64 iRecordId)
{
	char sNumber[64];
	snprintf(sNumber, sizeof(sNumber), ",\"RecordId\":%lld}", (long long)iRecordId);
	return xacmeTencentDomainBody(pBody, sZone, iDomainId) &&
		xrtBufferAppend(pBody, (xbytesview){ (const uint8*)sNumber, strlen(sNumber) }) &&
		xrtBufferAppendByte(pBody, 0u);
}

static bool xacmeTencentOwnedHandle(xacmednstencentcontext* pCtx, size_t iSlot,
	char* sZone, size_t iZoneCapacity, int64* pRecordId)
{
	char sRecordId[32];
	cstr p;
	int64 iRecordId = 0;
	if(iSlot >= pCtx->Records.iCount || pCtx->iDomainIds[iSlot] <= 0 ||
		!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '|', sZone, iZoneCapacity,
			sRecordId, sizeof(sRecordId)) || sRecordId[0] == '0') goto Invalid;
	for(p = sRecordId; *p != '\0'; p++) {
		if(*p < '0' || *p > '9' || iRecordId > (INT64_MAX - (*p - '0')) / 10) goto Invalid;
		iRecordId = iRecordId * 10 + (*p - '0');
	}
	*pRecordId = iRecordId;
	return true;
Invalid:
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		"acme dns_tencent tracked domain/record handle is invalid");
	return false;
}

static bool xacmeTencentRecordIdentity(const xvalue* pRecord, int64 iDomainId,
	int64 iRecordId, cstr sZone, xstrview Owner, xstrview Txt)
{
	int64 iId = 0, iDomain = 0, iEnabled = -1;
	xstrview Sub, Value;
	size_t iZone = strlen(sZone);
	return xrtValueIs(pRecord, XVALUE_OBJECT) &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Id")), &iId) && iId == iRecordId &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("DomainId")), &iDomain) && iDomain == iDomainId &&
		xacmeDnsJsonEqual(pRecord, "RecordType", "TXT") &&
		xacmeDnsJsonEqual(pRecord, "RecordLine", "默认") && xacmeDnsJsonEqual(pRecord, "RecordLineId", "0") &&
		xrtValueGetInt(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Enabled")), &iEnabled) && (iEnabled == 0 || iEnabled == 1) &&
		xrtValueGetString(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("SubDomain")), &Sub) &&
		((Owner.Size == iZone && memcmp(Owner.Data, sZone, iZone) == 0 &&
			Sub.Size == 1u && Sub.Data[0] == '@') ||
		(Owner.Size > iZone + 1u && Sub.Size == Owner.Size - iZone - 1u &&
		memcmp(Sub.Data, Owner.Data, Sub.Size) == 0 && Owner.Data[Sub.Size] == '.' &&
		memcmp(Owner.Data + Sub.Size + 1u, sZone, iZone) == 0)) &&
		xrtValueGetString(xrtValueObjectGet(pRecord, XRT_STR_LITERAL("Value")), &Value) &&
		Value.Size == Txt.Size && memcmp(Value.Data, Txt.Data, Txt.Size) == 0;
}

static int xacmeTencentCompareIds(const void* pLeft, const void* pRight)
{
	int64 Left = *(const int64*)pLeft, Right = *(const int64*)pRight;
	return (Left > Right) - (Left < Right);
}

/* The list index has no documented maximum delay. Inspect its inventory for
 * diagnostics after the recommended retry interval, but never release a saved
 * identity based on absence from that index, even after repeated observations. */
static void xacmeTencentInspectMissingCandidate(xacmednstencentcontext* pCtx,
	size_t iSlot, cstr sZone, int64 iRecordId)
{
	double uNow = xrtTimer();
	int64 iDomainId = pCtx->iDomainIds[iSlot], iTotal = -1, iListed = -1;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u, iCount, i;
	int64 Ids[3000];
	xbuffer Body;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	xvalue* pList;
	bool bValid = false, bPresent = false;
	if(uNow < pCtx->uCreatedAt[iSlot] || uNow - pCtx->uCreatedAt[iSlot] < 30.0) {
		xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
			"acme dns_tencent record absence cannot be checked during the create index delay"); return;
	}
	if(xacmeTencentReadDomain(pCtx, sZone, &iDomainId) != 1) return;
	xrtBufferInit(&Body);
	if(!xacmeTencentDomainBody(&Body, sZone, iDomainId) ||
		!xrtBufferAppend(&Body, XRT_BYTES_LITERAL(",\"Offset\":0,\"Limit\":3000,\"ErrorOnEmpty\":\"no\"}")) ||
		!xrtBufferAppendByte(&Body, 0u) ||
		!xacmeTencentReadCall(pCtx, "DescribeRecordList", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	pList = (pResponse != NULL) ? xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus) && xrtValueIs(pList, XVALUE_ARRAY) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL) {
		xvalue* pCount = xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo"));
		iCount = xrtValueCount(pList);
		bValid = xrtValueGetInt(xrtValueObjectGet(pCount, XRT_STR_LITERAL("TotalCount")), &iTotal) &&
			xrtValueGetInt(xrtValueObjectGet(pCount, XRT_STR_LITERAL("ListCount")), &iListed) &&
			iTotal >= 0 && iTotal <= 3000 && iTotal == iListed && (uint64)iTotal == iCount;
		for(i = 0u; bValid && i < iCount; i++) {
			int64 iId = 0;
			xvalue* pItem = xrtValueArrayGet(pList, i);
			xstrview Name, Type, Value;
			bValid = xrtValueIs(pItem, XVALUE_OBJECT) &&
				xrtValueGetInt(xrtValueObjectGet(pItem, XRT_STR_LITERAL("RecordId")), &iId) && iId > 0 &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Name")), &Name) && Name.Size != 0u &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Type")), &Type) && Type.Size != 0u &&
				xrtValueGetString(xrtValueObjectGet(pItem, XRT_STR_LITERAL("Value")), &Value) &&
				memchr(Name.Data, 0, Name.Size) == NULL && memchr(Type.Data, 0, Type.Size) == NULL &&
				memchr(Value.Data, 0, Value.Size) == NULL &&
				(xacmeDnsJsonEqual(pItem, "Status", "ENABLE") || xacmeDnsJsonEqual(pItem, "Status", "DISABLE"));
			if(iId == iRecordId) bPresent = true;
			Ids[i] = iId;
		}
		if(bValid) {
			qsort(Ids, iCount, sizeof(Ids[0]), xacmeTencentCompareIds);
			for(i = 1u; i < iCount; i++) if(Ids[i] == Ids[i - 1u]) { bValid = false; break; }
		}
	}
	if(bValid && !bPresent) xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
		"acme dns_tencent record absent from a possibly delayed index; original handle retained");
	else if(bValid) xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
		"acme dns_tencent record still appears in inventory; original handle retained");
	else xacmeTencentReadError(iStatus, pResponse);
Done:
	xrtBufferUnit(&Body); xrtValueRelease(pRoot); xrtFree(sResp);
}

static xacmetencentrecordresult xacmeTencentReadRecord(xacmednstencentcontext* pCtx,
	size_t iSlot, cstr sZone, int64 iRecordId, xstrview Owner, xstrview Txt, bool bAdding)
{
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iSize = 0u;
	xvalue* pRoot = NULL;
	xvalue* pResponse = NULL;
	bool bMissingCandidate = false;
	xacmetencentrecordresult Result = XACME_TENCENT_RECORD_ERROR;
	xrtBufferInit(&Body);
	if(!xacmeTencentRecordBody(&Body, sZone, pCtx->iDomainIds[iSlot], iRecordId) ||
		!xacmeTencentReadCall(pCtx, "DescribeRecord", (cstr)Body.Data, &iStatus, &sResp, &iSize)) goto Done;
	pRoot = xacmeTencentResponse((xstrview){ sResp, iSize });
	pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
	if(xacmeTencentResponseOk(pResponse, iStatus) &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
		xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
		xacmeTencentRecordIdentity(xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")),
			pCtx->iDomainIds[iSlot], iRecordId, sZone, Owner, Txt)) {
		int64 iEnabled = 0;
		if(bAdding && (!xrtValueGetInt(xrtValueObjectGet(
				xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")),
				XRT_STR_LITERAL("Enabled")), &iEnabled) || iEnabled != 1))
			xrtSetErrorInfo(XERR_STATE, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
				"acme dns_tencent owned record is disabled");
		else { Result = XACME_TENCENT_RECORD_FOUND; xrtClearError(); }
	} else if(xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.RecordIdInvalid")) bMissingCandidate = true;
	else xacmeTencentReadError(iStatus, pResponse);
Done:
	xrtBufferUnit(&Body); xrtValueRelease(pRoot); xrtFree(sResp);
	if(bMissingCandidate) {
		/* A failed Add revalidation sent no deletion. Allow another read of the
		 * same saved ID, but never release it based on the list index. */
		if(!bAdding) pCtx->bDeletePending[iSlot] = true;
		xacmeTencentInspectMissingCandidate(pCtx, iSlot, sZone, iRecordId);
	}
	return Result;
}

static bool xacmeTencentDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns", XACME_DNS_ERROR_UNCERTAIN,
		"Tencent DNS deletion outcome unknown; original record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static xacmednsownedresult xacmeTencentReuseOwned(xacmednstencentcontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256], sZone[256];
	size_t iSlot;
	int64 iRecordId = 0, iDomainId;
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, Owner, Txt)) {
		(void)xacmeTencentDeleteUncertain(); return XACME_DNS_OWNED_ERROR;
	}
	iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
	if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
	if(!xacmeTencentOwnedHandle(pCtx, iSlot, sZone, sizeof(sZone), &iRecordId)) return XACME_DNS_OWNED_ERROR;
	iDomainId = pCtx->iDomainIds[iSlot];
	/* Validate the saved domain as well as the record; do not rediscover a child. */
	if(xacmeTencentReadDomain(pCtx, sZone, &iDomainId) != 1) return XACME_DNS_OWNED_ERROR;
	return xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, Owner, Txt, true) ==
		XACME_TENCENT_RECORD_FOUND ? XACME_DNS_OWNED_VALID : XACME_DNS_OWNED_ERROR;
}

static bool xacmeTencentDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)pContext;
	char sZone[256];
	size_t iSlot;
	int64 iRecordId = 0;
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	bool bPreviousPending;
	xvalue* pRoot = NULL;
	xvalue* pResponse;
	xacmetencentrecordresult Result;
	for(iSlot = 0u; iSlot < pCtx->Records.iCount; iSlot++)
		if(strcmp(pCtx->Records.sIds[iSlot], sId) == 0) break;
	if(!xacmeTencentOwnedHandle(pCtx, iSlot, sZone, sizeof(sZone), &iRecordId)) return false;
	Result = xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, sFqdn, sTxt, false);
	if(Result != XACME_TENCENT_RECORD_FOUND) return false;
	xrtBufferInit(&Body);
	if(!xacmeTencentRecordBody(&Body, sZone, pCtx->iDomainIds[iSlot], iRecordId)) { xrtBufferUnit(&Body); return false; }
	bPreviousPending = pCtx->bDeletePending[iSlot]; pCtx->bDeletePending[iSlot] = true;
	bOk = xacmeTencentCall(pCtx, "DeleteRecord", (cstr)Body.Data, &iStatus, &sResp, &iRespSize);
	xrtBufferUnit(&Body);
	if(!bOk && !pCtx->Http.bWriteUncertain) { pCtx->bDeletePending[iSlot] = bPreviousPending; xrtFree(sResp); return false; }
	if(bOk) {
		int64 iAckId = 0;
		xvalue* pId;
		pRoot = xacmeTencentResponse((xstrview){ sResp, iRespSize });
		pResponse = (pRoot != NULL) ? xrtValueObjectGet(pRoot, XRT_STR_LITERAL("Response")) : NULL;
		pId = (pResponse != NULL) ? xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordId")) : NULL;
		bOk = xacmeTencentResponseOk(pResponse, iStatus) &&
			(pId == NULL || (xrtValueGetInt(pId, &iAckId) && iAckId == iRecordId)) &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL;
		/* A verified error must contain no result fields. All other failures
		 * could have happened after the server committed the deletion. */
		if(!bOk && xacmeTencentCreateRejected(pResponse, iStatus) &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordList")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("RecordCountInfo")) == NULL &&
			xrtValueObjectGet(pResponse, XRT_STR_LITERAL("DomainInfo")) == NULL &&
			!xacmeTencentErrorCode(pResponse, iStatus, "InvalidParameter.RecordIdInvalid") &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			xacmeTencentReadError(iStatus, pResponse);
			pCtx->bDeletePending[iSlot] = bPreviousPending;
			xrtValueRelease(pRoot); xrtFree(sResp); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) { pCtx->bDeletePending[iSlot] = false; return true; }
	(void)xacmeTencentDeleteUncertain();
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		Result = xacmeTencentReadRecord(pCtx, iSlot, sZone, iRecordId, sFqdn, sTxt, false);
		if(Result == XACME_TENCENT_RECORD_FOUND) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return false;
}

static bool xacmeTencentRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx =
		(xacmednstencentcontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"acme dns_tencent owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeTencentDeleteRecord, pCtx);
}

static bool xacmeTencentAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeTencentReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeTencentAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeTencentRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednstencentcontext* pCtx = (xacmednstencentcontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeTencentRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

void xrtAcmeDnsTencentConfigInit(xacmednstencentconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sSecretId = NULL;
	pConfig->sSecretKey = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsTencent(
	const xacmednstencentconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
{
	xacmednstencentcontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sSecretId == NULL) || (pConfig->sSecretKey == NULL) ||
		(pConfig->sSecretId[0] == '\0') ||
		(pConfig->sSecretKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_tencent requires secret id and key");
		return false;
	}
	if(strlen(pConfig->sSecretId) >= sizeof(pCtx->sId) ||
		strlen(pConfig->sSecretKey) >= sizeof(pCtx->sKey) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_tencent credentials or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednstencentcontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sId, sizeof(pCtx->sId), "%s", pConfig->sSecretId);
	snprintf(pCtx->sKey, sizeof(pCtx->sKey), "%s", pConfig->sSecretKey);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"dnspod.tencentcloudapi.com");
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		return false;
	}
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		(void)xrtMutexUnit(&pCtx->Lock);
		if(xacmeHttpUnit(&pCtx->Http))
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
		return false;
	}
	pProvider->sId = "tencent";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeTencentAdd;
	pProvider->Remove = xacmeTencentRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsTencentProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednstencentcontext* pCtx =
			(xacmednstencentcontext*)pProvider->pContext;
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_aws.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_AWS)

#if defined(XACME_FEATURE_DNS_AWS)



#include <stdlib.h>

/*
	AWS Route53 provider（SigV4 + XML API 2013-04-01）：
	  - canonical：host/x-amz-content-sha256/x-amz-date 三头（字典序）；
	  - StringToSign = "AWS4-HMAC-SHA256\n<x-amz-date>\n"
	    "<date>/<region>/route53/aws4_request\n<sha256hex(canonical)>"；
	  - 密钥链：HMAC("AWS4"+SK, date) → region → "route53" →
	    "aws4_request"；payload hash 走 x-amz-content-sha256 头；
	  - zone 发现：GET /hostedzonesbyname?dnsname=<候选>（逐级上探）；
	  - 同名 TXT 使用 ListResourceRecordSets 读取后 DELETE/CREATE 原子替换；
	  - 冲突时重新读取，保留原 TTL 与其他 TXT 值；
	  - Change 异步但权威侧近即时，不轮询 INSYNC。
	记录句柄无服务端 id，Remove 根据保存的 zoneid、fqdn、value 读取并删值。
*/

#define XACME_AWS_SERVICE "route53"
#define XACME_AWS_API "2013-04-01"

typedef struct xacmednsawscontext {
	xacmehttp Http;
	xmutex Lock;
	char sId[160];
	char sKey[160];
	char sRegion[32];
	char sEndpoint[160];
	/* 记录句柄：zone id + fqdn + 值（删除需完整回放）。 */
	struct
	{
		char sZoneId[64];
		char sFqdn[256];
		char sValue[208];
		bool bUncertain;
	} Records[XACME_DNS_RECORD_MAX];
	size_t iRecordCount;
} xacmednsawscontext;

typedef enum xacmeawsmutationresult {
	XACME_AWS_FAILED,
	XACME_AWS_UNCHANGED,
	XACME_AWS_CHANGED,
	XACME_AWS_CONFLICT,
	XACME_AWS_BUSY,
	XACME_AWS_UNCERTAIN
} xacmeawsmutationresult;

static bool xacmeAwsUncertain(void)
{
	xerror* pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN,
		"route53 add ownership unknown; reconcile before another add or cleanup");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

bool xacmeAwsSigningKey(cstr sSecret, cstr sDate, cstr sRegion,
	uint8 pKey[32])
{
	char sKeySeed[180];
	uint8 kDate[XRT_SHA256_SIZE];
	uint8 kRegion[XRT_SHA256_SIZE];
	uint8 kService[XRT_SHA256_SIZE];
	int iWritten;
	bool bOk;

	if((sSecret == NULL) || (sDate == NULL) || (sRegion == NULL) ||
		(pKey == NULL))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_aws signing key arguments are invalid");
		return false;
	}
	xrtSecureZero(pKey, XRT_SHA256_SIZE);
	iWritten = snprintf(sKeySeed, sizeof(sKeySeed), "AWS4%s", sSecret);
	if((iWritten < 0) || ((size_t)iWritten >= sizeof(sKeySeed)))
	{
		xrtSecureZero(sKeySeed, sizeof(sKeySeed));
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_aws secret exceeds signing buffer");
		return false;
	}
	bOk = xacmeSigHmac((const uint8*)sKeySeed, strlen(sKeySeed),
		sDate, strlen(sDate), kDate) &&
		xacmeSigHmac(kDate, sizeof(kDate), sRegion, strlen(sRegion),
			kRegion) &&
		xacmeSigHmac(kRegion, sizeof(kRegion), XACME_AWS_SERVICE,
			strlen(XACME_AWS_SERVICE), kService) &&
		xacmeSigHmac(kService, sizeof(kService), "aws4_request",
			sizeof("aws4_request") - 1u, pKey);
	xrtSecureZero(sKeySeed, sizeof(sKeySeed));
	xrtSecureZero(kDate, sizeof(kDate));
	xrtSecureZero(kRegion, sizeof(kRegion));
	xrtSecureZero(kService, sizeof(kService));
	if(!bOk)
	{
		xrtSecureZero(pKey, XRT_SHA256_SIZE);
	}
	return bOk;
}

/* 执行一次 SigV4 调用（sBody 为 XML 或 NULL）。 */
static bool xacmeAwsCall(
	xacmednsawscontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sContentType, cstr sBody, uint16* pOutStatus,
	str* pOutBody, size_t* pOutBodySize)
{
	static const char* sSignedHeaders =
		"host;x-amz-content-sha256;x-amz-date";
	char sDateText[16];      /* YYYYMMDD */
	char sStampText[24];     /* YYYYMMDDTHHMMSSZ */
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[2048];
	char sHeaders[360];
	char sStringToSign[240];
	char sHex[XACME_SIG_HASH_TEXT];
	char sAuth[640];
	char sUrl[640];
	char sCanonicalPath[440];
	cstr sQuery = "";
	xacmehttpheader Extra[3];
	xacmehttpresponse R;
	xdatetime Now;
	uint8 kSigning[XRT_SHA256_SIZE];
	uint8 Signature[XRT_SHA256_SIZE];

	if(!xrtTimeSplitAt(xrtNow(), 0, &Now))
	{
		return false;
	}
	snprintf(sDateText, sizeof(sDateText), "%04ld%02d%02d", (long)Now.Year,
		Now.Month, Now.Day);
	snprintf(sStampText, sizeof(sStampText),
		"%04ld%02d%02dT%02d%02d%02dZ", (long)Now.Year, Now.Month, Now.Day,
		Now.Hour, Now.Minute, Now.Second);
	if(!xacmeSigSha256Hex(
			(sBody != NULL) ? sBody : "",
			(sBody != NULL) ? strlen(sBody) : 0u, sPayloadHash))
	{
		return false;
	}
	snprintf(sHeaders, sizeof(sHeaders),
		"host:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n",
		pCtx->sEndpoint, sPayloadHash, sStampText);
	{
		const char* pQuestion = strchr(sPathAndQuery, '?');
		size_t iPathSize = (pQuestion != NULL) ?
			(size_t)(pQuestion - sPathAndQuery) : strlen(sPathAndQuery);
		if((iPathSize == 0u) || (iPathSize >= sizeof(sCanonicalPath)))
			return false;
		memcpy(sCanonicalPath, sPathAndQuery, iPathSize);
		sCanonicalPath[iPathSize] = '\0';
		if(pQuestion != NULL) sQuery = pQuestion + 1u;
	}
	if(!xacmeSigCanonical(sCanonical, sizeof(sCanonical), sMethod,
			sCanonicalPath, sQuery, sHeaders, sSignedHeaders, sPayloadHash))
	{
		return false;
	}
	if(!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex))
	{
		return false;
	}
	snprintf(sStringToSign, sizeof(sStringToSign),
		"AWS4-HMAC-SHA256\n%s\n%s/%s/" XACME_AWS_SERVICE
		"/aws4_request\n%s",
		sStampText, sDateText, pCtx->sRegion, sHex);
	if(!xacmeAwsSigningKey(pCtx->sKey, sDateText, pCtx->sRegion,
			kSigning))
	{
		return false;
	}
	{
		bool bSigned = xacmeSigHmac(kSigning, sizeof(kSigning),
			sStringToSign, strlen(sStringToSign), Signature);
		xrtSecureZero(kSigning, sizeof(kSigning));
		if(!bSigned)
		{
			return false;
		}
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	snprintf(sAuth, sizeof(sAuth),
		"AWS4-HMAC-SHA256 Credential=%s/%s/%s/" XACME_AWS_SERVICE
		"/aws4_request, SignedHeaders=%s, Signature=%s",
		pCtx->sId, sDateText, pCtx->sRegion, sSignedHeaders, sHex);
	{
		int iUrlSize = snprintf(sUrl, sizeof(sUrl), "https://%s%s",
			pCtx->sEndpoint, sPathAndQuery);
		if((iUrlSize < 0) || ((size_t)iUrlSize >= sizeof(sUrl)))
			return false;
	}

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "x-amz-content-sha256", sPayloadHash };
	Extra[2] = (xacmehttpheader){ "x-amz-date", sStampText };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl,
			(sContentType != NULL) ? sContentType : "application/xml",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 3u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	*pOutBodySize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* hostedzonesbyname 精确匹配候选 zone（返回 hostedzone id）。 */
static int xacmeAwsZoneId(
	xacmednsawscontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[400];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	int iResult;
	int iPathSize;

	/* Zone 名在 Route53 中带尾点。 */
	iPathSize = snprintf(sPath, sizeof(sPath), "/" XACME_AWS_API
		"/hostedzonesbyname?dnsname=%s.&maxitems=100", sZone);
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)) ||
		!xacmeAwsCall(pCtx, "GET", sPath, NULL, NULL,
			&iStatus, &sBody, &iBodySize))
	{
		return -1;
	}
	iResult = ((iStatus >= 200u) && (iStatus < 300u) &&
		(sBody != NULL) && (iBodySize <= 4u * 1024u * 1024u) &&
		(strlen(sBody) == iBodySize)) ?
		xacmeAwsSelectPublicZone(sBody, sZone, sOutId, iIdCap) : -1;
	xrtFree(sBody);
	if(iResult < 0)
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_ZONE,
			"acme dns_aws hosted zone listing is invalid or ambiguous");
	return iResult;
}

static bool xacmeAwsFindZone(
	xacmednsawscontext* pCtx, cstr sFqdn, char* sOutId, size_t iIdCap)
{
	cstr sCandidate = sFqdn;
	/* A previously found parent cannot prove that a child zone is absent.
	 * Probe the complete owner too: DNS-01 may be delegated at its apex. */
	for(;;)
	{
		const char* sDot;
		int iZoneResult = xacmeAwsZoneId(pCtx, sCandidate, sOutId, iIdCap);
		if(iZoneResult < 0) return false;
		if(iZoneResult == 1) return true;
		sDot = strchr(sCandidate, '.');
		if(sDot == NULL)
		{
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
				"acme dns_aws hosted zone was not found");
			return false;
		}
		sCandidate = sDot + 1u;
	}
}

/* 读取精确 TXT 记录集。最多请求一个结果，首项不匹配即记录不存在。 */
static bool xacmeAwsReadTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sFqdn, xacmeawstxtset* pSet, str* pBody)
{
	char sPath[440];
	uint16 iStatus = 0u;
	size_t iBodySize = 0u;
	int iPathSize = snprintf(sPath, sizeof(sPath),
		"/" XACME_AWS_API "/hostedzone/%s/rrset?maxitems=1&name=%s.&type=TXT",
		sZoneId, sFqdn);
	*pBody = NULL;
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)) ||
		!xacmeAwsCall(pCtx, "GET", sPath, NULL, NULL,
			&iStatus, pBody, &iBodySize))
		return false;
	if((iStatus < 200u) || (iStatus >= 300u) ||
		(iBodySize > 4u * 1024u * 1024u) ||
		(*pBody == NULL) || (strlen(*pBody) != iBodySize) ||
		!xacmeAwsParseTxtSet(*pBody, sFqdn, pSet))
	{
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws TXT listing was rejected or malformed");
		xrtFree(*pBody);
		*pBody = NULL;
		return false;
	}
	return true;
}

/* 冲突或提交结果不确定时，调用方重新读取当前记录集。 */
static xacmeawsmutationresult xacmeAwsChangeTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sBody)
{
	char sPath[128];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	xacmeawsmutationresult Result = XACME_AWS_FAILED;
	int iPathSize = snprintf(sPath, sizeof(sPath),
		"/" XACME_AWS_API "/hostedzone/%s/rrset/", sZoneId);
	if((iPathSize < 0) || ((size_t)iPathSize >= sizeof(sPath)))
		return XACME_AWS_FAILED;
	bOk = xacmeAwsCall(pCtx, "POST", sPath, "application/xml", sBody,
		&iStatus, &sResp, &iRespSize);
	/* 响应丢失时提交结果不确定；下轮读取权威当前状态再判定。 */
	if(!bOk && pCtx->Http.bWriteUncertain) Result = XACME_AWS_UNCERTAIN;
	if(bOk && iStatus >= 500u) Result = XACME_AWS_UNCERTAIN;
	if(bOk && (iStatus >= 200u) && (iStatus < 300u) &&
		((sResp == NULL) || (iRespSize > 4u * 1024u * 1024u) ||
		 (strlen(sResp) != iRespSize) || !xacmeAwsChangeAccepted(sResp))) {
		bOk = false;
		Result = XACME_AWS_UNCERTAIN;
		xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws TXT change acknowledgment was malformed");
	}
	if(bOk && ((iStatus < 200u) || (iStatus >= 300u))) {
		char Code[64];
		bool bError = xacmeAwsErrorCode((xstrview){ sResp, iRespSize }, Code);
		if(iStatus >= 400u && iStatus < 500u && bError) {
			bool bCredential = strcmp(Code, "AccessDenied") == 0 ||
				strcmp(Code, "InvalidClientTokenId") == 0 || strcmp(Code, "SignatureDoesNotMatch") == 0;
			if(iStatus == 400u && strcmp(Code, "InvalidChangeBatch") == 0)
				Result = XACME_AWS_CONFLICT;
			else if(iStatus == 400u && (strcmp(Code, "Throttling") == 0 ||
				strcmp(Code, "PriorRequestNotComplete") == 0)) Result = XACME_AWS_BUSY;
			xrtSetErrorInfo(Result == XACME_AWS_BUSY ? XERR_AGAIN : bCredential ? XERR_PERMISSION : XERR_PROTOCOL,
				"xrt.acme.dns", Result == XACME_AWS_BUSY ? XACME_DNS_ERROR_NETWORK :
				bCredential ? XACME_DNS_ERROR_CREDENTIAL : XACME_DNS_ERROR_PROTOCOL,
				"acme dns_aws TXT change was rejected");
		} else {
			/* A malformed error does not establish atomic rejection. In
			 * particular it cannot authorize either provider or flow replay. */
			Result = XACME_AWS_UNCERTAIN;
			xrtSetErrorInfo(iStatus >= 500u ? XERR_IO : XERR_PROTOCOL, "xrt.acme.dns",
				iStatus >= 500u ? XACME_DNS_ERROR_NETWORK : XACME_DNS_ERROR_PROTOCOL,
				"acme dns_aws TXT change outcome unknown after invalid error response");
		}
	}
	xrtFree(sResp);
	return bOk && (iStatus >= 200u) && (iStatus < 300u) ? XACME_AWS_CHANGED : Result;
}

static xacmeawsmutationresult xacmeAwsMutateTxt(xacmednsawscontext* pCtx,
	cstr sZoneId, cstr sFqdn, cstr sValue, bool bAdd)
{
	unsigned iAttempt;
	bool bRetried = false;
	xacmeawsmutationresult Last = XACME_AWS_FAILED;
	for(iAttempt = 0u; iAttempt < 4u; iAttempt++)
	{
		xacmeawstxtset Set;
		xbuffer Request;
		str sReadBody = NULL;
		bool bChanged = false;
		bool bOk;
		xacmeawsmutationresult Result;
		if(!xacmeAwsReadTxt(pCtx, sZoneId, sFqdn, &Set, &sReadBody))
			return XACME_AWS_FAILED;
		xrtBufferInit(&Request);
		bOk = xacmeAwsBuildTxtChange(&Set, sFqdn, sValue, bAdd,
			&Request, &bChanged);
		xrtFree(sReadBody);
		if(!bOk)
		{
			xrtBufferUnit(&Request);
			if(xrtErrorKind(xrtGetError()) == XERR_NONE)
				xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
					XACME_DNS_ERROR_PROTOCOL,
					"acme dns_aws TXT change exceeds supported size");
			return XACME_AWS_FAILED;
		}
		if(!bChanged)
		{
			xrtBufferUnit(&Request);
			if(bRetried) xrtClearError();
			return XACME_AWS_UNCHANGED;
		}
		Result = xacmeAwsChangeTxt(pCtx, sZoneId, (const char*)Request.Data);
		xrtBufferUnit(&Request);
		if(Result == XACME_AWS_CHANGED)
		{
			if(bRetried) xrtClearError();
			return Result;
		}
		/* An observed value after a lost create acknowledgment cannot prove
		 * who created it. Only deletion of an already-owned value is reconciled. */
		if(Result == XACME_AWS_FAILED || (bAdd && Result == XACME_AWS_UNCERTAIN)) return Result;
		Last = Result;
		bRetried = true;
		if(iAttempt + 1u < 4u) xrtSleep(500u << iAttempt);
	}
	if(Last == XACME_AWS_BUSY) return XACME_AWS_FAILED;
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
		XACME_DNS_ERROR_PROTOCOL,
		"acme dns_aws TXT record set changed repeatedly");
	return XACME_AWS_FAILED;
}

/* Both entry points validate the bounded ASCII owner before copying it. */
static xstrview xacmeAwsCanonicalOwner(xstrview Fqdn, char* sOut)
{
	size_t i;
	for(i = 0u; i < Fqdn.Size; i++) {
		unsigned char c = (unsigned char)Fqdn.Data[i];
		sOut[i] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
	}
	sOut[Fqdn.Size] = '\0';
	return (xstrview){ sOut, Fqdn.Size };
}

static bool xacmeAwsAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	char sFqdnText[256];
	char sTxtText[208];
	char sZoneId[64];
	size_t iSlot;
	size_t i;
	bool bExistingOwned = false;
	xacmeawsmutationresult Result;
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAwsCanonicalOwner(sFqdn, sFqdnText);
	for(i = 0u; i < pCtx->iRecordCount; i++) {
		if(strlen(pCtx->Records[i].sFqdn) == sFqdn.Size &&
			memcmp(pCtx->Records[i].sFqdn, sFqdn.Data, sFqdn.Size) == 0 &&
			strlen(pCtx->Records[i].sValue) == sTxt.Size &&
			memcmp(pCtx->Records[i].sValue, sTxt.Data, sTxt.Size) == 0) {
			if(pCtx->Records[i].bUncertain) return xacmeAwsUncertain();
			bExistingOwned = true;
			break;
		}
	}
	for(iSlot = 0u; iSlot < pCtx->iRecordCount; iSlot++)
		if(pCtx->Records[iSlot].sFqdn[0] == '\0') break;
	if(bExistingOwned) iSlot = i;
	if((iSlot == pCtx->iRecordCount) &&
		(pCtx->iRecordCount >= XACME_DNS_RECORD_MAX))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_PROTOCOL,
			"acme dns_aws record slots exhausted");
		return false;
	}
	memcpy(sTxtText, sTxt.Data, sTxt.Size);
	sTxtText[sTxt.Size] = '\0';

	/* Keep the cleanup handle bound to its original zone even if a more
	 * specific zone appears between Add calls. New pairs discover afresh. */
	if(bExistingOwned)
	{
		snprintf(sZoneId, sizeof(sZoneId), "%s", pCtx->Records[i].sZoneId);
	}
	else if(!xacmeAwsFindZone(pCtx, sFqdnText, sZoneId, sizeof(sZoneId)))
	{
		if(xrtErrorKind(xrtGetError()) == XERR_NONE)
			xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns",
				XACME_DNS_ERROR_ZONE,
				"acme dns_aws hosted zone was not found");
		return false;
	}
	Result = xacmeAwsMutateTxt(pCtx, sZoneId, sFqdnText, sTxtText, true);
	if(Result == XACME_AWS_FAILED)
	{
		return false;
	}
	/* A value already present before our acknowledged write is not ours. A
	 * repeated Add on an owned value keeps the original ownership entry. */
	if(Result == XACME_AWS_UNCHANGED) return true;
	snprintf(pCtx->Records[iSlot].sZoneId,
		sizeof(pCtx->Records[iSlot].sZoneId), "%s",
		sZoneId);
	snprintf(pCtx->Records[iSlot].sFqdn,
		sizeof(pCtx->Records[iSlot].sFqdn), "%s",
		sFqdnText);
	snprintf(pCtx->Records[iSlot].sValue,
		sizeof(pCtx->Records[iSlot].sValue), "%s",
		sTxtText);
	if(iSlot == pCtx->iRecordCount) pCtx->iRecordCount++;
	pCtx->Records[iSlot].bUncertain = Result == XACME_AWS_UNCERTAIN;
	if(pCtx->Records[iSlot].bUncertain) return xacmeAwsUncertain();
	return true;
}

static bool xacmeAwsRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
	char sFqdnText[256];
	size_t i;
	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme DNS-01 owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeAwsCanonicalOwner(sFqdn, sFqdnText);
	for(i = 0; i < pCtx->iRecordCount; i++)
	{
		if(pCtx->Records[i].sFqdn[0] == '\0')
		{
			continue;
		}
		if((strlen(pCtx->Records[i].sFqdn) != sFqdn.Size) ||
			(memcmp(pCtx->Records[i].sFqdn, sFqdn.Data, sFqdn.Size) != 0) ||
			(strlen(pCtx->Records[i].sValue) != sTxt.Size) ||
			(memcmp(pCtx->Records[i].sValue, sTxt.Data, sTxt.Size) != 0))
			continue;
		if(pCtx->Records[i].bUncertain) return xacmeAwsUncertain();
		if(xacmeAwsMutateTxt(pCtx, pCtx->Records[i].sZoneId,
				pCtx->Records[i].sFqdn, pCtx->Records[i].sValue, false) == XACME_AWS_FAILED) return false;
		pCtx->Records[i].sFqdn[0] = '\0';
	}
	return true;
}

static bool xacmeAwsAdd(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAwsAddLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeAwsRemove(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednsawscontext* pCtx = (xacmednsawscontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeAwsRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

void xrtAcmeDnsAwsConfigInit(xacmednsawsconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKeyId = NULL;
	pConfig->sSecretAccessKey = NULL;
	pConfig->sRegion = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsAws(
	const xacmednsawsconfig* pConfig, struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider)
{
	xacmednsawscontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKeyId == NULL) ||
		(pConfig->sSecretAccessKey == NULL) ||
		(pConfig->sAccessKeyId[0] == '\0') ||
		(pConfig->sSecretAccessKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_aws requires access key id and secret");
		return false;
	}
	if(strlen(pConfig->sAccessKeyId) >= sizeof(pCtx->sId) ||
		strlen(pConfig->sSecretAccessKey) >= sizeof(pCtx->sKey) ||
		((pConfig->sRegion != NULL) &&
		 strlen(pConfig->sRegion) >= sizeof(pCtx->sRegion)) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_aws credentials, region or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednsawscontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtFree(pCtx);
		return false;
	}
	snprintf(pCtx->sId, sizeof(pCtx->sId), "%s", pConfig->sAccessKeyId);
	snprintf(pCtx->sKey, sizeof(pCtx->sKey), "%s",
		pConfig->sSecretAccessKey);
	snprintf(pCtx->sRegion, sizeof(pCtx->sRegion), "%s",
		((pConfig->sRegion != NULL) && (pConfig->sRegion[0] != '\0')) ?
			pConfig->sRegion : "us-east-1");
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"route53.amazonaws.com");
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		bool bReady = xacmeHttpUnit(&pCtx->Http);
		(void)xrtMutexUnit(&pCtx->Lock);
		if(bReady)
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
		return false;
	}
	pProvider->sId = "aws";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeAwsAdd;
	pProvider->Remove = xacmeAwsRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsAwsProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednsawscontext* pCtx = (xacmednsawscontext*)pProvider->pContext;
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_aws_txt.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_AWS)

#if defined(XACME_FEATURE_DNS_AWS)

#include <string.h>

#define XACME_AWS_XML_MAX (4u * 1024u * 1024u)
#define XACME_AWS_CHANGE_XML_MAX 262144u
#define XACME_AWS_CHANGE_VALUE_MAX 32000u

static void xacmeAwsSkipSpace(const char** pp, const char* pEnd)
{
	while((*pp < pEnd) && ((**pp == ' ') || (**pp == '\t') ||
		(**pp == '\r') || (**pp == '\n')))
		(*pp)++;
}

static bool xacmeAwsConsume(const char** pp, const char* pEnd, cstr sText)
{
	size_t iSize = strlen(sText);
	xacmeAwsSkipSpace(pp, pEnd);
	if(((size_t)(pEnd - *pp) < iSize) ||
		(memcmp(*pp, sText, iSize) != 0))
		return false;
	*pp += iSize;
	return true;
}

static bool xacmeAwsTake(const char** pp, const char* pEnd,
	cstr sOpen, cstr sClose, xstrview* pText)
{
	const char* pClose;
	if(!xacmeAwsConsume(pp, pEnd, sOpen))
		return false;
	pClose = strstr(*pp, sClose);
	if((pClose == NULL) || (pClose > pEnd) ||
		(memchr(*pp, '<', (size_t)(pClose - *pp)) != NULL))
		return false;
	*pText = (xstrview){ *pp, (size_t)(pClose - *pp) };
	*pp = pClose + strlen(sClose);
	return true;
}

static bool xacmeAwsNameEqual(xstrview Name, cstr sFqdn)
{
	size_t i;
	size_t iLen = strlen(sFqdn);
	if((Name.Size == iLen + 1u) && (Name.Data[iLen] == '.'))
		Name.Size--;
	if(Name.Size != iLen)
		return false;
	for(i = 0u; i < iLen; i++)
	{
		char a = Name.Data[i];
		char b = sFqdn[i];
		if((a >= 'A') && (a <= 'Z')) a = (char)(a + ('a' - 'A'));
		if((b >= 'A') && (b <= 'Z')) b = (char)(b + ('a' - 'A'));
		if(a != b) return false;
	}
	return true;
}

/* Route53's REST-XML error schema. No allocation, DTD or external entities.
 * Only a unique Error/Code in a completely validated envelope can authorize
 * replay. Default namespaces and a consistently bound root prefix are accepted. */
typedef struct xacmeawserrorxml {
	const char* p;
	const char* pEnd;
	xstrview Prefix;
} xacmeawserrorxml;

static bool xacmeAwsXmlAt(const xacmeawserrorxml* pXml, cstr sText)
{
	size_t iSize = strlen(sText);
	return (size_t)(pXml->pEnd - pXml->p) >= iSize &&
		memcmp(pXml->p, sText, iSize) == 0;
}

static bool xacmeAwsXmlChar(uint32 c)
{
	return c == 9u || c == 10u || c == 13u || (c >= 32u && c <= 0xd7ffu) ||
		(c >= 0xe000u && c <= 0xfffdu) || (c >= 0x10000u && c <= 0x10ffffu);
}

/* Validate UTF-8 and XML character references even in ignored messages. */
static bool xacmeAwsXmlCharTake(xacmeawserrorxml* pXml, uint32* pValue, bool bReference)
{
	uint32 c;
	unsigned i, n;
	uint32 iMin;
	if(pXml->p == pXml->pEnd) return false;
	c = (unsigned char)*pXml->p++;
	if(c == '&' && bReference) {
		static const char* Names[] = { "amp;", "lt;", "gt;", "quot;", "apos;" };
		static const uint32 Values[] = { '&', '<', '>', '"', '\'' };
		for(i = 0u; i < 5u; i++) if(xacmeAwsXmlAt(pXml, Names[i])) {
			pXml->p += strlen(Names[i]); *pValue = Values[i]; return true;
		}
		if(!xacmeAwsXmlAt(pXml, "#")) return false;
		pXml->p++; n = 10u; c = 0u; i = 0u;
		if(xacmeAwsXmlAt(pXml, "x")) { n = 16u; pXml->p++; }
		while(pXml->p < pXml->pEnd && *pXml->p != ';') {
			unsigned char b = (unsigned char)*pXml->p++;
			uint32 d = b >= '0' && b <= '9' ? (uint32)(b - '0') :
				n == 16u && b >= 'a' && b <= 'f' ? b - 'a' + 10u :
				n == 16u && b >= 'A' && b <= 'F' ? b - 'A' + 10u : n;
			if(d >= n || c > (0x10ffffu - d) / n) return false;
			c = c * n + d; i++;
		}
		if(i == 0u || pXml->p == pXml->pEnd) return false;
		pXml->p++;
	} else if(c >= 0x80u) {
		if(c >= 0xc2u && c <= 0xdfu) { n = 1u; iMin = 0x80u; c &= 0x1fu; }
		else if(c >= 0xe0u && c <= 0xefu) { n = 2u; iMin = 0x800u; c &= 0x0fu; }
		else if(c >= 0xf0u && c <= 0xf4u) { n = 3u; iMin = 0x10000u; c &= 7u; }
		else return false;
		for(i = 0u; i < n; i++) {
			unsigned char b;
			if(pXml->p == pXml->pEnd) return false;
			b = (unsigned char)*pXml->p++;
			if((b & 0xc0u) != 0x80u) return false;
			c = (c << 6u) | (b & 0x3fu);
		}
		if(c < iMin) return false;
	}
	*pValue = c;
	return xacmeAwsXmlChar(c);
}

static bool xacmeAwsXmlComment(xacmeawserrorxml* pXml)
{
	if(!xacmeAwsXmlAt(pXml, "<!--")) return false;
	pXml->p += 4u;
	while(pXml->p < pXml->pEnd) {
		uint32 c;
		if(xacmeAwsXmlAt(pXml, "-->")) { pXml->p += 3u; return true; }
		if(xacmeAwsXmlAt(pXml, "--")) return false;
		if(!xacmeAwsXmlCharTake(pXml, &c, false)) return false;
	}
	return false;
}

static bool xacmeAwsXmlSpace(xacmeawserrorxml* pXml)
{
	for(;;) {
		xacmeAwsSkipSpace(&pXml->p, pXml->pEnd);
		if(!xacmeAwsXmlAt(pXml, "<!--")) return true;
		if(!xacmeAwsXmlComment(pXml)) return false;
	}
}

static bool xacmeAwsXmlTag(xacmeawserrorxml* pXml, cstr sName,
	bool bClose, bool* pEmpty)
{
	xacmeawserrorxml Next = *pXml;
	if(!xacmeAwsXmlSpace(&Next) || !xacmeAwsXmlAt(&Next, bClose ? "</" : "<")) return false;
	Next.p += bClose ? 2u : 1u;
	if(Next.Prefix.Size != 0u) {
		if((size_t)(Next.pEnd - Next.p) <= Next.Prefix.Size ||
			memcmp(Next.p, Next.Prefix.Data, Next.Prefix.Size) != 0 ||
			Next.p[Next.Prefix.Size] != ':') return false;
		Next.p += Next.Prefix.Size + 1u;
	}
	if(!xacmeAwsXmlAt(&Next, sName)) return false;
	Next.p += strlen(sName);
	xacmeAwsSkipSpace(&Next.p, Next.pEnd);
	if(pEmpty != NULL) *pEmpty = !bClose && xacmeAwsXmlAt(&Next, "/>");
	if(!bClose && pEmpty != NULL && *pEmpty) Next.p++;
	if(!xacmeAwsXmlAt(&Next, ">")) return false;
	Next.p++; *pXml = Next;
	return true;
}

static bool xacmeAwsXmlTextImpl(xacmeawserrorxml* pXml, cstr sName, char* sCode)
{
	bool bEmpty;
	bool bCdata = false;
	size_t iSize = 0u;
	if(!xacmeAwsXmlTag(pXml, sName, false, &bEmpty)) return false;
	if(bEmpty) return sCode == NULL;
	while(pXml->p < pXml->pEnd) {
		uint32 c;
		if(!bCdata && xacmeAwsXmlAt(pXml, "<!--")) {
			if(!xacmeAwsXmlComment(pXml)) return false;
			continue;
		}
		if(!bCdata && xacmeAwsXmlAt(pXml, "<![CDATA[")) { pXml->p += 9u; bCdata = true; continue; }
		if(bCdata && xacmeAwsXmlAt(pXml, "]]>")) { pXml->p += 3u; bCdata = false; continue; }
		if(!bCdata && *pXml->p == '<') break;
		if((!bCdata && xacmeAwsXmlAt(pXml, "]]>")) || !xacmeAwsXmlCharTake(pXml, &c, !bCdata)) return false;
		if(sCode != NULL) {
			if(iSize == 63u || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '_')) return false;
			sCode[iSize++] = (char)c;
		}
	}
	if(bCdata) return false;
	if(sCode != NULL) { sCode[iSize] = '\0'; if(iSize == 0u) return false; }
	return xacmeAwsXmlTag(pXml, sName, true, NULL);
}

static bool xacmeAwsXmlText(xacmeawserrorxml* pXml, cstr sName, char* sCode)
{
	xacmeawserrorxml Next = *pXml;
	if(!xacmeAwsXmlTextImpl(&Next, sName, sCode)) return false;
	*pXml = Next;
	return true;
}

bool xacmeAwsErrorCode(xstrview Xml, char sCode[64])
{
	xacmeawserrorxml X;
	char Code[64] = { 0 };
	unsigned iSeen = 0u;
	bool bEmpty;
	const char* pName;
	if(sCode == NULL) return false;
	sCode[0] = '\0';
	if(Xml.Data == NULL || Xml.Size == 0u || Xml.Size > XACME_AWS_XML_MAX) return false;
	X = (xacmeawserrorxml){ Xml.Data, Xml.Data + Xml.Size, { NULL, 0u } };
	if(xacmeAwsXmlAt(&X, "\xef\xbb\xbf")) X.p += 3u;
	/* XML declaration is accepted only in its documented UTF-8 forms. */
	if(xacmeAwsXmlAt(&X, "<?xml")) {
		if(xacmeAwsXmlAt(&X, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>")) X.p += sizeof("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") - 1u;
		else if(xacmeAwsXmlAt(&X, "<?xml version='1.0' encoding='UTF-8'?>")) X.p += sizeof("<?xml version='1.0' encoding='UTF-8'?>") - 1u;
		else if(xacmeAwsXmlAt(&X, "<?xml version=\"1.0\"?>")) X.p += sizeof("<?xml version=\"1.0\"?>") - 1u;
		else return false;
	}
	if(!xacmeAwsXmlSpace(&X) || !xacmeAwsXmlAt(&X, "<")) return false;
	X.p++; pName = X.p;
	while(X.p < X.pEnd && ((*X.p >= 'A' && *X.p <= 'Z') ||
		(*X.p >= 'a' && *X.p <= 'z') || (*X.p >= '0' && *X.p <= '9') ||
		*X.p == '_' || *X.p == '-')) X.p++;
	if(xacmeAwsXmlAt(&X, ":")) {
		if(X.p == pName || X.p - pName > 32 ||
			!((*pName >= 'A' && *pName <= 'Z') || (*pName >= 'a' && *pName <= 'z') || *pName == '_')) return false;
		X.Prefix = (xstrview){ pName, (size_t)(X.p - pName) }; X.p++; pName = X.p;
		if(!xacmeAwsXmlAt(&X, "ErrorResponse")) return false;
		X.p += 13u;
	}
	if((size_t)(X.p - pName) != 13u || memcmp(pName, "ErrorResponse", 13u) != 0) return false;
	if(X.p < X.pEnd && (*X.p == ' ' || *X.p == '\t' || *X.p == '\n' || *X.p == '\r')) {
		char q;
		xacmeAwsSkipSpace(&X.p, X.pEnd);
		if(xacmeAwsXmlAt(&X, "xmlns")) {
			X.p += 5u;
			if(X.Prefix.Size != 0u) {
				if(!xacmeAwsXmlAt(&X, ":") || (size_t)(X.pEnd - ++X.p) < X.Prefix.Size ||
					memcmp(X.p, X.Prefix.Data, X.Prefix.Size) != 0) return false;
				X.p += X.Prefix.Size;
			}
			xacmeAwsSkipSpace(&X.p, X.pEnd);
			if(!xacmeAwsXmlAt(&X, "=")) return false;
			X.p++; xacmeAwsSkipSpace(&X.p, X.pEnd);
			if(X.p == X.pEnd || (*X.p != '\'' && *X.p != '"')) return false;
			q = *X.p++;
			if(xacmeAwsXmlAt(&X, "https://route53.amazonaws.com/doc/2013-04-01/")) X.p += sizeof("https://route53.amazonaws.com/doc/2013-04-01/") - 1u;
			else if(xacmeAwsXmlAt(&X, "http://route53.amazonaws.com/doc/2013-04-01/")) X.p += sizeof("http://route53.amazonaws.com/doc/2013-04-01/") - 1u;
			else return false;
			if(X.p == X.pEnd || *X.p++ != q) return false;
			xacmeAwsSkipSpace(&X.p, X.pEnd);
		} else if(X.Prefix.Size != 0u) return false;
	} else if(X.Prefix.Size != 0u) return false;
	if(!xacmeAwsXmlAt(&X, ">")) return false;
	X.p++;
	if(!xacmeAwsXmlTag(&X, "Error", false, &bEmpty) || bEmpty) return false;
	for(;;) {
		xacmeawserrorxml Next = X;
		unsigned iField;
		if(xacmeAwsXmlTag(&Next, "Error", true, NULL)) { X = Next; break; }
		if(xacmeAwsXmlText(&X, "Code", Code)) iField = 1u;
		else if(xacmeAwsXmlText(&X, "Type", NULL)) iField = 2u;
		else if(xacmeAwsXmlText(&X, "Message", NULL)) iField = 4u;
		else if(xacmeAwsXmlTag(&X, "Messages", false, &bEmpty)) {
			iField = 8u;
			if(!bEmpty) {
				for(;;) {
					Next = X;
					if(xacmeAwsXmlTag(&Next, "Messages", true, NULL)) { X = Next; break; }
					if(!xacmeAwsXmlText(&X, "Message", NULL)) return false;
				}
			}
		} else return false;
		if((iSeen & iField) != 0u) return false;
		iSeen |= iField;
	}
	if((iSeen & 1u) == 0u) return false;
	/* RequestId is optional in service errors, but duplicate IDs are invalid. */
	{
		xacmeawserrorxml Next = X;
		if(xacmeAwsXmlText(&Next, "RequestId", NULL)) X = Next;
	}
	if(!xacmeAwsXmlTag(&X, "ErrorResponse", true, NULL) ||
		!xacmeAwsXmlSpace(&X) || X.p != X.pEnd) return false;
	memcpy(sCode, Code, strlen(Code) + 1u);
	return true;
}

bool xacmeAwsChangeAccepted(cstr sXml)
{
	const char* p;
	const char* pEnd;
	xstrview Id, Status, Submitted, Comment;
	if(sXml == NULL || strlen(sXml) > XACME_AWS_XML_MAX) return false;
	p = sXml; pEnd = p + strlen(sXml);
	xacmeAwsSkipSpace(&p, pEnd);
	if(strncmp(p, "<?xml ", 6u) == 0) {
		const char* pClose = strstr(p + 6u, "?>");
		if(pClose == NULL) return false;
		p = pClose + 2u;
	}
	if(!xacmeAwsConsume(&p, pEnd, "<ChangeResourceRecordSetsResponse>") &&
		!xacmeAwsConsume(&p, pEnd, "<ChangeResourceRecordSetsResponse xmlns=\"https://route53.amazonaws.com/doc/2013-04-01/\">"))
		return false;
	if(!xacmeAwsConsume(&p, pEnd, "<ChangeInfo>")) return false;
	xacmeAwsSkipSpace(&p, pEnd);
	if(!xacmeAwsConsume(&p, pEnd, "<Comment/>") &&
		!xacmeAwsConsume(&p, pEnd, "<Comment />") && strncmp(p, "<Comment>", 9u) == 0 &&
		!xacmeAwsTake(&p, pEnd, "<Comment>", "</Comment>", &Comment)) return false;
	if(!xacmeAwsTake(&p, pEnd, "<Id>", "</Id>", &Id) || Id.Size == 0u ||
		!xacmeAwsTake(&p, pEnd, "<Status>", "</Status>", &Status) ||
		!((Status.Size == 7u && memcmp(Status.Data, "PENDING", 7u) == 0) ||
		  (Status.Size == 6u && memcmp(Status.Data, "INSYNC", 6u) == 0)) ||
		!xacmeAwsTake(&p, pEnd, "<SubmittedAt>", "</SubmittedAt>", &Submitted) ||
		Submitted.Size == 0u || !xacmeAwsConsume(&p, pEnd, "</ChangeInfo>") ||
		!xacmeAwsConsume(&p, pEnd, "</ChangeResourceRecordSetsResponse>")) return false;
	xacmeAwsSkipSpace(&p, pEnd);
	return p == pEnd;
}

static bool xacmeAwsTagText(const char* pBegin, const char* pEnd,
	cstr sOpen, cstr sClose, xstrview* pText)
{
	const char* p = strstr(pBegin, sOpen);
	const char* pClose;
	if((p == NULL) || (p >= pEnd)) return false;
	p += strlen(sOpen);
	pClose = strstr(p, sClose);
	if((pClose == NULL) || (pClose >= pEnd) ||
		(memchr(p, '<', (size_t)(pClose - p)) != NULL)) return false;
	*pText = (xstrview){ p, (size_t)(pClose - p) };
	return true;
}

int xacmeAwsSelectPublicZone(cstr sXml, cstr sZone,
	char* sOutId, size_t iIdCap)
{
	const char* p;
	const char* pEnd;
	xstrview Truncated;
	bool bFound = false;
	if((sXml == NULL) || (sZone == NULL) || (sOutId == NULL) ||
		(iIdCap == 0u) || (strlen(sXml) > XACME_AWS_XML_MAX) ||
		(strstr(sXml, "<ListHostedZonesByNameResponse") == NULL) ||
		(strstr(sXml, "</ListHostedZonesByNameResponse>") == NULL))
		return -1;
	sOutId[0] = '\0';
	p = strstr(sXml, "<HostedZones");
	if(p == NULL) return -1;
	if(strncmp(p, "<HostedZones/>", strlen("<HostedZones/>")) == 0)
	{
		p += strlen("<HostedZones/>");
		pEnd = p;
	}
	else
	{
		if(strncmp(p, "<HostedZones>", strlen("<HostedZones>")) != 0)
			return -1;
		p += strlen("<HostedZones>");
		pEnd = strstr(p, "</HostedZones>");
		if(pEnd == NULL) return -1;
	}
	while(p < pEnd)
	{
		const char* pBlock;
		const char* pBlockEnd;
		xstrview Name;
		xstrview Private;
		xstrview Id;
		size_t iIdLen;
		while((p < pEnd) && ((*p == ' ') || (*p == '\t') ||
			(*p == '\r') || (*p == '\n'))) p++;
		if(p == pEnd) break;
		if(((size_t)(pEnd - p) < strlen("<HostedZone>")) ||
			(memcmp(p, "<HostedZone>", strlen("<HostedZone>")) != 0))
			return -1;
		pBlock = p + strlen("<HostedZone>");
		pBlockEnd = strstr(pBlock, "</HostedZone>");
		if((pBlockEnd == NULL) || (pBlockEnd > pEnd) ||
			!xacmeAwsTagText(pBlock, pBlockEnd,
				"<Name>", "</Name>", &Name)) return -1;
		if(xacmeAwsNameEqual(Name, sZone))
		{
			bool bHasPrivate = xacmeAwsTagText(pBlock, pBlockEnd,
				"<PrivateZone>", "</PrivateZone>", &Private);
			const char* pPrivateTag = strstr(pBlock, "<PrivateZone>");
			if(!bHasPrivate && (pPrivateTag != NULL) &&
				(pPrivateTag < pBlockEnd)) return -1;
			if(!bHasPrivate || ((Private.Size == 5u) &&
				(memcmp(Private.Data, "false", 5u) == 0)))
			{
				size_t i;
				if(bFound || !xacmeAwsTagText(pBlock, pBlockEnd,
						"<Id>", "</Id>", &Id)) return -1;
				if((Id.Size > strlen("/hostedzone/")) &&
					(memcmp(Id.Data, "/hostedzone/",
						strlen("/hostedzone/")) == 0))
				{
					/* API 中 Id 常带 /hostedzone/ 前缀。 */
					Id.Data += strlen("/hostedzone/");
					Id.Size -= strlen("/hostedzone/");
				}
				iIdLen = Id.Size;
				if((iIdLen == 0u) || (iIdLen > 32u) ||
					(iIdLen >= iIdCap)) return -1;
				for(i = 0u; i < iIdLen; i++)
					if(!((Id.Data[i] >= 'A' && Id.Data[i] <= 'Z') ||
						(Id.Data[i] >= 'a' && Id.Data[i] <= 'z') ||
						(Id.Data[i] >= '0' && Id.Data[i] <= '9')))
						return -1;
				memcpy(sOutId, Id.Data, iIdLen);
				sOutId[iIdLen] = '\0';
				bFound = true;
			}
			else if(!((Private.Size == 4u) &&
				(memcmp(Private.Data, "true", 4u) == 0))) return -1;
		}
		p = pBlockEnd + strlen("</HostedZone>");
	}
	if(!xacmeAwsTagText(pEnd, sXml + strlen(sXml),
		"<IsTruncated>", "</IsTruncated>", &Truncated)) return -1;
	if((Truncated.Size == 4u) &&
		(memcmp(Truncated.Data, "true", 4u) == 0))
	{
		xstrview NextName;
		if(!xacmeAwsTagText(pEnd, sXml + strlen(sXml),
			"<NextDNSName>", "</NextDNSName>", &NextName)) return -1;
		if(xacmeAwsNameEqual(NextName, sZone)) return -1;
	}
	else if(!((Truncated.Size == 5u) &&
		(memcmp(Truncated.Data, "false", 5u) == 0))) return -1;
	return bFound ? 1 : 0;
}

bool xacmeAwsParseTxtSet(cstr sXml, cstr sFqdn, xacmeawstxtset* pSet)
{
	const char* pRoot;
	const char* pRootEnd;
	const char* p;
	const char* pEnd;
	xstrview Name;
	xstrview Type;
	xstrview Ttl;
	size_t i;
	if((sXml == NULL) || (sFqdn == NULL) || (pSet == NULL) ||
		(strlen(sXml) > XACME_AWS_XML_MAX))
		return false;
	memset(pSet, 0, sizeof(*pSet));
	if((strstr(sXml, "<ListResourceRecordSetsResponse") == NULL) ||
		(strstr(sXml, "</ListResourceRecordSetsResponse>") == NULL))
		return false;
	pRoot = strstr(sXml, "<ResourceRecordSets");
	if(pRoot == NULL) return false;
	if((strncmp(pRoot, "<ResourceRecordSets/>",
			strlen("<ResourceRecordSets/>")) == 0) ||
		(strncmp(pRoot, "<ResourceRecordSets />",
			strlen("<ResourceRecordSets />")) == 0))
		return true;
	if(strncmp(pRoot, "<ResourceRecordSets>",
		strlen("<ResourceRecordSets>")) != 0) return false;
	pRoot += strlen("<ResourceRecordSets>");
	pRootEnd = strstr(pRoot, "</ResourceRecordSets>");
	if(pRootEnd == NULL) return false;
	p = pRoot;
	xacmeAwsSkipSpace(&p, pRootEnd);
	if(p == pRootEnd) return true;
	if(!xacmeAwsConsume(&p, pRootEnd, "<ResourceRecordSet>"))
		return false;
	pEnd = strstr(p, "</ResourceRecordSet>");
	if((pEnd == NULL) || (pEnd > pRootEnd) ||
		!xacmeAwsTake(&p, pEnd, "<Name>", "</Name>", &Name) ||
		!xacmeAwsTake(&p, pEnd, "<Type>", "</Type>", &Type))
		return false;
	if(!xacmeAwsNameEqual(Name, sFqdn) ||
		(Type.Size != 3u) || (memcmp(Type.Data, "TXT", 3u) != 0))
		return true;
	if(!xacmeAwsTake(&p, pEnd, "<TTL>", "</TTL>", &Ttl) ||
		(Ttl.Size == 0u) || (Ttl.Size >= sizeof(pSet->sTtl)))
		return false;
	for(i = 0u; i < Ttl.Size; i++)
		if((Ttl.Data[i] < '0') || (Ttl.Data[i] > '9')) return false;
	memcpy(pSet->sTtl, Ttl.Data, Ttl.Size);
	pSet->sTtl[Ttl.Size] = '\0';
	if(!xacmeAwsConsume(&p, pEnd, "<ResourceRecords>")) return false;
	for(;;)
	{
		xstrview Value;
		xacmeAwsSkipSpace(&p, pEnd);
		if(xacmeAwsConsume(&p, pEnd, "</ResourceRecords>")) break;
		if((pSet->iCount >= XACME_AWS_TXT_MAX_VALUES) ||
			!xacmeAwsConsume(&p, pEnd, "<ResourceRecord>") ||
			!xacmeAwsTake(&p, pEnd, "<Value>", "</Value>", &Value) ||
			(Value.Size == 0u) ||
			!xacmeAwsConsume(&p, pEnd, "</ResourceRecord>"))
			return false;
		pSet->Values[pSet->iCount++] = Value;
	}
	xacmeAwsSkipSpace(&p, pEnd);
	if((p != pEnd) || (pSet->iCount == 0u)) return false;
	pSet->bPresent = true;
	return true;
}

static bool xacmeAwsAppend(xbuffer* pOut, cstr sText)
{
	return xrtBufferAppend(pOut,
		(xbytesview){ (const uint8*)sText, strlen(sText) });
}

static bool xacmeAwsAppendView(xbuffer* pOut, xstrview Text)
{
	return xrtBufferAppend(pOut,
		(xbytesview){ (const uint8*)Text.Data, Text.Size });
}

static size_t xacmeAwsQuoteWidth(const char* pText, size_t iRemaining)
{
	if((iRemaining >= 1u) && (pText[0] == '"')) return 1u;
	if((iRemaining >= 6u) &&
		(memcmp(pText, "&quot;", 6u) == 0)) return 6u;
	if((iRemaining >= 5u) &&
		(memcmp(pText, "&#34;", 5u) == 0)) return 5u;
	if((iRemaining >= 6u) &&
		((memcmp(pText, "&#x22;", 6u) == 0) ||
		 (memcmp(pText, "&#X22;", 6u) == 0))) return 6u;
	return 0u;
}

static bool xacmeAwsValueEqual(xstrview Value, cstr sValue)
{
	size_t iPrefix;
	size_t iSuffix;
	size_t iSize = strlen(sValue);
	iPrefix = xacmeAwsQuoteWidth(Value.Data, Value.Size);
	if((iPrefix == 0u) || (Value.Size < iPrefix + iSize + 1u))
		return false;
	iSuffix = xacmeAwsQuoteWidth(Value.Data + iPrefix + iSize,
		Value.Size - iPrefix - iSize);
	return (iSuffix > 0u) &&
		(Value.Size == iPrefix + iSize + iSuffix) &&
		(memcmp(Value.Data + iPrefix, sValue, iSize) == 0);
}

static bool xacmeAwsAppendSet(xbuffer* pOut, const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, bool bNew)
{
	size_t i;
	if(!xacmeAwsAppend(pOut, "<ResourceRecordSet><Name>") ||
		!xacmeAwsAppend(pOut, sFqdn) ||
		!xacmeAwsAppend(pOut, ".</Name><Type>TXT</Type><TTL>") ||
		!xacmeAwsAppend(pOut, pOld->bPresent ? pOld->sTtl : "60") ||
		!xacmeAwsAppend(pOut, "</TTL><ResourceRecords>"))
		return false;
	for(i = 0u; i < pOld->iCount; i++)
	{
		if(bNew && !bAdd && xacmeAwsValueEqual(pOld->Values[i], sValue))
			continue;
		if(!xacmeAwsAppend(pOut, "<ResourceRecord><Value>") ||
			!xacmeAwsAppendView(pOut, pOld->Values[i]) ||
			!xacmeAwsAppend(pOut, "</Value></ResourceRecord>"))
			return false;
	}
	if(bNew && bAdd)
	{
		if(!xacmeAwsAppend(pOut, "<ResourceRecord><Value>\"") ||
			!xacmeAwsAppend(pOut, sValue) ||
			!xacmeAwsAppend(pOut, "\"</Value></ResourceRecord>"))
			return false;
	}
	return xacmeAwsAppend(pOut, "</ResourceRecords></ResourceRecordSet>");
}

static bool xacmeAwsAppendChange(xbuffer* pOut, cstr sAction,
	const xacmeawstxtset* pOld, cstr sFqdn, cstr sValue,
	bool bAdd, bool bNew)
{
	return xacmeAwsAppend(pOut, "<Change><Action>") &&
		xacmeAwsAppend(pOut, sAction) &&
		xacmeAwsAppend(pOut, "</Action>") &&
		xacmeAwsAppendSet(pOut, pOld, sFqdn, sValue, bAdd, bNew) &&
		xacmeAwsAppend(pOut, "</Change>");
}

bool xacmeAwsBuildTxtChange(const xacmeawstxtset* pOld,
	cstr sFqdn, cstr sValue, bool bAdd, xbuffer* pOut, bool* pChanged)
{
	size_t i;
	size_t iOldValueBytes = 0u;
	size_t iNewValueBytes = 0u;
	bool bFound = false;
	bool bKeep;
	if((pOld == NULL) || (sFqdn == NULL) || (sValue == NULL) ||
		(pOut == NULL) || (pChanged == NULL)) return false;
	*pChanged = false;
	for(i = 0u; i < pOld->iCount; i++)
	{
		iOldValueBytes += pOld->Values[i].Size;
		if(xacmeAwsValueEqual(pOld->Values[i], sValue)) bFound = true;
		else iNewValueBytes += pOld->Values[i].Size;
	}
	if((bAdd && bFound) || (!bAdd && !bFound)) return true;
	if(bAdd) iNewValueBytes = iOldValueBytes + strlen(sValue) + 2u;
	if((pOld->iCount > XACME_AWS_TXT_MAX_VALUES) ||
		(bAdd && (pOld->iCount == XACME_AWS_TXT_MAX_VALUES)) ||
		(iOldValueBytes > XACME_AWS_CHANGE_VALUE_MAX) ||
		(iNewValueBytes > XACME_AWS_CHANGE_VALUE_MAX - iOldValueBytes))
		return false;
	bKeep = bAdd || (pOld->iCount > 1u);
	xrtBufferClear(pOut);
	if(!xacmeAwsAppend(pOut,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<ChangeResourceRecordSetsRequest xmlns=\"https://route53."
		"amazonaws.com/doc/2013-04-01/\"><ChangeBatch><Changes>"))
		return false;
	if(pOld->bPresent &&
		!xacmeAwsAppendChange(pOut, "DELETE", pOld, sFqdn, sValue,
			bAdd, false)) return false;
	if(bKeep &&
		!xacmeAwsAppendChange(pOut, "CREATE", pOld, sFqdn, sValue,
			bAdd, true)) return false;
	if(!xacmeAwsAppend(pOut,
		"</Changes></ChangeBatch></ChangeResourceRecordSetsRequest>") ||
		(pOut->Size > XACME_AWS_CHANGE_XML_MAX) ||
		!xrtBufferAppendByte(pOut, 0u)) return false;
	*pChanged = true;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/dns/xacme_dns_huawei.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_DNS_HUAWEI)

#if defined(XACME_FEATURE_DNS_HUAWEI)



#include <stdarg.h>
#include <stdlib.h>

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256）：
	  - canonical：content-type/host/x-sdk-date 三头（小写字典序）；
	  - StringToSign = "SDK-HMAC-SHA256\n<X-Sdk-Date>\n"
	    "<sha256hex(canonical)>"（无凭据范围）；
	  - 签名：HMAC(SK, StringToSign)，canonical URI 与查询串分开；
	  - zone 发现：GET /v2/zones?name=<候选>&limit=2&search_mode=equal；
	  - 加 TXT：POST /v2/zones/<id>/recordsets
	    （name 带尾点，records 值内嵌双引号）；
	  - 删 TXT：DELETE /v2/zones/<id>/recordsets/<recordset id>。
*/

typedef struct xacmednshuaaweicontext {
	xacmehttp Http;
	xmutex Lock;
	char sAk[160];
	char sSk[160];
	char sEndpoint[160];
	xacmednsrecords Records;
	bool bUncertain[XACME_DNS_RECORD_MAX];
	bool bDeletePending[XACME_DNS_RECORD_MAX];
	bool bDeleteAccepted[XACME_DNS_RECORD_MAX];
} xacmednshuaaweicontext;

static void xacmeHuaweiError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns", XACME_DNS_ERROR_PROTOCOL,
		sMessage);
}

bool xacmeDnsHuaweiBuildUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery)
{
	return xacmeDnsHttpsUrl(sOutput, iCapacity, sEndpoint,
		sPathAndQuery);
}

static bool xacmeHuaweiFormat(char* sOut, size_t iCapacity,
	cstr sFormat, ...)
{
	va_list Args;
	int iWritten;
	va_start(Args, sFormat);
	iWritten = vsnprintf(sOut, iCapacity, sFormat, Args);
	va_end(Args);
	if((iWritten < 0) || ((size_t)iWritten >= iCapacity))
	{
		if((sOut != NULL) && (iCapacity > 0u))
		{
			sOut[0] = '\0';
		}
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei signature input exceeds capacity");
		return false;
	}
	return true;
}

static bool xacmeHuaweiUnreserved(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ||
		((c >= 'a') && (c <= 'z')) ||
		((c >= '0') && (c <= '9')) ||
		(c == '-') || (c == '_') || (c == '.') || (c == '~');
}

/* 当前 provider 只构造 ASCII unreserved 路径/参数；拒绝未编码的分隔符。 */
bool xacmeDnsHuaweiCanonicalTarget(cstr sTarget,
	char* sUri, size_t iUriCapacity, char* sQuery, size_t iQueryCapacity)
{
	char sParts[320];
	char* pParts[16];
	const char* pQuery;
	size_t iPathSize;
	size_t iQuerySize;
	size_t iCount = 0u;
	size_t i;
	size_t iUsed = 0u;
	if((sTarget == NULL) || (sUri == NULL) || (sQuery == NULL) ||
		(iUriCapacity == 0u) || (iQueryCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT, "acme dns_huawei target is invalid");
		return false;
	}
	sUri[0] = '\0';
	sQuery[0] = '\0';
	pQuery = strchr(sTarget, '?');
	iPathSize = (pQuery != NULL) ? (size_t)(pQuery - sTarget) :
		strlen(sTarget);
	if((iPathSize == 0u) || (sTarget[0] != '/') ||
		(iPathSize + ((sTarget[iPathSize - 1u] == '/') ? 1u : 2u) >
			iUriCapacity))
	{
		goto Invalid;
	}
	for(i = 0u; i < iPathSize; ++i)
	{
		if(!xacmeHuaweiUnreserved(sTarget[i]) && (sTarget[i] != '/'))
		{
			goto Invalid;
		}
		if((sTarget[i] == '.') && ((i == 0u) || (sTarget[i - 1u] == '/')) &&
			((i + 1u == iPathSize) || (sTarget[i + 1u] == '/') ||
			((sTarget[i + 1u] == '.') &&
				((i + 2u == iPathSize) || (sTarget[i + 2u] == '/')))))
		{
			goto Invalid;
		}
	}
	memcpy(sUri, sTarget, iPathSize);
	if(sUri[iPathSize - 1u] != '/')
	{
		sUri[iPathSize++] = '/';
	}
	sUri[iPathSize] = '\0';
	if(pQuery == NULL)
	{
		return true;
	}
	++pQuery;
	iQuerySize = strlen(pQuery);
	if((iQuerySize == 0u) || (iQuerySize >= sizeof(sParts)))
	{
		goto Invalid;
	}
	for(i = 0u; i < iQuerySize; ++i)
	{
		if(!xacmeHuaweiUnreserved(pQuery[i]) &&
			(pQuery[i] != '=') && (pQuery[i] != '&'))
		{
			goto Invalid;
		}
	}
	memcpy(sParts, pQuery, iQuerySize + 1u);
	pParts[iCount++] = sParts;
	for(i = 0u; i < iQuerySize; ++i)
	{
		if(sParts[i] == '&')
		{
			if((iCount >= (sizeof(pParts) / sizeof(pParts[0]))) ||
				(i == 0u) || (sParts[i + 1u] == '\0') ||
				(sParts[i - 1u] == '&'))
			{
				goto Invalid;
			}
			sParts[i] = '\0';
			pParts[iCount++] = sParts + i + 1u;
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t j;
		char* pEquals = strchr(pParts[i], '=');
		if((pEquals == NULL) || (pParts[i][0] == '=') ||
			(strchr(pEquals + 1u, '=') != NULL))
		{
			goto Invalid;
		}
		for(j = i + 1u; j < iCount; ++j)
		{
			if(strcmp(pParts[i], pParts[j]) > 0)
			{
				char* pSwap = pParts[i];
				pParts[i] = pParts[j];
				pParts[j] = pSwap;
			}
		}
	}
	for(i = 0u; i < iCount; ++i)
	{
		size_t iPartSize = strlen(pParts[i]);
		if((iUsed + iPartSize + ((i > 0u) ? 1u : 0u) + 1u) >
			iQueryCapacity)
		{
			goto Invalid;
		}
		if(i > 0u)
		{
			sQuery[iUsed++] = '&';
		}
		memcpy(sQuery + iUsed, pParts[i], iPartSize);
		iUsed += iPartSize;
		sQuery[iUsed] = '\0';
	}
	return true;
Invalid:
	sUri[0] = '\0';
	sQuery[0] = '\0';
	xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
		XACME_DNS_ERROR_ARGUMENT,
		"acme dns_huawei canonical target is invalid or too long");
	return false;
}

bool xacmeDnsHuaweiAuthorization(
	cstr sAk, cstr sSk, cstr sEndpoint, cstr sMethod,
	cstr sPathAndQuery, cstr sBody, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sStamp, size_t iStampCapacity)
{
	static const char* sSignedHeaders = "content-type;host;x-sdk-date";
	char sPayloadHash[XACME_SIG_HASH_TEXT];
	char sCanonical[1600];
	char sHeaders[360];
	char sStringToSign[160];
	char sHex[XACME_SIG_HASH_TEXT];
	char sUri[512];
	char sQuery[320];
	xdatetime Now;
	uint8 Signature[XRT_SHA256_SIZE];
	if((sAk == NULL) || (sSk == NULL) || (sEndpoint == NULL) ||
		(sMethod == NULL) || (sPathAndQuery == NULL) ||
		(sAuth == NULL) || (sStamp == NULL) ||
		(iAuthCapacity == 0u) || (iStampCapacity == 0u))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei signature arguments are invalid");
		return false;
	}
	sAuth[0] = '\0';
	sStamp[0] = '\0';
	if(!xrtTimeSplitAt(iNow, 0, &Now) ||
		!xacmeHuaweiFormat(sStamp, iStampCapacity,
			"%04ld%02d%02dT%02d%02d%02dZ", (long)Now.Year,
			Now.Month, Now.Day, Now.Hour, Now.Minute, Now.Second) ||
		!xacmeDnsHuaweiCanonicalTarget(sPathAndQuery,
			sUri, sizeof(sUri), sQuery, sizeof(sQuery)) ||
		!xacmeSigSha256Hex((sBody != NULL) ? sBody : "",
			(sBody != NULL) ? strlen(sBody) : 0u, sPayloadHash) ||
		!xacmeHuaweiFormat(sHeaders, sizeof(sHeaders),
			"content-type:application/json\nhost:%s\nx-sdk-date:%s\n",
			sEndpoint, sStamp) ||
		!xacmeSigCanonical(sCanonical, sizeof(sCanonical), sMethod,
			sUri, sQuery, sHeaders, sSignedHeaders, sPayloadHash) ||
		!xacmeSigSha256Hex(sCanonical, strlen(sCanonical), sHex) ||
		!xacmeHuaweiFormat(sStringToSign, sizeof(sStringToSign),
			"SDK-HMAC-SHA256\n%s\n%s", sStamp, sHex))
	{
		return false;
	}
	if(!xacmeSigHmac((const uint8*)sSk, strlen(sSk), sStringToSign,
			strlen(sStringToSign), Signature))
	{
		xrtSecureZero(Signature, sizeof(Signature));
		return false;
	}
	xacmeSigHex(Signature, sizeof(Signature), sHex);
	xrtSecureZero(Signature, sizeof(Signature));
	return xacmeHuaweiFormat(sAuth, iAuthCapacity,
		"SDK-HMAC-SHA256 Access=%s, SignedHeaders=%s, Signature=%s",
		sAk, sSignedHeaders, sHex);
}

static bool xacmeHuaweiCall(
	xacmednshuaaweicontext* pCtx, cstr sMethod, cstr sPathAndQuery,
	cstr sBody, uint16* pOutStatus, str* pOutBody, size_t* pOutSize)
{
	char sStampText[24];
	char sAuth[560];
	char sUrl[512];
	xacmehttpheader Extra[3];
	xacmehttpresponse R;

	pCtx->Http.bWriteUncertain = false;
	if(!xacmeDnsHuaweiAuthorization(pCtx->sAk, pCtx->sSk,
			pCtx->sEndpoint, sMethod, sPathAndQuery, sBody, xrtNow(),
			sAuth, sizeof(sAuth), sStampText, sizeof(sStampText)) ||
		!xacmeDnsHuaweiBuildUrl(sUrl, sizeof(sUrl), pCtx->sEndpoint,
			sPathAndQuery))
	{
		return false;
	}

	Extra[0] = (xacmehttpheader){ "Authorization", sAuth };
	Extra[1] = (xacmehttpheader){ "X-Sdk-Date", sStampText };
	if(!xacmeHttpExchangeV(
			&pCtx->Http, sMethod, sUrl, "application/json",
			(xstrview){ sBody, (sBody != NULL) ? strlen(sBody) : 0u },
			Extra, 2u, &R))
	{
		return false;
	}
	*pOutStatus = R.iStatus;
	*pOutBody = R.sBody;
	if(pOutSize != NULL) *pOutSize = R.iBodySize;
	R.sBody = NULL;
	xacmeHttpResponseUnit(&R);
	return true;
}

/* zones?name= 精确匹配候选（zone 名带尾点，比较时剥除）。 */
static xacmednszoneresult xacmeHuaweiZoneId(
	xacmednshuaaweicontext* pCtx, cstr sZone, char* sOutId, size_t iIdCap)
{
	char sPath[300];
	char sWantDot[280];
	uint16 iStatus = 0u;
	str sBody = NULL;
	size_t iBodySize = 0u;
	xacmednszoneresult Result;

	if(!xacmeHuaweiFormat(sPath, sizeof(sPath),
			"/v2/zones?name=%s.&limit=2&search_mode=equal", sZone) ||
		!xacmeHuaweiFormat(sWantDot, sizeof(sWantDot), "%s.", sZone))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(!xacmeHuaweiCall(pCtx, "GET", sPath, NULL, &iStatus, &sBody, &iBodySize))
	{
		return XACME_DNS_ZONE_ERROR;
	}
	if(iStatus == 200u)
		Result = xacmeDnsJsonZoneId(
			(xstrview){ sBody, iBodySize },
			"zones", sWantDot, false, 2u, sOutId, iIdCap);
	else
	{
		Result = XACME_DNS_ZONE_ERROR;
		xacmeHuaweiError(
			(iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION :
			(iStatus == 429u || iStatus >= 500u) ? XERR_AGAIN : XERR_PROTOCOL,
			"acme dns_huawei zone query rejected or unavailable");
	}
	xrtFree(sBody);
	return Result;
}

static bool xacmeHuaweiFindZone(
	xacmednshuaaweicontext* pCtx, cstr sFqdn, char* sOutZone,
	size_t iZoneCap, char* sOutId, size_t iIdCap)
{
	char sCandidate[256];
	xacmednszoneresult Lookup;
	snprintf(sCandidate, sizeof(sCandidate), "%s", sFqdn);
	for(;;)
	{
		char* sDot;
		Lookup = xacmeHuaweiZoneId(pCtx, sCandidate, sOutId, iIdCap);
		if(Lookup == XACME_DNS_ZONE_FOUND)
		{
			snprintf(sOutZone, iZoneCap, "%s", sCandidate);
			return true;
		}
		if(Lookup == XACME_DNS_ZONE_ERROR) return false;
		sDot = strchr(sCandidate, '.');
		if((sDot == NULL) || (strchr(sDot + 1, '.') == NULL)) break;
		memmove(sCandidate, sDot + 1, strlen(sDot + 1) + 1u);
	}
	xrtSetErrorInfo(XERR_PROTOCOL, "xrt.acme.dns", XACME_DNS_ERROR_ZONE,
		"acme dns_huawei managed zone not found");
	return false;
}

bool xacmeDnsHuaweiBuildCreateBody(
	xbuffer* pBody, xstrview sFqdn, xstrview sTxt)
{
	char sDotted[257];
	char sQuotedTxt[203];
	if((pBody == NULL) || (sFqdn.Data == NULL) || (sTxt.Data == NULL) ||
		(sFqdn.Size == 0u) || (sFqdn.Size > 255u) ||
		(sTxt.Size == 0u) || (sTxt.Size > 200u))
		return false;
	memcpy(sDotted, sFqdn.Data, sFqdn.Size);
	sDotted[sFqdn.Size] = '.';
	sDotted[sFqdn.Size + 1u] = '\0';
	sQuotedTxt[0] = '"';
	memcpy(sQuotedTxt + 1u, sTxt.Data, sTxt.Size);
	sQuotedTxt[sTxt.Size + 1u] = '"';
	sQuotedTxt[sTxt.Size + 2u] = '\0';
	return xrtBufferAppend(pBody, XRT_BYTES_LITERAL("{\"name\":")) &&
		xacmeDnsJsonQuote(pBody,
			(xstrview){ sDotted, sFqdn.Size + 1u }) &&
		xrtBufferAppend(pBody, XRT_BYTES_LITERAL(
			",\"type\":\"TXT\",\"ttl\":60,\"records\":[")) &&
		xacmeDnsJsonQuote(pBody,
			(xstrview){ sQuotedTxt, sTxt.Size + 2u }) &&
		xrtBufferAppend(pBody, XRT_BYTES_LITERAL("]}"));
}

static bool xacmeHuaweiCreateRejected(const xvalue* pRoot, uint16 iStatus)
{
	char sCode[128], sMessage[512];
	return iStatus >= 400u && iStatus < 500u && pRoot != NULL && xrtValueIs(pRoot, XVALUE_OBJECT) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("id")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("type")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("zone_id")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records")) == NULL &&
		xacmeDnsJsonText(pRoot, "error_code", sCode, sizeof(sCode)) && sCode[0] != '\0' &&
		xacmeDnsJsonText(pRoot, "error_msg", sMessage, sizeof(sMessage)) && sMessage[0] != '\0';
}

static bool xacmeHuaweiCreateIdentity(const xvalue* pRoot, cstr sOwner,
	cstr sZoneId, xstrview Txt)
{
	char sDotted[257], sQuoted[203];
	xvalue* pRecords;
	xstrview Value;
	snprintf(sDotted, sizeof(sDotted), "%s.", sOwner);
	sQuoted[0] = '"'; memcpy(sQuoted + 1u, Txt.Data, Txt.Size);
	sQuoted[Txt.Size + 1u] = '"'; sQuoted[Txt.Size + 2u] = '\0';
	if(pRoot == NULL || !xrtValueIs(pRoot, XVALUE_OBJECT) ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_code")) != NULL ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_msg")) != NULL ||
		!xacmeDnsJsonEqual(pRoot, "name", sDotted) || !xacmeDnsJsonEqual(pRoot, "type", "TXT") ||
		!xacmeDnsJsonEqual(pRoot, "zone_id", sZoneId)) return false;
	pRecords = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records"));
	return xrtValueIs(pRecords, XVALUE_ARRAY) && xrtValueCount(pRecords) == 1u &&
		xrtValueGetString(xrtValueArrayGet(pRecords, 0u), &Value) &&
		Value.Size == Txt.Size + 2u && memcmp(Value.Data, sQuoted, Value.Size) == 0;
}

static bool xacmeHuaweiDeletePendingError(void)
{
	if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xrtSetErrorInfo(XERR_AGAIN, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
			"acme dns_huawei deletion is not complete or recordset is not steady");
	return false;
}

static bool xacmeHuaweiAddLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)pProvider->pContext;
	char sFqdnText[256];
	char sZone[256];
	char sZoneId[80];
	xbuffer Body;
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot = NULL;
	char sRecordId[80];
	bool bOk = false;
	bool bTracked = false;
	bool bSent = false;
	size_t iSlot;

	if(!xacmeDnsChallengeValid(sFqdn, sTxt))
	{
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei owner or digest is invalid");
		return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sFqdnText);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, sFqdn, sTxt))
		return xacmeHuaweiDeletePendingError();
	iSlot = xacmeDnsCreateSlot(&pCtx->Records, pCtx->bUncertain);
	if(iSlot >= XACME_DNS_RECORD_MAX)
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns_huawei record tracking capacity exhausted");
		return false;
	}

	if(!xacmeHuaweiFindZone(pCtx, sFqdnText, sZone, sizeof(sZone), sZoneId,
			sizeof(sZoneId)))
	{
		return false;
	}

	/* name 带尾点；records 值必须内嵌双引号。 */
	xrtBufferInit(&Body);
	if(xacmeDnsHuaweiBuildCreateBody(&Body, sFqdn, sTxt) &&
		xrtBufferAppendByte(&Body, 0u))
	{
		char sPath[128];
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets",
			sZoneId);
		xacmeDnsCreateReserve(&pCtx->Records, pCtx->bUncertain, iSlot, sFqdn, sTxt);
		bSent = true;
		bOk = xacmeHuaweiCall(pCtx, "POST", sPath, (cstr)Body.Data,
			&iStatus, &sResp, &iRespSize);
	}
	xrtBufferUnit(&Body);
	if(!bOk)
	{
		xrtFree(sResp);
		if(!bSent) return false;
		if(!pCtx->Http.bWriteUncertain) {
			xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
			return false;
		}
		return xacmeDnsCreateUncertainError();
	}
	if(sResp != NULL)
	{
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	}
	if(xacmeHuaweiCreateRejected(pRoot, iStatus)) {
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
		xrtValueRelease(pRoot); xrtFree(sResp);
		xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_huawei create was rejected");
		return false;
	}
	if(iStatus >= 200u && iStatus < 300u && xacmeHuaweiCreateIdentity(pRoot, sFqdnText, sZoneId, sTxt) &&
		xacmeDnsJsonPathId(pRoot, "id", sRecordId, sizeof(sRecordId)))
	{
		bTracked = xacmeDnsCreateCommit(&pCtx->Records, pCtx->bUncertain,
			iSlot, sZoneId, '|', sRecordId);
	}
	xrtValueRelease(pRoot);
	xrtFree(sResp);
	return bTracked ? true : xacmeDnsCreateUncertainError();
}

static bool xacmeHuaweiRecordMissing(const xvalue* pRoot, uint16 iStatus)
{
	return iStatus == 404u && xacmeHuaweiCreateRejected(pRoot, iStatus) &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("status")) == NULL &&
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")) == NULL &&
		xacmeDnsJsonEqual(pRoot, "error_code", "DNS.0313");
}

static bool xacmeHuaweiRecordIdentity(const xvalue* pRoot, cstr sOwner,
	cstr sZoneId, cstr sRecordId, xstrview Txt, bool bRequireValues)
{
	char sDotted[257];
	bool bDefault = true;
	snprintf(sDotted, sizeof(sDotted), "%s.", sOwner);
	if(!xrtValueIs(pRoot, XVALUE_OBJECT) ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_code")) != NULL ||
		xrtValueObjectGet(pRoot, XRT_STR_LITERAL("error_msg")) != NULL ||
		!xacmeDnsJsonEqual(pRoot, "id", sRecordId) ||
		!xacmeDnsJsonEqual(pRoot, "name", sDotted) ||
		!xacmeDnsJsonEqual(pRoot, "type", "TXT") ||
		!xacmeDnsJsonEqual(pRoot, "zone_id", sZoneId) ||
		!xrtValueGetBool(xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")), &bDefault) || bDefault) return false;
	/* The documented DELETE example omits records. Present values must still
	 * match the unique owned TXT; GET always requires the complete value list. */
	return (!bRequireValues && xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records")) == NULL) ||
		xacmeHuaweiCreateIdentity(pRoot, sOwner, sZoneId, Txt);
}

typedef enum xacmehuaweirecordresult {
	XACME_HUAWEI_RECORD_ERROR,
	XACME_HUAWEI_RECORD_FOUND,
	XACME_HUAWEI_RECORD_MISSING,
	XACME_HUAWEI_RECORD_DELETING,
	XACME_HUAWEI_RECORD_BUSY,
	XACME_HUAWEI_RECORD_INACTIVE
} xacmehuaweirecordresult;

static xacmehuaweirecordresult xacmeHuaweiReadRecord(xacmednshuaaweicontext* pCtx,
	cstr sPath, cstr sZoneId, cstr sRecordId, cstr sOwner, xstrview Txt, bool bAdding)
{
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	xvalue* pRoot;
	xacmehuaweirecordresult Result = XACME_HUAWEI_RECORD_ERROR;
	char sStatus[32];
	xrtClearError();
	if(!xacmeHuaweiCall(pCtx, "GET", sPath, NULL, &iStatus, &sResp, &iRespSize)) {
		xrtFree(sResp); return Result;
	}
	pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
	if(xacmeHuaweiRecordMissing(pRoot, iStatus)) Result = XACME_HUAWEI_RECORD_MISSING;
	else if(iStatus == 200u && xacmeHuaweiRecordIdentity(pRoot, sOwner, sZoneId, sRecordId, Txt, true) &&
		xacmeDnsJsonText(pRoot, "status", sStatus, sizeof(sStatus))) {
		if(strcmp(sStatus, "PENDING_DELETE") == 0) Result = XACME_HUAWEI_RECORD_DELETING;
		else if(strcmp(sStatus, "ACTIVE") == 0) Result = XACME_HUAWEI_RECORD_FOUND;
		else if(strcmp(sStatus, "DISABLE") == 0 ||
			strcmp(sStatus, "FREEZE") == 0 || strcmp(sStatus, "ILLEGAL") == 0 ||
			strcmp(sStatus, "POLICE") == 0 || strcmp(sStatus, "ERROR") == 0)
			Result = bAdding ? XACME_HUAWEI_RECORD_INACTIVE : XACME_HUAWEI_RECORD_FOUND;
		else if(strcmp(sStatus, "PENDING_CREATE") == 0 || strcmp(sStatus, "PENDING_UPDATE") == 0 ||
			strcmp(sStatus, "PENDING_FREEZE") == 0 || strcmp(sStatus, "PENDING_DISABLE") == 0) Result = XACME_HUAWEI_RECORD_BUSY;
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(Result == XACME_HUAWEI_RECORD_BUSY) (void)xacmeHuaweiDeletePendingError();
	else if(Result == XACME_HUAWEI_RECORD_INACTIVE)
		xacmeHuaweiError(XERR_STATE, "acme dns_huawei owned recordset is not active");
	else if(Result != XACME_HUAWEI_RECORD_ERROR) xrtClearError();
	else if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
			"acme dns_huawei record read failed or does not match its tracked identity");
	return Result;
}

static xacmednsownedresult xacmeHuaweiReuseOwned(xacmednshuaaweicontext* pCtx,
	xstrview Owner, xstrview Txt)
{
	char sOwner[256];
	if(!xacmeDnsChallengeValid(Owner, Txt)) return XACME_DNS_OWNED_NONE;
	Owner = xacmeDnsCanonicalOwner(Owner, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, Owner, Txt)) {
		(void)xacmeDnsCreateUncertainError(); return XACME_DNS_OWNED_ERROR;
	}
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bDeletePending, Owner, Txt)) {
		(void)xacmeHuaweiDeletePendingError(); return XACME_DNS_OWNED_ERROR;
	}
	for(;;) {
		char sZoneId[80], sRecordId[80], sPath[200];
		size_t iSlot = xacmeDnsRecordFindOwned(&pCtx->Records, Owner, Txt);
		xacmehuaweirecordresult Result;
		if(iSlot == XACME_DNS_RECORD_MAX) return XACME_DNS_OWNED_NONE;
		if(!xacmeDnsRecordSplit(pCtx->Records.sIds[iSlot], '|', sZoneId,
				sizeof(sZoneId), sRecordId, sizeof(sRecordId))) {
			xacmeHuaweiError(XERR_PROTOCOL, "acme dns_huawei tracked record handle is invalid");
			return XACME_DNS_OWNED_ERROR;
		}
		snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets/%s", sZoneId, sRecordId);
		Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, Txt, true);
		if(Result == XACME_HUAWEI_RECORD_FOUND) return XACME_DNS_OWNED_VALID;
		if(Result == XACME_HUAWEI_RECORD_DELETING) {
			pCtx->bDeletePending[iSlot] = true; pCtx->bDeleteAccepted[iSlot] = true;
			(void)xacmeHuaweiDeletePendingError(); return XACME_DNS_OWNED_ERROR;
		}
		if(Result != XACME_HUAWEI_RECORD_MISSING) return XACME_DNS_OWNED_ERROR;
		pCtx->bDeletePending[iSlot] = false; pCtx->bDeleteAccepted[iSlot] = false;
		xacmeDnsCreateCancel(&pCtx->Records, pCtx->bUncertain, iSlot);
	}
}

static bool xacmeHuaweiAwaitDelete(xacmednshuaaweicontext* pCtx, size_t iSlot,
	cstr sPath, cstr sZoneId, cstr sRecordId, cstr sOwner, xstrview Txt)
{
	uint32 iAttempt;
	static const uint32 Delays[] = { 500u, 1000u, 2000u };
	for(iAttempt = 0u; iAttempt < 4u; iAttempt++) {
		xacmehuaweirecordresult Result;
		if(iAttempt != 0u) xrtSleep(Delays[iAttempt - 1u]);
		Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, Txt, false);
		if(Result == XACME_HUAWEI_RECORD_MISSING) {
			pCtx->bDeletePending[iSlot] = false;
			pCtx->bDeleteAccepted[iSlot] = false;
			return true;
		}
		if(Result == XACME_HUAWEI_RECORD_ERROR || Result == XACME_HUAWEI_RECORD_BUSY) return false;
		if(Result == XACME_HUAWEI_RECORD_DELETING) {
			pCtx->bDeletePending[iSlot] = true;
			pCtx->bDeleteAccepted[iSlot] = true;
		}
		/* Without a verified acceptance, an ACTIVE record does not prove that
		 * the uncertain write committed. Leave its error and handle to caller. */
		if(!pCtx->bDeleteAccepted[iSlot]) return false;
	}
	return xacmeHuaweiDeletePendingError();
}

static bool xacmeHuaweiDeleteUncertain(void)
{
	xerror* pError;
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	pError = xrtErrorWrap(xrtGetError(), XERR_IO, "xrt.acme.dns",
		XACME_DNS_ERROR_UNCERTAIN, "Huawei DNS deletion outcome unknown; original record retained");
	if(pError != NULL) xrtSetErrorTake(pError);
	return false;
}

static bool xacmeHuaweiDeleteRecord(void* pContext, cstr sId,
	xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)pContext;
	char sZoneId[80];
	char sRecordId[80];
	char sPath[200];
	uint16 iStatus = 0u;
	str sResp = NULL;
	size_t iRespSize = 0u;
	bool bOk;
	bool bPreviousPending;
	char sOwner[256];
	size_t iSlot;
	xvalue* pRoot = NULL;
	xacmehuaweirecordresult Result;
	if(!xacmeDnsRecordSplit(sId, '|', sZoneId, sizeof(sZoneId),
			sRecordId, sizeof(sRecordId)))
	{
		xacmeHuaweiError(XERR_PROTOCOL, "acme dns_huawei tracked record handle is invalid"); return false;
	}
	for(iSlot = 0u; iSlot < pCtx->Records.iCount; iSlot++)
		if(strcmp(pCtx->Records.sIds[iSlot], sId) == 0) break;
	if(iSlot == pCtx->Records.iCount) {
		xacmeHuaweiError(XERR_STATE, "acme dns_huawei record is not tracked"); return false;
	}
	memcpy(sOwner, sFqdn.Data, sFqdn.Size); sOwner[sFqdn.Size] = '\0';
	snprintf(sPath, sizeof(sPath), "/v2/zones/%s/recordsets/%s",
		sZoneId, sRecordId);
	Result = xacmeHuaweiReadRecord(pCtx, sPath, sZoneId, sRecordId, sOwner, sTxt, false);
	if(Result == XACME_HUAWEI_RECORD_MISSING) {
		pCtx->bDeletePending[iSlot] = false; pCtx->bDeleteAccepted[iSlot] = false; return true;
	}
	if(Result == XACME_HUAWEI_RECORD_ERROR || Result == XACME_HUAWEI_RECORD_BUSY) return false;
	if(Result == XACME_HUAWEI_RECORD_DELETING) {
		pCtx->bDeletePending[iSlot] = true; pCtx->bDeleteAccepted[iSlot] = true;
	}
	if(pCtx->bDeleteAccepted[iSlot])
		return xacmeHuaweiAwaitDelete(pCtx, iSlot, sPath, sZoneId, sRecordId, sOwner, sTxt);
	bPreviousPending = pCtx->bDeletePending[iSlot];
	pCtx->bDeletePending[iSlot] = true;
	bOk = xacmeHuaweiCall(pCtx, "DELETE", sPath, NULL, &iStatus, &sResp, &iRespSize);
	if(!bOk && !pCtx->Http.bWriteUncertain) {
		pCtx->bDeletePending[iSlot] = bPreviousPending; xrtFree(sResp); return false;
	}
	if(bOk) {
		pRoot = xrtJsonParse((xstrview){ sResp, iRespSize });
		bOk = iStatus == 202u && xacmeHuaweiRecordIdentity(pRoot, sOwner, sZoneId, sRecordId, sTxt, false) &&
			xacmeDnsJsonEqual(pRoot, "status", "PENDING_DELETE");
		if(!bOk && xacmeHuaweiCreateRejected(pRoot, iStatus) && !xacmeHuaweiRecordMissing(pRoot, iStatus) &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("status")) == NULL &&
			xrtValueObjectGet(pRoot, XRT_STR_LITERAL("default")) == NULL &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
			bool bBusy = xacmeDnsJsonEqual(pRoot, "error_code", "DNS.0314");
			pCtx->bDeletePending[iSlot] = bPreviousPending;
			xrtValueRelease(pRoot); xrtFree(sResp);
			if(bBusy) return xacmeHuaweiDeletePendingError();
			xacmeHuaweiError((iStatus == 401u || iStatus == 403u) ? XERR_PERMISSION : XERR_PROTOCOL,
				"acme dns_huawei record deletion was rejected"); return false;
		}
	}
	xrtValueRelease(pRoot); xrtFree(sResp);
	if(bOk) pCtx->bDeleteAccepted[iSlot] = true;
	else (void)xacmeHuaweiDeleteUncertain();
	if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) return false;
	{
		xerror* pWriteError = xrtTakeError();
		bOk = xacmeHuaweiAwaitDelete(pCtx, iSlot, sPath, sZoneId, sRecordId, sOwner, sTxt);
		if(!bOk && xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorTake(pWriteError);
		else xrtErrorFree(pWriteError);
	}
	return bOk;
}

static bool xacmeHuaweiRemoveLocked(
	xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx =
		(xacmednshuaaweicontext*)pProvider->pContext;
	char sOwner[256];
	if(!xacmeDnsChallengeValid(sFqdn, sTxt)) {
		xacmeHuaweiError(XERR_ARGUMENT, "acme dns_huawei owner or digest is invalid"); return false;
	}
	sFqdn = xacmeDnsCanonicalOwner(sFqdn, sOwner);
	if(xacmeDnsCreateBlocked(&pCtx->Records, pCtx->bUncertain, sFqdn, sTxt))
		return xacmeDnsCreateUncertainError();
	return xacmeDnsRecordRemoveMatching(&pCtx->Records, sFqdn, sTxt,
		xacmeHuaweiDeleteRecord, pCtx);
}

static bool xacmeHuaweiAdd(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	xacmednsownedresult Existing;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	Existing = xacmeHuaweiReuseOwned(pCtx, sFqdn, sTxt);
	bOk = Existing == XACME_DNS_OWNED_VALID ||
		(Existing == XACME_DNS_OWNED_NONE && xacmeHuaweiAddLocked(pProvider, sFqdn, sTxt));
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

static bool xacmeHuaweiRemove(xacmednsprovider* pProvider, xstrview sFqdn, xstrview sTxt)
{
	xacmednshuaaweicontext* pCtx = (xacmednshuaaweicontext*)xacmeDnsProviderContext(pProvider);
	bool bOk;
	if(pCtx == NULL) return false;
	if(!xrtMutexLock(&pCtx->Lock)) return false;
	if(!xacmeDnsProviderReady(&pCtx->Http))
	{
		(void)xrtMutexUnlock(&pCtx->Lock);
		return false;
	}
	bOk = xacmeHuaweiRemoveLocked(pProvider, sFqdn, sTxt);
	(void)xrtMutexUnlock(&pCtx->Lock);
	return bOk;
}

void xrtAcmeDnsHuaweiConfigInit(xacmednshuaaweiconfig* pConfig)
{
	if(pConfig == NULL)
	{
		return;
	}
	pConfig->sAccessKey = NULL;
	pConfig->sSecretKey = NULL;
	pConfig->sEndpoint = NULL;
}

bool xrtAcmeDnsHuawei(
	const xacmednshuaaweiconfig* pConfig,
	struct xnetengine* pBorrowedEngine, xacmednsprovider* pProvider)
{
	xacmednshuaaweicontext* pCtx;
	if((pConfig == NULL) || (pProvider == NULL) ||
		(pConfig->sAccessKey == NULL) || (pConfig->sSecretKey == NULL) ||
		(pConfig->sAccessKey[0] == '\0') ||
		(pConfig->sSecretKey[0] == '\0'))
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_huawei requires access key and secret");
		return false;
	}
	if(strlen(pConfig->sAccessKey) >= sizeof(pCtx->sAk) ||
		strlen(pConfig->sSecretKey) >= sizeof(pCtx->sSk) ||
		((pConfig->sEndpoint != NULL) &&
		 strlen(pConfig->sEndpoint) >= sizeof(pCtx->sEndpoint)))
	{
		xrtSetErrorInfo(XERR_RANGE, "xrt.acme.dns",
			XACME_DNS_ERROR_CREDENTIAL,
			"acme dns_huawei credentials or endpoint exceed capacity");
		return false;
	}
	pCtx = (xacmednshuaaweicontext*)xrtCalloc(1, sizeof(*pCtx));
	if(pCtx == NULL)
	{
		return false;
	}
	snprintf(pCtx->sAk, sizeof(pCtx->sAk), "%s", pConfig->sAccessKey);
	snprintf(pCtx->sSk, sizeof(pCtx->sSk), "%s", pConfig->sSecretKey);
	snprintf(pCtx->sEndpoint, sizeof(pCtx->sEndpoint), "%s",
		(pConfig->sEndpoint != NULL) ? pConfig->sEndpoint :
			"dns.myhuaweicloud.com");
	if(!xrtMutexInit(&pCtx->Lock))
	{
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		return false;
	}
	if(!xacmeHttpInit(&pCtx->Http, pBorrowedEngine, NULL, 0u))
	{
		(void)xrtMutexUnit(&pCtx->Lock);
		if(xacmeHttpUnit(&pCtx->Http))
		{
			xrtSecureZero(pCtx, sizeof(*pCtx));
			xrtFree(pCtx);
		}
		else xacmeHttpDeferOwner(&pCtx->Http, sizeof(*pCtx));
		return false;
	}
	pProvider->sId = "huawei";
	pProvider->iCaps = 0u;
	pProvider->pContext = pCtx;
	pProvider->Add = xacmeHuaweiAdd;
	pProvider->Remove = xacmeHuaweiRemove;
	pProvider->Propagate = NULL;
	return true;
}

void xrtAcmeDnsHuaweiProviderUnit(xacmednsprovider* pProvider)
{
	if((pProvider != NULL) && (pProvider->pContext != NULL))
	{
		xacmednshuaaweicontext* pCtx =
			(xacmednshuaaweicontext*)pProvider->pContext;
		if(!xacmeHttpUnit(&pCtx->Http)) return;
		(void)xrtMutexUnit(&pCtx->Lock);
		xrtSecureZero(pCtx, sizeof(*pCtx));
		xrtFree(pCtx);
		pProvider->pContext = NULL;
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xacme/src/acme/xacme_obtain.c */
/* ========================================================================== */

#if defined(XACME_FEATURE_ACME_OBTAIN)

#if defined(XACME_FEATURE_ACME_OBTAIN)



#include <string.h>

/*
	一站式组合：账户持久化复用 + IssueStored。
	账户钥从 store 读出为 xrtMalloc 文本，账户配置借用它完成
	客户端构建后由本函数释放。
*/

static void xacmeObtainError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.obtain", 1, sMessage);
}

void xrtAcmeObtainConfigInit(xacmeobtainconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xacmeObtainError(
			XERR_ARGUMENT, "acme obtain config init requires config");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}

bool xrtAcmeObtain(
	const xacmeobtainconfig* pConfig, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	xacmeissuegrant* pOut, bool* pbRenewed)
{
	xacmeaccountconfig Account;
	xacmeclientconfig ClientConfig;
	struct xacmeclient* pClient = NULL;
	str sStoredAccountPem = NULL;
	str sFreshAccountPem = NULL;
	bool bResult = false;

	if((pConfig == NULL) || (pConfig->pAccount == NULL) ||
		(pDomains == NULL) || (iDomainCount == 0u) || (pDns == NULL) ||
		(pOut == NULL) || (pbRenewed == NULL) ||
		(pConfig->sStoreRoot == NULL) || (pConfig->sStoreRoot[0] == '\0'))
	{
		xacmeObtainError(
			XERR_ARGUMENT,
			"acme obtain requires config with account, store root, "
				"domains, dns provider and outputs");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	*pbRenewed = false;

	/* 账户层：store 有则复用，无则开户后持久化。 */
	Account = *pConfig->pAccount;
	sStoredAccountPem = xrtAcmeStoreLoadAccount(
		pConfig->sStoreRoot, Account.sDirectoryUrl);
	if(sStoredAccountPem != NULL)
	{
		Account.sAccountKeyPem = sStoredAccountPem;
		Account.Eab.sKid = NULL; /* 复用账户无需再绑定。 */
		Account.Eab.sHmac = NULL;
	}

	xrtAcmeClientConfigInit(&ClientConfig);
	ClientConfig.pAccount = &Account;
	ClientConfig.sCaPem = pConfig->sCaPem;
	ClientConfig.pBorrowedEngine = pConfig->pBorrowedEngine;
	ClientConfig.uTimeoutMs = pConfig->uTimeoutMs;
	ClientConfig.sPropagateResolvers = pConfig->sPropagateResolvers;
	ClientConfig.iPropagateResolverCount =
		pConfig->iPropagateResolverCount;
	ClientConfig.uPropagateTimeoutMs = pConfig->uPropagateTimeoutMs;
	ClientConfig.uIssueTimeoutMs = pConfig->uIssueTimeoutMs;
	ClientConfig.sCertKeyPem = pConfig->sCertKeyPem;

	pClient = xrtAcmeClientCreate(&ClientConfig);
	if(pClient == NULL)
	{
		goto Done;
	}
	if(sStoredAccountPem == NULL)
	{
		sFreshAccountPem = xrtAcmeClientAccountPem(pClient);
		if((sFreshAccountPem != NULL) &&
			!xrtAcmeStoreSaveAccount(
				pConfig->sStoreRoot, Account.sDirectoryUrl,
				sFreshAccountPem))
		{
			/* 账户落盘失败不作废本次签发；续期时会再开新账户。 */
			xacmeObtainError(
				XERR_IO, "acme obtain save account failed");
			goto Done;
		}
	}

	if(!xrtAcmeClientIssueStored(
			pClient, pDomains, iDomainCount, pDns, pConfig->sStoreRoot,
			(pConfig->iRenewalDays != 0) ? pConfig->iRenewalDays : 30,
			pOut, pbRenewed))
	{
		goto Done;
	}
	bResult = true;

Done:
	xrtFree(sFreshAccountPem);
	xrtFree(sStoredAccountPem);
	if(pClient != NULL)
	{
		/* 此临时客户端不会交付调用者；回滚单独留出退休预算。 */
		if(pClient->Http.uTimeoutMs < INT64_C(30000))
			pClient->Http.uTimeoutMs = INT64_C(30000);
		xacmeClientDiscard(pClient);
	}
	if(!bResult)
	{
		xrtFree(pOut->sFullchainPem);
		xrtFree(pOut->sKeyPem);
		memset(pOut, 0, sizeof(*pOut));
	}
	return bResult;
}

#endif
#endif

#endif
