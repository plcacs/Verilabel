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

        if (static_cast<size_t>(newIdx) > p_->size_) {
            p_->eof_ = true;
            return 1;
        }

        p_->idx_ = static_cast<size_t>(newIdx);
        p_->eof_ = false;
        return 0;
    }