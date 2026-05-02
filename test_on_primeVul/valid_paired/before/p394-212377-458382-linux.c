static int mt_touch_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max, struct mt_application *app)
{
	struct mt_device *td = hid_get_drvdata(hdev);
	struct mt_class *cls = &td->mtclass;
	int code;
	struct hid_usage *prev_usage = NULL;

	/*
	 * Model touchscreens providing buttons as touchpads.
	 */
	if (field->application == HID_DG_TOUCHSCREEN &&
	    (usage->hid & HID_USAGE_PAGE) == HID_UP_BUTTON) {
		app->mt_flags |= INPUT_MT_POINTER;
		td->inputmode_value = MT_INPUTMODE_TOUCHPAD;
	}

	/* count the buttons on touchpads */
	if ((usage->hid & HID_USAGE_PAGE) == HID_UP_BUTTON)
		app->buttons_count++;

	if (usage->usage_index)
		prev_usage = &field->usage[usage->usage_index - 1];

	switch (usage->hid & HID_USAGE_PAGE) {

	case HID_UP_GENDESK:
		switch (usage->hid) {
		case HID_GD_X:
			if (prev_usage && (prev_usage->hid == usage->hid)) {
				code = ABS_MT_TOOL_X;
				MT_STORE_FIELD(cx);
			} else {
				code = ABS_MT_POSITION_X;
				MT_STORE_FIELD(x);
			}

			set_abs(hi->input, code, field, cls->sn_move);

			/*
			 * A system multi-axis that exports X and Y has a high
			 * chance of being used directly on a surface
			 */
			if (field->application == HID_GD_SYSTEM_MULTIAXIS) {
				__set_bit(INPUT_PROP_DIRECT,
					  hi->input->propbit);
				input_set_abs_params(hi->input,
						     ABS_MT_TOOL_TYPE,
						     MT_TOOL_DIAL,
						     MT_TOOL_DIAL, 0, 0);
			}

			return 1;
		case HID_GD_Y:
			if (prev_usage && (prev_usage->hid == usage->hid)) {
				code = ABS_MT_TOOL_Y;
				MT_STORE_FIELD(cy);
			} else {
				code = ABS_MT_POSITION_Y;
				MT_STORE_FIELD(y);
			}

			set_abs(hi->input, code, field, cls->sn_move);

			return 1;
		}
		return 0;

	case HID_UP_DIGITIZER:
		switch (usage->hid) {
		case HID_DG_INRANGE:
			if (app->quirks & MT_QUIRK_HOVERING) {
				input_set_abs_params(hi->input,
					ABS_MT_DISTANCE, 0, 1, 0, 0);
			}
			MT_STORE_FIELD(inrange_state);
			return 1;
		case HID_DG_CONFIDENCE:
			if (cls->name == MT_CLS_WIN_8 &&
				(field->application == HID_DG_TOUCHPAD ||
				 field->application == HID_DG_TOUCHSCREEN))
				app->quirks |= MT_QUIRK_CONFIDENCE;

			if (app->quirks & MT_QUIRK_CONFIDENCE)
				input_set_abs_params(hi->input,
						     ABS_MT_TOOL_TYPE,
						     MT_TOOL_FINGER,
						     MT_TOOL_PALM, 0, 0);

			MT_STORE_FIELD(confidence_state);
			return 1;
		case HID_DG_TIPSWITCH:
			if (field->application != HID_GD_SYSTEM_MULTIAXIS)
				input_set_capability(hi->input,
						     EV_KEY, BTN_TOUCH);
			MT_STORE_FIELD(tip_state);
			return 1;
		case HID_DG_CONTACTID:
			MT_STORE_FIELD(contactid);
			app->touches_by_report++;
			return 1;
		case HID_DG_WIDTH:
			if (!(app->quirks & MT_QUIRK_NO_AREA))
				set_abs(hi->input, ABS_MT_TOUCH_MAJOR, field,
					cls->sn_width);
			MT_STORE_FIELD(w);
			return 1;
		case HID_DG_HEIGHT:
			if (!(app->quirks & MT_QUIRK_NO_AREA)) {
				set_abs(hi->input, ABS_MT_TOUCH_MINOR, field,
					cls->sn_height);

				/*
				 * Only set ABS_MT_ORIENTATION if it is not
				 * already set by the HID_DG_AZIMUTH usage.
				 */
				if (!test_bit(ABS_MT_ORIENTATION,
						hi->input->absbit))
					input_set_abs_params(hi->input,
						ABS_MT_ORIENTATION, 0, 1, 0, 0);
			}
			MT_STORE_FIELD(h);
			return 1;
		case HID_DG_TIPPRESSURE:
			set_abs(hi->input, ABS_MT_PRESSURE, field,
				cls->sn_pressure);
			MT_STORE_FIELD(p);
			return 1;
		case HID_DG_SCANTIME:
			input_set_capability(hi->input, EV_MSC, MSC_TIMESTAMP);
			app->scantime = &field->value[usage->usage_index];
			app->scantime_logical_max = field->logical_maximum;
			return 1;
		case HID_DG_CONTACTCOUNT:
			app->have_contact_count = true;
			app->raw_cc = &field->value[usage->usage_index];
			return 1;
		case HID_DG_AZIMUTH:
			/*
			 * Azimuth has the range of [0, MAX) representing a full
			 * revolution. Set ABS_MT_ORIENTATION to a quarter of
			 * MAX according the definition of ABS_MT_ORIENTATION
			 */
			input_set_abs_params(hi->input, ABS_MT_ORIENTATION,
				-field->logical_maximum / 4,
				field->logical_maximum / 4,
				cls->sn_move ?
				field->logical_maximum / cls->sn_move : 0, 0);
			MT_STORE_FIELD(a);
			return 1;
		case HID_DG_CONTACTMAX:
			/* contact max are global to the report */
			return -1;
		case HID_DG_TOUCH:
			/* Legacy devices use TIPSWITCH and not TOUCH.
			 * Let's just ignore this field. */
			return -1;
		}
		/* let hid-input decide for the others */
		return 0;

	case HID_UP_BUTTON:
		code = BTN_MOUSE + ((usage->hid - 1) & HID_USAGE);
		/*
		 * MS PTP spec says that external buttons left and right have
		 * usages 2 and 3.
		 */
		if ((app->quirks & MT_QUIRK_WIN8_PTP_BUTTONS) &&
		    field->application == HID_DG_TOUCHPAD &&
		    (usage->hid & HID_USAGE) > 1)
			code--;

		if (field->application == HID_GD_SYSTEM_MULTIAXIS)
			code = BTN_0  + ((usage->hid - 1) & HID_USAGE);

		hid_map_usage(hi, usage, bit, max, EV_KEY, code);
		input_set_capability(hi->input, EV_KEY, code);
		return 1;

	case 0xff000000:
		/* we do not want to map these: no input-oriented meaning */
		return -1;
	}

	return 0;
}