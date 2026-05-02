  void UrlParser::parse(char ch)
  {
    switch(_state)
    {
      case state_0:
        if (ch == '=')
          _state = state_value;
        else if (ch == '&')
          ;
        else if (ch == '%')
          _state = state_keyesc;
        else
        {
          _key = ch;
          _state = state_key;
        }
        break;

      case state_key:
        if (ch == '=')
          _state = state_value;
        else if (ch == '&')
        {
          _q.add(_key);
          _key.clear();
          _state = state_0;
        }
        else if (ch == '%')
          _state = state_keyesc;
        else
          _key += ch;
        break;

      case state_value:
        if (ch == '%')
          _state = state_valueesc;
        else if (ch == '&')
        {
          _q.add(_key, _value);
          _key.clear();
          _value.clear();
          _state = state_0;
        }
        else if (ch == '+')
          _value += ' ';
        else
          _value += ch;
        break;

      case state_keyesc:
      case state_valueesc:
        if (ch >= '0' && ch <= '9')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - '0');
        }
        else if (ch >= 'a' && ch <= 'f')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - 'a' + 10);
        }
        else if (ch >= 'A' && ch <= 'F')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - 'A' + 10);
        }
        else
        {
          if (_cnt == 0)
          {
            if (_state == state_keyesc)
            {
              _key += '%';
              _state = state_key;
            }
            else
            {
              _value += '%';
              _state = state_value;
            }
          }
          else
          {
            if (_state == state_keyesc)
            {
              _key += static_cast<char>(_v);
              _state = state_key;
            }
            else
            {
              _value += static_cast<char>(_v);
              _state = state_value;
            }

            _cnt = 0;
            _v = 0;
          }

          parse(ch);
          break;
        }

        if (_cnt >= 2)
        {
          if (_state == state_keyesc)
          {
            _key += static_cast<char>(_v);
            _state = state_key;
          }
          else
          {
            _value += static_cast<char>(_v);
            _state = state_value;
          }
          _cnt = 0;
          _v = 0;
        }

        break;

    }

  }