GetNumWrongData(const byte * curPtr, const int maxnum)
{
    int count = 0;

    if (1 == maxnum) {
        return (1);
    }
    while (maxnum > count+1 && *(curPtr + count) != *(curPtr + count + 1)) {
        count++;
    }

    return (count);
}