void RateTransposer::setChannels(int nChannels)
{
    if (!verifyNumberOfChannels(nChannels) ||
        (pTransposer->numChannels == nChannels)) return;

    pTransposer->setChannels(nChannels);
    inputBuffer.setChannels(nChannels);
    midBuffer.setChannels(nChannels);
    outputBuffer.setChannels(nChannels);
}