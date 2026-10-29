#ifndef XRT_MQTT_H
#define XRT_MQTT_H

#ifndef XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_BUFFER_HEX
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"

#define XMQTT_MAX_REMAINING 268435455u
#define XMQTT_DEFAULT_PACKET_LIMIT 1048576u

typedef enum xmqttstatus {
    XMQTT_ERROR = -1,
    XMQTT_MORE = 0,
    XMQTT_READY = 1
} xmqttstatus;

typedef struct xmqttheader {
    uint8 Type;
    uint8 Flags;
    size_t HeaderSize;
    size_t Remaining;
    size_t PacketSize;
} xmqttheader;

/* 只借用输入帧；输入销毁或修改后，Topic / Payload 视图不再有效。 */
typedef struct xmqttmessage {
    xstrview Topic;
    xbytesview Payload;
    uint16 PacketId;
    uint8 QoS;
    bool Retain;
    bool Duplicate;
} xmqttmessage;

#ifdef __cplusplus
extern "C" {
#endif

/* 增量检查固定头；MORE 不设置错误。完整固定头会填充 PacketSize，即使正文尚未收齐。 */
xmqttstatus xrtMqttInspect(xbytesview Data, xmqttheader* pHeader);

/* 构造结果由调用方 xrtBufferDestroy；NULL 表示失败。空 username/password 表示不携带该字段。 */
xbuffer* xrtMqttConnect(xstrview ClientId, xstrview Username,
    xbytesview Password, uint16 iKeepalive, bool bCleanSession);
xbuffer* xrtMqttPublish(uint8 iQoS, xstrview Topic, xbytesview Payload,
    uint16 iPacketId, bool bRetain, bool bDuplicate);
xbuffer* xrtMqttPuback(uint16 iPacketId);
xbuffer* xrtMqttSubscribe(uint16 iPacketId, xstrview Filter, uint8 iQoS);
xbuffer* xrtMqttPingreq(void);
xbuffer* xrtMqttDisconnect(void);

/* 单帧必须恰好消费输入；对象复制字符串/二进制内容，调用方用 xrtValueRelease 释放。 */
xvalue* xrtMqttParse(xbytesview Frame);
bool xrtMqttPublishView(xbytesview Frame, xmqttmessage* pMessage);

#ifdef __cplusplus
}
#endif
#endif
