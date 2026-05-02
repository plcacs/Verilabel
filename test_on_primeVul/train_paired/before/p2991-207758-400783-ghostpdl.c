GetNumWrongData(const byte * curPtr, const int maxnum)
{
    int count = 0;

    if (1 == maxnum) {
        return (1);
    }
    while (*(curPtr + count) != *(curPtr + count + 1) && maxnum > count) {
        count++;
    }

    return (count);
}