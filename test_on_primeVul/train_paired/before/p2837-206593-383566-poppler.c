_poppler_attachment_new (FileSpec *emb_file)
{
  PopplerAttachment *attachment;
  PopplerAttachmentPrivate *priv;
  EmbFile *embFile;

  g_assert (emb_file != nullptr);

  attachment = (PopplerAttachment *) g_object_new (POPPLER_TYPE_ATTACHMENT, nullptr);
  priv = POPPLER_ATTACHMENT_GET_PRIVATE (attachment);

  if (emb_file->getFileName ())
    attachment->name = _poppler_goo_string_to_utf8 (emb_file->getFileName ());
  if (emb_file->getDescription ())
    attachment->description = _poppler_goo_string_to_utf8 (emb_file->getDescription ());

  embFile = emb_file->getEmbeddedFile();
  attachment->size = embFile->size ();

  if (embFile->createDate ())
    _poppler_convert_pdf_date_to_gtime (embFile->createDate (), (time_t *)&attachment->ctime);
  if (embFile->modDate ())
    _poppler_convert_pdf_date_to_gtime (embFile->modDate (), (time_t *)&attachment->mtime);

  if (embFile->checksum () && embFile->checksum ()->getLength () > 0)
    attachment->checksum = g_string_new_len (embFile->checksum ()->getCString (),
                                             embFile->checksum ()->getLength ());
  priv->obj_stream = embFile->streamObject()->copy();

  return attachment;
}