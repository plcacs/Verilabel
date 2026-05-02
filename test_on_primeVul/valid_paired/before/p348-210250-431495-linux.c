static int decode_attr_security_label(struct xdr_stream *xdr, uint32_t *bitmap,
					struct nfs4_label *label)
{
	uint32_t pi = 0;
	uint32_t lfs = 0;
	__u32 len;
	__be32 *p;
	int status = 0;

	if (unlikely(bitmap[2] & (FATTR4_WORD2_SECURITY_LABEL - 1U)))
		return -EIO;
	if (likely(bitmap[2] & FATTR4_WORD2_SECURITY_LABEL)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			return -EIO;
		lfs = be32_to_cpup(p++);
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			return -EIO;
		pi = be32_to_cpup(p++);
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			return -EIO;
		len = be32_to_cpup(p++);
		p = xdr_inline_decode(xdr, len);
		if (unlikely(!p))
			return -EIO;
		if (len < NFS4_MAXLABELLEN) {
			if (label) {
				memcpy(label->label, p, len);
				label->len = len;
				label->pi = pi;
				label->lfs = lfs;
				status = NFS_ATTR_FATTR_V4_SECURITY_LABEL;
			}
			bitmap[2] &= ~FATTR4_WORD2_SECURITY_LABEL;
		} else
			printk(KERN_WARNING "%s: label too long (%u)!\n",
					__func__, len);
	}
	if (label && label->label)
		dprintk("%s: label=%s, len=%d, PI=%d, LFS=%d\n", __func__,
			(char *)label->label, label->len, label->pi, label->lfs);
	return status;
}