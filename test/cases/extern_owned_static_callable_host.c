/* C invokes the static language method through its existing raw callback ABI. */
int static_callable_apply(int (*callback)(int), int value) { return callback(value); }
