static int stszin(int size)
{
    int cnt;
    uint32_t ofs;

    // version/flags
    u32in();
    // Sample size
    u32in();
    // Number of entries
    mp4config.frame.ents = u32in();
    // fixme: check atom size
    mp4config.frame.data = malloc(sizeof(*mp4config.frame.data)
                                  * (mp4config.frame.ents + 1));

    if (!mp4config.frame.data)
        return ERR_FAIL;

    ofs = 0;
    mp4config.frame.data[0] = ofs;
    for (cnt = 0; cnt < mp4config.frame.ents; cnt++)
    {
        uint32_t fsize = u32in();

        ofs += fsize;
        if (mp4config.frame.maxsize < fsize)
            mp4config.frame.maxsize = fsize;

        mp4config.frame.data[cnt + 1] = ofs;

        if (ofs < mp4config.frame.data[cnt])
            return ERR_FAIL;
    }

    return size;
}