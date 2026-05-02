int caca_dither_bitmap(caca_canvas_t *cv, int x, int y, int w, int h,
                        caca_dither_t const *d, void const *pixels)
{
    int *floyd_steinberg, *fs_r, *fs_g, *fs_b;
    uint32_t savedattr;
    int fs_length;
    int x1, y1, x2, y2, pitch, deltax, deltay, dchmax;

    if(!d || !pixels)
        return 0;

    savedattr = caca_get_attr(cv, -1, -1);

    x1 = x; x2 = x + w - 1;
    y1 = y; y2 = y + h - 1;

    /* FIXME: do not overwrite arguments */
    w = d->w;
    h = d->h;
    pitch = d->pitch;

    deltax = x2 - x1 + 1;
    deltay = y2 - y1 + 1;
    dchmax = d->glyph_count;

    fs_length = ((int)cv->width <= x2 ? (int)cv->width : x2) + 1;
    floyd_steinberg = malloc(3 * (fs_length + 2) * sizeof(int));
    memset(floyd_steinberg, 0, 3 * (fs_length + 2) * sizeof(int));
    fs_r = floyd_steinberg + 1;
    fs_g = fs_r + fs_length + 2;
    fs_b = fs_g + fs_length + 2;

    for(y = y1 > 0 ? y1 : 0; y <= y2 && y <= (int)cv->height; y++)
    {
        int remain_r = 0, remain_g = 0, remain_b = 0;

        for(x = x1 > 0 ? x1 : 0, d->init_dither(y);
            x <= x2 && x <= (int)cv->width;
            x++)
    {
        unsigned int rgba[4];
        int error[3];
        int i, ch = 0, distmin;
        int fg_r = 0, fg_g = 0, fg_b = 0, bg_r, bg_g, bg_b;
        int fromx, fromy, tox, toy, myx, myy, dots, dist;

        int outfg = 0, outbg = 0;
        uint32_t outch;

        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;

        /* First get RGB */
        if(d->antialias)
        {
            fromx = (x - x1) * w / deltax;
            fromy = (y - y1) * h / deltay;
            tox = (x - x1 + 1) * w / deltax;
            toy = (y - y1 + 1) * h / deltay;

            /* We want at least one pixel */
            if(tox == fromx) tox++;
            if(toy == fromy) toy++;

            dots = 0;

            for(myx = fromx; myx < tox; myx++)
                for(myy = fromy; myy < toy; myy++)
            {
                dots++;
                get_rgba_default(d, pixels, myx, myy, rgba);
            }

            /* Normalize */
            rgba[0] /= dots;
            rgba[1] /= dots;
            rgba[2] /= dots;
            rgba[3] /= dots;
        }
        else
        {
            fromx = (x - x1) * w / deltax;
            fromy = (y - y1) * h / deltay;
            tox = (x - x1 + 1) * w / deltax;
            toy = (y - y1 + 1) * h / deltay;

            /* tox and toy can overflow the canvas, but they cannot overflow
             * when averaged with fromx and fromy because these are guaranteed
             * to be within the pixel boundaries. */
            myx = (fromx + tox) / 2;
            myy = (fromy + toy) / 2;

            get_rgba_default(d, pixels, myx, myy, rgba);
        }

        /* FIXME: hack to force greyscale */
        if(d->color == COLOR_MODE_FULLGRAY)
        {
            unsigned int gray = (3 * rgba[0] + 4 * rgba[1] + rgba[2] + 4) / 8;
            rgba[0] = rgba[1] = rgba[2] = gray;
        }

        if(d->has_alpha && rgba[3] < 0x800)
        {
            remain_r = remain_g = remain_b = 0;
            fs_r[x] = 0;
            fs_g[x] = 0;
            fs_b[x] = 0;
            continue;
        }

        /* XXX: OMG HAX */
        if(d->init_dither == init_fstein_dither)
        {
            rgba[0] += remain_r;
            rgba[1] += remain_g;
            rgba[2] += remain_b;
        }
        else
        {
            rgba[0] += (d->get_dither() - 0x80) * 4;
            rgba[1] += (d->get_dither() - 0x80) * 4;
            rgba[2] += (d->get_dither() - 0x80) * 4;
        }

        distmin = INT_MAX;
        for(i = 0; i < 16; i++)
        {
            if(d->color == COLOR_MODE_FULLGRAY
                && (rgb_palette[i * 3] != rgb_palette[i * 3 + 1]
                     || rgb_palette[i * 3] != rgb_palette[i * 3 + 2]))
                continue;
            dist = sq(rgba[0] - rgb_palette[i * 3])
                 + sq(rgba[1] - rgb_palette[i * 3 + 1])
                 + sq(rgba[2] - rgb_palette[i * 3 + 2]);
            dist *= rgb_weight[i];
            if(dist < distmin)
            {
                outbg = i;
                distmin = dist;
            }
        }
        bg_r = rgb_palette[outbg * 3];
        bg_g = rgb_palette[outbg * 3 + 1];
        bg_b = rgb_palette[outbg * 3 + 2];

        /* FIXME: we currently only honour "full16" */
        if(d->color == COLOR_MODE_FULL16 || d->color == COLOR_MODE_FULLGRAY)
        {
            distmin = INT_MAX;
            for(i = 0; i < 16; i++)
            {
                if(i == outbg)
                    continue;
                if(d->color == COLOR_MODE_FULLGRAY
                    && (rgb_palette[i * 3] != rgb_palette[i * 3 + 1]
                         || rgb_palette[i * 3] != rgb_palette[i * 3 + 2]))
                    continue;
                dist = sq(rgba[0] - rgb_palette[i * 3])
                     + sq(rgba[1] - rgb_palette[i * 3 + 1])
                     + sq(rgba[2] - rgb_palette[i * 3 + 2]);
                dist *= rgb_weight[i];
                if(dist < distmin)
                {
                    outfg = i;
                    distmin = dist;
                }
            }
            fg_r = rgb_palette[outfg * 3];
            fg_g = rgb_palette[outfg * 3 + 1];
            fg_b = rgb_palette[outfg * 3 + 2];

            distmin = INT_MAX;
            for(i = 0; i < dchmax - 1; i++)
            {
                int newr = i * fg_r + ((2*dchmax-1) - i) * bg_r;
                int newg = i * fg_g + ((2*dchmax-1) - i) * bg_g;
                int newb = i * fg_b + ((2*dchmax-1) - i) * bg_b;
                dist = abs(rgba[0] * (2*dchmax-1) - newr)
                     + abs(rgba[1] * (2*dchmax-1) - newg)
                     + abs(rgba[2] * (2*dchmax-1) - newb);

                if(dist < distmin)
                {
                    ch = i;
                    distmin = dist;
                }
            }
            outch = d->glyphs[ch];

            /* XXX: OMG HAX */
            if(d->init_dither == init_fstein_dither)
            {
                error[0] = rgba[0] - (fg_r * ch + bg_r * ((2*dchmax-1) - ch)) / (2*dchmax-1);
                error[1] = rgba[1] - (fg_g * ch + bg_g * ((2*dchmax-1) - ch)) / (2*dchmax-1);
                error[2] = rgba[2] - (fg_b * ch + bg_b * ((2*dchmax-1) - ch)) / (2*dchmax-1);
            }
        }
        else
        {
            unsigned int lum = rgba[0];
            if(rgba[1] > lum) lum = rgba[1];
            if(rgba[2] > lum) lum = rgba[2];
            outfg = outbg;
            outbg = CACA_BLACK;

            ch = lum * dchmax / 0x1000;
            if(ch < 0)
                ch = 0;
            else if(ch > (int)(dchmax - 1))
                ch = dchmax - 1;
            outch = d->glyphs[ch];

            /* XXX: OMG HAX */
            if(d->init_dither == init_fstein_dither)
            {
                error[0] = rgba[0] - bg_r * ch / (dchmax-1);
                error[1] = rgba[1] - bg_g * ch / (dchmax-1);
                error[2] = rgba[2] - bg_b * ch / (dchmax-1);
            }
        }

        /* XXX: OMG HAX */
        if(d->init_dither == init_fstein_dither)
        {
            remain_r = fs_r[x+1] + 7 * error[0] / 16;
            remain_g = fs_g[x+1] + 7 * error[1] / 16;
            remain_b = fs_b[x+1] + 7 * error[2] / 16;
            fs_r[x-1] += 3 * error[0] / 16;
            fs_g[x-1] += 3 * error[1] / 16;
            fs_b[x-1] += 3 * error[2] / 16;
            fs_r[x] = 5 * error[0] / 16;
            fs_g[x] = 5 * error[1] / 16;
            fs_b[x] = 5 * error[2] / 16;
            fs_r[x+1] = 1 * error[0] / 16;
            fs_g[x+1] = 1 * error[1] / 16;
            fs_b[x+1] = 1 * error[2] / 16;
        }

        if(d->invert)
        {
            outfg = 15 - outfg;
            outbg = 15 - outbg;
        }

        /* Now output the character */
        caca_set_color_ansi(cv, outfg, outbg);
        caca_put_char(cv, x, y, outch);

        d->increment_dither();
    }
        /* end loop */
    }

    free(floyd_steinberg);

    caca_set_attr(cv, savedattr);

    return 0;
}