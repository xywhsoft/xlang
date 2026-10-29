#ifndef XLANG_MQTT_NATIVE_H
#define XLANG_MQTT_NATIVE_H
#include "xrt_mqtt_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/* point 使用精确 void* 原型；返回缓冲/字符串均转移所有权，不借用静态存储。 */
void* xmq_connect_frame(const char* client, const char* username, void* password,
    size_t size, int64_t keepalive, bool clean);
void* xmq_connect_text(const char* client, const char* username, const char* password,
    int64_t keepalive, bool clean);
void* xmq_publish_frame(int64_t qos, const char* topic, void* payload,
    size_t size, int64_t packetId, bool retain, bool duplicate);
void* xmq_publish_text(int64_t qos, const char* topic, const char* payload,
    int64_t packetId, bool retain, bool duplicate);
void* xmq_puback_frame(int64_t packetId);
void* xmq_subscribe_frame(int64_t packetId, const char* filter, int64_t qos);
void* xmq_pingreq_frame(void);
void* xmq_disconnect_frame(void);
void* xmq_hex_to_frame(const char* hex);
void* xmq_frame_to_hex(void* data, size_t size);
/* var 返回拥有型 xvalue；失败转成语言 null 并保留错误，null 自身分配失败仍返回 NULL。 */
xvalue* xmq_parse_frame(void* data, size_t size);
void* xmq_payload_bytes(void* data, size_t size);
int64_t xmq_frame_size(void* data, size_t size);

bool xmq_send_frame(void* stream, void* data, size_t size, int64_t timeout, void* cancel);
void* xmq_recv_frame(void* stream, int64_t limit, int64_t timeout, void* cancel);
xvalue* xmq_handshake(void* stream, const char* client, const char* username,
    const char* password, int64_t keepalive, int64_t timeout, void* cancel);

#ifdef __cplusplus
}
#endif
#endif
