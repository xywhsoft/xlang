/* Public name, source/runtime operation, wait mode, timeout operand.
 * The TRY operation shares the owned concurrent queue result vocabulary. */
XL6_WAIT_QUEUE_RECEIVE_OP("tryPop", CONCURRENT_QUEUE_TRY_POP, 0, false)
XL6_WAIT_QUEUE_RECEIVE_OP("pop", WAIT_QUEUE_POP, 1, false)
XL6_WAIT_QUEUE_RECEIVE_OP("popTimeout", WAIT_QUEUE_POP_TIMEOUT, 2, true)
