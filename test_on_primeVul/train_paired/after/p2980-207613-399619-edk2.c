InitializeCpuMpWorker (
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                      Status;
  EFI_VECTOR_HANDOFF_INFO         *VectorInfo;
  EFI_PEI_VECTOR_HANDOFF_INFO_PPI *VectorHandoffInfoPpi;

  //
  // Get Vector Hand-off Info PPI
  //
  VectorInfo = NULL;
  Status = PeiServicesLocatePpi (
             &gEfiVectorHandoffInfoPpiGuid,
             0,
             NULL,
             (VOID **)&VectorHandoffInfoPpi
             );
  if (Status == EFI_SUCCESS) {
    VectorInfo = VectorHandoffInfoPpi->Info;
  }

  //
  // Initialize default handlers
  //
  Status = InitializeCpuExceptionHandlers (VectorInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MpInitLibInitialize ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Special initialization for the sake of Stack Guard
  //
  InitializeMpExceptionStackSwitchHandlers ();

  //
  // Update and publish CPU BIST information
  //
  CollectBistDataFromPpi (PeiServices);

  //
  // Install CPU MP PPI
  //
  Status = PeiServicesInstallPpi(&mPeiCpuMpPpiDesc);
  ASSERT_EFI_ERROR (Status);

  return Status;
}