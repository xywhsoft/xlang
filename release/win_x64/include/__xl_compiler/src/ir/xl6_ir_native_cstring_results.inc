/* Canonical native C signatures: mutable char* result and const char* inputs.
 * Ownership remains in the language adapter's explicit TAKE_CSTR operation. */
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathName", 1)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathStem", 1)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathExt", 1)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathParent", 1)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathAbs", 1)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathRel", 2)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathCwd", 0)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathHome", 0)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathTemp", 0)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathAppDir", 0)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathWithExt", 2)
XL6_IR_NATIVE_CSTRING_RESULT("xrtPathWithName", 2)
