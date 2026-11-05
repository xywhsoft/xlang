__attribute__((selectany)) int coff_shared_value = 17;
int coff_duplicate(void) { return coff_shared_value + 1; }
