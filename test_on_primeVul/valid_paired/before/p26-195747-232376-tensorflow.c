inline void ComputeInterpolationWeights(
    const int64 out_size, const int64 in_size, const float scale,
    const int resolution, InterpolationCache<T_SCALE>* interpolation) {
  const Scaler scaler;
  interpolation->lower.resize(out_size + 1);
  interpolation->upper.resize(out_size + 1);
  interpolation->lerp.resize(out_size + 1);
  interpolation->ilerp.resize(out_size + 1);

  interpolation->lower[out_size] = 0;
  interpolation->upper[out_size] = 0;
  for (int64 i = out_size - 1; i >= 0; --i) {
    const float in = scaler(i, scale);
    const float in_f = std::floor(in);
    interpolation->lower[i] =
        std::max(static_cast<int64>(in_f), static_cast<int64>(0));
    interpolation->upper[i] =
        std::min(static_cast<int64>(std::ceil(in)), in_size - 1);
    interpolation->lerp[i] = in - in_f;
    interpolation->ilerp[i] =
        static_cast<T_SCALE>((in - in_f) * (1 << resolution));
  }
}