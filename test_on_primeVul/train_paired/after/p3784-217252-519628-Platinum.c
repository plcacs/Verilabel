PLT_HttpServer::ServeFile(const NPT_HttpRequest&        request, 
                          const NPT_HttpRequestContext& context,
                          NPT_HttpResponse&             response,
                          NPT_String                    file_path) 
{
    NPT_InputStreamReference stream;
    NPT_File                 file(file_path);
    NPT_FileInfo             file_info;
    
    // prevent hackers from accessing files outside of our root
    if ((file_path.Find("../") >= 0) || (file_path.Find("..\\") >= 0) ||
        NPT_FAILED(NPT_File::GetInfo(file_path, &file_info))) {
        return NPT_ERROR_NO_SUCH_ITEM;
    }
    
    // check for range requests
    const NPT_String* range_spec = request.GetHeaders().GetHeaderValue(NPT_HTTP_HEADER_RANGE);
    
    // handle potential 304 only if range header not set
    NPT_DateTime  date;
    NPT_TimeStamp timestamp;
    if (NPT_SUCCEEDED(PLT_UPnPMessageHelper::GetIfModifiedSince((NPT_HttpMessage&)request, date)) &&
        !range_spec) {
        date.ToTimeStamp(timestamp);
        
        NPT_LOG_INFO_5("File %s timestamps: request=%d (%s) vs file=%d (%s)", 
                       (const char*)request.GetUrl().GetPath(),
                       (NPT_UInt32)timestamp.ToSeconds(),
                       (const char*)date.ToString(),
                       (NPT_UInt32)file_info.m_ModificationTime,
                       (const char*)NPT_DateTime(file_info.m_ModificationTime).ToString());
        
        if (timestamp >= file_info.m_ModificationTime) {
            // it's a match
            NPT_LOG_FINE_1("Returning 304 for %s", request.GetUrl().GetPath().GetChars());
            response.SetStatus(304, "Not Modified", NPT_HTTP_PROTOCOL_1_1);
            return NPT_SUCCESS;
        }
    }
    
    // open file
    if (NPT_FAILED(file.Open(NPT_FILE_OPEN_MODE_READ)) || 
        NPT_FAILED(file.GetInputStream(stream))        ||
        stream.IsNull()) {
        return NPT_ERROR_NO_SUCH_ITEM;
    }
    
    // set Last-Modified and Cache-Control headers
    if (file_info.m_ModificationTime) {
        NPT_DateTime last_modified = NPT_DateTime(file_info.m_ModificationTime);
        response.GetHeaders().SetHeader("Last-Modified", last_modified.ToString(NPT_DateTime::FORMAT_RFC_1123), true);
        response.GetHeaders().SetHeader("Cache-Control", "max-age=0,must-revalidate", true);
        //response.GetHeaders().SetHeader("Cache-Control", "max-age=1800", true);
    }
    
    PLT_HttpRequestContext tmp_context(request, context);
    return ServeStream(request, context, response, stream, PLT_MimeType::GetMimeType(file_path, &tmp_context));
}