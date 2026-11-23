#ifndef XLANG_ANALYSIS_IR_PROFILE_H
#define XLANG_ANALYSIS_IR_PROFILE_H
/*0.107's exact-coordinate sharing did not pass its latency gate. Preserve
 * the implementation for development;shipping builds use the0.106 path.
 * This changes private layouts:rebuild every consumer when overriding it. */
#ifndef XLS_EXPERIMENTAL_IR_NODE_VIEWS
#define XLS_EXPERIMENTAL_IR_NODE_VIEWS 0
#endif
#if XLS_EXPERIMENTAL_IR_NODE_VIEWS != 0 && XLS_EXPERIMENTAL_IR_NODE_VIEWS != 1
#error XLS_EXPERIMENTAL_IR_NODE_VIEWS must be0or1
#endif
/* Exit-result reuse remains opt-in after small-input performance failures.
 * Keep the shipping producer, memo layout and statistics identical to0.108.
 * Override only in a fresh build; never mix objects from these profiles. */
#ifndef XLS_EXPERIMENTAL_IR_EXIT_REUSE
#define XLS_EXPERIMENTAL_IR_EXIT_REUSE 0
#endif
#if XLS_EXPERIMENTAL_IR_EXIT_REUSE != 0 && XLS_EXPERIMENTAL_IR_EXIT_REUSE != 1
#error XLS_EXPERIMENTAL_IR_EXIT_REUSE must be0or1
#endif
#ifndef XLS_EXPERIMENTAL_IR_FUNCTION_EXITS
#define XLS_EXPERIMENTAL_IR_FUNCTION_EXITS 0
#endif
#if XLS_EXPERIMENTAL_IR_FUNCTION_EXITS != 0 && XLS_EXPERIMENTAL_IR_FUNCTION_EXITS != 1
#error XLS_EXPERIMENTAL_IR_FUNCTION_EXITS must be0or1
#endif
#if XLS_EXPERIMENTAL_IR_FUNCTION_EXITS && XLS_EXPERIMENTAL_IR_EXIT_REUSE
#error Select one experimental exit producer
#endif
#endif
