static void unzzip_cat_file(ZZIP_DIR* disk, char* name, FILE* out)
{
    ZZIP_FILE* file = zzip_file_open (disk, name, 0);
    if (file) 
    {
	char buffer[1024]; int len;
	while ((len = zzip_file_read (file, buffer, 1024))) 
	{
	    fwrite (buffer, 1, len, out);
	}
	
	zzip_file_close (file);
    }
}