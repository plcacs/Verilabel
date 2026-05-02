nbd_unlocked_opt_go (struct nbd_handle *h)
{
  int err;
  nbd_completion_callback c = { .callback = go_complete, .user_data = &err };
  int r = nbd_unlocked_aio_opt_go (h, &c);

  if (r == -1)
    return r;

  r = wait_for_option (h);
  if (r == 0 && err) {
    assert (nbd_internal_is_state_negotiating (get_next_state (h)));
    set_error (err, "server replied with error to opt_go request");
    return -1;
  }
  if (r == 0)
    assert (nbd_internal_is_state_ready (get_next_state (h)));
  return r;
}