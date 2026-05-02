Status GetMatchingPaths(FileSystem* fs, Env* env, const string& pattern,
                        std::vector<string>* results) {
  // Check that `fs`, `env` and `results` are non-null.
  if (fs == nullptr || env == nullptr || results == nullptr) {
    return Status(tensorflow::error::INVALID_ARGUMENT,
                  "Filesystem calls GetMatchingPaths with nullptr arguments");
  }

  // By design, we don't match anything on empty pattern
  results->clear();
  if (pattern.empty()) {
    return Status::OK();
  }

  // The pattern can contain globbing characters at multiple levels, e.g.:
  //
  //   foo/ba?/baz/f*r
  //
  // To match the full pattern, we must match every prefix subpattern and then
  // operate on the children for each match. Thus, we separate all subpatterns
  // in the `dirs` vector below.
  std::vector<std::string> dirs = AllDirectoryPrefixes(pattern);

  // We can have patterns that have several parents where no globbing is being
  // done, for example, `foo/bar/baz/*`. We don't need to expand the directories
  // which don't contain the globbing characters.
  int matching_index = GetFirstGlobbingEntry(dirs);

  // If we don't have globbing characters in the pattern then it specifies a
  // path in the filesystem. We add it to the result set if it exists.
  if (matching_index == dirs.size()) {
    if (fs->FileExists(pattern).ok()) {
      results->emplace_back(pattern);
    }
    return Status::OK();
  }

  // To expand the globbing, we do a BFS from `dirs[matching_index-1]`.
  // At every step, we work on a pair `{dir, ix}` such that `dir` is a real
  // directory, `ix < dirs.size() - 1` and `dirs[ix+1]` is a globbing pattern.
  // To expand the pattern, we select from all the children of `dir` only those
  // that match against `dirs[ix+1]`.
  // If there are more entries in `dirs` after `dirs[ix+1]` this mean we have
  // more patterns to match. So, we add to the queue only those children that
  // are also directories, paired with `ix+1`.
  // If there are no more entries in `dirs`, we return all children as part of
  // the answer.
  // Since we can get into a combinatorial explosion issue (e.g., pattern
  // `/*/*/*`), we process the queue in parallel. Each parallel processing takes
  // elements from `expand_queue` and adds them to `next_expand_queue`, after
  // which we swap these two queues (similar to double buffering algorithms).
  // PRECONDITION: `IsGlobbingPattern(dirs[0]) == false`
  // PRECONDITION: `matching_index > 0`
  // INVARIANT: If `{d, ix}` is in queue, then `d` and `dirs[ix]` are at the
  //            same level in the filesystem tree.
  // INVARIANT: If `{d, _}` is in queue, then `IsGlobbingPattern(d) == false`.
  // INVARIANT: If `{d, _}` is in queue, then `d` is a real directory.
  // INVARIANT: If `{_, ix}` is in queue, then `ix < dirs.size() - 1`.
  // INVARIANT: If `{_, ix}` is in queue, `IsGlobbingPattern(dirs[ix + 1])`.
  std::deque<std::pair<string, int>> expand_queue;
  std::deque<std::pair<string, int>> next_expand_queue;
  expand_queue.emplace_back(dirs[matching_index - 1], matching_index - 1);

  // Adding to `result` or `new_expand_queue` need to be protected by mutexes
  // since there are multiple threads writing to these.
  mutex result_mutex;
  mutex queue_mutex;

  while (!expand_queue.empty()) {
    next_expand_queue.clear();

    // The work item for every item in `expand_queue`.
    // pattern, we process them in parallel.
    auto handle_level = [&fs, &results, &dirs, &expand_queue,
                         &next_expand_queue, &result_mutex,
                         &queue_mutex](int i) {
      // See invariants above, all of these are valid accesses.
      const auto& queue_item = expand_queue.at(i);
      const std::string& parent = queue_item.first;
      const int index = queue_item.second + 1;
      const std::string& match_pattern = dirs[index];

      // Get all children of `parent`. If this fails, return early.
      std::vector<std::string> children;
      Status s = fs->GetChildren(parent, &children);
      if (s.code() == tensorflow::error::PERMISSION_DENIED) {
        return;
      }

      // Also return early if we don't have any children
      if (children.empty()) {
        return;
      }

      // Since we can get extremely many children here and on some filesystems
      // `IsDirectory` is expensive, we process the children in parallel.
      // We also check that children match the pattern in parallel, for speedup.
      // We store the status of the match and `IsDirectory` in
      // `children_status` array, one element for each children.
      std::vector<Status> children_status(children.size());
      auto handle_children = [&fs, &match_pattern, &parent, &children,
                              &children_status](int j) {
        const std::string path = io::JoinPath(parent, children[j]);
        if (!fs->Match(path, match_pattern)) {
          children_status[j] =
              Status(tensorflow::error::CANCELLED, "Operation not needed");
        } else {
          children_status[j] = fs->IsDirectory(path);
        }
      };
      ForEach(0, children.size(), handle_children);

      // At this point, pairing `children` with `children_status` will tell us
      // if a children:
      //   * does not match the pattern
      //   * matches the pattern and is a directory
      //   * matches the pattern and is not a directory
      // We fully ignore the first case.
      // If we matched the last pattern (`index == dirs.size() - 1`) then all
      // remaining children get added to the result.
      // Otherwise, only the directories get added to the next queue.
      for (size_t j = 0; j < children.size(); j++) {
        if (children_status[j].code() == tensorflow::error::CANCELLED) {
          continue;
        }

        const std::string path = io::JoinPath(parent, children[j]);
        if (index == dirs.size() - 1) {
          mutex_lock l(result_mutex);
          results->emplace_back(path);
        } else if (children_status[j].ok()) {
          mutex_lock l(queue_mutex);
          next_expand_queue.emplace_back(path, index);
        }
      }
    };
    ForEach(0, expand_queue.size(), handle_level);

    // After evaluating one level, swap the "buffers"
    std::swap(expand_queue, next_expand_queue);
  }

  return Status::OK();
}