    bool parse( char* ptr )
    {
        bool first = true;
        bool ok = true;
        FileNode root_collection(fs->getFS(), 0, 0);

        for(;;)
        {
            // 0. skip leading comments and directives  and ...
            // 1. reach the first item
            for(;;)
            {
                ptr = skipSpaces( ptr, 0, INT_MAX );
                if( !ptr || *ptr == '\0' )
                {
                    ok = !first;
                    break;
                }

                if( *ptr == '%' )
                {
                    if( memcmp( ptr, "%YAML", 5 ) == 0 &&
                        memcmp( ptr, "%YAML:1.", 8 ) != 0 &&
                        memcmp( ptr, "%YAML 1.", 8 ) != 0)
                        CV_PARSE_ERROR_CPP( "Unsupported YAML version (it must be 1.x)" );
                    *ptr = '\0';
                }
                else if( *ptr == '-' )
                {
                    if( memcmp(ptr, "---", 3) == 0 )
                    {
                        ptr += 3;
                        break;
                    }
                    else if( first )
                        break;
                }
                else if( cv_isalnum(*ptr) || *ptr=='_')
                {
                    if( !first )
                        CV_PARSE_ERROR_CPP( "The YAML streams must start with '---', except the first one" );
                    break;
                }
                else if( fs->eof() )
                    break;
                else
                    CV_PARSE_ERROR_CPP( "Invalid or unsupported syntax" );
            }

            if( ptr )
                ptr = skipSpaces( ptr, 0, INT_MAX );
            if( !ptr || !ptr[0] )
                break;
            if( memcmp( ptr, "...", 3 ) != 0 )
            {
                // 2. parse the collection
                FileNode root_node = fs->addNode(root_collection, std::string(), FileNode::NONE);

                ptr = parseValue( ptr, root_node, 0, false );
                if( !root_node.isMap() && !root_node.isSeq() )
                    CV_PARSE_ERROR_CPP( "Only collections as YAML streams are supported by this parser" );

                // 3. parse until the end of file or next collection
                ptr = skipSpaces( ptr, 0, INT_MAX );
                if( !ptr )
                    break;
            }

            if( fs->eof() )
                break;
            ptr += 3;
            first = false;
        }

        return ok;
    }