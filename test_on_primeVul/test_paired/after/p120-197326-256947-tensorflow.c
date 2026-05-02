  static Status ParseEquation(const string& equation,
                              OperandLabels* input_labels,
                              Labels* output_labels,
                              std::vector<DimensionType>* label_types,
                              OperandLabelCounts* input_label_counts,
                              LabelCounts* output_label_counts,
                              gtl::InlinedVector<bool, 2>* input_has_ellipsis,
                              bool* output_has_ellipsis) {
    gtl::InlinedVector<string, 2> input_str;
    string output_str;
    TF_RETURN_IF_ERROR(ParseEinsumEquation(equation, &input_str, &output_str));

    // Temporary map from single character labels to (consecutive) integer
    // labels.
    absl::flat_hash_map<char, int> label_mapping;
    int num_inputs = input_str.size();
    input_labels->resize(num_inputs);

    // Map from single characters to integer labels.
    for (int i = 0; i < num_inputs; ++i) {
      MapToLabels(input_str[i], &input_labels->at(i), &label_mapping);
    }
    MapToLabels(output_str, output_labels, &label_mapping);

    // Compute counts for input and output labels.
    int num_labels = label_mapping.size();
    input_label_counts->resize(num_inputs);
    input_has_ellipsis->resize(num_inputs);
    for (int i = 0; i < num_inputs; ++i) {
      input_label_counts->at(i).resize(num_labels);
      input_has_ellipsis->at(i) = false;
      for (const int label : input_labels->at(i)) {
        if (label != kEllipsisLabel)
          input_label_counts->at(i)[label] += 1;
        else
          input_has_ellipsis->at(i) = true;
      }
    }
    output_label_counts->resize(num_labels);
    *output_has_ellipsis = false;
    for (const int label : *output_labels) {
      if (label != kEllipsisLabel)
        output_label_counts->at(label) += 1;
      else
        *output_has_ellipsis = true;
    }

    // Map each label to a unique DimensionType.
    label_types->resize(num_labels);
    for (int label = 0; label < num_labels; ++label) {
      if (label == kEllipsisLabel) continue;
      bool removed = (*output_label_counts)[label] == 0;
      bool unique = num_inputs == 1 || (*input_label_counts)[0][label] == 0 ||
                    (*input_label_counts)[1][label] == 0;
      (*label_types)[label] = GetDimensionType(removed, unique);
    }
    return Status::OK();
  }