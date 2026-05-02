START_TEST(test_tm_adopt_ispidowner)
  {
  /* we are the owner of this pid so should return true */
  fail_unless(TRUE == ispidowner(getpid()));

  /* when unit test run as non-root user, owner of this pid is not the owner of pid 1 (init) */
  if (getuid() != 0)
    fail_unless(FALSE == ispidowner(1));
  }