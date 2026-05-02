static irqreturn_t sunkbd_interrupt(struct serio *serio,
		unsigned char data, unsigned int flags)
{
	struct sunkbd *sunkbd = serio_get_drvdata(serio);

	if (sunkbd->reset <= -1) {
		/*
		 * If cp[i] is 0xff, sunkbd->reset will stay -1.
		 * The keyboard sends 0xff 0xff 0xID on powerup.
		 */
		sunkbd->reset = data;
		wake_up_interruptible(&sunkbd->wait);
		goto out;
	}

	if (sunkbd->layout == -1) {
		sunkbd->layout = data;
		wake_up_interruptible(&sunkbd->wait);
		goto out;
	}

	switch (data) {

	case SUNKBD_RET_RESET:
		schedule_work(&sunkbd->tq);
		sunkbd->reset = -1;
		break;

	case SUNKBD_RET_LAYOUT:
		sunkbd->layout = -1;
		break;

	case SUNKBD_RET_ALLUP: /* All keys released */
		break;

	default:
		if (!sunkbd->enabled)
			break;

		if (sunkbd->keycode[data & SUNKBD_KEY]) {
			input_report_key(sunkbd->dev,
					 sunkbd->keycode[data & SUNKBD_KEY],
					 !(data & SUNKBD_RELEASE));
			input_sync(sunkbd->dev);
		} else {
			printk(KERN_WARNING
				"sunkbd.c: Unknown key (scancode %#x) %s.\n",
				data & SUNKBD_KEY,
				data & SUNKBD_RELEASE ? "released" : "pressed");
		}
	}
out:
	return IRQ_HANDLED;
}