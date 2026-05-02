START_TEST(test_tm_adopt_ispidowner)
  {
  /* we are the owner of this pid so should return true */
  fail_unless(TRUE == ispidowner(getpid()));

  /* assuming the unit test is not run as root, owner */
  /* owner of this pid is not the owner of pid 1 (init) */
  fail_unless(FALSE == ispidowner(1));
  }