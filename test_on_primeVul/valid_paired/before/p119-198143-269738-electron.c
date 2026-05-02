content::WebContents* WebContents::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (params.disposition != WindowOpenDisposition::CURRENT_TAB) {
    Emit("-new-window", params.url, "", params.disposition, "", params.referrer,
         params.post_data);
    return nullptr;
  }

  // Give user a chance to cancel navigation.
  if (Emit("will-navigate", params.url))
    return nullptr;

  // Don't load the URL if the web contents was marked as destroyed from a
  // will-navigate event listener
  if (IsDestroyed())
    return nullptr;

  return CommonWebContentsDelegate::OpenURLFromTab(source, params);
}