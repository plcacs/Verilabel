static void LoadOpenCLDevices(MagickCLEnv clEnv)
{
  cl_context_properties
    properties[3];

  cl_device_id
    *devices;

  cl_int
    status;

  cl_platform_id
    *platforms;

  cl_uint
    i,
    j,
    next,
    number_devices,
    number_platforms;

  size_t
    length;

  number_platforms=0;
  if (openCL_library->clGetPlatformIDs(0,NULL,&number_platforms) != CL_SUCCESS)
    return;
  if (number_platforms == 0)
    return;
  platforms=(cl_platform_id *) AcquireMagickMemory(number_platforms*
    sizeof(cl_platform_id));
  if (platforms == (cl_platform_id *) NULL)
    return;
  if (openCL_library->clGetPlatformIDs(number_platforms,platforms,NULL) != CL_SUCCESS)
    {
       platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
       return;
    }
  for (i = 0; i < number_platforms; i++)
  {
    number_devices=GetOpenCLDeviceCount(clEnv,platforms[i]);
    if (number_devices == 0)
      platforms[i]=(cl_platform_id) NULL;
    else
      clEnv->number_devices+=number_devices;
  }
  if (clEnv->number_devices == 0)
    {
      platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
      return;
    }
  clEnv->devices=(MagickCLDevice *) AcquireQuantumMemory(clEnv->number_devices,
    sizeof(MagickCLDevice));
  if (clEnv->devices == (MagickCLDevice *) NULL)
    {
      RelinquishMagickCLDevices(clEnv);
      platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
      return;
    }
  (void) ResetMagickMemory(clEnv->devices,0,clEnv->number_devices*
    sizeof(MagickCLDevice));
  devices=(cl_device_id *) AcquireQuantumMemory(clEnv->number_devices,
    sizeof(cl_device_id));
  if (devices == (cl_device_id *) NULL)
    {
      platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
      RelinquishMagickCLDevices(clEnv);
      return;
    }
  clEnv->number_contexts=(size_t) number_platforms;
  clEnv->contexts=(cl_context *) AcquireQuantumMemory(clEnv->number_contexts,
    sizeof(cl_context));
  if (clEnv->contexts == (cl_context *) NULL)
    {
      devices=(cl_device_id *) RelinquishMagickMemory(devices);
      platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
      RelinquishMagickCLDevices(clEnv);
      return;
    }
  next=0;
  for (i = 0; i < number_platforms; i++)
  {
    if (platforms[i] == (cl_platform_id) NULL)
      continue;

    status=clEnv->library->clGetDeviceIDs(platforms[i],CL_DEVICE_TYPE_CPU | 
      CL_DEVICE_TYPE_GPU,(cl_uint) clEnv->number_devices,devices,&number_devices);
    if (status != CL_SUCCESS)
      continue;

    properties[0]=CL_CONTEXT_PLATFORM;
    properties[1]=(cl_context_properties) platforms[i];
    properties[2]=0;
    clEnv->contexts[i]=openCL_library->clCreateContext(properties,number_devices,
      devices,NULL,NULL,&status);
    if (status != CL_SUCCESS)
      continue;

    for (j = 0; j < number_devices; j++,next++)
    {
      MagickCLDevice
        device;

      device=AcquireMagickCLDevice();
      if (device == (MagickCLDevice) NULL)
        break;

      device->context=clEnv->contexts[i];
      device->deviceID=devices[j];

      openCL_library->clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,0,NULL,
        &length);
      device->platform_name=AcquireQuantumMemory(length,
        sizeof(*device->platform_name));
      openCL_library->clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,length,
        device->platform_name,NULL);

      openCL_library->clGetPlatformInfo(platforms[i],CL_PLATFORM_VENDOR,0,NULL,
        &length);
      device->vendor_name=AcquireQuantumMemory(length,
        sizeof(*device->vendor_name));
      openCL_library->clGetPlatformInfo(platforms[i],CL_PLATFORM_VENDOR,length,
        device->vendor_name,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_NAME,0,NULL,
        &length);
      device->name=AcquireQuantumMemory(length,sizeof(*device->name));
      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_NAME,length,
        device->name,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DRIVER_VERSION,0,NULL,
        &length);
      device->version=AcquireQuantumMemory(length,sizeof(*device->version));
      openCL_library->clGetDeviceInfo(devices[j],CL_DRIVER_VERSION,length,
        device->version,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_MAX_CLOCK_FREQUENCY,
        sizeof(cl_uint),&device->max_clock_frequency,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_MAX_COMPUTE_UNITS,
        sizeof(cl_uint),&device->max_compute_units,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_TYPE,
        sizeof(cl_device_type),&device->type,NULL);

      openCL_library->clGetDeviceInfo(devices[j],CL_DEVICE_LOCAL_MEM_SIZE,
        sizeof(cl_ulong),&device->local_memory_size,NULL);

      clEnv->devices[next]=device;
    }
  }
  if (next != clEnv->number_devices)
    RelinquishMagickCLDevices(clEnv);
  platforms=(cl_platform_id *) RelinquishMagickMemory(platforms);
  devices=(cl_device_id *) RelinquishMagickMemory(devices);
}