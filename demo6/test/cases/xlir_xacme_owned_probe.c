/* Test-only pointer witness. The ordinary C string input must borrow the
 * same PEM allocation through aliases and specialized language calls. */
void* test_xacme_pointer(const char* text) { return (void*)text; }
