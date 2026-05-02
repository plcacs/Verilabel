static void set_error_response(h2_stream *stream, int http_status)
{
    if (!h2_stream_is_ready(stream)) {
        stream->rtmp->http_status = http_status;
    }
}