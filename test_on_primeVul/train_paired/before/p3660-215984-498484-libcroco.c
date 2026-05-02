cr_tknzr_parse_rgb (CRTknzr * a_this, CRRgb ** a_rgb)
{
        enum CRStatus status = CR_OK;
        CRInputPos init_pos;
        CRNum *num = NULL;
        guchar next_bytes[3] = { 0 }, cur_byte = 0;
        glong red = 0,
                green = 0,
                blue = 0,
                i = 0;
        gboolean is_percentage = FALSE;
        CRParsingLocation location = {0} ;

        g_return_val_if_fail (a_this && PRIVATE (a_this), CR_BAD_PARAM_ERROR);

        RECORD_INITIAL_POS (a_this, &init_pos);

        PEEK_BYTE (a_this, 1, &next_bytes[0]);
        PEEK_BYTE (a_this, 2, &next_bytes[1]);
        PEEK_BYTE (a_this, 3, &next_bytes[2]);

        if (((next_bytes[0] == 'r') || (next_bytes[0] == 'R'))
            && ((next_bytes[1] == 'g') || (next_bytes[1] == 'G'))
            && ((next_bytes[2] == 'b') || (next_bytes[2] == 'B'))) {
                SKIP_CHARS (a_this, 1);
                cr_tknzr_get_parsing_location (a_this, &location) ;
                SKIP_CHARS (a_this, 2);
        } else {
                status = CR_PARSING_ERROR;
                goto error;
        }
        READ_NEXT_BYTE (a_this, &cur_byte);
        ENSURE_PARSING_COND (cur_byte == '(');

        cr_tknzr_try_to_skip_spaces (a_this);
        status = cr_tknzr_parse_num (a_this, &num);
        ENSURE_PARSING_COND ((status == CR_OK) && (num != NULL));

        red = num->val;
        cr_num_destroy (num);
        num = NULL;

        PEEK_BYTE (a_this, 1, &next_bytes[0]);
        if (next_bytes[0] == '%') {
                SKIP_CHARS (a_this, 1);
                is_percentage = TRUE;
        }
        cr_tknzr_try_to_skip_spaces (a_this);

        for (i = 0; i < 2; i++) {
                READ_NEXT_BYTE (a_this, &cur_byte);
                ENSURE_PARSING_COND (cur_byte == ',');

                cr_tknzr_try_to_skip_spaces (a_this);
                status = cr_tknzr_parse_num (a_this, &num);
                ENSURE_PARSING_COND ((status == CR_OK) && (num != NULL));

                PEEK_BYTE (a_this, 1, &next_bytes[0]);
                if (next_bytes[0] == '%') {
                        SKIP_CHARS (a_this, 1);
                        is_percentage = 1;
                }

                if (i == 0) {
                        green = num->val;
                } else if (i == 1) {
                        blue = num->val;
                }

                if (num) {
                        cr_num_destroy (num);
                        num = NULL;
                }
                cr_tknzr_try_to_skip_spaces (a_this);
        }

        READ_NEXT_BYTE (a_this, &cur_byte);
        if (*a_rgb == NULL) {
                *a_rgb = cr_rgb_new_with_vals (red, green, blue,
                                               is_percentage);

                if (*a_rgb == NULL) {
                        status = CR_ERROR;
                        goto error;
                }
                status = CR_OK;
        } else {
                (*a_rgb)->red = red;
                (*a_rgb)->green = green;
                (*a_rgb)->blue = blue;
                (*a_rgb)->is_percentage = is_percentage;

                status = CR_OK;
        }

        if (status == CR_OK) {
                if (a_rgb && *a_rgb) {
                        cr_parsing_location_copy 
                                (&(*a_rgb)->location, 
                                 &location) ;
                }
                return CR_OK;
        }

 error:
        if (num) {
                cr_num_destroy (num);
                num = NULL;
        }

        cr_tknzr_set_cur_pos (a_this, &init_pos);
        return CR_OK;
}