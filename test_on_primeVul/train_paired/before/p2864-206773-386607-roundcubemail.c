main(int argc, char *argv[])
{
  int cnt,rc,cc;
  char cmnd[255];

  strcpy(cmnd, CMD);

  if (argc > 1)
  {
    for (cnt = 1; cnt < argc; cnt++)
    {
      strcat(cmnd, " ");
      strcat(cmnd, argv[cnt]);
    }
  }
  else
  {
    fprintf(stderr, "__ %s:  failed %d  %d\n", argv[0], rc, cc);
    return 255;
  }

  cc = setuid(UID);
  rc = system(cmnd);

  if ((rc != 0) || (cc != 0))
  {
    fprintf(stderr, "__ %s:  failed %d  %d\n", argv[0], rc, cc);
    return 1;
  }

  return 0;
}