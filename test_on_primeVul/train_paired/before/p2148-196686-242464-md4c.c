md_analyze_line(MD_CTX* ctx, OFF beg, OFF* p_end,
                const MD_LINE_ANALYSIS* pivot_line, MD_LINE_ANALYSIS* line)
{
    unsigned total_indent = 0;
    int n_parents = 0;
    int n_brothers = 0;
    int n_children = 0;
    MD_CONTAINER container = { 0 };
    int prev_line_has_list_loosening_effect = ctx->last_line_has_list_loosening_effect;
    OFF off = beg;
    OFF hr_killer = 0;
    int ret = 0;

    line->indent = md_line_indentation(ctx, total_indent, off, &off);
    total_indent += line->indent;
    line->beg = off;

    /* Given the indentation and block quote marks '>', determine how many of
     * the current containers are our parents. */
    while(n_parents < ctx->n_containers) {
        MD_CONTAINER* c = &ctx->containers[n_parents];

        if(c->ch == _T('>')  &&  line->indent < ctx->code_indent_offset  &&
            off < ctx->size  &&  CH(off) == _T('>'))
        {
            /* Block quote mark. */
            off++;
            total_indent++;
            line->indent = md_line_indentation(ctx, total_indent, off, &off);
            total_indent += line->indent;

            /* The optional 1st space after '>' is part of the block quote mark. */
            if(line->indent > 0)
                line->indent--;

            line->beg = off;

        } else if(c->ch != _T('>')  &&  line->indent >= c->contents_indent) {
            /* List. */
            line->indent -= c->contents_indent;
        } else {
            break;
        }

        n_parents++;
    }

    if(off >= ctx->size  ||  ISNEWLINE(off)) {
        /* Blank line does not need any real indentation to be nested inside
         * a list. */
        if(n_brothers + n_children == 0) {
            while(n_parents < ctx->n_containers  &&  ctx->containers[n_parents].ch != _T('>'))
                n_parents++;
        }
    }

    while(TRUE) {
        /* Check whether we are fenced code continuation. */
        if(pivot_line->type == MD_LINE_FENCEDCODE) {
            line->beg = off;

            /* We are another MD_LINE_FENCEDCODE unless we are closing fence
             * which we transform into MD_LINE_BLANK. */
            if(line->indent < ctx->code_indent_offset) {
                if(md_is_closing_code_fence(ctx, CH(pivot_line->beg), off, &off)) {
                    line->type = MD_LINE_BLANK;
                    ctx->last_line_has_list_loosening_effect = FALSE;
                    break;
                }
            }

            /* Change indentation accordingly to the initial code fence. */
            if(n_parents == ctx->n_containers) {
                if(line->indent > pivot_line->indent)
                    line->indent -= pivot_line->indent;
                else
                    line->indent = 0;

                line->type = MD_LINE_FENCEDCODE;
                break;
            }
        }

        /* Check whether we are HTML block continuation. */
        if(pivot_line->type == MD_LINE_HTML  &&  ctx->html_block_type > 0) {
            if(n_parents < ctx->n_containers) {
                /* HTML block is implicitly ended if the enclosing container
                 * block ends. */
                ctx->html_block_type = 0;
            } else {
                int html_block_type;

                html_block_type = md_is_html_block_end_condition(ctx, off, &off);
                if(html_block_type > 0) {
                    MD_ASSERT(html_block_type == ctx->html_block_type);

                    /* Make sure this is the last line of the block. */
                    ctx->html_block_type = 0;

                    /* Some end conditions serve as blank lines at the same time. */
                    if(html_block_type == 6 || html_block_type == 7) {
                        line->type = MD_LINE_BLANK;
                        line->indent = 0;
                        break;
                    }
                }

                line->type = MD_LINE_HTML;
                n_parents = ctx->n_containers;
                break;
            }
        }

        /* Check for blank line. */
        if(off >= ctx->size  ||  ISNEWLINE(off)) {
            if(pivot_line->type == MD_LINE_INDENTEDCODE  &&  n_parents == ctx->n_containers) {
                line->type = MD_LINE_INDENTEDCODE;
                if(line->indent > ctx->code_indent_offset)
                    line->indent -= ctx->code_indent_offset;
                else
                    line->indent = 0;
                ctx->last_line_has_list_loosening_effect = FALSE;
            } else {
                line->type = MD_LINE_BLANK;
                ctx->last_line_has_list_loosening_effect = (n_parents > 0  &&
                        n_brothers + n_children == 0  &&
                        ctx->containers[n_parents-1].ch != _T('>'));

    #if 1
                /* See https://github.com/mity/md4c/issues/6
                 *
                 * This ugly checking tests we are in (yet empty) list item but
                 * not its very first line (i.e. not the line with the list
                 * item mark).
                 *
                 * If we are such a blank line, then any following non-blank
                 * line which would be part of the list item actually has to
                 * end the list because according to the specification, "a list
                 * item can begin with at most one blank line."
                 */
                if(n_parents > 0  &&  ctx->containers[n_parents-1].ch != _T('>')  &&
                   n_brothers + n_children == 0  &&  ctx->current_block == NULL  &&
                   ctx->n_block_bytes > (int) sizeof(MD_BLOCK))
                {
                    MD_BLOCK* top_block = (MD_BLOCK*) ((char*)ctx->block_bytes + ctx->n_block_bytes - sizeof(MD_BLOCK));
                    if(top_block->type == MD_BLOCK_LI)
                        ctx->last_list_item_starts_with_two_blank_lines = TRUE;
                }
    #endif
            }
            break;
        } else {
    #if 1
            /* This is the 2nd half of the hack. If the flag is set (i.e. there
             * was a 2nd blank line at the beginning of the list item) and if
             * we would otherwise still belong to the list item, we enforce
             * the end of the list. */
            ctx->last_line_has_list_loosening_effect = FALSE;
            if(ctx->last_list_item_starts_with_two_blank_lines) {
                if(n_parents > 0  &&  ctx->containers[n_parents-1].ch != _T('>')  &&
                   n_brothers + n_children == 0  &&  ctx->current_block == NULL  &&
                   ctx->n_block_bytes > (int) sizeof(MD_BLOCK))
                {
                    MD_BLOCK* top_block = (MD_BLOCK*) ((char*)ctx->block_bytes + ctx->n_block_bytes - sizeof(MD_BLOCK));
                    if(top_block->type == MD_BLOCK_LI)
                        n_parents--;
                }

                ctx->last_list_item_starts_with_two_blank_lines = FALSE;
            }
    #endif
        }

        /* Check whether we are Setext underline. */
        if(line->indent < ctx->code_indent_offset  &&  pivot_line->type == MD_LINE_TEXT
            &&  (CH(off) == _T('=') || CH(off) == _T('-'))
            &&  (n_parents == ctx->n_containers))
        {
            unsigned level;

            if(md_is_setext_underline(ctx, off, &off, &level)) {
                line->type = MD_LINE_SETEXTUNDERLINE;
                line->data = level;
                break;
            }
        }

        /* Check for thematic break line. */
        if(line->indent < ctx->code_indent_offset  &&  ISANYOF(off, _T("-_*"))  &&  off >= hr_killer) {
            if(md_is_hr_line(ctx, off, &off, &hr_killer)) {
                line->type = MD_LINE_HR;
                break;
            }
        }

        /* Check for "brother" container. I.e. whether we are another list item
         * in already started list. */
        if(n_parents < ctx->n_containers  &&  n_brothers + n_children == 0) {
            OFF tmp;

            if(md_is_container_mark(ctx, line->indent, off, &tmp, &container)  &&
               md_is_container_compatible(&ctx->containers[n_parents], &container))
            {
                pivot_line = &md_dummy_blank_line;

                off = tmp;

                total_indent += container.contents_indent - container.mark_indent;
                line->indent = md_line_indentation(ctx, total_indent, off, &off);
                total_indent += line->indent;
                line->beg = off;

                /* Some of the following whitespace actually still belongs to the mark. */
                if(off >= ctx->size || ISNEWLINE(off)) {
                    container.contents_indent++;
                } else if(line->indent <= ctx->code_indent_offset) {
                    container.contents_indent += line->indent;
                    line->indent = 0;
                } else {
                    container.contents_indent += 1;
                    line->indent--;
                }

                ctx->containers[n_parents].mark_indent = container.mark_indent;
                ctx->containers[n_parents].contents_indent = container.contents_indent;

                n_brothers++;
                continue;
            }
        }

        /* Check for indented code.
         * Note indented code block cannot interrupt a paragraph. */
        if(line->indent >= ctx->code_indent_offset  &&
            (pivot_line->type == MD_LINE_BLANK || pivot_line->type == MD_LINE_INDENTEDCODE))
        {
            line->type = MD_LINE_INDENTEDCODE;
            MD_ASSERT(line->indent >= ctx->code_indent_offset);
            line->indent -= ctx->code_indent_offset;
            line->data = 0;
            break;
        }

        /* Check for start of a new container block. */
        if(line->indent < ctx->code_indent_offset  &&
           md_is_container_mark(ctx, line->indent, off, &off, &container))
        {
            if(pivot_line->type == MD_LINE_TEXT  &&  n_parents == ctx->n_containers  &&
                        (off >= ctx->size || ISNEWLINE(off))  &&  container.ch != _T('>'))
            {
                /* Noop. List mark followed by a blank line cannot interrupt a paragraph. */
            } else if(pivot_line->type == MD_LINE_TEXT  &&  n_parents == ctx->n_containers  &&
                        (container.ch == _T('.') || container.ch == _T(')'))  &&  container.start != 1)
            {
                /* Noop. Ordered list cannot interrupt a paragraph unless the start index is 1. */
            } else {
                total_indent += container.contents_indent - container.mark_indent;
                line->indent = md_line_indentation(ctx, total_indent, off, &off);
                total_indent += line->indent;

                line->beg = off;
                line->data = container.ch;

                /* Some of the following whitespace actually still belongs to the mark. */
                if(off >= ctx->size || ISNEWLINE(off)) {
                    container.contents_indent++;
                } else if(line->indent <= ctx->code_indent_offset) {
                    container.contents_indent += line->indent;
                    line->indent = 0;
                } else {
                    container.contents_indent += 1;
                    line->indent--;
                }

                if(n_brothers + n_children == 0)
                    pivot_line = &md_dummy_blank_line;

                if(n_children == 0)
                    MD_CHECK(md_leave_child_containers(ctx, n_parents + n_brothers));

                n_children++;
                MD_CHECK(md_push_container(ctx, &container));
                continue;
            }
        }

        /* Check whether we are table continuation. */
        if(pivot_line->type == MD_LINE_TABLE  &&  n_parents == ctx->n_containers) {
            line->type = MD_LINE_TABLE;
            break;
        }

        /* Check for ATX header. */
        if(line->indent < ctx->code_indent_offset  &&  CH(off) == _T('#')) {
            unsigned level;

            if(md_is_atxheader_line(ctx, off, &line->beg, &off, &level)) {
                line->type = MD_LINE_ATXHEADER;
                line->data = level;
                break;
            }
        }

        /* Check whether we are starting code fence. */
        if(CH(off) == _T('`') || CH(off) == _T('~')) {
            if(md_is_opening_code_fence(ctx, off, &off)) {
                line->type = MD_LINE_FENCEDCODE;
                line->data = 1;
                break;
            }
        }

        /* Check for start of raw HTML block. */
        if(CH(off) == _T('<')  &&  !(ctx->parser.flags & MD_FLAG_NOHTMLBLOCKS))
        {
            ctx->html_block_type = md_is_html_block_start_condition(ctx, off);

            /* HTML block type 7 cannot interrupt paragraph. */
            if(ctx->html_block_type == 7  &&  pivot_line->type == MD_LINE_TEXT)
                ctx->html_block_type = 0;

            if(ctx->html_block_type > 0) {
                /* The line itself also may immediately close the block. */
                if(md_is_html_block_end_condition(ctx, off, &off) == ctx->html_block_type) {
                    /* Make sure this is the last line of the block. */
                    ctx->html_block_type = 0;
                }

                line->type = MD_LINE_HTML;
                break;
            }
        }

        /* Check for table underline. */
        if((ctx->parser.flags & MD_FLAG_TABLES)  &&  pivot_line->type == MD_LINE_TEXT  &&
           (CH(off) == _T('|') || CH(off) == _T('-') || CH(off) == _T(':'))  &&
           n_parents == ctx->n_containers)
        {
            unsigned col_count;

            if(ctx->current_block != NULL  &&  ctx->current_block->n_lines == 1  &&
                md_is_table_underline(ctx, off, &off, &col_count))
            {
                line->data = col_count;
                line->type = MD_LINE_TABLEUNDERLINE;
                break;
            }
        }

        /* By default, we are normal text line. */
        line->type = MD_LINE_TEXT;
        if(pivot_line->type == MD_LINE_TEXT  &&  n_brothers + n_children == 0) {
            /* Lazy continuation. */
            n_parents = ctx->n_containers;
        }

        /* Check for task mark. */
        if((ctx->parser.flags & MD_FLAG_TASKLISTS)  &&  n_brothers + n_children > 0  &&
           ISANYOF_(ctx->containers[ctx->n_containers-1].ch, _T("-+*.)")))
        {
            OFF tmp = off;

            while(tmp < ctx->size  &&  tmp < off + 3  &&  ISBLANK(tmp))
                tmp++;
            if(tmp + 2 < ctx->size  &&  CH(tmp) == _T('[')  &&
               ISANYOF(tmp+1, _T("xX "))  &&  CH(tmp+2) == _T(']')  &&
               (tmp + 3 == ctx->size  ||  ISBLANK(tmp+3)  ||  ISNEWLINE(tmp+3)))
            {
                MD_CONTAINER* task_container = (n_children > 0 ? &ctx->containers[ctx->n_containers-1] : &container);
                task_container->is_task = TRUE;
                task_container->task_mark_off = tmp + 1;
                off = tmp + 3;
                while(ISWHITESPACE(off))
                    off++;
                line->beg = off;
            }
        }

        break;
    }

    /* Scan for end of the line.
     *
     * Note this is quite a bottleneck of the parsing as we here iterate almost
     * over compete document.
     */
#if defined __linux__ && !defined MD4C_USE_UTF16
    /* Recent glibc versions have superbly optimized strcspn(), even using
     * vectorization if available. */
    if(ctx->doc_ends_with_newline  &&  off < ctx->size) {
        while(TRUE) {
            off += (OFF) strcspn(STR(off), "\r\n");

            /* strcspn() can stop on zero terminator; but that can appear
             * anywhere in the Markfown input... */
            if(CH(off) == _T('\0'))
                off++;
            else
                break;
        }
    } else
#endif
    {
        /* Optimization: Use some loop unrolling. */
        while(off + 3 < ctx->size  &&  !ISNEWLINE(off+0)  &&  !ISNEWLINE(off+1)
                                   &&  !ISNEWLINE(off+2)  &&  !ISNEWLINE(off+3))
            off += 4;
        while(off < ctx->size  &&  !ISNEWLINE(off))
            off++;
    }

    /* Set end of the line. */
    line->end = off;

    /* But for ATX header, we should exclude the optional trailing mark. */
    if(line->type == MD_LINE_ATXHEADER) {
        OFF tmp = line->end;
        while(tmp > line->beg && CH(tmp-1) == _T(' '))
            tmp--;
        while(tmp > line->beg && CH(tmp-1) == _T('#'))
            tmp--;
        if(tmp == line->beg || CH(tmp-1) == _T(' ') || (ctx->parser.flags & MD_FLAG_PERMISSIVEATXHEADERS))
            line->end = tmp;
    }

    /* Trim trailing spaces. */
    if(line->type != MD_LINE_INDENTEDCODE  &&  line->type != MD_LINE_FENCEDCODE) {
        while(line->end > line->beg && CH(line->end-1) == _T(' '))
            line->end--;
    }

    /* Eat also the new line. */
    if(off < ctx->size && CH(off) == _T('\r'))
        off++;
    if(off < ctx->size && CH(off) == _T('\n'))
        off++;

    *p_end = off;

    /* If we belong to a list after seeing a blank line, the list is loose. */
    if(prev_line_has_list_loosening_effect  &&  line->type != MD_LINE_BLANK  &&  n_parents + n_brothers > 0) {
        MD_CONTAINER* c = &ctx->containers[n_parents + n_brothers - 1];
        if(c->ch != _T('>')) {
            MD_BLOCK* block = (MD_BLOCK*) (((char*)ctx->block_bytes) + c->block_byte_off);
            block->flags |= MD_BLOCK_LOOSE_LIST;
        }
    }

    /* Leave any containers we are not part of anymore. */
    if(n_children == 0  &&  n_parents + n_brothers < ctx->n_containers)
        MD_CHECK(md_leave_child_containers(ctx, n_parents + n_brothers));

    /* Enter any container we found a mark for. */
    if(n_brothers > 0) {
        MD_ASSERT(n_brothers == 1);
        MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                    ctx->containers[n_parents].task_mark_off,
                    (ctx->containers[n_parents].is_task ? CH(ctx->containers[n_parents].task_mark_off) : 0),
                    MD_BLOCK_CONTAINER_CLOSER));
        MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                    container.task_mark_off,
                    (container.is_task ? CH(container.task_mark_off) : 0),
                    MD_BLOCK_CONTAINER_OPENER));
        ctx->containers[n_parents].is_task = container.is_task;
        ctx->containers[n_parents].task_mark_off = container.task_mark_off;
    }

    if(n_children > 0)
        MD_CHECK(md_enter_child_containers(ctx, n_children));

abort:
    return ret;
}