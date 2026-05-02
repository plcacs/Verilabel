void APar_ExtractDetails(FILE *isofile, uint8_t optional_output) {
  char uint32_buffer[5];
  Trackage track = {0};

  AtomicInfo *mvhdAtom = APar_FindAtom("moov.mvhd", false, VERSIONED_ATOM, 0);
  if (mvhdAtom != NULL) {
    APar_ExtractMovieDetails(uint32_buffer, isofile, mvhdAtom);
    fprintf(stdout,
            "Movie duration: %.3lf seconds (%s) - %.2lf* kbp/sec bitrate "
            "(*=approximate)\n",
            movie_info.seconds,
            secsTOtime(movie_info.seconds),
            movie_info.simple_bitrate_calc);
    if (optional_output & SHOW_DATE_INFO) {
      fprintf(stdout,
              "  Presentation Creation Date (UTC):     %s\n",
              APar_extract_UTC(movie_info.creation_time));
      fprintf(stdout,
              "  Presentation Modification Date (UTC): %s\n",
              APar_extract_UTC(movie_info.modified_time));
    }
  }

  AtomicInfo *iodsAtom = APar_FindAtom("moov.iods", false, VERSIONED_ATOM, 0);
  if (iodsAtom != NULL) {
    movie_info.contains_iods = true;
    APar_Extract_iods_Info(isofile, iodsAtom);
  }

  if (optional_output & SHOW_TRACK_INFO) {
    APar_TrackLevelInfo(&track,
                        NULL); // With track_num set to 0, it will return the
                               // total trak atom into total_tracks here.

    fprintf(
        stdout, "Low-level details. Total tracks: %u\n", track.total_tracks);
    fprintf(stdout,
            "Trk  Type  Handler                    Kind  Lang  Bytes\n");

    if (track.total_tracks > 0) {
      while (track.total_tracks > track.track_num) {
        track.track_num += 1;
        TrackInfo track_info = {0};

        // tracknum, handler type, handler name
        APar_ExtractTrackDetails(uint32_buffer, isofile, &track, &track_info);
        uint16_t more_whitespace =
            purge_extraneous_characters(track_info.track_hdlr_name);

        if (strlen(track_info.track_hdlr_name) == 0) {
          memcpy(track_info.track_hdlr_name, "[none listed]", 13);
        }
        fprintf(stdout,
                "%u    %s  %s",
                track.track_num,
                uint32tochar4(track_info.track_type, uint32_buffer),
                track_info.track_hdlr_name);

        uint16_t handler_len = strlen(track_info.track_hdlr_name);
        if (handler_len < 25 + more_whitespace) {
          for (uint16_t i = handler_len; i < 25 + more_whitespace; i++) {
            fprintf(stdout, " ");
          }
        }

        // codec, language
        fprintf(stdout,
                "  %s  %s   %" PRIu64,
                uint32tochar4(track_info.track_codec, uint32_buffer),
                track_info.unpacked_lang,
                track_info.sample_aggregate);

        if (track_info.encoder_name[0] != 0 && track_info.contains_esds) {
          purge_extraneous_characters(track_info.encoder_name);
          fprintf(stdout, "   Encoder: %s", track_info.encoder_name);
        }
        if (track_info.type_of_track & DRM_PROTECTED_TRACK) {
          fprintf(stdout,
                  " (protected %s)",
                  uint32tochar4(track_info.protected_codec, uint32_buffer));
        }

        fprintf(stdout, "\n");
        /*---------------------------------*/

        if (track_info.type_of_track & VIDEO_TRACK ||
            track_info.type_of_track & AUDIO_TRACK) {
          APar_Print_TrackDetails(&track_info);
        }

        if (optional_output & SHOW_DATE_INFO) {
          fprintf(stdout,
                  "       Creation Date (UTC):     %s\n",
                  APar_extract_UTC(track_info.creation_time));
          fprintf(stdout,
                  "       Modification Date (UTC): %s\n",
                  APar_extract_UTC(track_info.modified_time));
        }
      }
    }
  }
}