#ifndef FOREIGN_POD_FIXED_ARRAY_H
#define FOREIGN_POD_FIXED_ARRAY_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PodArrayItem {char name[8];int values[2];} PodArrayItem;
int podArraySum(const PodArrayItem*,int);
#ifdef __cplusplus
}
#endif
#endif
