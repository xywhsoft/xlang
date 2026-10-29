#include "xl_clock.h"
#ifndef XRT_MQTT_NET_H
#define XRT_MQTT_NET_H

#ifndef XRT_MODULE_NET_TCP_SYNC
#define XRT_MODULE_NET_TCP_SYNC
#endif
#include "xrt_mqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 只借用 Stream / Cancel。调用方串行化同一连接的收包、发包和握手。
 * Deadline 使用 XRT 单调毫秒时钟；发送和接收不重置总预算。
 * 传输层验证固定头和帧边界；完整字段语义由 xrtMqttParse 验证。
 */
bool xrtMqttSendFrame(xnetstream* pStream, xbytesview Frame,
    XL6_DeadlineV1 Deadline, xcancel* pCancel);
xbuffer* xrtMqttRecvFrame(xnetstream* pStream, size_t iPacketLimit,
    XL6_DeadlineV1 Deadline, xcancel* pCancel);

/* Clean Session 握手，成功返回拥有型 CONNACK 对象；拒绝或错误将终止连接。 */
xvalue* xrtMqttHandshake(xnetstream* pStream, xstrview ClientId, xstrview Username,
    xbytesview Password, uint16 iKeepalive, XL6_DeadlineV1 Deadline, xcancel* pCancel);

#ifdef __cplusplus
}
#endif
#endif
