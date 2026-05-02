static void __dvb_frontend_free(struct dvb_frontend *fe)
{
	struct dvb_frontend_private *fepriv = fe->frontend_priv;

	if (fepriv)
		dvb_free_device(fepriv->dvbdev);

	dvb_frontend_invoke_release(fe, fe->ops.release);

	if (fepriv)
		kfree(fepriv);
}