typedef union ProbeOpaqueUnion ProbeOpaqueUnion;
union ProbeForwardUnion;
typedef union BindingPayload {
	int i;
	double d;
} BindingPayload, BindingPayloadAlias, *BindingPayloadPtr;
union BindingNamedPayload {
	long long i;
	void* p;
};

ProbeOpaqueUnion* binding_opaque_union_identity(ProbeOpaqueUnion* handle);
union ProbeForwardUnion* binding_forward_union_identity(union ProbeForwardUnion* handle);
int binding_unnamed_exact_params(unsigned int, signed char, unsigned long long);
int binding_array_param_probe(char text[260], int values[], const unsigned char bytes[16]);
int binding_char_pointer_depth_probe(char *text, char **outText, const char **items);
intptr_t binding_alias_signed_pointer_width(intptr_t value, ptrdiff_t delta, SSIZE_T size);
uintptr_t binding_alias_unsigned_pointer_width(uintptr_t value, UINT_PTR mask, DWORD_PTR cookie);
int binding_qualifier_param_probe(char const *text, unsigned int const value, volatile DWORD flags);
typedef unsigned int const BindingConstUint;
BindingConstUint binding_qualifier_typedef_probe(BindingConstUint value);
typedef struct BindingPtrTagged {
	int value;
} BindingPtrAlias, *PBindingPtrAlias;
PBindingPtrAlias binding_struct_ptr_alias_identity(PBindingPtrAlias handle);
BindingPtrAlias binding_struct_ptr_alias_make(void);
