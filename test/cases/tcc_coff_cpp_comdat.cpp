/* Genuine GCC C++ linkonce text plus GNU COFF xdata/pdata section symbols. */
struct CoffInline {
    int value;
    __attribute__((noinline,noclone,used)) int get()const{return value+17;}
};
#if defined(COFF_CPP_SECOND)
extern "C" int coff_cpp_second(int value){CoffInline object{value};return object.get();}
#else
extern "C" int coff_cpp_first(int value){CoffInline object{value};return object.get();}
#endif
