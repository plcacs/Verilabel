static int mb86a20s_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	struct mb86a20s_state *state = fe->demodulator_priv;
	int val;

	*status = 0;

	val = mb86a20s_readreg(state, 0x0a) & 0xf;
	if (val < 0)
		return val;

	if (val >= 2)
		*status |= FE_HAS_SIGNAL;

	if (val >= 4)
		*status |= FE_HAS_CARRIER;

	if (val >= 5)
		*status |= FE_HAS_VITERBI;

	if (val >= 7)
		*status |= FE_HAS_SYNC;

	if (val >= 8)				/* Maybe 9? */
		*status |= FE_HAS_LOCK;

	dev_dbg(&state->i2c->dev, "%s: Status = 0x%02x (state = %d)\n",
		 __func__, *status, val);

	return val;
}