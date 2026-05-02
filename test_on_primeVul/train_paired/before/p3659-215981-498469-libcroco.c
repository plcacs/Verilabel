cr_input_read_byte (CRInput * a_this, guchar * a_byte)
{
        g_return_val_if_fail (a_this && PRIVATE (a_this)
                              && a_byte, CR_BAD_PARAM_ERROR);

        g_return_val_if_fail (PRIVATE (a_this)->next_byte_index <=
                              PRIVATE (a_this)->nb_bytes, CR_BAD_PARAM_ERROR);

        if (PRIVATE (a_this)->end_of_input == TRUE)
                return CR_END_OF_INPUT_ERROR;

        *a_byte = PRIVATE (a_this)->in_buf[PRIVATE (a_this)->next_byte_index];

        if (PRIVATE (a_this)->nb_bytes -
            PRIVATE (a_this)->next_byte_index < 2) {
                PRIVATE (a_this)->end_of_input = TRUE;
        } else {
                PRIVATE (a_this)->next_byte_index++;
        }

        return CR_OK;
}