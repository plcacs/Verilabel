createRandomCursorExecutor(const CollectionPtr& coll,
                           const boost::intrusive_ptr<ExpressionContext>& expCtx,
                           long long sampleSize,
                           long long numRecords,
                           boost::optional<BucketUnpacker> bucketUnpacker) {
    OperationContext* opCtx = expCtx->opCtx;

    // Verify that we are already under a collection lock. We avoid taking locks ourselves in this
    // function because double-locking forces any PlanExecutor we create to adopt a NO_YIELD policy.
    invariant(opCtx->lockState()->isCollectionLockedForMode(coll->ns(), MODE_IS));

    static const double kMaxSampleRatioForRandCursor = 0.05;
    if (!expCtx->ns.isTimeseriesBucketsCollection()) {
        if (sampleSize > numRecords * kMaxSampleRatioForRandCursor || numRecords <= 100) {
            return std::pair{nullptr, false};
        }
    } else {
        // Suppose that a time-series bucket collection is observed to contain 200 buckets, and the
        // 'gTimeseriesBucketMaxCount' parameter is set to 1000. If all buckets are full, then the
        // maximum possible measurment count would be 200 * 1000 = 200,000. While the
        // 'SampleFromTimeseriesBucket' plan is more efficient when the sample size is small
        // relative to the total number of measurements in the time-series collection, for larger
        // sample sizes the top-k sort based sample is faster. Experiments have approximated that
        // the tipping point is roughly when the requested sample size is greater than 1% of the
        // maximum possible number of measurements in the collection (i.e. numBuckets *
        // maxMeasurementsPerBucket).
        static const double kCoefficient = 0.01;
        if (sampleSize > kCoefficient * numRecords * gTimeseriesBucketMaxCount) {
            return std::pair{nullptr, false};
        }
    }

    // Attempt to get a random cursor from the RecordStore.
    auto rsRandCursor = coll->getRecordStore()->getRandomCursor(opCtx);
    if (!rsRandCursor) {
        // The storage engine has no random cursor support.
        return std::pair{nullptr, false};
    }

    // Build a MultiIteratorStage and pass it the random-sampling RecordCursor.
    auto ws = std::make_unique<WorkingSet>();
    std::unique_ptr<PlanStage> root =
        std::make_unique<MultiIteratorStage>(expCtx.get(), ws.get(), coll);
    static_cast<MultiIteratorStage*>(root.get())->addIterator(std::move(rsRandCursor));

    // If the incoming operation is sharded, use the CSS to infer the filtering metadata for the
    // collection, otherwise treat it as unsharded
    auto collectionFilter =
        CollectionShardingState::get(opCtx, coll->ns())
            ->getOwnershipFilter(
                opCtx, CollectionShardingState::OrphanCleanupPolicy::kDisallowOrphanCleanup);

    TrialStage* trialStage = nullptr;

    // Because 'numRecords' includes orphan documents, our initial decision to optimize the $sample
    // cursor may have been mistaken. For sharded collections, build a TRIAL plan that will switch
    // to a collection scan if the ratio of orphaned to owned documents encountered over the first
    // 100 works() is such that we would have chosen not to optimize.
    static const size_t kMaxPresampleSize = 100;
    if (collectionFilter.isSharded() && !expCtx->ns.isTimeseriesBucketsCollection()) {
        // The ratio of owned to orphaned documents must be at least equal to the ratio between the
        // requested sampleSize and the maximum permitted sampleSize for the original constraints to
        // be satisfied. For instance, if there are 200 documents and the sampleSize is 5, then at
        // least (5 / (200*0.05)) = (5/10) = 50% of those documents must be owned. If less than 5%
        // of the documents in the collection are owned, we default to the backup plan.
        const auto minAdvancedToWorkRatio = std::max(
            sampleSize / (numRecords * kMaxSampleRatioForRandCursor), kMaxSampleRatioForRandCursor);
        // The trial plan is SHARDING_FILTER-MULTI_ITERATOR.
        auto randomCursorPlan = std::make_unique<ShardFilterStage>(
            expCtx.get(), collectionFilter, ws.get(), std::move(root));
        // The backup plan is SHARDING_FILTER-COLLSCAN.
        std::unique_ptr<PlanStage> collScanPlan = std::make_unique<CollectionScan>(
            expCtx.get(), coll, CollectionScanParams{}, ws.get(), nullptr);
        collScanPlan = std::make_unique<ShardFilterStage>(
            expCtx.get(), collectionFilter, ws.get(), std::move(collScanPlan));
        // Place a TRIAL stage at the root of the plan tree, and pass it the trial and backup plans.
        root = std::make_unique<TrialStage>(expCtx.get(),
                                            ws.get(),
                                            std::move(randomCursorPlan),
                                            std::move(collScanPlan),
                                            kMaxPresampleSize,
                                            minAdvancedToWorkRatio);
        trialStage = static_cast<TrialStage*>(root.get());
    } else if (expCtx->ns.isTimeseriesBucketsCollection()) {
        // We can't take ARHASH optimization path for a direct $sample on the system.buckets
        // collection because data is in compressed form. If we did have a direct $sample on the
        // system.buckets collection, then the 'bucketUnpacker' would not be set up properly. We
        // also should bail out early if a $sample is made against a time series collection that is
        // empty. If we don't the 'minAdvancedToWorkRatio' can be nan/-nan depending on the
        // architecture.
        if (!(bucketUnpacker && numRecords)) {
            return std::pair{nullptr, false};
        }

        // Use a 'TrialStage' to run a trial between 'SampleFromTimeseriesBucket' and
        // 'UnpackTimeseriesBucket' with $sample left in the pipeline in-place. If the buckets are
        // not sufficiently full, or the 'SampleFromTimeseriesBucket' plan draws too many
        // duplicates, then we will fall back to the 'TrialStage' backup plan. This backup plan uses
        // the top-k sort sampling approach.
        //
        // Suppose the 'gTimeseriesBucketMaxCount' is 1000, but each bucket only contains 500
        // documents on average. The observed trial advanced/work ratio approximates the average
        // bucket fullness, noted here as "abf". In this example, abf = 500 / 1000 = 0.5.
        // Experiments have shown that the optimized 'SampleFromTimeseriesBucket' algorithm performs
        // better than backup plan when
        //
        //     sampleSize < 0.02 * abf * numRecords * gTimeseriesBucketMaxCount
        //
        //  This inequality can be rewritten as
        //
        //     abf > sampleSize / (0.02 * numRecords * gTimeseriesBucketMaxCount)
        //
        // Therefore, if the advanced/work ratio exceeds this threshold, we will use the
        // 'SampleFromTimeseriesBucket' plan. Note that as the sample size requested by the user
        // becomes larger with respect to the number of buckets, we require a higher advanced/work
        // ratio in order to justify using 'SampleFromTimeseriesBucket'.
        //
        // Additionally, we require the 'TrialStage' to approximate the abf as at least 0.25. When
        // buckets are mostly empty, the 'SampleFromTimeseriesBucket' will be inefficient due to a
        // lot of sampling "misses".
        static const auto kCoefficient = 0.02;
        static const auto kMinBucketFullness = 0.25;
        const auto minAdvancedToWorkRatio = std::max(
            std::min(sampleSize / (kCoefficient * numRecords * gTimeseriesBucketMaxCount), 1.0),
            kMinBucketFullness);

        auto arhashPlan = std::make_unique<SampleFromTimeseriesBucket>(
            expCtx.get(),
            ws.get(),
            std::move(root),
            *bucketUnpacker,
            // By using a quantity slightly higher than 'kMaxPresampleSize', we ensure that the
            // 'SampleFromTimeseriesBucket' stage won't fail due to too many consecutive sampling
            // attempts during the 'TrialStage's trial period.
            kMaxPresampleSize + 5,
            sampleSize,
            gTimeseriesBucketMaxCount);

        std::unique_ptr<PlanStage> collScanPlan = std::make_unique<CollectionScan>(
            expCtx.get(), coll, CollectionScanParams{}, ws.get(), nullptr);

        auto topkSortPlan = std::make_unique<UnpackTimeseriesBucket>(
            expCtx.get(), ws.get(), std::move(collScanPlan), *bucketUnpacker);

        root = std::make_unique<TrialStage>(expCtx.get(),
                                            ws.get(),
                                            std::move(arhashPlan),
                                            std::move(topkSortPlan),
                                            kMaxPresampleSize,
                                            minAdvancedToWorkRatio);
        trialStage = static_cast<TrialStage*>(root.get());
    }

    auto execStatus = plan_executor_factory::make(expCtx,
                                                  std::move(ws),
                                                  std::move(root),
                                                  &coll,
                                                  opCtx->inMultiDocumentTransaction()
                                                      ? PlanYieldPolicy::YieldPolicy::INTERRUPT_ONLY
                                                      : PlanYieldPolicy::YieldPolicy::YIELD_AUTO,
                                                  QueryPlannerParams::RETURN_OWNED_DATA);
    if (!execStatus.isOK()) {
        return execStatus.getStatus();
    }

    // For sharded collections, the root of the plan tree is a TrialStage that may have chosen
    // either a random-sampling cursor trial plan or a COLLSCAN backup plan. We can only optimize
    // the $sample aggregation stage if the trial plan was chosen.
    return std::pair{std::move(execStatus.getValue()),
                     !trialStage || !trialStage->pickedBackupPlan()};
}