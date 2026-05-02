void DDGifSlurp(GifInfo *info, bool decode, bool exitAfterFrame) {
	GifRecordType RecordType;
	GifByteType *ExtData;
	int ExtFunction;
	GifFileType *gifFilePtr;
	gifFilePtr = info->gifFilePtr;
	uint_fast32_t lastAllocatedGCBIndex = 0;
	do {
		if (DGifGetRecordType(gifFilePtr, &RecordType) == GIF_ERROR) {
			break;
		}
		bool isInitialPass = !decode && !exitAfterFrame;
		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:

				if (DGifGetImageDesc(gifFilePtr, isInitialPass) == GIF_ERROR) {
					break;
				}

				if (isInitialPass) {
					int_fast32_t widthOverflow = gifFilePtr->Image.Width - gifFilePtr->SWidth;
					int_fast32_t heightOverflow = gifFilePtr->Image.Height - gifFilePtr->SHeight;
					if (widthOverflow > 0 || heightOverflow > 0) {
						gifFilePtr->SWidth += widthOverflow;
						gifFilePtr->SHeight += heightOverflow;
					}
					SavedImage *sp = &gifFilePtr->SavedImages[gifFilePtr->ImageCount - 1];
					int_fast32_t topOverflow = gifFilePtr->Image.Top + gifFilePtr->Image.Height - gifFilePtr->SHeight;
					if (topOverflow > 0) {
						sp->ImageDesc.Top -= topOverflow;
					}

					int_fast32_t leftOverflow = gifFilePtr->Image.Left + gifFilePtr->Image.Width - gifFilePtr->SWidth;
					if (leftOverflow > 0) {
						sp->ImageDesc.Left -= leftOverflow;
					}
					if (!updateGCB(info, &lastAllocatedGCBIndex)) {
						break;
					}
				}

				if (decode) {
					int_fast32_t widthOverflow = gifFilePtr->Image.Width - info->originalWidth;
					int_fast32_t heightOverflow = gifFilePtr->Image.Height - info->originalHeight;
					const uint_fast32_t newRasterSize = gifFilePtr->Image.Width * gifFilePtr->Image.Height;
					if (newRasterSize > info->rasterSize || widthOverflow > 0 || heightOverflow > 0) {
						void *tmpRasterBits = reallocarray(info->rasterBits, newRasterSize, sizeof(GifPixelType));
						if (tmpRasterBits == NULL) {
							gifFilePtr->Error = D_GIF_ERR_NOT_ENOUGH_MEM;
							break;
						}
						info->rasterBits = tmpRasterBits;
						info->rasterSize = newRasterSize;
					}
					if (gifFilePtr->Image.Interlace) {
						uint_fast16_t i, j;
						/*
						 * The way an interlaced image should be read -
						 * offsets and jumps...
						 */
						uint_fast8_t InterlacedOffset[] = {0, 4, 2, 1};
						uint_fast8_t InterlacedJumps[] = {8, 8, 4, 2};
						/* Need to perform 4 passes on the image */
						for (i = 0; i < 4; i++)
							for (j = InterlacedOffset[i]; j < gifFilePtr->Image.Height; j += InterlacedJumps[i]) {
								if (DGifGetLine(gifFilePtr, info->rasterBits + j * gifFilePtr->Image.Width, gifFilePtr->Image.Width) == GIF_ERROR)
									break;
							}
					} else {
						if (DGifGetLine(gifFilePtr, info->rasterBits, gifFilePtr->Image.Width * gifFilePtr->Image.Height) == GIF_ERROR) {
							break;
						}
					}

					if (info->sampleSize > 1) {
						unsigned char *dst = info->rasterBits;
						unsigned char *src = info->rasterBits;
						unsigned char *const srcEndImage = info->rasterBits + gifFilePtr->Image.Width * gifFilePtr->Image.Height;
						do {
							unsigned char *srcNextLineStart = src + gifFilePtr->Image.Width * info->sampleSize;
							unsigned char *const srcEndLine = src + gifFilePtr->Image.Width;
							unsigned char *dstEndLine = dst + gifFilePtr->Image.Width / info->sampleSize;
							do {
								*dst = *src;
								dst++;
								src += info->sampleSize;
							} while (src < srcEndLine);
							dst = dstEndLine;
							src = srcNextLineStart;
						} while (src < srcEndImage);
					}
					return;
				} else {
					do {
						if (DGifGetCodeNext(gifFilePtr, &ExtData) == GIF_ERROR) {
							break;
						}
					} while (ExtData != NULL);
					if (exitAfterFrame) {
						return;
					}
				}
				break;

			case EXTENSION_RECORD_TYPE:
				if (DGifGetExtension(gifFilePtr, &ExtFunction, &ExtData) == GIF_ERROR) {
					break;
				}
				if (isInitialPass) {
					updateGCB(info, &lastAllocatedGCBIndex);
					if (readExtensions(ExtFunction, ExtData, info) == GIF_ERROR) {
						break;
					}
				}
				while (ExtData != NULL) {
					if (DGifGetExtensionNext(gifFilePtr, &ExtData) == GIF_ERROR) {
						break;
					}
					if (isInitialPass && readExtensions(ExtFunction, ExtData, info) == GIF_ERROR) {
						break;
					}
				}
				break;

			case TERMINATE_RECORD_TYPE:
				break;

			default: /* Should be trapped by DGifGetRecordType */
				break;
		}
	} while (RecordType != TERMINATE_RECORD_TYPE);

	info->rewindFunction(info);
}