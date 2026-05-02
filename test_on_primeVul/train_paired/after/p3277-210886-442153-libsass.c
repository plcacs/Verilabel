  static int handle_error(Sass_Context* c_ctx) {
    try {
      throw;
    }
    catch (Exception::Base& e) {
      std::stringstream msg_stream;
      std::string cwd(Sass::File::get_cwd());
      std::string msg_prefix(e.errtype());
      bool got_newline = false;
      msg_stream << msg_prefix << ": ";
      const char* msg = e.what();
      while (msg && *msg) {
        if (*msg == '\r') {
          got_newline = true;
        }
        else if (*msg == '\n') {
          got_newline = true;
        }
        else if (got_newline) {
          msg_stream << std::string(msg_prefix.size() + 2, ' ');
          got_newline = false;
        }
        msg_stream << *msg;
        ++msg;
      }
      if (!got_newline) msg_stream << "\n";

      if (e.traces.empty()) {
        // we normally should have some traces, still here as a fallback
        std::string rel_path(Sass::File::abs2rel(e.pstate.path, cwd, cwd));
        msg_stream << std::string(msg_prefix.size() + 2, ' ');
        msg_stream << " on line " << e.pstate.line + 1 << " of " << rel_path << "\n";
      }
      else {
        std::string rel_path(Sass::File::abs2rel(e.pstate.path, cwd, cwd));
        msg_stream << traces_to_string(e.traces, "        ");
      }

      // now create the code trace (ToDo: maybe have util functions?)
      if (e.pstate.line != std::string::npos &&
          e.pstate.column != std::string::npos &&
          e.pstate.src != nullptr) {
        size_t lines = e.pstate.line;
        // scan through src until target line
        // move line_beg pointer to line start
        const char* line_beg;
        for (line_beg = e.pstate.src; *line_beg != '\0'; ++line_beg) {
          if (lines == 0) break;
          if (*line_beg == '\n') --lines;
        }
        // move line_end before next newline character
        const char* line_end;
        for (line_end = line_beg; *line_end != '\0'; ++line_end) {
          if (*line_end == '\n' || *line_end == '\r') break;
        }
        if (*line_end != '\0') ++line_end;
        size_t line_len = line_end - line_beg;
        size_t move_in = 0; size_t shorten = 0;
        size_t left_chars = 42; size_t max_chars = 76;
        // reported excerpt should not exceed `max_chars` chars
        if (e.pstate.column > line_len) left_chars = e.pstate.column;
        if (e.pstate.column > left_chars) move_in = e.pstate.column - left_chars;
        if (line_len > max_chars + move_in) shorten = line_len - move_in - max_chars;
        utf8::advance(line_beg, move_in, line_end);
        utf8::retreat(line_end, shorten, line_beg);
        std::string sanitized; std::string marker(e.pstate.column - move_in, '-');
        utf8::replace_invalid(line_beg, line_end, std::back_inserter(sanitized));
        msg_stream << ">> " << sanitized << "\n";
        msg_stream << "   " << marker << "^\n";
      }

      JsonNode* json_err = json_mkobject();
      json_append_member(json_err, "status", json_mknumber(1));
      json_append_member(json_err, "file", json_mkstring(e.pstate.path));
      json_append_member(json_err, "line", json_mknumber((double)(e.pstate.line + 1)));
      json_append_member(json_err, "column", json_mknumber((double)(e.pstate.column + 1)));
      json_append_member(json_err, "message", json_mkstring(e.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(e.what());
      c_ctx->error_status = 1;
      c_ctx->error_file = sass_copy_c_string(e.pstate.path);
      c_ctx->error_line = e.pstate.line + 1;
      c_ctx->error_column = e.pstate.column + 1;
      c_ctx->error_src = e.pstate.src;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    catch (std::bad_alloc& ba) {
      std::stringstream msg_stream;
      JsonNode* json_err = json_mkobject();
      msg_stream << "Unable to allocate memory: " << ba.what() << std::endl;
      json_append_member(json_err, "status", json_mknumber(2));
      json_append_member(json_err, "message", json_mkstring(ba.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(ba.what());
      c_ctx->error_status = 2;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    catch (std::exception& e) {
      std::stringstream msg_stream;
      JsonNode* json_err = json_mkobject();
      msg_stream << "Internal Error: " << e.what() << std::endl;
      json_append_member(json_err, "status", json_mknumber(3));
      json_append_member(json_err, "message", json_mkstring(e.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(e.what());
      c_ctx->error_status = 3;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    catch (std::string& e) {
      std::stringstream msg_stream;
      JsonNode* json_err = json_mkobject();
      msg_stream << "Internal Error: " << e << std::endl;
      json_append_member(json_err, "status", json_mknumber(4));
      json_append_member(json_err, "message", json_mkstring(e.c_str()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(e.c_str());
      c_ctx->error_status = 4;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    catch (const char* e) {
      std::stringstream msg_stream;
      JsonNode* json_err = json_mkobject();
      msg_stream << "Internal Error: " << e << std::endl;
      json_append_member(json_err, "status", json_mknumber(4));
      json_append_member(json_err, "message", json_mkstring(e));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(e);
      c_ctx->error_status = 4;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    catch (...) {
      std::stringstream msg_stream;
      JsonNode* json_err = json_mkobject();
      msg_stream << "Unknown error occurred" << std::endl;
      json_append_member(json_err, "status", json_mknumber(5));
      json_append_member(json_err, "message", json_mkstring("unknown"));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {}
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string("unknown");
      c_ctx->error_status = 5;
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
      json_delete(json_err);
    }
    return c_ctx->error_status;
  }