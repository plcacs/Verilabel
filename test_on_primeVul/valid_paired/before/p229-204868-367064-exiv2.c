    int MemIo::seek(int64 offset, Position pos )
    {
        int64 newIdx = 0;

        switch (pos) {
            case BasicIo::cur:
                newIdx = p_->idx_ + offset;
                break;
            case BasicIo::beg:
                newIdx = offset;
                break;
            case BasicIo::end:
                newIdx = p_->size_ + offset;
                break;
        }

        if (newIdx < 0)
            return 1;

        p_->idx_ = static_cast<long>(newIdx);   //not very sure about this. need more test!!    - note by Shawn  fly2xj@gmail.com //TODO
        p_->eof_ = false;
        return 0;
    }