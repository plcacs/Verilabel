static int uvc_scan_chain_forward(struct uvc_video_chain *chain,
	struct uvc_entity *entity, struct uvc_entity *prev)
{
	struct uvc_entity *forward;
	int found;

	/* Forward scan */
	forward = NULL;
	found = 0;

	while (1) {
		forward = uvc_entity_by_reference(chain->dev, entity->id,
			forward);
		if (forward == NULL)
			break;
		if (forward == prev)
			continue;

		switch (UVC_ENTITY_TYPE(forward)) {
		case UVC_VC_EXTENSION_UNIT:
			if (forward->bNrInPins != 1) {
				uvc_trace(UVC_TRACE_DESCR, "Extension unit %d "
					  "has more than 1 input pin.\n",
					  entity->id);
				return -EINVAL;
			}

			list_add_tail(&forward->chain, &chain->entities);
			if (uvc_trace_param & UVC_TRACE_PROBE) {
				if (!found)
					printk(KERN_CONT " (->");

				printk(KERN_CONT " XU %d", forward->id);
				found = 1;
			}
			break;

		case UVC_OTT_VENDOR_SPECIFIC:
		case UVC_OTT_DISPLAY:
		case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
		case UVC_TT_STREAMING:
			if (UVC_ENTITY_IS_ITERM(forward)) {
				uvc_trace(UVC_TRACE_DESCR, "Unsupported input "
					"terminal %u.\n", forward->id);
				return -EINVAL;
			}

			list_add_tail(&forward->chain, &chain->entities);
			if (uvc_trace_param & UVC_TRACE_PROBE) {
				if (!found)
					printk(KERN_CONT " (->");

				printk(KERN_CONT " OT %d", forward->id);
				found = 1;
			}
			break;
		}
	}
	if (found)
		printk(KERN_CONT ")");

	return 0;
}