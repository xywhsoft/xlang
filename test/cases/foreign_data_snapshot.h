#ifndef FOREIGN_DATA_SNAPSHOT_H
#define FOREIGN_DATA_SNAPSHOT_H
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SnapshotReceiver SnapshotReceiver;
struct SnapshotReceiver { int (*consume)(SnapshotReceiver*,xvalue*); };
typedef struct SnapshotPack { int tag; xvalue* value; } SnapshotPack;
SnapshotReceiver* snapshot_receiver(void);
int snapshot_receiver_store(SnapshotReceiver*,xvalue*);
int snapshot_store(xvalue*);
int snapshot_pair(xvalue*,int);
xvalue* snapshot_owned(xvalue*);
int snapshot_const(const xvalue*);
int snapshot_failing_store(xvalue*);
int snapshot_two(xvalue*,xvalue*);
int snapshot_pack(const SnapshotPack*);
int snapshot_calls(void);
int snapshot_number(void);
int snapshot_same(xvalue*);
xvalue* snapshot_opaque(void);
int snapshot_opaque_live(void);
int snapshot_public(void);
int snapshot_array(void);
int snapshot_sequence(void);
int snapshot_scalar(int);
void snapshot_clear(void);
void snapshot_budget(int);
int snapshot_triggered(void);
void snapshot_mark(void);
int snapshot_balanced(void);
#ifdef __cplusplus
}
#endif
#endif
