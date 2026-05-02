static Jsi_RC NumberToPrecisionCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char buf[100];
    int prec = 0, skip = 0;
    Jsi_Number num;
    Jsi_Value *v;
    ChkStringN(_this, funcPtr, v);
    if (Jsi_GetIntFromValue(interp, Jsi_ValueArrayIndex(interp, args, skip), &prec) != JSI_OK)
        return JSI_ERROR;
    if (prec<=0) return JSI_ERROR;
    Jsi_GetDoubleFromValue(interp, v, &num);
    snprintf(buf, sizeof(buf),"%.*" JSI_NUMFFMT, prec, num);
    if (num<0)
        prec++;
    buf[prec+1] = 0;
    if (buf[prec] == '.')
        buf[prec] = 0;
    Jsi_ValueMakeStringDup(interp, ret, buf);
    return JSI_OK;
}