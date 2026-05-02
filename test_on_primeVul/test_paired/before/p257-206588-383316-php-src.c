gdImageFillToBorder (gdImagePtr im, int x, int y, int border, int color)
{
  int lastBorder;
  /* Seek left */
  int leftLimit, rightLimit;
  int i;
  leftLimit = (-1);
  if (border < 0)
    {
      /* Refuse to fill to a non-solid border */
      return;
    }
  for (i = x; (i >= 0); i--)
    {
      if (gdImageGetPixel (im, i, y) == border)
	{
	  break;
	}
      gdImageSetPixel (im, i, y, color);
      leftLimit = i;
    }
  if (leftLimit == (-1))
    {
      return;
    }
  /* Seek right */
  rightLimit = x;
  for (i = (x + 1); (i < im->sx); i++)
    {
      if (gdImageGetPixel (im, i, y) == border)
	{
	  break;
	}
      gdImageSetPixel (im, i, y, color);
      rightLimit = i;
    }
  /* Look at lines above and below and start paints */
  /* Above */
  if (y > 0)
    {
      lastBorder = 1;
      for (i = leftLimit; (i <= rightLimit); i++)
	{
	  int c;
	  c = gdImageGetPixel (im, i, y - 1);
	  if (lastBorder)
	    {
	      if ((c != border) && (c != color))
		{
		  gdImageFillToBorder (im, i, y - 1,
				       border, color);
		  lastBorder = 0;
		}
	    }
	  else if ((c == border) || (c == color))
	    {
	      lastBorder = 1;
	    }
	}
    }
  /* Below */
  if (y < ((im->sy) - 1))
    {
      lastBorder = 1;
      for (i = leftLimit; (i <= rightLimit); i++)
	{
	  int c;
	  c = gdImageGetPixel (im, i, y + 1);
	  if (lastBorder)
	    {
	      if ((c != border) && (c != color))
		{
		  gdImageFillToBorder (im, i, y + 1,
				       border, color);
		  lastBorder = 0;
		}
	    }
	  else if ((c == border) || (c == color))
	    {
	      lastBorder = 1;
	    }
	}
    }
}