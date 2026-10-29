/* id, source method, search argument, key/value pair, required, native query.
 * Source results: required GET/TAKE -> V; required pairs -> (K,V);
 * optional GET/TAKE -> (bool,V?); optional pairs -> (bool,K?,V?).
 * All return independently owned slots and have STATUS_OUTPUT failure ABI;
 * presence is language data, never the operation's success status. */
XL6_TREE_QUERY_OP(GET,         "get",           1, 0, 1, GET)
XL6_TREE_QUERY_OP(TAKE,        "take",          1, 0, 1, TAKE)
XL6_TREE_QUERY_OP(FIRST,       "first",         0, 1, 1, FIRST)
XL6_TREE_QUERY_OP(LAST,        "last",          0, 1, 1, LAST)
XL6_TREE_QUERY_OP(LOWER_BOUND, "lowerBound",    1, 1, 1, LOWER)
XL6_TREE_QUERY_OP(UPPER_BOUND, "upperBound",    1, 1, 1, UPPER)
XL6_TREE_QUERY_OP(TRY_GET,     "tryGet",        1, 0, 0, GET)
XL6_TREE_QUERY_OP(TRY_TAKE,    "tryTake",       1, 0, 0, TAKE)
XL6_TREE_QUERY_OP(TRY_FIRST,   "tryFirst",      0, 1, 0, FIRST)
XL6_TREE_QUERY_OP(TRY_LAST,    "tryLast",       0, 1, 0, LAST)
XL6_TREE_QUERY_OP(TRY_LOWER,   "tryLowerBound", 1, 1, 0, LOWER)
XL6_TREE_QUERY_OP(TRY_UPPER,   "tryUpperBound", 1, 1, 0, UPPER)
