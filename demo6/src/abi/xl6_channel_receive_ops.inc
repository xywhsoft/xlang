/* Id, public method, native wait mode, timeout operand.
 * Output-variable Into names deliberately have no entry. */
XL6_CHANNEL_RECEIVE_OP(WAIT, "recv", 1, false)
XL6_CHANNEL_RECEIVE_OP(TRY, "tryRecv", 0, false)
XL6_CHANNEL_RECEIVE_OP(FOR, "recvTimeout", 2, true)
