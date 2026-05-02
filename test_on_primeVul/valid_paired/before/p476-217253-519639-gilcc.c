static int src_parser_trans_stage_1_2_3(const int tmp_fd, const char *src, const struct trans_config cfg)
{
    struct parser_buf pbuf = {
        .f_indx = 0,
        .tmp_indx = 0,
        .f_read_size = 0
    };

    int write_count = 0;
    int src_fd;
    int p_state = P_STATE_CODE;

    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        fprintf(stderr, "**Error: Could not open source file: %s.\n", src);
        return -1;
    }

    while (p_buf_refill(&pbuf, src_fd) > 0) {

        while (PBUF_F_REMD(pbuf)) {

            switch (p_state) {
            case P_STATE_COMMENT_C:

                switch (PBUF_F_CHAR(pbuf)) {
                case '*':
                    p_buf_push_tmp_char(&pbuf, '*');
                    continue;

                case '/':
                    if (pbuf.tmp_indx && (PBUF_TMP_PREV_CHAR(pbuf) == '*')) {
                        pbuf.tmp_indx--;
                        p_state = P_STATE_CODE;
                    }
                    break;

                default:
                    if (pbuf.tmp_indx && (PBUF_TMP_PREV_CHAR(pbuf) == '*'))
                        pbuf.tmp_indx--;
                    break;
                }

                pbuf.f_indx++;

            case P_STATE_CODE:
            default:

                /* TODO: add trigraph support */

                switch (PBUF_F_CHAR(pbuf)) {
                case ' ':
                case '\t':
                    if (pbuf.tmp_indx &&
                            (PBUF_TMP_PREV_CHAR(pbuf) == ' ' || PBUF_TMP_PREV_CHAR(pbuf) == '\t' ||
                             PBUF_TMP_PREV_CHAR(pbuf) == '\n'))
                        pbuf.f_indx++;
                    else
                        p_buf_push_tmp_char(&pbuf, ' ');

                    continue;

                case '\r':
                case '\n':
                    if (pbuf.tmp_indx &&
                            (PBUF_TMP_PREV_CHAR(pbuf) == ' ' || PBUF_TMP_PREV_CHAR(pbuf) == '\t' ||
                             PBUF_TMP_PREV_CHAR(pbuf) == '\n')) {
                        pbuf.f_indx++;
                    } else if (pbuf.tmp_indx && 
                            (PBUF_TMP_PREV_CHAR(pbuf) == '\\')) {
                        pbuf.tmp_indx--;
                        pbuf.f_indx++;
                    } else {
                        p_buf_push_tmp_char(&pbuf, '\n');
                    }

                    continue;

                case '\\':
                    p_buf_push_tmp_char(&pbuf, '\\');
                    continue;

                case '/':
                    p_buf_push_tmp_char(&pbuf, '/');
                    continue;

                case '*':
                    if (pbuf.tmp_indx &&
                            (PBUF_TMP_PREV_CHAR(pbuf) == '/')) {
                        pbuf.tmp_indx--;
                        pbuf.f_indx++;
                        p_state = P_STATE_COMMENT_C;
                        continue;
                    }

                default:
                    break;
                }

                /* TODO: check return values */
                p_buf_write_tmp(&pbuf, tmp_fd);
                p_buf_write_f_char(&pbuf, tmp_fd);
            }
        }
    }

    p_buf_write_tmp(&pbuf, tmp_fd);
    return 0;
}