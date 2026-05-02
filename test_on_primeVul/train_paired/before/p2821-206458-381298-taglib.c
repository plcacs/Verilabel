ByteVector ByteVector::mid(uint index, uint length) const
{
  ByteVector v;

  if(index > size())
    return v;

  ConstIterator endIt;

  if(length < 0xffffffff && length + index < size())
    endIt = d->data.begin() + index + length;
  else
    endIt = d->data.end();

  v.d->data.insert(v.d->data.begin(), ConstIterator(d->data.begin() + index), endIt);
  v.d->size = v.d->data.size();

  return v;
}