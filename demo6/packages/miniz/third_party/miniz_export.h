#pragma once

#if defined(_WIN32) || defined(_WIN64)
#if defined(MINIZ_DLL_EXPORT)
#define MINIZ_EXPORT __declspec(dllexport)
#elif defined(MINIZ_DLL_IMPORT)
#define MINIZ_EXPORT __declspec(dllimport)
#else
#define MINIZ_EXPORT
#endif
#else
#define MINIZ_EXPORT
#endif
