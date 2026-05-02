void hid_reset(HIDState *hs)
{
    switch (hs->kind) {
    case HID_KEYBOARD:
        memset(hs->kbd.keycodes, 0, sizeof(hs->kbd.keycodes));
        memset(hs->kbd.key, 0, sizeof(hs->kbd.key));
        hs->kbd.keys = 0;
        break;
    case HID_MOUSE:
    case HID_TABLET:
        memset(hs->ptr.queue, 0, sizeof(hs->ptr.queue));
        break;
    }
    hs->head = 0;
    hs->n = 0;
    hs->protocol = 1;
    hs->idle = 0;
    hs->idle_pending = false;
    hid_del_idle_timer(hs);
}