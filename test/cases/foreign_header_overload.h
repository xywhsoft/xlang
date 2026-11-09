#ifndef XL6_FOREIGN_HEADER_OVERLOAD_H
#define XL6_FOREIGN_HEADER_OVERLOAD_H
typedef enum ForeignHeaderOrdinal { FOREIGN_HEADER_NEGATIVE=-1,FOREIGN_HEADER_ZERO=0 } ForeignHeaderOrdinal;
#ifdef __cplusplus
extern "C" {
#define XL6_HEADER_NOEXCEPT noexcept
#else
#define XL6_HEADER_NOEXCEPT
#endif
static inline double foreign_header_overload(double value){return value;}
static inline ForeignHeaderOrdinal foreign_header_ordinal(ForeignHeaderOrdinal value){return value;}
static inline int foreign_header_nothrow(int value) XL6_HEADER_NOEXCEPT {return value+1;}
#ifdef __cplusplus
}
extern "C++" {
static inline float foreign_header_overload(float value){return value;}
static inline long double foreign_header_overload(long double value){return value;}
}
#endif
#undef XL6_HEADER_NOEXCEPT
#endif
