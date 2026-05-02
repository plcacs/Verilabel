  void Compute(OpKernelContext* ctx) override {
    const Tensor& shape_t = ctx->input(0);
    const Tensor& alpha_t = ctx->input(1);

    OP_REQUIRES(ctx,
                TensorShapeUtils::IsVector(shape_t.shape()) &&
                    (shape_t.dtype() == DataType::DT_INT32 ||
                     shape_t.dtype() == DataType::DT_INT64),
                errors::InvalidArgument(
                    "shape must be a vector of {int32,int64}, got shape: ",
                    shape_t.DebugString()));
    TensorShape samples_shape;
    if (shape_t.dtype() == DataType::DT_INT32) {
      auto vec = shape_t.flat<int32>();
      OP_REQUIRES_OK(ctx, TensorShapeUtils::MakeShape(vec.data(), vec.size(),
                                                      &samples_shape));
    } else if (shape_t.dtype() == DataType::DT_INT64) {
      auto vec = shape_t.flat<int64>();
      OP_REQUIRES_OK(ctx, TensorShapeUtils::MakeShape(vec.data(), vec.size(),
                                                      &samples_shape));
    }
    const int64 samples_per_alpha = samples_shape.num_elements();

    samples_shape.AppendShape(alpha_t.shape());
    // Allocate output samples.
    Tensor* samples_t = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, samples_shape, &samples_t));

    if (samples_shape.num_elements() == 0) return;

    using random::PhiloxRandom;

    typedef random::NormalDistribution<PhiloxRandom, double> Normal;
    typedef random::UniformDistribution<PhiloxRandom, double> Uniform;
#define UNIFORM(X)                                    \
  if (uniform_remaining == 0) {                       \
    uniform_remaining = Uniform::kResultElementCount; \
    uniform_result = uniform(&gen);                   \
  }                                                   \
  uniform_remaining--;                                \
  double X = uniform_result[uniform_remaining]

    // Each attempt is 95+% successful, and requires 1-2 normal + 1 uniform
    static constexpr int kReservedSamplesPerOutput = 256;

    const auto alpha_flat = alpha_t.flat<T>().data();
    const int64 num_alphas = alpha_t.NumElements();
    OP_REQUIRES(ctx, num_alphas > 0,
                errors::InvalidArgument(
                    "Input alpha should have non-zero element count, got: ",
                    num_alphas));
    auto samples_flat = samples_t->flat<T>().data();
    PhiloxRandom rng = generator_.ReserveRandomOutputs(
        samples_per_alpha * num_alphas, kReservedSamplesPerOutput);

    // We partition work first across alphas then across samples-per-alpha to
    // avoid a couple flops which can be done on a per-alpha basis.

    auto DoWork = [samples_per_alpha, num_alphas, &rng, samples_flat,
                   alpha_flat](int64 start_output, int64 limit_output) {
      using Eigen::numext::exp;
      using Eigen::numext::log;
      using Eigen::numext::log1p;
      using Eigen::numext::pow;

      // Capturing "rng" by-value would only make a copy for the _shared_
      // lambda.  Since we want to let each worker have its own copy, we pass
      // "rng" by reference and explicitly do a copy assignment.

      Normal normal;
      Uniform uniform;
      typename Normal::ResultType norm_result;
      typename Uniform::ResultType uniform_result;
      for (int64 output_idx = start_output; output_idx < limit_output;
           /* output_idx incremented within inner loop below */) {
        int64 alpha_idx = output_idx / samples_per_alpha;

        // Instead of +alpha_idx for each sample, we offset the pointer once.
        T* const samples_alpha_offset = samples_flat + alpha_idx;

        // Several calculations can be done on a per-alpha basis.
        const double alpha = static_cast<double>(alpha_flat[alpha_idx]);

        DISABLE_FLOAT_EQUALITY_WARNING
        if (alpha == static_cast<double>(1.0)) {
          ENABLE_FLOAT_EQUALITY_WARNING
          // Sample from an exponential distribution.
          for (int64 sample_idx = output_idx % samples_per_alpha;
               sample_idx < samples_per_alpha && output_idx < limit_output;
               sample_idx++, output_idx++) {
            // As we want data stable regardless of sharding
            // (including eventually on GPU), we skip on a per-sample basis.
            PhiloxRandom gen = rng;
            gen.Skip(kReservedSamplesPerOutput * output_idx);
            int16 uniform_remaining = 0;
            UNIFORM(u);
            const double res = -log1p(-u);
            samples_alpha_offset[sample_idx * num_alphas] = static_cast<T>(res);
          }       // for (sample_idx)
        } else {  // if alpha != 1.0
          // Transformation-rejection from pairs of uniform and normal random
          // variables. http://dl.acm.org/citation.cfm?id=358414
          //
          // The algorithm has an acceptance rate of ~95% for small alpha (~1),
          // and higher accept rates for higher alpha, so runtime is
          // O(NumAlphas * NumSamples * k) with k ~ 1 / 0.95.
          //
          // For alpha<1, we add one to d=alpha-1/3, and multiply the final
          // result by uniform()^(1/alpha)
          const bool alpha_less_than_one = alpha < 1;
          const double d = alpha + (alpha_less_than_one ? 2.0 / 3 : -1.0 / 3);
          const double c = 1.0 / 3 / sqrt(d);

          // Compute the rest of the samples for the current alpha value.
          for (int64 sample_idx = output_idx % samples_per_alpha;
               sample_idx < samples_per_alpha && output_idx < limit_output;
               sample_idx++, output_idx++) {
            // Since each sample may use a variable number of normal/uniform
            // samples, and we want data stable regardless of sharding
            // (including eventually on GPU), we skip on a per-sample basis.
            PhiloxRandom gen = rng;
            gen.Skip(kReservedSamplesPerOutput * output_idx);
            int16 norm_remaining = 0;
            int16 uniform_remaining = 0;

            // Keep trying until we don't reject a sample. In practice, we will
            // only reject ~5% at worst, for low alpha near 1.
            while (true) {
              if (norm_remaining == 0) {
                norm_remaining = Normal::kResultElementCount;
                norm_result = normal(&gen);
              }
              norm_remaining--;
              const double x = norm_result[norm_remaining];
              double v = 1 + c * x;
              if (v <= 0) {
                continue;
              }
              v = v * v * v;
              UNIFORM(u);
              // The first option in the if is a "squeeze" short-circuit to
              // dodge the two logs. Magic constant sourced from the paper
              // linked above. Upward of .91 of the area covered by the log
              // inequality is covered by the squeeze as well (larger coverage
              // for smaller values of alpha).
              if ((u < 1 - 0.0331 * (x * x) * (x * x)) ||
                  (log(u) < 0.5 * x * x + d * (1 - v + log(v)))) {
                double res = d * v;
                if (alpha_less_than_one) {
                  UNIFORM(b);
                  res *= pow(b, 1 / alpha);
                }
                samples_alpha_offset[sample_idx * num_alphas] =
                    static_cast<T>(res);
                break;
              }
            }  // while: true
          }    // for: sample_idx
        }      // if (alpha == 1.0)
      }        // for: output_idx
    };         // DoWork
#undef UNIFORM
    // Two calls to log only occur for ~10% of samples reaching the log line.
    //   2 x 100 (64-bit cycles per log) x 0.10 = ~20.
    // Other ops: sqrt, +, *, /, %... something like 15 of these, at 3-6 cycles
    // each = ~60.
    // All of this /0.95 due to the rejection possibility = ~85.
    static const int kElementCost = 85 + 2 * Normal::kElementCost +
                                    Uniform::kElementCost +
                                    3 * PhiloxRandom::kElementCost;
    auto worker_threads = *(ctx->device()->tensorflow_cpu_worker_threads());
    Shard(worker_threads.num_threads, worker_threads.workers,
          num_alphas * samples_per_alpha, kElementCost, DoWork);
  }