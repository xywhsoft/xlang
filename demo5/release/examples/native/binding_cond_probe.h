#pragma once
#ifndef BINDING_COND_PROBE_H
#define BINDING_COND_PROBE_H

#define BINDING_COND_FEATURE
#define BINDING_COND_TEMP

#if 0
#error inactive binding error should be ignored
#define BINDING_COND_DISABLED 100
int pkg_clock_artifact_missing_disabled(int value);
#else
#define BINDING_COND_ENABLED 40
#endif

#warning active binding warning should be non-fatal

#ifdef BINDING_COND_FEATURE
#define BINDING_COND_IFDEF 1
#else
#define BINDING_COND_IFDEF 0
#endif

#ifndef BINDING_COND_UNKNOWN
#define BINDING_COND_IFNDEF 1
#endif

#if defined(BINDING_COND_FEATURE)
#define BINDING_COND_DEFINED 1
#endif

#if !defined(BINDING_COND_UNKNOWN)
#define BINDING_COND_NOT_DEFINED 1
#endif

#if BINDING_COND_ENABLED
#define BINDING_COND_EXPR 1
#endif

#if defined(BINDING_COND_FEATURE) && (BINDING_COND_ENABLED >= 40) && !defined(BINDING_COND_UNKNOWN)
#define BINDING_COND_COMPLEX 1
#else
#define BINDING_COND_COMPLEX 0
#endif

#if BINDING_COND_UNKNOWN || (BINDING_COND_ENABLED < 40)
#define BINDING_COND_BAD_BRANCH 1
#endif

#undef BINDING_COND_TEMP

#ifdef BINDING_COND_TEMP
#define BINDING_COND_UNDEF_BAD 1
#else
#define BINDING_COND_UNDEF_OK 1
#endif

int pkg_clock_artifact_add_one(int value);

#endif
