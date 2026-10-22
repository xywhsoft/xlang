/* The shared visitor stays unchanged except this allocation-policy seam.
 * Legacy JSON keeps its 1536-byte per-record allowance. Native typed capture
 * precharges both fixed arenas and reserves 64 bytes per node for the
 * canonical builder's minimum capacity; the original 6*length charges bound
 * its geometric growth. Neither mode changes collected semantic facts. */
static _Thread_local XL6_IndexBuild* xlsTypedIndexBuild;
static int xl6IndexBudget(XL6_IndexBuild*,size_t);
static int xlsIndexRecordBudget(XL6_IndexBuild* build)
{ return xl6IndexBudget(build,build==xlsTypedIndexBuild ? 64 : 1536); }
