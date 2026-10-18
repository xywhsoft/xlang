/* Id, public method, native wait mode, time operand, required Token.
 * Output-variable Into names deliberately have no entry. */
XL6_CHANNEL_RECEIVE_OP(WAIT, "recv", 1, false, false)
XL6_CHANNEL_RECEIVE_OP(TRY, "tryRecv", 0, false, false)
XL6_CHANNEL_RECEIVE_OP(FOR, "recvTimeout", 2, true, false)
XL6_CHANNEL_RECEIVE_OP(UNTIL, "recvUntil", 3, true, false)
XL6_CHANNEL_RECEIVE_OP(CANCEL, "recvCancel", 1, false, true)
XL6_CHANNEL_RECEIVE_OP(FOR_CANCEL, "recvTimeoutCancel", 2, true, true)
XL6_CHANNEL_RECEIVE_OP(UNTIL_CANCEL, "recvUntilCancel", 3, true, true)
