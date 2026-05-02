void FIFOSampleBuffer::setChannels(int numChannels)
{
    uint usedBytes;

    assert(numChannels > 0);
    usedBytes = channels * samplesInBuffer;
    channels = (uint)numChannels;
    samplesInBuffer = usedBytes / channels;
}