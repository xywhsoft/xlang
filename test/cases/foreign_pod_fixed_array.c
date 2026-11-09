#include "foreign_pod_fixed_array.h"
int podArraySum(const PodArrayItem* items,int count)
{
    int sum=0;
    for(int i=0;i<count;++i)sum+=items[i].name[0]+items[i].values[0]+items[i].values[1];
    return sum;
}
