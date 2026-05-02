void FIFOSampleBuffer::setChannels(int numChannels)
{
    uint usedBytes;

    if (!verifyNumberOfChannels(numChannels)) return;

    usedBytes = channels * samplesInBuffer;
    channels = (uint)numChannels;
    samplesInBuffer = usedBytes / channels;
}