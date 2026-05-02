static inline void phar_set_inode(phar_entry_info *entry TSRMLS_DC) /* {{{ */
{
	char tmp[MAXPATHLEN];
	int tmp_len;

	tmp_len = entry->filename_len + entry->phar->fname_len;
	memcpy(tmp, entry->phar->fname, entry->phar->fname_len);
	memcpy(tmp + entry->phar->fname_len, entry->filename, entry->filename_len);
	entry->inode = (unsigned short)zend_get_hash_value(tmp, tmp_len);
}