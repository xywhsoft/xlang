// Copyright (c) 2004-2013 Sergey Lyubka
// Copyright (c) 2013-2025 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see http://www.gnu.org/licenses/
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, as set out in https://www.mongoose.ws/licensing/
//
// SPDX-License-Identifier: GPL-2.0-only or commercial

#ifndef MONGOOSE_H
#define MONGOOSE_H

#define MG_VERSION "7.19"

#ifdef __cplusplus
extern "C" {
#endif





#define MG_SOCKET_TYPE int
#define MG_DATA_SIZE 32
#define MG_MAX_HTTP_HEADERS 30
#ifdef PATH_MAX
	#define MG_PATH_MAX PATH_MAX
#else
	#define MG_PATH_MAX 128
#endif





// 请求类型
#define HTTP_UNKNOWN	0
#define HTTP_GET		1
#define HTTP_POST		2
#define HTTP_HEAD		3
#define HTTP_OPTIONS	4
#define HTTP_PUT		5
#define HTTP_DELETE		6
#define HTTP_TRACE		7
#define HTTP_CONNECT	8





// Describes an arbitrary chunk of memory
struct mg_str {
  char *buf;   // String data
  size_t len;  // String length
};

// Using macro to avoid shadowing C++ struct constructor, see #1298
#define mg_str(s) mg_str_s(s)

struct mg_str mg_str(const char *s);
struct mg_str mg_str_n(const char *s, size_t n);
int mg_casecmp(const char *s1, const char *s2);
int mg_strcmp(const struct mg_str str1, const struct mg_str str2);
int mg_strcasecmp(const struct mg_str str1, const struct mg_str str2);
struct mg_str mg_strdup(const struct mg_str s);
bool mg_match(struct mg_str str, struct mg_str pattern, struct mg_str *caps);
bool mg_span(struct mg_str s, struct mg_str *a, struct mg_str *b, char delim);

bool mg_str_to_num(struct mg_str, int base, void *val, size_t val_len);




// Single producer, single consumer non-blocking queue

struct mg_queue {
  char *buf;
  size_t size;
  volatile size_t tail;
  volatile size_t head;
};

void mg_queue_init(struct mg_queue *, char *, size_t);        // Init queue
size_t mg_queue_book(struct mg_queue *, char **buf, size_t);  // Reserve space
void mg_queue_add(struct mg_queue *, size_t);                 // Add new message
size_t mg_queue_next(struct mg_queue *, char **);  // Get oldest message
void mg_queue_del(struct mg_queue *, size_t);      // Delete oldest message




typedef void (*mg_pfn_t)(char, void *);                  // Output function
typedef size_t (*mg_pm_t)(mg_pfn_t, void *, va_list *);  // %M printer

size_t mg_vxprintf(void (*)(char, void *), void *, const char *fmt, va_list *);
size_t mg_xprintf(void (*fn)(char, void *), void *, const char *fmt, ...);






// Convenience wrappers around mg_xprintf
size_t mg_vsnprintf(char *buf, size_t len, const char *fmt, va_list *ap);
size_t mg_snprintf(char *, size_t, const char *fmt, ...);
char *mg_vmprintf(const char *fmt, va_list *ap);
char *mg_mprintf(const char *fmt, ...);
size_t mg_queue_vprintf(struct mg_queue *, const char *fmt, va_list *);
size_t mg_queue_printf(struct mg_queue *, const char *fmt, ...);

// %M print helper functions
size_t mg_print_base64(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_esc(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_hex(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_ip(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_ip_port(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_ip4(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_ip6(void (*out)(char, void *), void *arg, va_list *ap);
size_t mg_print_mac(void (*out)(char, void *), void *arg, va_list *ap);

// Various output functions
void mg_pfn_iobuf(char ch, void *param);  // param: struct mg_iobuf *
void mg_pfn_stdout(char c, void *param);  // param: ignored

// A helper macro for printing JSON: mg_snprintf(buf, len, "%m", MG_ESC("hi"))
#define MG_ESC(str) mg_print_esc, 0, (str)






enum { MG_LL_NONE, MG_LL_ERROR, MG_LL_INFO, MG_LL_DEBUG, MG_LL_VERBOSE };
extern int mg_log_level;  // Current log level, one of MG_LL_*

void mg_log(const char *fmt, ...);
void mg_log_prefix(int ll, const char *file, int line, const char *fname);
// bool mg_log2(int ll, const char *file, int line, const char *fmt, ...);
void mg_hexdump(const void *buf, size_t len);
void mg_log_set_fn(mg_pfn_t fn, void *param);

#define mg_log_set(level_) mg_log_level = (level_)

#if MG_ENABLE_LOG
#define MG_LOG(level, args)                                 \
  do {                                                      \
    if ((level) <= mg_log_level) {                          \
      mg_log_prefix((level), __FILE__, __LINE__, __func__); \
      mg_log args;                                          \
    }                                                       \
  } while (0)
#else
#define MG_LOG(level, args) \
  do {                      \
    if (0) mg_log args;     \
  } while (0)
#endif

#define MG_ERROR(args) MG_LOG(MG_LL_ERROR, args)
#define MG_INFO(args) MG_LOG(MG_LL_INFO, args)
#define MG_DEBUG(args) MG_LOG(MG_LL_DEBUG, args)
#define MG_VERBOSE(args) MG_LOG(MG_LL_VERBOSE, args)




struct mg_timer {
  uint64_t period_ms;          // Timer period in milliseconds
  uint64_t expire;             // Expiration timestamp in milliseconds
  unsigned flags;              // Possible flags values below
#define MG_TIMER_ONCE 0        // Call function once
#define MG_TIMER_REPEAT 1      // Call function periodically
#define MG_TIMER_RUN_NOW 2     // Call immediately when timer is set
#define MG_TIMER_CALLED 4      // Timer function was called at least once
#define MG_TIMER_AUTODELETE 8  // mg_free() timer when done
  void (*fn)(void *);          // Function to call
  void *arg;                   // Function argument
  struct mg_timer *next;       // Linkage
};

void mg_timer_init(struct mg_timer **head, struct mg_timer *timer,
                   uint64_t milliseconds, unsigned flags, void (*fn)(void *),
                   void *arg);
void mg_timer_free(struct mg_timer **head, struct mg_timer *);
void mg_timer_poll(struct mg_timer **head, uint64_t new_ms);
bool mg_timer_expired(uint64_t *expiration, uint64_t period, uint64_t now);





enum { MG_FS_READ = 1, MG_FS_WRITE = 2, MG_FS_DIR = 4 };

// Filesystem API functions
// st() returns MG_FS_* flags and populates file size and modification time
// ls() calls fn() for every directory entry, allowing to list a directory
//
// NOTE: UNIX-style shorthand names for the API functions are deliberately
// chosen to avoid conflicts with some libraries that make macros for e.g.
// stat(), write(), read() calls.
struct mg_fs {
  int (*st)(const char *path, size_t *size, time_t *mtime);  // stat file
  void (*ls)(const char *path, void (*fn)(const char *, void *),
             void *);  // List directory entries: call fn(file_name, fn_data)
                       // for each directory entry
  void *(*op)(const char *path, int flags);             // Open file
  void (*cl)(void *fd);                                 // Close file
  size_t (*rd)(void *fd, void *buf, size_t len);        // Read file
  size_t (*wr)(void *fd, const void *buf, size_t len);  // Write file
  size_t (*sk)(void *fd, size_t offset);                // Set file position
  bool (*mv)(const char *from, const char *to);         // Rename file
  bool (*rm)(const char *path);                         // Delete file
  bool (*mkd)(const char *path);                        // Create directory
};

extern struct mg_fs mg_fs_posix;   // POSIX open/close/read/write/seek
extern struct mg_fs mg_fs_packed;  // see tutorials/core/embedded-filesystem
extern struct mg_fs mg_fs_fat;     // FAT FS

// File descriptor
struct mg_fd {
  void *fd;
  struct mg_fs *fs;
};

struct mg_fd *mg_fs_open(struct mg_fs *fs, const char *path, int flags);
void mg_fs_close(struct mg_fd *fd);
bool mg_fs_ls(struct mg_fs *fs, const char *path, char *buf, size_t len);
struct mg_str mg_file_read(struct mg_fs *fs, const char *path);
bool mg_file_write(struct mg_fs *fs, const char *path, const void *, size_t);
bool mg_file_printf(struct mg_fs *fs, const char *path, const char *fmt, ...);

// Packed API
const char *mg_unpack(const char *path, size_t *size, time_t *mtime);
const char *mg_unlist(size_t no);             // Get no'th packed filename
struct mg_str mg_unpacked(const char *path);  // Packed file as mg_str



void *mg_calloc(size_t count, size_t size);
void mg_free(void *ptr);
void mg_bzero(volatile unsigned char *buf, size_t len);
bool mg_random(void *buf, size_t len);
char *mg_random_str(char *buf, size_t len);
uint32_t mg_crc32(uint32_t crc, const char *buf, size_t len);
uint64_t mg_millis(void);  // Return milliseconds since boot
bool mg_path_is_sane(const struct mg_str path);
void mg_delayms(unsigned int ms);

#define MG_U32(a, b, c, d)                                         \
  (((uint32_t) ((a) &255) << 24) | ((uint32_t) ((b) &255) << 16) | \
   ((uint32_t) ((c) &255) << 8) | (uint32_t) ((d) &255))

#define MG_IPV4(a, b, c, d) mg_htonl(MG_U32(a, b, c, d))

// For printing IPv4 addresses: printf("%d.%d.%d.%d\n", MG_IPADDR_PARTS(&ip))
#define MG_U8P(ADDR) ((uint8_t *) (ADDR))
#define MG_IPADDR_PARTS(ADDR) \
  MG_U8P(ADDR)[0], MG_U8P(ADDR)[1], MG_U8P(ADDR)[2], MG_U8P(ADDR)[3]

#define MG_LOAD_BE16(p) \
  ((uint16_t) (((uint16_t) MG_U8P(p)[0] << 8U) | MG_U8P(p)[1]))
#define MG_LOAD_BE24(p)                           \
  ((uint32_t) (((uint32_t) MG_U8P(p)[0] << 16U) | \
               ((uint32_t) MG_U8P(p)[1] << 8U) | MG_U8P(p)[2]))
#define MG_LOAD_BE32(p)                           \
  ((uint32_t) (((uint32_t) MG_U8P(p)[0] << 24U) | \
               ((uint32_t) MG_U8P(p)[1] << 16U) | \
               ((uint32_t) MG_U8P(p)[2] << 8U) | MG_U8P(p)[3]))
#define MG_STORE_BE16(p, n)           \
  do {                                \
    MG_U8P(p)[0] = ((n) >> 8U) & 255; \
    MG_U8P(p)[1] = (n) &255;          \
  } while (0)
#define MG_STORE_BE24(p, n)            \
  do {                                 \
    MG_U8P(p)[0] = ((n) >> 16U) & 255; \
    MG_U8P(p)[1] = ((n) >> 8U) & 255;  \
    MG_U8P(p)[2] = (n) &255;           \
  } while (0)
#define MG_STORE_BE32(p, n)            \
  do {                                 \
    MG_U8P(p)[0] = ((n) >> 24U) & 255; \
    MG_U8P(p)[1] = ((n) >> 16U) & 255; \
    MG_U8P(p)[2] = ((n) >> 8U) & 255;  \
    MG_U8P(p)[3] = (n) &255;           \
  } while (0)

uint16_t mg_ntohs(uint16_t net);
uint32_t mg_ntohl(uint32_t net);
#define mg_htons(x) mg_ntohs(x)
#define mg_htonl(x) mg_ntohl(x)

#define MG_REG(x) ((volatile uint32_t *) (x))[0]
#define MG_BIT(x) (((uint32_t) 1U) << (x))
#define MG_SET_BITS(R, CLRMASK, SETMASK) (R) = ((R) & ~(CLRMASK)) | (SETMASK)

#define MG_ROUND_UP(x, a) ((a) == 0 ? (x) : ((((x) + (a) -1) / (a)) * (a)))
#define MG_ROUND_DOWN(x, a) ((a) == 0 ? (x) : (((x) / (a)) * (a)))

#if defined(__GNUC__) && defined(__arm__)
#ifdef __ZEPHYR__
#define MG_ARM_DISABLE_IRQ() __asm__ __volatile__("cpsid i" : : : "memory")
#define MG_ARM_ENABLE_IRQ() __asm__ __volatile__("cpsie i" : : : "memory")
#else
#define MG_ARM_DISABLE_IRQ() asm volatile("cpsid i" : : : "memory")
#define MG_ARM_ENABLE_IRQ() asm volatile("cpsie i" : : : "memory")
#endif // !ZEPHYR
#elif defined(__CCRH__)
#define MG_RH850_DISABLE_IRQ() __DI()
#define MG_RH850_ENABLE_IRQ() __EI()
#else
#define MG_ARM_DISABLE_IRQ()
#define MG_ARM_ENABLE_IRQ()
#endif

#if defined(__CC_ARM)
#define MG_DSB() __dsb(0xf)
#elif defined(__ARMCC_VERSION)
#define MG_DSB() __builtin_arm_dsb(0xf)
#elif defined(__GNUC__) && defined(__arm__) && defined(__thumb__)
#ifdef __ZEPHYR__
#define MG_DSB() __asm__("DSB 0xf")
#else
#define MG_DSB() asm("DSB 0xf")
#endif // !ZEPHYR
#elif defined(__ICCARM__)
#define MG_DSB() __iar_builtin_DSB()
#else
#define MG_DSB()
#endif

struct mg_addr;
int mg_check_ip_acl(struct mg_str acl, struct mg_addr *remote_ip);

// Linked list management macros
#define LIST_ADD_HEAD(type_, head_, elem_) \
  do {                                     \
    (elem_)->next = (*head_);              \
    *(head_) = (elem_);                    \
  } while (0)

#define LIST_ADD_TAIL(type_, head_, elem_) \
  do {                                     \
    type_ **h = head_;                     \
    while (*h != NULL) h = &(*h)->next;    \
    *h = (elem_);                          \
  } while (0)

#define LIST_DELETE(type_, head_, elem_)   \
  do {                                     \
    type_ **h = head_;                     \
    while (*h != (elem_)) h = &(*h)->next; \
    *h = (elem_)->next;                    \
  } while (0)



unsigned short mg_url_port(const char *url);
int mg_url_is_ssl(const char *url);
struct mg_str mg_url_host(const char *url);
struct mg_str mg_url_user(const char *url);
struct mg_str mg_url_pass(const char *url);
const char *mg_url_uri(const char *url);




struct mg_iobuf {
  unsigned char *buf;  // Pointer to stored data
  size_t size;         // Total size available
  size_t len;          // Current number of bytes
  size_t align;        // Alignment during allocation
};

int mg_iobuf_init(struct mg_iobuf *, size_t, size_t);
int mg_iobuf_resize(struct mg_iobuf *, size_t);
void mg_iobuf_free(struct mg_iobuf *);
size_t mg_iobuf_add(struct mg_iobuf *, size_t, const void *, size_t);
size_t mg_iobuf_del(struct mg_iobuf *, size_t ofs, size_t len);


size_t mg_base64_update(unsigned char input_byte, char *buf, size_t len);
size_t mg_base64_final(char *buf, size_t len);
size_t mg_base64_encode(const unsigned char *p, size_t n, char *buf, size_t);
size_t mg_base64_decode(const char *src, size_t n, char *dst, size_t);




typedef struct {
  uint32_t buf[4];
  uint32_t bits[2];
  unsigned char in[64];
} mg_md5_ctx;

void mg_md5_init(mg_md5_ctx *c);
void mg_md5_update(mg_md5_ctx *c, const unsigned char *data, size_t len);
void mg_md5_final(mg_md5_ctx *c, unsigned char[16]);




typedef struct {
  uint32_t state[5];
  uint32_t count[2];
  unsigned char buffer[64];
} mg_sha1_ctx;

void mg_sha1_init(mg_sha1_ctx *);
void mg_sha1_update(mg_sha1_ctx *, const unsigned char *data, size_t len);
void mg_sha1_final(unsigned char digest[20], mg_sha1_ctx *);
// https://github.com/B-Con/crypto-algorithms
// Author:     Brad Conte (brad AT bradconte.com)
// Disclaimer: This code is presented "as is" without any guarantees.
// Details:    Defines the API for the corresponding SHA1 implementation.
// Copyright:  public domain





typedef struct {
  uint32_t state[8];
  uint64_t bits;
  uint32_t len;
  unsigned char buffer[64];
} mg_sha256_ctx;


void mg_sha256_init(mg_sha256_ctx *);
void mg_sha256_update(mg_sha256_ctx *, const unsigned char *data, size_t len);
void mg_sha256_final(unsigned char digest[32], mg_sha256_ctx *);
void mg_sha256(uint8_t dst[32], uint8_t *data, size_t datasz);
void mg_hmac_sha256(uint8_t dst[32], uint8_t *key, size_t keysz, uint8_t *data,
                    size_t datasz);

typedef struct {
    uint64_t state[8];
    uint8_t buffer[128];
    uint64_t bitlen[2];
    uint32_t datalen;
} mg_sha384_ctx;
void mg_sha384_init(mg_sha384_ctx *ctx);
void mg_sha384_update(mg_sha384_ctx *ctx, const uint8_t *data, size_t len);
void mg_sha384_final(uint8_t digest[48], mg_sha384_ctx *ctx);
void mg_sha384(uint8_t dst[48], uint8_t *data, size_t datasz);



struct mg_connection;
typedef void (*mg_event_handler_t)(struct mg_connection *, int ev,
                                   void *ev_data);
void mg_call(struct mg_connection *c, int ev, void *ev_data);
void mg_error(struct mg_connection *c, const char *fmt, ...);

enum {
  MG_EV_ERROR,      // Error                        char *error_message
  MG_EV_OPEN,       // Connection created           NULL
  MG_EV_POLL,       // mg_mgr_poll iteration        uint64_t *uptime_millis
  MG_EV_RESOLVE,    // Host name is resolved        NULL
  MG_EV_CONNECT,    // Connection established       NULL
  MG_EV_ACCEPT,     // Connection accepted          NULL
  MG_EV_TLS_HS,     // TLS handshake succeeded      NULL
  MG_EV_READ,       // Data received from socket    long *bytes_read
  MG_EV_WRITE,      // Data written to socket       long *bytes_written
  MG_EV_CLOSE,      // Connection closed            NULL
  MG_EV_HTTP_HDRS,  // HTTP headers                 struct mg_http_message *
  MG_EV_HTTP_MSG,   // Full HTTP request/response   struct mg_http_message *
  MG_EV_WS_OPEN,    // Websocket handshake done     struct mg_http_message *
  MG_EV_WS_MSG,     // Websocket msg, text or bin   struct mg_ws_message *
  MG_EV_WS_CTL,     // Websocket control msg        struct mg_ws_message *
  MG_EV_MQTT_CMD,   // MQTT low-level command       struct mg_mqtt_message *
  MG_EV_MQTT_MSG,   // MQTT PUBLISH received        struct mg_mqtt_message *
  MG_EV_MQTT_OPEN,  // MQTT CONNACK received        int *connack_status_code
  MG_EV_SNTP_TIME,  // SNTP time received           uint64_t *epoch_millis
  MG_EV_WAKEUP,     // mg_wakeup() data received    struct mg_str *data
  MG_EV_TLS_HSCH,	// TLS handshake clienthello	host or NULL(server_name = null)
  MG_EV_USER        // Starting ID for user events
};










struct mg_dns {
  const char *url;          // DNS server URL
  struct mg_connection *c;  // DNS server connection
};

struct mg_addr {
  uint8_t ip[16];    // Holds IPv4 or IPv6 address, in network byte order
  uint16_t port;     // TCP or UDP port in network byte order
  uint8_t scope_id;  // IPv6 scope ID
  bool is_ip6;       // True when address is IPv6 address
};

struct mg_mgr {
  struct mg_connection *conns;  // List of active connections
  struct mg_dns dns4;           // DNS for IPv4
  struct mg_dns dns6;           // DNS for IPv6
  int dnstimeout;               // DNS resolve timeout in milliseconds
  bool use_dns6;                // Use DNS6 server by default, see #1532
  unsigned long nextid;         // Next connection ID
  void *userdata;               // Arbitrary user data pointer
  void *tls_ctx;                // TLS context shared by all TLS sessions
  uint16_t mqtt_id;             // MQTT IDs for pub/sub
  void *active_dns_requests;    // DNS requests in progress
  struct mg_timer *timers;      // Active timers
  int epoll_fd;                 // Used when MG_EPOLL_ENABLE=1
  struct mg_tcpip_if *ifp;      // Builtin TCP/IP stack only. Interface pointer
  size_t extraconnsize;         // Builtin TCP/IP stack only. Extra space
  MG_SOCKET_TYPE pipe;          // Socketpair end for mg_wakeup()
#if MG_ENABLE_FREERTOS_TCP
  SocketSet_t ss;  // NOTE(lsm): referenced from socket struct
#endif
};

struct mg_connection {
  struct mg_connection *next;     // Linkage in struct mg_mgr :: connections
  struct mg_mgr *mgr;             // Our container
  struct mg_addr loc;             // Local address
  struct mg_addr rem;             // Remote address
  void *fd;                       // Connected socket, or LWIP data
  unsigned long id;               // Auto-incrementing unique connection ID
  struct mg_iobuf recv;           // Incoming data
  struct mg_iobuf send;           // Outgoing data
  struct mg_iobuf prof;           // Profile data enabled by MG_ENABLE_PROFILE
  struct mg_iobuf rtls;           // TLS only. Incoming encrypted data
  mg_event_handler_t fn;          // User-specified event handler function
  void *fn_data;                  // User-specified function parameter
  mg_event_handler_t pfn;         // Protocol-specific handler function
  void *pfn_data;                 // Protocol-specific function parameter
  char data[MG_DATA_SIZE];        // Arbitrary connection data
  void *tls;                      // TLS specific data
  unsigned is_listening : 1;      // Listening connection
  unsigned is_client : 1;         // Outbound (client) connection
  unsigned is_accepted : 1;       // Accepted (server) connection
  unsigned is_resolving : 1;      // Non-blocking DNS resolution is in progress
  unsigned is_arplooking : 1;     // Non-blocking ARP resolution is in progress
  unsigned is_connecting : 1;     // Non-blocking connect is in progress
  unsigned is_tls : 1;            // TLS-enabled connection
  unsigned is_tls_hs : 1;         // TLS handshake is in progress
  unsigned is_udp : 1;            // UDP connection
  unsigned is_websocket : 1;      // WebSocket connection
  unsigned is_mqtt5 : 1;          // For MQTT connection, v5 indicator
  unsigned is_hexdumping : 1;     // Hexdump in/out traffic
  unsigned is_draining : 1;       // Send remaining data, then close and free
  unsigned is_closing : 1;        // Close and free the connection immediately
  unsigned is_full : 1;           // Stop reads, until cleared
  unsigned is_tls_throttled : 1;  // Last TLS write: MG_SOCK_PENDING() was true
  unsigned is_resp : 1;           // Response is still being generated
  unsigned is_readable : 1;       // Connection is ready to read
  unsigned is_writable : 1;       // Connection is ready to write
};

void mg_mgr_poll(struct mg_mgr *, int ms);
void mg_mgr_init(struct mg_mgr *);
void mg_mgr_free(struct mg_mgr *);

struct mg_connection *mg_listen(struct mg_mgr *, const char *url,
                                mg_event_handler_t fn, void *fn_data);
struct mg_connection *mg_connect(struct mg_mgr *, const char *url,
                                 mg_event_handler_t fn, void *fn_data);
struct mg_connection *mg_wrapfd(struct mg_mgr *mgr, int fd,
                                mg_event_handler_t fn, void *fn_data);
void mg_connect_resolved(struct mg_connection *);
bool mg_send(struct mg_connection *, const void *, size_t);
size_t mg_printf(struct mg_connection *, const char *fmt, ...);
size_t mg_vprintf(struct mg_connection *, const char *fmt, va_list *ap);
bool mg_aton(struct mg_str str, struct mg_addr *addr);

// These functions are used to integrate with custom network stacks
struct mg_connection *mg_alloc_conn(struct mg_mgr *);
void mg_close_conn(struct mg_connection *c);
bool mg_open_listener(struct mg_connection *c, const char *url);

// Utility functions
bool mg_wakeup(struct mg_mgr *, unsigned long id, const void *buf, size_t len);
bool mg_wakeup_init(struct mg_mgr *);
struct mg_timer *mg_timer_add(struct mg_mgr *mgr, uint64_t milliseconds,
                              unsigned flags, void (*fn)(void *), void *arg);
struct mg_connection *mg_connect_svc(struct mg_mgr *mgr, const char *url,
                                     mg_event_handler_t fn, void *fn_data,
                                     mg_event_handler_t pfn, void *pfn_data);








struct mg_http_header {
  struct mg_str name;   // Header name
  struct mg_str value;  // Header value
};

struct mg_http_message {
  struct mg_str method, uri, query, proto;             // Request/response line
  struct mg_http_header headers[MG_MAX_HTTP_HEADERS];  // Headers
  struct mg_str body;                                  // Body
  struct mg_str head;                                  // Request + headers
  struct mg_str message;  // Request + headers + body
  int methodCode;
  xdict cookies;
  xvalue session;
};

// Parameter for mg_http_serve_dir()
struct mg_http_serve_opts {
  const char *root_dir;       // Web root directory, must be non-NULL
  const char *ssi_pattern;    // SSI file name pattern, e.g. #.shtml
  const char *extra_headers;  // Extra HTTP headers to add in responses
  const char *mime_types;     // Extra mime types, ext1=type1,ext2=type2,..
  const char *page404;        // Path to the 404 page, or NULL by default
  struct mg_fs *fs;           // Filesystem implementation. Use NULL for POSIX
};

// Parameter for mg_http_next_multipart
struct mg_http_part {
  struct mg_str name;      // Form field name
  struct mg_str filename;  // Filename for file uploads
  struct mg_str body;      // Part contents
};

int mg_http_parse(const char *s, size_t len, struct mg_http_message *);
int mg_http_get_request_len(const unsigned char *buf, size_t buf_len);
void mg_http_printf_chunk(struct mg_connection *cnn, const char *fmt, ...);
void mg_http_write_chunk(struct mg_connection *c, const char *buf, size_t len);
void mg_http_delete_chunk(struct mg_connection *c, struct mg_http_message *hm);
struct mg_connection *mg_http_listen(struct mg_mgr *, const char *url,
                                     mg_event_handler_t fn, void *fn_data);
struct mg_connection *mg_http_connect(struct mg_mgr *, const char *url,
                                      mg_event_handler_t fn, void *fn_data);
void mg_http_serve_dir(struct mg_connection *, struct mg_http_message *hm,
                       const struct mg_http_serve_opts *);
void mg_http_serve_file(struct mg_connection *, struct mg_http_message *hm,
                        const char *path, const struct mg_http_serve_opts *);
void mg_http_reply(struct mg_connection *, int status_code, const char *headers,
                   const char *body_fmt, ...);
struct mg_str *mg_http_get_header(struct mg_http_message *, const char *name);
struct mg_str mg_http_var(struct mg_str buf, struct mg_str name);
int mg_http_get_var(const struct mg_str *, const char *name, char *, size_t);
int mg_url_decode(const char *s, size_t n, char *to, size_t to_len, int form);
size_t mg_url_encode(const char *s, size_t n, char *buf, size_t len);
void mg_http_creds(struct mg_http_message *, char *, size_t, char *, size_t);
long mg_http_upload(struct mg_connection *c, struct mg_http_message *hm,
                    struct mg_fs *fs, const char *dir, size_t max_size);
void mg_http_bauth(struct mg_connection *, const char *user, const char *pass);
struct mg_str mg_http_get_header_var(struct mg_str s, struct mg_str v);
size_t mg_http_next_multipart(struct mg_str, size_t, struct mg_http_part *);
int mg_http_status(const struct mg_http_message *hm);


void mg_http_serve_ssi(struct mg_connection *c, const char *root,
                       const char *fullpath);


#define MG_TLS_NONE 0     // No TLS support
#define MG_TLS_MBED 1     // mbedTLS
#define MG_TLS_OPENSSL 2  // OpenSSL
#define MG_TLS_WOLFSSL 5  // WolfSSL (based on OpenSSL)
#define MG_TLS_BUILTIN 3  // Built-in
#define MG_TLS_CUSTOM 4   // Custom implementation

#ifndef MG_TLS
#define MG_TLS MG_TLS_NONE
#endif





struct mg_tls_opts {
  struct mg_str ca;       // PEM or DER
  struct mg_str cert;     // PEM or DER
  struct mg_str key;      // PEM or DER
  struct mg_str name;     // If not empty, enable host name verification
  int skip_verification;  // Skip certificate and host name verification
};

void mg_tls_init_accept(struct mg_connection *);
void mg_tls_init(struct mg_connection *, const struct mg_tls_opts *opts);
void mg_tls_free(struct mg_connection *);
long mg_tls_send(struct mg_connection *, const void *buf, size_t len);
long mg_tls_recv(struct mg_connection *, void *buf, size_t len);
size_t mg_tls_pending(struct mg_connection *);
void mg_tls_flush(struct mg_connection *);
void mg_tls_handshake(struct mg_connection *);

// Private
void mg_tls_ctx_init(struct mg_mgr *);
void mg_tls_ctx_free(struct mg_mgr *);

// Low-level IO primives used by TLS layer
enum { MG_IO_ERR = -1, MG_IO_WAIT = -2, MG_IO_RESET = -3 };
long mg_io_send(struct mg_connection *c, const void *buf, size_t len);
long mg_io_recv(struct mg_connection *c, void *buf, size_t len);



int mg_gcm_initialize(void);

int mg_aes_gcm_encrypt(unsigned char *output, const unsigned char *input,
                       size_t input_length, const unsigned char *key,
                       const size_t key_len, const unsigned char *iv,
                       const size_t iv_len, unsigned char *aead,
                       size_t aead_len, unsigned char *tag,
                       const size_t tag_len);

int mg_aes_gcm_decrypt(unsigned char *output, const unsigned char *input,
                       size_t input_length, const unsigned char *key,
                       const size_t key_len, const unsigned char *iv,
                       const size_t iv_len);



int mg_rsa_mod_pow(const uint8_t *mod, size_t modsz, const uint8_t *exp, size_t expsz, const uint8_t *msg, size_t msgsz, uint8_t *out, size_t outsz);



#define WEBSOCKET_OP_CONTINUE 0
#define WEBSOCKET_OP_TEXT 1
#define WEBSOCKET_OP_BINARY 2
#define WEBSOCKET_OP_CLOSE 8
#define WEBSOCKET_OP_PING 9
#define WEBSOCKET_OP_PONG 10



struct mg_ws_message {
  struct mg_str data;  // Websocket message data
  uint8_t flags;       // Websocket message flags
};

struct mg_connection *mg_ws_connect(struct mg_mgr *, const char *url,
                                    mg_event_handler_t fn, void *fn_data,
                                    const char *fmt, ...);
void mg_ws_upgrade(struct mg_connection *, struct mg_http_message *,
                   const char *fmt, ...);
size_t mg_ws_send(struct mg_connection *, const void *buf, size_t len, int op);
size_t mg_ws_wrap(struct mg_connection *, size_t len, int op);
size_t mg_ws_printf(struct mg_connection *c, int op, const char *fmt, ...);
size_t mg_ws_vprintf(struct mg_connection *c, int op, const char *fmt,
                     va_list *);




struct mg_connection *mg_sntp_connect(struct mg_mgr *mgr, const char *url,
                                      mg_event_handler_t fn, void *fn_data);
void mg_sntp_request(struct mg_connection *c);
int64_t mg_sntp_parse(const unsigned char *buf, size_t len);

uint64_t mg_now(void);     // Return milliseconds since Epoch





#define MQTT_CMD_CONNECT 1
#define MQTT_CMD_CONNACK 2
#define MQTT_CMD_PUBLISH 3
#define MQTT_CMD_PUBACK 4
#define MQTT_CMD_PUBREC 5
#define MQTT_CMD_PUBREL 6
#define MQTT_CMD_PUBCOMP 7
#define MQTT_CMD_SUBSCRIBE 8
#define MQTT_CMD_SUBACK 9
#define MQTT_CMD_UNSUBSCRIBE 10
#define MQTT_CMD_UNSUBACK 11
#define MQTT_CMD_PINGREQ 12
#define MQTT_CMD_PINGRESP 13
#define MQTT_CMD_DISCONNECT 14
#define MQTT_CMD_AUTH 15

#define MQTT_PROP_PAYLOAD_FORMAT_INDICATOR 0x01
#define MQTT_PROP_MESSAGE_EXPIRY_INTERVAL 0x02
#define MQTT_PROP_CONTENT_TYPE 0x03
#define MQTT_PROP_RESPONSE_TOPIC 0x08
#define MQTT_PROP_CORRELATION_DATA 0x09
#define MQTT_PROP_SUBSCRIPTION_IDENTIFIER 0x0B
#define MQTT_PROP_SESSION_EXPIRY_INTERVAL 0x11
#define MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER 0x12
#define MQTT_PROP_SERVER_KEEP_ALIVE 0x13
#define MQTT_PROP_AUTHENTICATION_METHOD 0x15
#define MQTT_PROP_AUTHENTICATION_DATA 0x16
#define MQTT_PROP_REQUEST_PROBLEM_INFORMATION 0x17
#define MQTT_PROP_WILL_DELAY_INTERVAL 0x18
#define MQTT_PROP_REQUEST_RESPONSE_INFORMATION 0x19
#define MQTT_PROP_RESPONSE_INFORMATION 0x1A
#define MQTT_PROP_SERVER_REFERENCE 0x1C
#define MQTT_PROP_REASON_STRING 0x1F
#define MQTT_PROP_RECEIVE_MAXIMUM 0x21
#define MQTT_PROP_TOPIC_ALIAS_MAXIMUM 0x22
#define MQTT_PROP_TOPIC_ALIAS 0x23
#define MQTT_PROP_MAXIMUM_QOS 0x24
#define MQTT_PROP_RETAIN_AVAILABLE 0x25
#define MQTT_PROP_USER_PROPERTY 0x26
#define MQTT_PROP_MAXIMUM_PACKET_SIZE 0x27
#define MQTT_PROP_WILDCARD_SUBSCRIPTION_AVAILABLE 0x28
#define MQTT_PROP_SUBSCRIPTION_IDENTIFIER_AVAILABLE 0x29
#define MQTT_PROP_SHARED_SUBSCRIPTION_AVAILABLE 0x2A

enum {
  MQTT_PROP_TYPE_BYTE,
  MQTT_PROP_TYPE_STRING,
  MQTT_PROP_TYPE_STRING_PAIR,
  MQTT_PROP_TYPE_BINARY_DATA,
  MQTT_PROP_TYPE_VARIABLE_INT,
  MQTT_PROP_TYPE_INT,
  MQTT_PROP_TYPE_SHORT
};

enum { MQTT_OK, MQTT_INCOMPLETE, MQTT_MALFORMED };

struct mg_mqtt_prop {
  uint8_t id;         // Enumerated at MQTT5 Reference
  uint32_t iv;        // Integer value for 8-, 16-, 32-bit integers types
  struct mg_str key;  // Non-NULL only for user property type
  struct mg_str val;  // Non-NULL only for UTF-8 types and user properties
};

struct mg_mqtt_opts {
  struct mg_str user;               // Username, can be empty
  struct mg_str pass;               // Password, can be empty
  struct mg_str client_id;          // Client ID
  struct mg_str topic;              // message/subscription topic
  struct mg_str message;            // message content
  uint8_t qos;                      // message quality of service
  uint8_t version;                  // Can be 4 (3.1.1), or 5. If 0, assume 4
  uint16_t keepalive;               // Keep-alive timer in seconds
  uint16_t retransmit_id;           // For PUBLISH, init to 0
  bool retain;                      // Retain flag
  bool clean;                       // Clean session flag
  struct mg_mqtt_prop *props;       // MQTT5 props array
  size_t num_props;                 // number of props
  struct mg_mqtt_prop *will_props;  // Valid only for CONNECT packet (MQTT5)
  size_t num_will_props;            // Number of will props
};

struct mg_mqtt_message {
  struct mg_str topic;  // Parsed topic for PUBLISH
  struct mg_str data;   // Parsed message for PUBLISH
  struct mg_str dgram;  // Whole MQTT packet, including headers
  uint16_t id;          // For PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, PUBLISH
  uint8_t cmd;          // MQTT command, one of MQTT_CMD_*
  uint8_t qos;          // Quality of service
  uint8_t ack;          // CONNACK return code, 0 = success
  size_t props_start;   // Offset to the start of the properties (MQTT5)
  size_t props_size;    // Length of the properties
};

struct mg_connection *mg_mqtt_connect(struct mg_mgr *, const char *url,
                                      const struct mg_mqtt_opts *opts,
                                      mg_event_handler_t fn, void *fn_data);
struct mg_connection *mg_mqtt_listen(struct mg_mgr *mgr, const char *url,
                                     mg_event_handler_t fn, void *fn_data);
void mg_mqtt_login(struct mg_connection *c, const struct mg_mqtt_opts *opts);
uint16_t mg_mqtt_pub(struct mg_connection *c, const struct mg_mqtt_opts *opts);
void mg_mqtt_sub(struct mg_connection *, const struct mg_mqtt_opts *opts);
int mg_mqtt_parse(const uint8_t *, size_t, uint8_t, struct mg_mqtt_message *);
void mg_mqtt_send_header(struct mg_connection *, uint8_t cmd, uint8_t flags,
                         uint32_t len);
void mg_mqtt_ping(struct mg_connection *);
void mg_mqtt_pong(struct mg_connection *);
void mg_mqtt_disconnect(struct mg_connection *, const struct mg_mqtt_opts *);
size_t mg_mqtt_next_prop(struct mg_mqtt_message *, struct mg_mqtt_prop *,
                         size_t ofs);





// Mongoose sends DNS queries that contain only one question:
// either A (IPv4) or AAAA (IPv6) address lookup.
// Therefore, we expect zero or one answer.
// If `resolved` is true, then `addr` contains resolved IPv4 or IPV6 address.
struct mg_dns_message {
  uint16_t txnid;       // Transaction ID
  bool resolved;        // Resolve successful, addr is set
  struct mg_addr addr;  // Resolved address
  char name[256];       // Host name
};

struct mg_dns_header {
  uint16_t txnid;  // Transaction ID
  uint16_t flags;
  uint16_t num_questions;
  uint16_t num_answers;
  uint16_t num_authority_prs;
  uint16_t num_other_prs;
};

// DNS resource record
struct mg_dns_rr {
  uint16_t nlen;    // Name or pointer length
  uint16_t atype;   // Address type
  uint16_t aclass;  // Address class
  uint16_t alen;    // Address length
};

void mg_resolve(struct mg_connection *, const char *url);
void mg_resolve_cancel(struct mg_connection *);
bool mg_dns_parse(const uint8_t *buf, size_t len, struct mg_dns_message *);
size_t mg_dns_parse_rr(const uint8_t *buf, size_t len, size_t ofs,
                       bool is_question, struct mg_dns_rr *);
                       
struct mg_connection *mg_mdns_listen(struct mg_mgr *mgr, char *name);





#ifndef MG_JSON_MAX_DEPTH
#define MG_JSON_MAX_DEPTH 30
#endif

// Error return values - negative. Successful returns are >= 0
enum { MG_JSON_TOO_DEEP = -1, MG_JSON_INVALID = -2, MG_JSON_NOT_FOUND = -3 };
int mg_json_get(struct mg_str json, const char *path, int *toklen);

struct mg_str mg_json_get_tok(struct mg_str json, const char *path);
bool mg_json_get_num(struct mg_str json, const char *path, double *v);
bool mg_json_get_bool(struct mg_str json, const char *path, bool *v);
long mg_json_get_long(struct mg_str json, const char *path, long dflt);
char *mg_json_get_str(struct mg_str json, const char *path);
char *mg_json_get_hex(struct mg_str json, const char *path, int *len);
char *mg_json_get_b64(struct mg_str json, const char *path, int *len);

bool mg_json_unescape(struct mg_str str, char *buf, size_t len);
size_t mg_json_next(struct mg_str obj, size_t ofs, struct mg_str *key,
                    struct mg_str *val);




// JSON-RPC request descriptor
struct mg_rpc_req {
  struct mg_rpc **head;  // RPC handlers list head
  struct mg_rpc *rpc;    // RPC handler being called
  mg_pfn_t pfn;          // Response printing function
  void *pfn_data;        // Response printing function data
  void *req_data;        // Arbitrary request data
  struct mg_str frame;   // Request, e.g. {"id":1,"method":"add","params":[1,2]}
};

// JSON-RPC method handler
struct mg_rpc {
  struct mg_rpc *next;              // Next in list
  struct mg_str method;             // Method pattern
  void (*fn)(struct mg_rpc_req *);  // Handler function
  void *fn_data;                    // Handler function argument
};

void mg_rpc_add(struct mg_rpc **head, struct mg_str method_pattern,
                void (*handler)(struct mg_rpc_req *), void *handler_data);
void mg_rpc_del(struct mg_rpc **head, void (*handler)(struct mg_rpc_req *));
void mg_rpc_process(struct mg_rpc_req *);

// Helper functions to print result or error frame
void mg_rpc_ok(struct mg_rpc_req *, const char *fmt, ...);
void mg_rpc_vok(struct mg_rpc_req *, const char *fmt, va_list *ap);
void mg_rpc_err(struct mg_rpc_req *, int code, const char *fmt, ...);
void mg_rpc_verr(struct mg_rpc_req *, int code, const char *fmt, va_list *);
void mg_rpc_list(struct mg_rpc_req *r);
// Copyright (c) 2023 Cesanta Software Limited
// All rights reserved





#define MG_OTA_NONE 0       // No OTA support
#define MG_OTA_STM32H5 1    // STM32 H5
#define MG_OTA_STM32H7 2    // STM32 H7
#define MG_OTA_STM32H7_DUAL_CORE 3 // STM32 H7 dual core
#define MG_OTA_STM32F  4    // STM32 F7/F4/F2
#define MG_OTA_CH32V307 100 // WCH CH32V307
#define MG_OTA_U2A 200      // Renesas U2A16, U2A8, U2A6
#define MG_OTA_RT1020 300   // IMXRT1020
#define MG_OTA_RT1050 301   // IMXRT1050
#define MG_OTA_RT1060 302   // IMXRT1060
#define MG_OTA_RT1064 303   // IMXRT1064
#define MG_OTA_RT1170 304   // IMXRT1170
#define MG_OTA_MCXN 310 	  // MCXN947
#define MG_OTA_FRDM 320    // FRDM-RW612
#define MG_OTA_FLASH 900    // OTA via an internal flash
#define MG_OTA_ESP32 910    // ESP32 OTA implementation
#define MG_OTA_PICOSDK 920  // RP2040/2350 using Pico-SDK hardware_flash
#define MG_OTA_CUSTOM 1000  // Custom implementation

#ifndef MG_OTA
#define MG_OTA MG_OTA_NONE
#else
#ifndef MG_IRAM
#if defined(__GNUC__)
#define MG_IRAM __attribute__((noinline, section(".iram")))
#else
#define MG_IRAM
#endif // compiler
#endif // IRAM
#endif // OTA

// Firmware update API
bool mg_ota_begin(size_t new_firmware_size);     // Start writing
bool mg_ota_write(const void *buf, size_t len);  // Write chunk, aligned to 1k
bool mg_ota_end(void);                           // Stop writing



struct mg_wifi_scan_bss_data {
    struct mg_str SSID;
    char *BSSID;
    int16_t RSSI;
    uint8_t security;
#define MG_WIFI_SECURITY_OPEN 0
#define MG_WIFI_SECURITY_WEP  MG_BIT(0)
#define MG_WIFI_SECURITY_WPA  MG_BIT(1)
#define MG_WIFI_SECURITY_WPA2 MG_BIT(2)
#define MG_WIFI_SECURITY_WPA3 MG_BIT(3)
    uint8_t channel;
    unsigned band :2;
#define MG_WIFI_BAND_2G 0
#define MG_WIFI_BAND_5G 1
    unsigned has_n :1;
};


bool mg_wifi_scan(void);
bool mg_wifi_connect(char *ssid, char *pass);
bool mg_wifi_disconnect(void);
bool mg_wifi_ap_start(char *ssid, char *pass, unsigned int channel);
bool mg_wifi_ap_stop(void);



struct mg_phy {
  uint16_t (*read_reg)(uint8_t addr, uint8_t reg);
  void (*write_reg)(uint8_t addr, uint8_t reg, uint16_t value);
};

// PHY configuration settings, bitmask
enum {
  // Set if PHY LEDs are connected to ground
  MG_PHY_LEDS_ACTIVE_HIGH = (1 << 0),
  // Set when PHY clocks MAC. Otherwise, MAC clocks PHY
  MG_PHY_CLOCKS_MAC = (1 << 1)
};

enum { MG_PHY_SPEED_10M, MG_PHY_SPEED_100M, MG_PHY_SPEED_1000M };

void mg_phy_init(struct mg_phy *, uint8_t addr, uint8_t config);
bool mg_phy_up(struct mg_phy *, uint8_t addr, bool *full_duplex,
               uint8_t *speed);



#ifdef __cplusplus
}
#endif
#endif  // MONGOOSE_H
