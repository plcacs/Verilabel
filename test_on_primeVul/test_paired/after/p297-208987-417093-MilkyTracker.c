PlayerGeneric::~PlayerGeneric()
{

	if (player)
	{
		if (mixer && mixer->isActive() && !mixer->isDeviceRemoved(player))
			mixer->removeDevice(player);
		delete player;
	}
	
	if (mixer)
		delete mixer;

	delete[] audioDriverName;
	
	delete listener;
}