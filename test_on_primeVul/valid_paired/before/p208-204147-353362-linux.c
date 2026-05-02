int input_set_keycode(struct input_dev *dev,
		      const struct input_keymap_entry *ke)
{
	unsigned long flags;
	unsigned int old_keycode;
	int retval;

	if (ke->keycode > KEY_MAX)
		return -EINVAL;

	spin_lock_irqsave(&dev->event_lock, flags);

	retval = dev->setkeycode(dev, ke, &old_keycode);
	if (retval)
		goto out;

	/* Make sure KEY_RESERVED did not get enabled. */
	__clear_bit(KEY_RESERVED, dev->keybit);

	/*
	 * Simulate keyup event if keycode is not present
	 * in the keymap anymore
	 */
	if (test_bit(EV_KEY, dev->evbit) &&
	    !is_event_supported(old_keycode, dev->keybit, KEY_MAX) &&
	    __test_and_clear_bit(old_keycode, dev->key)) {
		struct input_value vals[] =  {
			{ EV_KEY, old_keycode, 0 },
			input_value_sync
		};

		input_pass_values(dev, vals, ARRAY_SIZE(vals));
	}

 out:
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return retval;
}