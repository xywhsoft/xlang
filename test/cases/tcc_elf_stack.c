/* Native ELF producer control: no language runtime or provider substitutions. */
int elfStackProbe(void) { return 42; }
int main(void) { return elfStackProbe() == 42 ? 0 : 1; }
