#ifndef MQTT_TEST_FIXTURE_H
#define MQTT_TEST_FIXTURE_H
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#include "../include/xrt_mqtt_net.h"

typedef struct mqtt_test_pair {
    xnetengine* Engine;
    xnetlistener* Listener;
    xnetstream* Client;
    xnetstream* Peer;
} mqtt_test_pair;

void mqtt_test_open(mqtt_test_pair* pair);
void mqtt_test_close(mqtt_test_pair* pair);
void mqtt_test_send(xnetstream* stream, xbytesview data);

/* 语言测试使用本地真实 socket；该夹具不属于公开扩展 API。 */
void* mqtt_fixture_open(void);
void* mqtt_fixture_client(void* fixture);
bool mqtt_fixture_close(void* fixture);
#endif
