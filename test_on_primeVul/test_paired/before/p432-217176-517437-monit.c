static void do_viewlog(HttpRequest req, HttpResponse res) {
        if (is_readonly(req)) {
                send_error(req, res, SC_FORBIDDEN, "You do not have sufficient privileges to access this page");
                return;
        }
        do_head(res, "_viewlog", "View log", 100);
        if ((Run.flags & Run_Log) && ! (Run.flags & Run_UseSyslog)) {
                FILE *f = fopen(Run.files.log, "r");
                if (f) {
                        size_t n;
                        char buf[512];
                        StringBuffer_append(res->outputbuffer, "<br><p><form><textarea cols=120 rows=30 readonly>");
                        while ((n = fread(buf, sizeof(char), sizeof(buf) - 1, f)) > 0) {
                                buf[n] = 0;
                                StringBuffer_append(res->outputbuffer, "%s", buf);
                        }
                        fclose(f);
                        StringBuffer_append(res->outputbuffer, "</textarea></form>");
                } else {
                        StringBuffer_append(res->outputbuffer, "Error opening logfile: %s", STRERROR);
                }
        } else {
                StringBuffer_append(res->outputbuffer,
                                    "<b>Cannot view logfile:</b><br>");
                if (! (Run.flags & Run_Log))
                        StringBuffer_append(res->outputbuffer, "Monit was started without logging");
                else
                        StringBuffer_append(res->outputbuffer, "Monit uses syslog");
        }
        do_foot(res);
}