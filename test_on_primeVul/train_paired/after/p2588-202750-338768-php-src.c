static inline void phar_set_inode(phar_entry_info *entry TSRMLS_DC) /* {{{ */
{
	char tmp[MAXPATHLEN];
	int tmp_len;
	size_t len;

	tmp_len = MIN(MAXPATHLEN, entry->filename_len + entry->phar->fname_len);
	len = MIN(entry->phar->fname_len, tmp_len);
	memcpy(tmp, entry->phar->fname, len);
	len = MIN(tmp_len - len, entry->filename_len);
	memcpy(tmp + entry->phar->fname_len, entry->filename, len);
	entry->inode = (unsigned short)zend_get_hash_value(tmp, tmp_len);
}