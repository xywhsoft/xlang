#ifndef XL6_TEST_PARTICLE_DEFINITION_H
#define XL6_TEST_PARTICLE_DEFINITION_H
#include <xge3_runtime.h>
void fixtureDefinitionNativeValidation(void);
void fixtureDefinitionBudget(int);
void fixtureDefinitionPrefixBegin(int);
void fixtureDefinitionPrefixEnd(void);
void fixtureDefinitionHeapEqual(void);
void fixtureDefinitionHotBegin(void);
void fixtureDefinitionHotEnd(int);
double fixtureDefinitionNativeHot(xge_particle_definition);
void fixtureDefinitionKeep(xge_particle_definition);
void fixtureDefinitionCheckShare(xge_particle_definition, xge_particle_definition);
int fixtureDefinitionReferences(xge_particle_definition);
void fixtureDefinitionNativeShares(xge_particle_definition);
void fixtureDefinitionCheckKept(void);
void fixtureDefinitionReleaseKept(void);
int fixtureDefinitionSerializationOperations(xge_particle_definition,int);
void fixtureDefinitionColdEnd(int);
void fixtureDefinitionNativeCreate(void);
void fixtureDefinitionNativeSerialization(xge_particle_definition,int);
#endif
