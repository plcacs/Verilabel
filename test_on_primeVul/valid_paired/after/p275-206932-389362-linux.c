static void gprinter_free(struct usb_function *f)
{
	struct printer_dev *dev = func_to_printer(f);
	struct f_printer_opts *opts;

	opts = container_of(f->fi, struct f_printer_opts, func_inst);

	kref_put(&dev->kref, printer_dev_free);
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);
}