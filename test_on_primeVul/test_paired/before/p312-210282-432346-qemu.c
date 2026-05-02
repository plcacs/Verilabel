static int i2c_ddc_rx(I2CSlave *i2c)
{
    I2CDDCState *s = I2CDDC(i2c);

    int value;
    value = s->edid_blob[s->reg];
    s->reg++;
    return value;
}