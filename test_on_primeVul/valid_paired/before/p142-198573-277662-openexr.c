calculateNumTiles (int *numTiles,
		   int numLevels,
		   int min, int max,
		   int size,
		   LevelRoundingMode rmode)
{
    for (int i = 0; i < numLevels; i++)
    {
        int l = levelSize (min, max, i, rmode);
        if (l > std::numeric_limits<int>::max() - size + 1)
            throw IEX_NAMESPACE::ArgExc ("Invalid size.");

        numTiles[i] = (l + size - 1) / size;
    }
}