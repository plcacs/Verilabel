int Dispatcher::getparam( size_t N, int defaultval )
{
  int ret = defaultval;
  if ( !parsed ) {
    parse_params();
  }

  if ( parsed_params.size() > N ) {
    ret = parsed_params[ N ];
  }

  if ( ret > PARAM_MAX ) {
    ret = defaultval;
  }

  if ( ret < 1 ) ret = defaultval;

  return ret;
}