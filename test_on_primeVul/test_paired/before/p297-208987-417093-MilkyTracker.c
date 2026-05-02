PlayerGeneric::~PlayerGeneric()
{
	if (mixer)
		delete mixer;

	if (player)
	{
		if (mixer->isActive() && !mixer->isDeviceRemoved(player))
			mixer->removeDevice(player);
		delete player;
	}

	delete[] audioDriverName;
	
	delete listener;
}