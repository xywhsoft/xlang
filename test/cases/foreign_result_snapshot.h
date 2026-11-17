#ifndef FOREIGN_RESULT_SNAPSHOT_H
#define FOREIGN_RESULT_SNAPSHOT_H
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ResultReceiver ResultReceiver;
struct ResultReceiver { xvalue* (*read)(ResultReceiver*); };
void result_store(xvalue*);
xvalue* result_get(void);
xvalue* result_move(void);
xvalue* result_echo(xvalue*);
int result_pair(xvalue**,xvalue**);
int result_mixed(xvalue**,const char**,xvalue**,int);
ResultReceiver* result_receiver(void);
xvalue* result_receiver_get(ResultReceiver*);
int result_receiver_output(ResultReceiver*,xvalue**);
int result_calls(void);
int result_same(xvalue*);
xvalue* result_opaque(int);
int result_opaque_live(void);
int result_drops(void);
void result_clear(void);
void result_budget(int);
int result_triggered(void);
void result_mark(void);
int result_balanced(void);
#ifdef __cplusplus
}
#endif
#endif
