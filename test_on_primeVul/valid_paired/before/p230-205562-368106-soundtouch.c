BPMDetect::BPMDetect(int numChannels, int aSampleRate) :
    beat_lpf(_LPF_coeffs)
{
    beats.reserve(250); // initial reservation to prevent frequent reallocation

    this->sampleRate = aSampleRate;
    this->channels = numChannels;

    decimateSum = 0;
    decimateCount = 0;

    // choose decimation factor so that result is approx. 1000 Hz
    decimateBy = sampleRate / TARGET_SRATE;
    assert(decimateBy > 0);
    assert(INPUT_BLOCK_SIZE < decimateBy * DECIMATED_BLOCK_SIZE);

    // Calculate window length & starting item according to desired min & max bpms
    windowLen = (60 * sampleRate) / (decimateBy * MIN_BPM);
    windowStart = (60 * sampleRate) / (decimateBy * MAX_BPM_RANGE);

    assert(windowLen > windowStart);

    // allocate new working objects
    xcorr = new float[windowLen];
    memset(xcorr, 0, windowLen * sizeof(float));

    pos = 0;
    peakPos = 0;
    peakVal = 0;
    init_scaler = 1;
    beatcorr_ringbuffpos = 0;
    beatcorr_ringbuff = new float[windowLen];
    memset(beatcorr_ringbuff, 0, windowLen * sizeof(float));

    // allocate processing buffer
    buffer = new FIFOSampleBuffer();
    // we do processing in mono mode
    buffer->setChannels(1);
    buffer->clear();

    // calculate hamming windows
    hamw = new float[XCORR_UPDATE_SEQUENCE];
    hamming(hamw, XCORR_UPDATE_SEQUENCE);
    hamw2 = new float[XCORR_UPDATE_SEQUENCE / 2];
    hamming(hamw2, XCORR_UPDATE_SEQUENCE / 2);
}