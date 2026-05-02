bool format_go_output(const string& file_path) {

  // formatting via gofmt deactivated due to THRIFT-3893
  // Please look at the ticket and make sure you fully understand all the implications
  // before submitting a patch that enables this feature again. Thank you.
  (void) file_path;
  return false;
  
  /*
  const string command = "gofmt -w " + file_path;

  if (system(command.c_str()) == 0) {
    return true;
  }

  fprintf(stderr, "WARNING - Running '%s' failed.\n", command.c_str());
  return false;
  */
 }