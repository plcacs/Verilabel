void SoundTouch::setChannels(uint numChannels)
{
    /*if (numChannels != 1 && numChannels != 2) 
    {
        //ST_THROW_RT_ERROR("Illegal number of channels");
        return;
    }*/
    channels = numChannels;
    pRateTransposer->setChannels((int)numChannels);
    pTDStretch->setChannels((int)numChannels);
}