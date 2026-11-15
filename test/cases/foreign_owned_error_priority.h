#ifndef FOREIGN_OWNED_ERROR_PRIORITY_H
#define FOREIGN_OWNED_ERROR_PRIORITY_H
#include "foreign_output_snapshots.h"
int priorityProduce(int,xvalue**);
int priorityProduceScoped(OutputValueRoot*,xvalue**);
int priorityCode(xerror*);
int priorityReleaseCount(void);
#endif
