sha1_handle_first_client_response (DBusAuth         *auth,
                                   const DBusString *data)
{
  /* We haven't sent a challenge yet, we're expecting a desired
   * username from the client.
   */
  DBusString tmp;
  DBusString tmp2;
  dbus_bool_t retval = FALSE;
  DBusError error = DBUS_ERROR_INIT;

  _dbus_string_set_length (&auth->challenge, 0);
  
  if (_dbus_string_get_length (data) > 0)
    {
      if (_dbus_string_get_length (&auth->identity) > 0)
        {
          /* Tried to send two auth identities, wtf */
          _dbus_verbose ("%s: client tried to send auth identity, but we already have one\n",
                         DBUS_AUTH_NAME (auth));
          return send_rejected (auth);
        }
      else
        {
          /* this is our auth identity */
          if (!_dbus_string_copy (data, 0, &auth->identity, 0))
            return FALSE;
        }
    }
      
  if (!_dbus_credentials_add_from_user (auth->desired_identity, data))
    {
      _dbus_verbose ("%s: Did not get a valid username from client\n",
                     DBUS_AUTH_NAME (auth));
      return send_rejected (auth);
    }
      
  if (!_dbus_string_init (&tmp))
    return FALSE;

  if (!_dbus_string_init (&tmp2))
    {
      _dbus_string_free (&tmp);
      return FALSE;
    }

  /* we cache the keyring for speed, so here we drop it if it's the
   * wrong one. FIXME caching the keyring here is useless since we use
   * a different DBusAuth for every connection.
   */
  if (auth->keyring &&
      !_dbus_keyring_is_for_credentials (auth->keyring,
                                         auth->desired_identity))
    {
      _dbus_keyring_unref (auth->keyring);
      auth->keyring = NULL;
    }
  
  if (auth->keyring == NULL)
    {
      auth->keyring = _dbus_keyring_new_for_credentials (auth->desired_identity,
                                                         &auth->context,
                                                         &error);

      if (auth->keyring == NULL)
        {
          if (dbus_error_has_name (&error,
                                   DBUS_ERROR_NO_MEMORY))
            {
              dbus_error_free (&error);
              goto out;
            }
          else
            {
              _DBUS_ASSERT_ERROR_IS_SET (&error);
              _dbus_verbose ("%s: Error loading keyring: %s\n",
                             DBUS_AUTH_NAME (auth), error.message);
              if (send_rejected (auth))
                retval = TRUE; /* retval is only about mem */
              dbus_error_free (&error);
              goto out;
            }
        }
      else
        {
          _dbus_assert (!dbus_error_is_set (&error));
        }
    }

  _dbus_assert (auth->keyring != NULL);

  auth->cookie_id = _dbus_keyring_get_best_key (auth->keyring, &error);
  if (auth->cookie_id < 0)
    {
      _DBUS_ASSERT_ERROR_IS_SET (&error);
      _dbus_verbose ("%s: Could not get a cookie ID to send to client: %s\n",
                     DBUS_AUTH_NAME (auth), error.message);
      if (send_rejected (auth))
        retval = TRUE;
      dbus_error_free (&error);
      goto out;
    }
  else
    {
      _dbus_assert (!dbus_error_is_set (&error));
    }

  if (!_dbus_string_copy (&auth->context, 0,
                          &tmp2, _dbus_string_get_length (&tmp2)))
    goto out;

  if (!_dbus_string_append (&tmp2, " "))
    goto out;

  if (!_dbus_string_append_int (&tmp2, auth->cookie_id))
    goto out;

  if (!_dbus_string_append (&tmp2, " "))
    goto out;  
  
  if (!_dbus_generate_random_bytes (&tmp, N_CHALLENGE_BYTES, &error))
    {
      if (dbus_error_has_name (&error, DBUS_ERROR_NO_MEMORY))
        {
          dbus_error_free (&error);
          goto out;
        }
      else
        {
          _DBUS_ASSERT_ERROR_IS_SET (&error);
          _dbus_verbose ("%s: Error generating challenge: %s\n",
                         DBUS_AUTH_NAME (auth), error.message);
          if (send_rejected (auth))
            retval = TRUE; /* retval is only about mem */

          dbus_error_free (&error);
          goto out;
        }
    }

  _dbus_string_set_length (&auth->challenge, 0);
  if (!_dbus_string_hex_encode (&tmp, 0, &auth->challenge, 0))
    goto out;
  
  if (!_dbus_string_hex_encode (&tmp, 0, &tmp2,
                                _dbus_string_get_length (&tmp2)))
    goto out;

  if (!send_data (auth, &tmp2))
    goto out;
      
  goto_state (auth, &server_state_waiting_for_data);
  retval = TRUE;
  
 out:
  _dbus_string_zero (&tmp);
  _dbus_string_free (&tmp);
  _dbus_string_zero (&tmp2);
  _dbus_string_free (&tmp2);

  return retval;
}