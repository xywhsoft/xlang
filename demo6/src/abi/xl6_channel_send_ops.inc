/* Enum suffix, public name, native wait mode, timeout operand, diagnostic name.
 * Existing enum identities stay fixed; new operations append to their enums. */
XL6_CHANNEL_SEND_OP(SEND, "send", 1, false, "send")
XL6_CHANNEL_SEND_OP(TRY_SEND, "trySend", 0, false, "try-send")
XL6_CHANNEL_SEND_OP(SEND_TIMEOUT, "sendTimeout", 2, true, "send-timeout")
