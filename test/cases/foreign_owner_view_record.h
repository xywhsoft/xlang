#ifndef XL6_TEST_FOREIGN_OWNER_VIEW_RECORD_H
#define XL6_TEST_FOREIGN_OWNER_VIEW_RECORD_H
#include "foreign_managed_view.h"

typedef struct WatchedRecord {
    ManagedChild* first;
    ManagedChild* second;
    const char* text;
    int stamp;
} WatchedRecord;

int watched_record_seed(ManagedRoot* root);
int watched_record_query(ManagedRoot* root, int mode, WatchedRecord* result);
int watched_record_watches(ManagedRoot* root);
int watched_record_queries(void);
void watched_record_set_budget(int count);
void watched_record_reset(void);
void watched_record_mark(void);
void watched_record_balanced(void);
#endif
