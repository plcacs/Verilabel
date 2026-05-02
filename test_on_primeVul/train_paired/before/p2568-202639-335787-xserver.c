ProcXChangeFeedbackControl(ClientPtr client)
{
    unsigned len;
    DeviceIntPtr dev;
    KbdFeedbackPtr k;
    PtrFeedbackPtr p;
    IntegerFeedbackPtr i;
    StringFeedbackPtr s;
    BellFeedbackPtr b;
    LedFeedbackPtr l;
    int rc;

    REQUEST(xChangeFeedbackControlReq);
    REQUEST_AT_LEAST_SIZE(xChangeFeedbackControlReq);

    len = stuff->length - bytes_to_int32(sizeof(xChangeFeedbackControlReq));
    rc = dixLookupDevice(&dev, stuff->deviceid, client, DixManageAccess);
    if (rc != Success)
        return rc;

    switch (stuff->feedbackid) {
    case KbdFeedbackClass:
        if (len != bytes_to_int32(sizeof(xKbdFeedbackCtl)))
            return BadLength;

        for (k = dev->kbdfeed; k; k = k->next)
            if (k->ctrl.id == ((xKbdFeedbackCtl *) &stuff[1])->id)
                return ChangeKbdFeedback(client, dev, stuff->mask, k,
                                         (xKbdFeedbackCtl *) &stuff[1]);
        break;
    case PtrFeedbackClass:
        if (len != bytes_to_int32(sizeof(xPtrFeedbackCtl)))
            return BadLength;

        for (p = dev->ptrfeed; p; p = p->next)
            if (p->ctrl.id == ((xPtrFeedbackCtl *) &stuff[1])->id)
                return ChangePtrFeedback(client, dev, stuff->mask, p,
                                         (xPtrFeedbackCtl *) &stuff[1]);
        break;
    case StringFeedbackClass:
    {
        xStringFeedbackCtl *f = ((xStringFeedbackCtl *) &stuff[1]);

        if (client->swapped) {
            if (len < bytes_to_int32(sizeof(xStringFeedbackCtl)))
                return BadLength;
            swaps(&f->num_keysyms);
        }
        if (len !=
            (bytes_to_int32(sizeof(xStringFeedbackCtl)) + f->num_keysyms))
            return BadLength;

        for (s = dev->stringfeed; s; s = s->next)
            if (s->ctrl.id == ((xStringFeedbackCtl *) &stuff[1])->id)
                return ChangeStringFeedback(client, dev, stuff->mask, s,
                                            (xStringFeedbackCtl *) &stuff[1]);
        break;
    }
    case IntegerFeedbackClass:
        if (len != bytes_to_int32(sizeof(xIntegerFeedbackCtl)))
            return BadLength;

        for (i = dev->intfeed; i; i = i->next)
            if (i->ctrl.id == ((xIntegerFeedbackCtl *) &stuff[1])->id)
                return ChangeIntegerFeedback(client, dev, stuff->mask, i,
                                             (xIntegerFeedbackCtl *) &
                                             stuff[1]);
        break;
    case LedFeedbackClass:
        if (len != bytes_to_int32(sizeof(xLedFeedbackCtl)))
            return BadLength;

        for (l = dev->leds; l; l = l->next)
            if (l->ctrl.id == ((xLedFeedbackCtl *) &stuff[1])->id)
                return ChangeLedFeedback(client, dev, stuff->mask, l,
                                         (xLedFeedbackCtl *) &stuff[1]);
        break;
    case BellFeedbackClass:
        if (len != bytes_to_int32(sizeof(xBellFeedbackCtl)))
            return BadLength;

        for (b = dev->bell; b; b = b->next)
            if (b->ctrl.id == ((xBellFeedbackCtl *) &stuff[1])->id)
                return ChangeBellFeedback(client, dev, stuff->mask, b,
                                          (xBellFeedbackCtl *) &stuff[1]);
        break;
    default:
        break;
    }

    return BadMatch;
}