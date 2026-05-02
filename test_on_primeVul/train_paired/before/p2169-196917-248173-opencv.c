void DISOpticalFlowImpl::calc(InputArray I0, InputArray I1, InputOutputArray flow)
{
    CV_Assert(!I0.empty() && I0.depth() == CV_8U && I0.channels() == 1);
    CV_Assert(!I1.empty() && I1.depth() == CV_8U && I1.channels() == 1);
    CV_Assert(I0.sameSize(I1));
    CV_Assert(I0.isContinuous());
    CV_Assert(I1.isContinuous());

    CV_OCL_RUN(flow.isUMat() &&
               (patch_size == 8) && (use_spatial_propagation == true),
               ocl_calc(I0, I1, flow));

    Mat I0Mat = I0.getMat();
    Mat I1Mat = I1.getMat();
    bool use_input_flow = false;
    if (flow.sameSize(I0) && flow.depth() == CV_32F && flow.channels() == 2)
        use_input_flow = true;
    else
        flow.create(I1Mat.size(), CV_32FC2);
    Mat flowMat = flow.getMat();
    coarsest_scale = min((int)(log(max(I0Mat.cols, I0Mat.rows) / (4.0 * patch_size)) / log(2.0) + 0.5), /* Original code serach for maximal movement of width/4 */
                         (int)(log(min(I0Mat.cols, I0Mat.rows) / patch_size) / log(2.0)));              /* Deepest pyramid level greater or equal than patch*/
    int num_stripes = getNumThreads();

    prepareBuffers(I0Mat, I1Mat, flowMat, use_input_flow);
    Ux[coarsest_scale].setTo(0.0f);
    Uy[coarsest_scale].setTo(0.0f);

    for (int i = coarsest_scale; i >= finest_scale; i--)
    {
        w = I0s[i].cols;
        h = I0s[i].rows;
        ws = 1 + (w - patch_size) / patch_stride;
        hs = 1 + (h - patch_size) / patch_stride;

        precomputeStructureTensor(I0xx_buf, I0yy_buf, I0xy_buf, I0x_buf, I0y_buf, I0xs[i], I0ys[i]);
        if (use_spatial_propagation)
        {
            /* Use a fixed number of stripes regardless the number of threads to make inverse search
             * with spatial propagation reproducible
             */
            parallel_for_(Range(0, 8), PatchInverseSearch_ParBody(*this, 8, hs, Sx, Sy, Ux[i], Uy[i], I0s[i],
                                                                  I1s_ext[i], I0xs[i], I0ys[i], 2, i));
        }
        else
        {
            parallel_for_(Range(0, num_stripes),
                          PatchInverseSearch_ParBody(*this, num_stripes, hs, Sx, Sy, Ux[i], Uy[i], I0s[i], I1s_ext[i],
                                                     I0xs[i], I0ys[i], 1, i));
        }

        parallel_for_(Range(0, num_stripes),
                      Densification_ParBody(*this, num_stripes, I0s[i].rows, Ux[i], Uy[i], Sx, Sy, I0s[i], I1s[i]));
        if (variational_refinement_iter > 0)
            variational_refinement_processors[i]->calcUV(I0s[i], I1s[i], Ux[i], Uy[i]);

        if (i > finest_scale)
        {
            resize(Ux[i], Ux[i - 1], Ux[i - 1].size());
            resize(Uy[i], Uy[i - 1], Uy[i - 1].size());
            Ux[i - 1] *= 2;
            Uy[i - 1] *= 2;
        }
    }
    Mat uxy[] = {Ux[finest_scale], Uy[finest_scale]};
    merge(uxy, 2, U);
    resize(U, flowMat, flowMat.size());
    flowMat *= 1 << finest_scale;
}