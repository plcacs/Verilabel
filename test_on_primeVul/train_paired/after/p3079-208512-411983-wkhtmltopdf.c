LoadPage::LoadPage():
	jsdelay(200),
	windowStatus(""),
	zoomFactor(1.0),
	repeatCustomHeaders(false),
	blockLocalFileAccess(true),
	stopSlowScripts(true),
	debugJavascript(false),
	loadErrorHandling(abort),
	mediaLoadErrorHandling(ignore),
	cacheDir(""),
	proxyHostNameLookup(false) {};