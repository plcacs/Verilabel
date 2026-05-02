bool DISOpticalFlowImpl::ocl_calc(InputArray I0, InputArray I1, InputOutputArray flow)
{
    UMat I0Mat = I0.getUMat();
    UMat I1Mat = I1.getUMat();
    bool use_input_flow = false;
    if (flow.sameSize(I0) && flow.depth() == CV_32F && flow.channels() == 2)
        use_input_flow = true;
    else
        flow.create(I1Mat.size(), CV_32FC2);
    UMat &u_flowMat = flow.getUMatRef();
    coarsest_scale = min((int)(log(max(I0Mat.cols, I0Mat.rows) / (4.0 * patch_size)) / log(2.0) + 0.5), /* Original code serach for maximal movement of width/4 */
                         (int)(log(min(I0Mat.cols, I0Mat.rows) / patch_size) / log(2.0)));              /* Deepest pyramid level greater or equal than patch*/

    ocl_prepareBuffers(I0Mat, I1Mat, u_flowMat, use_input_flow);
    u_Ux[coarsest_scale].setTo(0.0f);
    u_Uy[coarsest_scale].setTo(0.0f);

    for (int i = coarsest_scale; i >= finest_scale; i--)
    {
        w = u_I0s[i].cols;
        h = u_I0s[i].rows;
        ws = 1 + (w - patch_size) / patch_stride;
        hs = 1 + (h - patch_size) / patch_stride;

        if (!ocl_precomputeStructureTensor(u_I0xx_buf, u_I0yy_buf, u_I0xy_buf,
                                           u_I0x_buf, u_I0y_buf, u_I0xs[i], u_I0ys[i]))
            return false;

        if (!ocl_PatchInverseSearch(u_Ux[i], u_Uy[i], u_I0s[i], u_I1s_ext[i], u_I0xs[i], u_I0ys[i], 2, i))
            return false;

        if (!ocl_Densification(u_Ux[i], u_Uy[i], u_Sx, u_Sy, u_I0s[i], u_I1s[i]))
            return false;

        if (variational_refinement_iter > 0)
            variational_refinement_processors[i]->calcUV(u_I0s[i], u_I1s[i],
                                                         u_Ux[i].getMat(ACCESS_WRITE), u_Uy[i].getMat(ACCESS_WRITE));

        if (i > finest_scale)
        {
            resize(u_Ux[i], u_Ux[i - 1], u_Ux[i - 1].size());
            resize(u_Uy[i], u_Uy[i - 1], u_Uy[i - 1].size());
            multiply(u_Ux[i - 1], 2, u_Ux[i - 1]);
            multiply(u_Uy[i - 1], 2, u_Uy[i - 1]);
        }
    }
    vector<UMat> uxy(2);
    uxy[0] = u_Ux[finest_scale];
    uxy[1] = u_Uy[finest_scale];
    merge(uxy, u_U);
    resize(u_U, u_flowMat, u_flowMat.size());
    multiply(u_flowMat, 1 << finest_scale, u_flowMat);

    return true;
}