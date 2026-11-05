#ifndef TEST_NATIVE_SEQUENCE_CONTRACT_H
#define TEST_NATIVE_SEQUENCE_CONTRACT_H
#include <stddef.h>
#include "src/runtime/xl6_foreign_view_abi.h"
typedef struct SequenceContractItem { int value; double weight; } SequenceContractItem;
typedef struct SequenceContractOwner SequenceContractOwner;
const XL6_ForeignViewDescriptorV1* sequenceContractPolicyGet(void);
#define sequenceContractPolicy (*sequenceContractPolicyGet())
int sequence_contract_hold(SequenceContractOwner*);
int sequence_contract_hold_positive(SequenceContractOwner*);
int sequence_contract_drop(SequenceContractOwner*);
/* Both are trusted immutable arrays lasting as long as one independent owner. */
int sequence_contract_ints(SequenceContractOwner*,const int**,int*);
int sequence_contract_records(SequenceContractOwner*,const SequenceContractItem**,size_t*);
#endif
