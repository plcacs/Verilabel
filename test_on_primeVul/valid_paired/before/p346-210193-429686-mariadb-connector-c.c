int ma_read_ok_packet(MYSQL *mysql, uchar *pos, ulong length)
{
  size_t item_len;
  mysql->affected_rows= net_field_length_ll(&pos);
  mysql->insert_id=	  net_field_length_ll(&pos);
  mysql->server_status=uint2korr(pos);
  pos+=2;
  mysql->warning_count=uint2korr(pos);
  pos+=2;
  if (pos < mysql->net.read_pos+length)
  {
    if ((item_len= net_field_length(&pos)))
      mysql->info=(char*) pos;

    /* check if server supports session tracking */
    if (mysql->server_capabilities & CLIENT_SESSION_TRACKING)
    {
      ma_clear_session_state(mysql);
      pos+= item_len;

      if (mysql->server_status & SERVER_SESSION_STATE_CHANGED)
      {
        int i;
        if (pos < mysql->net.read_pos + length)
        {
          LIST *session_item;
          MYSQL_LEX_STRING *str= NULL;
          enum enum_session_state_type si_type;
          uchar *old_pos= pos;
          size_t item_len= net_field_length(&pos);  /* length for all items */

          /* length was already set, so make sure that info will be zero terminated */
          if (mysql->info)
            *old_pos= 0;

          while (item_len > 0)
          {
            size_t plen;
            char *data;
            old_pos= pos;
            si_type= (enum enum_session_state_type)net_field_length(&pos);
            switch(si_type) {
            case SESSION_TRACK_SCHEMA:
            case SESSION_TRACK_STATE_CHANGE:
            case SESSION_TRACK_TRANSACTION_CHARACTERISTICS:
            case SESSION_TRACK_SYSTEM_VARIABLES:
              if (si_type != SESSION_TRACK_STATE_CHANGE)
                net_field_length(&pos); /* ignore total length, item length will follow next */
              plen= net_field_length(&pos);
              if (!(session_item= ma_multi_malloc(0,
                                  &session_item, sizeof(LIST),
                                  &str, sizeof(MYSQL_LEX_STRING),
                                  &data, plen,
                                  NULL)))
              {
                ma_clear_session_state(mysql);
                SET_CLIENT_ERROR(mysql, CR_OUT_OF_MEMORY, SQLSTATE_UNKNOWN, 0);
                return -1;
              }
              str->length= plen;
              str->str= data;
              memcpy(str->str, (char *)pos, plen);
              pos+= plen;
              session_item->data= str;
              mysql->extension->session_state[si_type].list= list_add(mysql->extension->session_state[si_type].list, session_item);

              /* in case schema has changed, we have to update mysql->db */
              if (si_type == SESSION_TRACK_SCHEMA)
              {
                free(mysql->db);
                mysql->db= malloc(plen + 1);
                memcpy(mysql->db, str->str, plen);
                mysql->db[plen]= 0;
              }
              else if (si_type == SESSION_TRACK_SYSTEM_VARIABLES)
              {
                my_bool set_charset= 0;
                /* make sure that we update charset in case it has changed */
                if (!strncmp(str->str, "character_set_client", str->length))
                  set_charset= 1;
                plen= net_field_length(&pos);
                if (!(session_item= ma_multi_malloc(0,
                                    &session_item, sizeof(LIST),
                                    &str, sizeof(MYSQL_LEX_STRING),
                                    &data, plen,
                                    NULL)))
                {
                  ma_clear_session_state(mysql);
                  SET_CLIENT_ERROR(mysql, CR_OUT_OF_MEMORY, SQLSTATE_UNKNOWN, 0);
                  return -1;
                }
                str->length= plen;
                str->str= data;
                memcpy(str->str, (char *)pos, plen);
                pos+= plen;
                session_item->data= str;
                mysql->extension->session_state[si_type].list= list_add(mysql->extension->session_state[si_type].list, session_item);
                if (set_charset &&
                    strncmp(mysql->charset->csname, str->str, str->length) != 0)
                {
                  char cs_name[64];
                  MARIADB_CHARSET_INFO *cs_info;
                  memcpy(cs_name, str->str, str->length);
                  cs_name[str->length]= 0;
                  if ((cs_info = (MARIADB_CHARSET_INFO *)mysql_find_charset_name(cs_name)))
                    mysql->charset= cs_info;
                }
              }
              break;
            default:
              /* not supported yet */
              plen= net_field_length(&pos);
              pos+= plen;
              break;
            }
            item_len-= (pos - old_pos);
          }
        }
        for (i= SESSION_TRACK_BEGIN; i <= SESSION_TRACK_END; i++)
        {
          mysql->extension->session_state[i].list= list_reverse(mysql->extension->session_state[i].list);
          mysql->extension->session_state[i].current= mysql->extension->session_state[i].list;
        }
      }
    }
  }
  /* CONC-351: clear session state information */
  else if (mysql->server_capabilities & CLIENT_SESSION_TRACKING)
    ma_clear_session_state(mysql);
  return(0);
}