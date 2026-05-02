calculateNumTiles (int *numTiles,
		   int numLevels,
		   int min, int max,
		   int size,
		   LevelRoundingMode rmode)
{
    for (int i = 0; i < numLevels; i++)
    {
        // use 64 bits to avoid int overflow if size is large.
        Int64 l = levelSize (min, max, i, rmode);
        numTiles[i] = (l + size - 1) / size;
    }
}