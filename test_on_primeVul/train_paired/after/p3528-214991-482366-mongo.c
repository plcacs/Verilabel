            Status readUTF8String( StringData* out ) {
                int sz;
                if ( !readNumber<int>( &sz ) )
                    return Status( ErrorCodes::InvalidBSON, "invalid bson" );

                if ( sz <= 0 ) {
                    // must have NULL at the very least
                    return Status( ErrorCodes::InvalidBSON, "invalid bson");
                }

                if ( out ) {
                    *out = StringData( _buffer + _position, sz );
                }

                if ( !skip( sz - 1 ) )
                    return Status( ErrorCodes::InvalidBSON, "invalid bson" );

                char c;
                if ( !readNumber<char>( &c ) )
                    return Status( ErrorCodes::InvalidBSON, "invalid bson" );

                if ( c != 0 )
                    return Status( ErrorCodes::InvalidBSON, "not null terminate string" );

                return Status::OK();
            }