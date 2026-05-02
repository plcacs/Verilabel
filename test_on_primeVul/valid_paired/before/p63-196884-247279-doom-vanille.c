void M_LoadDefaults (void)
{
    int		i;
    int		len;
    FILE*	f;
    char	def[80];
    char	strparm[100];
    char*	newstring;
    int		parm;
    boolean	isstring;
    
    // set everything to base values
    numdefaults = sizeof(defaults)/sizeof(defaults[0]);
    for (i=0 ; i<numdefaults ; i++)
	*defaults[i].location = defaults[i].defaultvalue;
    
    // check for a custom default file
    i = M_CheckParm ("-config");
    if (i && i<myargc-1)
    {
	defaultfile = myargv[i+1];
	printf ("	default file: %s\n",defaultfile);
    }
    else
	defaultfile = basedefault;
    
    // read the file in, overriding any set defaults
    f = fopen (defaultfile, "r");
    if (f)
    {
	while (!feof(f))
	{
	    isstring = false;
	    if (fscanf (f, "%79s %[^\n]\n", def, strparm) == 2)
	    {
		if (strparm[0] == '"')
		{
		    // get a string default
		    isstring = true;
		    len = strlen(strparm);
		    newstring = (char *) malloc(len);
		    strparm[len-1] = 0;
		    strcpy(newstring, strparm+1);
		}
		else if (strparm[0] == '0' && strparm[1] == 'x')
		    sscanf(strparm+2, "%x", &parm);
		else
		    sscanf(strparm, "%i", &parm);
		for (i=0 ; i<numdefaults ; i++)
		    if (!strcmp(def, defaults[i].name))
		    {
			if (!isstring)
			    *defaults[i].location = parm;
			else
			    *defaults[i].location =
				(int) newstring;
			break;
		    }
	    }
	}
		
	fclose (f);
    }

    for (i = 0; i < numdefaults; i++)
    {
        if (defaults[i].scantranslate)
        {
            parm = *defaults[i].location;
            defaults[i].untranslated = parm;
            *defaults[i].location = scantokey[parm];
        }
    }
}