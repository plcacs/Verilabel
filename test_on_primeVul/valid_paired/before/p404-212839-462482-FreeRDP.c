INLINE void gdi_RectToCRgn(const HGDI_RECT rect, INT32* x, INT32* y, INT32* w, INT32* h)
{
	*x = rect->left;
	*y = rect->top;
	*w = rect->right - rect->left + 1;
	*h = rect->bottom - rect->top + 1;
}