void gps_tracker( void )
{
	ssize_t unused;
    int gpsd_sock;
    char line[256], *temp;
    struct sockaddr_in gpsd_addr;
    int ret, is_json, pos;
    fd_set read_fd;
    struct timeval timeout;

    /* attempt to connect to localhost, port 2947 */

    pos = 0;
    gpsd_sock = socket( AF_INET, SOCK_STREAM, 0 );

    if( gpsd_sock < 0 ) {
        return;
    }

    gpsd_addr.sin_family      = AF_INET;
    gpsd_addr.sin_port        = htons( 2947 );
    gpsd_addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

    if( connect( gpsd_sock, (struct sockaddr *) &gpsd_addr,
                 sizeof( gpsd_addr ) ) < 0 ) {
        return;
    }

    // Check if it's GPSd < 2.92 or the new one
    // 2.92+ immediately send stuff
    // < 2.92 requires to send PVTAD command
    FD_ZERO(&read_fd);
    FD_SET(gpsd_sock, &read_fd);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    is_json = select(gpsd_sock + 1, &read_fd, NULL, NULL, &timeout);
    if (is_json) {
    	/*
			{"class":"VERSION","release":"2.95","rev":"2010-11-16T21:12:35","proto_major":3,"proto_minor":3}
			?WATCH={"json":true};
			{"class":"DEVICES","devices":[]}
    	 */


    	// Get the crap and ignore it: {"class":"VERSION","release":"2.95","rev":"2010-11-16T21:12:35","proto_major":3,"proto_minor":3}
    	if( recv( gpsd_sock, line, sizeof( line ) - 1, 0 ) <= 0 )
    		return;

    	is_json = (line[0] == '{');
    	if (is_json) {
			// Send ?WATCH={"json":true};
			memset( line, 0, sizeof( line ) );
			strcpy(line, "?WATCH={\"json\":true};\n");
			if( send( gpsd_sock, line, 22, 0 ) != 22 )
				return;

			// Check that we have devices
			memset(line, 0, sizeof(line));
			if( recv( gpsd_sock, line, sizeof( line ) - 1, 0 ) <= 0 )
				return;

			// Stop processing if there is no device
			if (strncmp(line, "{\"class\":\"DEVICES\",\"devices\":[]}", 32) == 0) {
				close(gpsd_sock);
				return;
			} else {
				pos = strlen(line);
			}
    	}
    }

    /* loop reading the GPS coordinates */

    while( G.do_exit == 0 )
    {
        usleep( 500000 );
        memset( G.gps_loc, 0, sizeof( float ) * 5 );

        /* read position, speed, heading, altitude */
        if (is_json) {
        	// Format definition: http://catb.org/gpsd/gpsd_json.html

        	if (pos == sizeof( line )) {
        		memset(line, 0, sizeof(line));
        		pos = 0;
        	}

        	// New version, JSON
        	if( recv( gpsd_sock, line + pos, sizeof( line ) - 1, 0 ) <= 0 )
        		return;

        	// search for TPV class: {"class":"TPV"
        	temp = strstr(line, "{\"class\":\"TPV\"");
        	if (temp == NULL) {
        		continue;
        	}

        	// Make sure the data we have is complete
        	if (strchr(temp, '}') == NULL) {
        		// Move the data at the beginning of the buffer;
        		pos = strlen(temp);
        		if (temp != line) {
        			memmove(line, temp, pos);
        			memset(line + pos, 0, sizeof(line) - pos);
        		}
        	}

			// Example line: {"class":"TPV","tag":"MID2","device":"/dev/ttyUSB0","time":1350957517.000,"ept":0.005,"lat":46.878936576,"lon":-115.832602964,"alt":1968.382,"track":0.0000,"speed":0.000,"climb":0.000,"mode":3}

        	// Latitude
        	temp = strstr(temp, "\"lat\":");
			if (temp == NULL) {
				continue;
			}

			ret = sscanf(temp + 6, "%f", &G.gps_loc[0]);

			// Longitude
			temp = strstr(temp, "\"lon\":");
			if (temp == NULL) {
				continue;
			}

			ret = sscanf(temp + 6, "%f", &G.gps_loc[1]);

			// Altitude
			temp = strstr(temp, "\"alt\":");
			if (temp == NULL) {
				continue;
			}

			ret = sscanf(temp + 6, "%f", &G.gps_loc[4]);

			// Speed
			temp = strstr(temp, "\"speed\":");
			if (temp == NULL) {
				continue;
			}

			ret = sscanf(temp + 6, "%f", &G.gps_loc[2]);

			// No more heading

			// Get the next TPV class
			temp = strstr(temp, "{\"class\":\"TPV\"");
			if (temp == NULL) {
				memset( line, 0, sizeof( line ) );
				pos = 0;
			} else {
				pos = strlen(temp);
				memmove(line, temp, pos);
				memset(line + pos, 0, sizeof(line) - pos);
			}

        } else {
        	memset( line, 0, sizeof( line ) );

			snprintf( line,  sizeof( line ) - 1, "PVTAD\r\n" );
			if( send( gpsd_sock, line, 7, 0 ) != 7 )
				return;

			memset( line, 0, sizeof( line ) );
			if( recv( gpsd_sock, line, sizeof( line ) - 1, 0 ) <= 0 )
				return;

			if( memcmp( line, "GPSD,P=", 7 ) != 0 )
				continue;

			/* make sure the coordinates are present */

			if( line[7] == '?' )
				continue;

			ret = sscanf( line + 7, "%f %f", &G.gps_loc[0], &G.gps_loc[1] );

			if( ( temp = strstr( line, "V=" ) ) == NULL ) continue;
			ret = sscanf( temp + 2, "%f", &G.gps_loc[2] ); /* speed */

			if( ( temp = strstr( line, "T=" ) ) == NULL ) continue;
			ret = sscanf( temp + 2, "%f", &G.gps_loc[3] ); /* heading */

			if( ( temp = strstr( line, "A=" ) ) == NULL ) continue;
			ret = sscanf( temp + 2, "%f", &G.gps_loc[4] ); /* altitude */
        }

        if (G.record_data)
			fputs( line, G.f_gps );

		G.save_gps = 1;

        if (G.do_exit == 0)
		{
			unused = write( G.gc_pipe[1], G.gps_loc, sizeof( float ) * 5 );
			kill( getppid(), SIGUSR2 );
		}
    }
}