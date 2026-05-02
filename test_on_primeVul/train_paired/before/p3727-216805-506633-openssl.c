int ASN1_item_ex_i2d(const ASN1_VALUE **pval, unsigned char **out,
                     const ASN1_ITEM *it, int tag, int aclass)
{
    const ASN1_TEMPLATE *tt = NULL;
    int i, seqcontlen, seqlen, ndef = 1;
    const ASN1_EXTERN_FUNCS *ef;
    const ASN1_AUX *aux = it->funcs;
    ASN1_aux_const_cb *asn1_cb = NULL;

    if ((it->itype != ASN1_ITYPE_PRIMITIVE) && *pval == NULL)
        return 0;

    if (aux != NULL) {
        asn1_cb = ((aux->flags & ASN1_AFLG_CONST_CB) != 0) ? aux->asn1_const_cb
            : (ASN1_aux_const_cb *)aux->asn1_cb; /* backward compatibility */
    }

    switch (it->itype) {

    case ASN1_ITYPE_PRIMITIVE:
        if (it->templates)
            return asn1_template_ex_i2d(pval, out, it->templates,
                                        tag, aclass);
        return asn1_i2d_ex_primitive(pval, out, it, tag, aclass);

    case ASN1_ITYPE_MSTRING:
        return asn1_i2d_ex_primitive(pval, out, it, -1, aclass);

    case ASN1_ITYPE_CHOICE:
        if (asn1_cb && !asn1_cb(ASN1_OP_I2D_PRE, pval, it, NULL))
            return 0;
        i = asn1_get_choice_selector_const(pval, it);
        if ((i >= 0) && (i < it->tcount)) {
            const ASN1_VALUE **pchval;
            const ASN1_TEMPLATE *chtt;
            chtt = it->templates + i;
            pchval = asn1_get_const_field_ptr(pval, chtt);
            return asn1_template_ex_i2d(pchval, out, chtt, -1, aclass);
        }
        /* Fixme: error condition if selector out of range */
        if (asn1_cb && !asn1_cb(ASN1_OP_I2D_POST, pval, it, NULL))
            return 0;
        break;

    case ASN1_ITYPE_EXTERN:
        /* If new style i2d it does all the work */
        ef = it->funcs;
        return ef->asn1_ex_i2d(pval, out, it, tag, aclass);

    case ASN1_ITYPE_NDEF_SEQUENCE:
        /* Use indefinite length constructed if requested */
        if (aclass & ASN1_TFLG_NDEF)
            ndef = 2;
        /* fall through */

    case ASN1_ITYPE_SEQUENCE:
        i = asn1_enc_restore(&seqcontlen, out, pval, it);
        /* An error occurred */
        if (i < 0)
            return 0;
        /* We have a valid cached encoding... */
        if (i > 0)
            return seqcontlen;
        /* Otherwise carry on */
        seqcontlen = 0;
        /* If no IMPLICIT tagging set to SEQUENCE, UNIVERSAL */
        if (tag == -1) {
            tag = V_ASN1_SEQUENCE;
            /* Retain any other flags in aclass */
            aclass = (aclass & ~ASN1_TFLG_TAG_CLASS)
                | V_ASN1_UNIVERSAL;
        }
        if (asn1_cb && !asn1_cb(ASN1_OP_I2D_PRE, pval, it, NULL))
            return 0;
        /* First work out sequence content length */
        for (i = 0, tt = it->templates; i < it->tcount; tt++, i++) {
            const ASN1_TEMPLATE *seqtt;
            const ASN1_VALUE **pseqval;
            int tmplen;
            seqtt = asn1_do_adb(*pval, tt, 1);
            if (!seqtt)
                return 0;
            pseqval = asn1_get_const_field_ptr(pval, seqtt);
            tmplen = asn1_template_ex_i2d(pseqval, NULL, seqtt, -1, aclass);
            if (tmplen == -1 || (tmplen > INT_MAX - seqcontlen))
                return -1;
            seqcontlen += tmplen;
        }

        seqlen = ASN1_object_size(ndef, seqcontlen, tag);
        if (!out || seqlen == -1)
            return seqlen;
        /* Output SEQUENCE header */
        ASN1_put_object(out, ndef, seqcontlen, tag, aclass);
        for (i = 0, tt = it->templates; i < it->tcount; tt++, i++) {
            const ASN1_TEMPLATE *seqtt;
            const ASN1_VALUE **pseqval;
            seqtt = asn1_do_adb(*pval, tt, 1);
            if (!seqtt)
                return 0;
            pseqval = asn1_get_const_field_ptr(pval, seqtt);
            /* FIXME: check for errors in enhanced version */
            asn1_template_ex_i2d(pseqval, out, seqtt, -1, aclass);
        }
        if (ndef == 2)
            ASN1_put_eoc(out);
        if (asn1_cb && !asn1_cb(ASN1_OP_I2D_POST, pval, it, NULL))
            return 0;
        return seqlen;

    default:
        return 0;

    }
    return 0;
}