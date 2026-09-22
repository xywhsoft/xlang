#ifndef XRT_INTERNAL_SMTP_CLIENT_H
#define XRT_INTERNAL_SMTP_CLIENT_H

#include <xrt/smtp_client.h>



#if defined(XMAIL_FEATURE_SMTP_CLIENT)

/* 认证层成功完成 AUTH 后只通过该内部边界更新会话状态。 */
void __xrtSmtpClientAuthComplete(xsmtpclient* pClient);

#endif

#endif
