#ifndef XMQTT_INTERNAL_H
#define XMQTT_INTERNAL_H

#include "include/xrt_mqtt.h"
#include <string.h>

bool xmqtt_error(xerrkind Kind, const char* Message);
bool xmqtt_text(xstrview Text, bool bEmpty);
bool xmqtt_topic(xstrview Text, bool bFilter);
xbuffer* xmqtt_frame(uint8 iHeader, size_t iRemaining, size_t* pPosition);

/* 写入方先计算完整帧容量，以下操作不会分配或部分失败。 */
static inline void xmqtt_put16(uint8* pData, size_t* pPosition, uint16 iValue)
{
    pData[(*pPosition)++] = (uint8)(iValue >> 8);
    pData[(*pPosition)++] = (uint8)iValue;
}

static inline void xmqtt_put_bytes(uint8* pData, size_t* pPosition, xbytesview Data)
{
    if (Data.Size != 0) memcpy(pData + *pPosition, Data.Data, Data.Size);
    *pPosition += Data.Size;
}

static inline void xmqtt_put_text(uint8* pData, size_t* pPosition, xstrview Text)
{
    xmqtt_put16(pData, pPosition, (uint16)Text.Size);
    xmqtt_put_bytes(pData, pPosition, (xbytesview){(cbytes)Text.Data, Text.Size});
}

#endif
