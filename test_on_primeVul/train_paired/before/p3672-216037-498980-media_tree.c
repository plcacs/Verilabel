static int technisat_usb2_get_ir(struct dvb_usb_device *d)
{
	struct technisat_usb2_state *state = d->priv;
	u8 *buf = state->buf;
	u8 *b;
	int ret;
	struct ir_raw_event ev;

	buf[0] = GET_IR_DATA_VENDOR_REQUEST;
	buf[1] = 0x08;
	buf[2] = 0x8f;
	buf[3] = MINIMUM_IR_BIT_TRANSITION_TICK_COUNT;
	buf[4] = MAXIMUM_IR_BIT_TIME_TICK_COUNT;

	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;
	ret = usb_control_msg(d->udev, usb_sndctrlpipe(d->udev, 0),
			GET_IR_DATA_VENDOR_REQUEST,
			USB_TYPE_VENDOR | USB_DIR_OUT,
			0, 0,
			buf, 5, 500);
	if (ret < 0)
		goto unlock;

	buf[1] = 0;
	buf[2] = 0;
	ret = usb_control_msg(d->udev, usb_rcvctrlpipe(d->udev, 0),
			GET_IR_DATA_VENDOR_REQUEST,
			USB_TYPE_VENDOR | USB_DIR_IN,
			0x8080, 0,
			buf, 62, 500);

unlock:
	mutex_unlock(&d->i2c_mutex);

	if (ret < 0)
		return ret;

	if (ret == 1)
		return 0; /* no key pressed */

	/* decoding */
	b = buf+1;

#if 0
	deb_rc("RC: %d ", ret);
	debug_dump(b, ret, deb_rc);
#endif

	ev.pulse = 0;
	while (1) {
		ev.pulse = !ev.pulse;
		ev.duration = (*b * FIRMWARE_CLOCK_DIVISOR * FIRMWARE_CLOCK_TICK) / 1000;
		ir_raw_event_store(d->rc_dev, &ev);

		b++;
		if (*b == 0xff) {
			ev.pulse = 0;
			ev.duration = 888888*2;
			ir_raw_event_store(d->rc_dev, &ev);
			break;
		}
	}

	ir_raw_event_handle(d->rc_dev);

	return 1;
}