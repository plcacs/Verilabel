void dtls1_reset_seq_numbers(SSL *s, int rw)
{
    unsigned char *seq;
    unsigned int seq_bytes = sizeof(s->s3->read_sequence);

    if (rw & SSL3_CC_READ) {
        seq = s->s3->read_sequence;
        s->d1->r_epoch++;
        memcpy(&(s->d1->bitmap), &(s->d1->next_bitmap), sizeof(DTLS1_BITMAP));
        memset(&(s->d1->next_bitmap), 0x00, sizeof(DTLS1_BITMAP));

        /*
         * We must not use any buffered messages received from the previous
         * epoch
         */
        dtls1_clear_received_buffer(s);
    } else {
        seq = s->s3->write_sequence;
        memcpy(s->d1->last_write_sequence, seq,
               sizeof(s->s3->write_sequence));
        s->d1->w_epoch++;
    }

    memset(seq, 0x00, seq_bytes);
}