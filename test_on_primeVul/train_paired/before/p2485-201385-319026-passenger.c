	SpawnPreparationInfo prepareSpawn(const Options &options) {
		TRACE_POINT();
		SpawnPreparationInfo info;
		prepareChroot(info, options);
		info.userSwitching = prepareUserSwitching(options);
		prepareSwitchingWorkingDirectory(info, options);
		inferApplicationInfo(info);
		return info;
	}