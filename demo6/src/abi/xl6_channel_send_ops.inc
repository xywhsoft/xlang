/* Enum suffix, public name, wait mode, time operand, required Token, label.
 * Existing enum identities stay fixed; new operations append to their enums. */
XL6_CHANNEL_SEND_OP(SEND, "send", 1, false, false, "send")
XL6_CHANNEL_SEND_OP(TRY_SEND, "trySend", 0, false, false, "try-send")
XL6_CHANNEL_SEND_OP(SEND_TIMEOUT, "sendTimeout", 2, true, false, "send-timeout")
XL6_CHANNEL_SEND_OP(SEND_UNTIL, "sendUntil", 3, true, false, "send-until")
XL6_CHANNEL_SEND_OP(SEND_CANCEL, "sendCancel", 1, false, true, "send-cancel")
XL6_CHANNEL_SEND_OP(SEND_TIMEOUT_CANCEL, "sendTimeoutCancel", 2, true, true, "send-timeout-cancel")
XL6_CHANNEL_SEND_OP(SEND_UNTIL_CANCEL, "sendUntilCancel", 3, true, true, "send-until-cancel")
