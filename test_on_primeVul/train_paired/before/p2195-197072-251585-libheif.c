Error HeifContext::interpret_heif_file()
{
  m_all_images.clear();
  m_top_level_images.clear();
  m_primary_image.reset();


  // --- reference all non-hidden images

  std::vector<heif_item_id> image_IDs = m_heif_file->get_item_IDs();

  bool primary_is_grid = false;
  for (heif_item_id id : image_IDs) {
    auto infe_box = m_heif_file->get_infe_box(id);
    if (!infe_box) {
      // TODO(farindk): Should we return an error instead of skipping the invalid id?
      continue;
    }

    if (item_type_is_image(infe_box->get_item_type())) {
      auto image = std::make_shared<Image>(this, id);
      m_all_images.insert(std::make_pair(id, image));

      if (!infe_box->is_hidden_item()) {
        if (id==m_heif_file->get_primary_image_ID()) {
          image->set_primary(true);
          m_primary_image = image;
          primary_is_grid = infe_box->get_item_type() == "grid";
        }

        m_top_level_images.push_back(image);
      }
    }
  }


  if (!m_primary_image) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Nonexisting_item_referenced,
                 "'pitm' box references a non-existing image");
  }


  // --- remove thumbnails from top-level images and assign to their respective image

  auto iref_box = m_heif_file->get_iref_box();
  if (iref_box) {
    // m_top_level_images.clear();

    for (auto& pair : m_all_images) {
      auto& image = pair.second;

      std::vector<Box_iref::Reference> references = iref_box->get_references_from(image->get_id());

      for (const Box_iref::Reference& ref : references) {
        uint32_t type = ref.header.get_short_type();

        if (type==fourcc("thmb")) {
          // --- this is a thumbnail image, attach to the main image

          std::vector<heif_item_id> refs = ref.to_item_ID;
          if (refs.size() != 1) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Unspecified,
                         "Too many thumbnail references");
          }

          image->set_is_thumbnail_of(refs[0]);

          auto master_iter = m_all_images.find(refs[0]);
          if (master_iter == m_all_images.end()) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Nonexisting_item_referenced,
                         "Thumbnail references a non-existing image");
          }

          if (master_iter->second->is_thumbnail()) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Nonexisting_item_referenced,
                         "Thumbnail references another thumbnail");
          }

          if (image.get() == master_iter->second.get()) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Nonexisting_item_referenced,
                         "Recursive thumbnail image detected");
          }
          master_iter->second->add_thumbnail(image);

          remove_top_level_image(image);
        }
        else if (type==fourcc("auxl")) {

          // --- this is an auxiliary image
          //     check whether it is an alpha channel and attach to the main image if yes

          std::vector<Box_ipco::Property> properties;
          Error err = m_heif_file->get_properties(image->get_id(), properties);
          if (err) {
            return err;
          }

          std::shared_ptr<Box_auxC> auxC_property;
          for (const auto& property : properties) {
            auto auxC = std::dynamic_pointer_cast<Box_auxC>(property.property);
            if (auxC) {
              auxC_property = auxC;
            }
          }

          if (!auxC_property) {
            std::stringstream sstr;
            sstr << "No auxC property for image " << image->get_id();
            return Error(heif_error_Invalid_input,
                         heif_suberror_Auxiliary_image_type_unspecified,
                         sstr.str());
          }

          std::vector<heif_item_id> refs = ref.to_item_ID;
          if (refs.size() != 1) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Unspecified,
                         "Too many auxiliary image references");
          }


          // alpha channel

          if (auxC_property->get_aux_type() == "urn:mpeg:avc:2015:auxid:1" ||
              auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:1") {
            image->set_is_alpha_channel_of(refs[0]);

            auto master_iter = m_all_images.find(refs[0]);
            if (image.get() == master_iter->second.get()) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Nonexisting_item_referenced,
                           "Recursive alpha image detected");
            }
            master_iter->second->set_alpha_channel(image);
          }


          // depth channel

          if (auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:2") {
            image->set_is_depth_channel_of(refs[0]);

            auto master_iter = m_all_images.find(refs[0]);
            if (image.get() == master_iter->second.get()) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Nonexisting_item_referenced,
                           "Recursive depth image detected");
            }
            master_iter->second->set_depth_channel(image);

            auto subtypes = auxC_property->get_subtypes();

            std::vector<std::shared_ptr<SEIMessage>> sei_messages;
            Error err = decode_hevc_aux_sei_messages(subtypes, sei_messages);

            for (auto& msg : sei_messages) {
              auto depth_msg = std::dynamic_pointer_cast<SEIMessage_depth_representation_info>(msg);
              if (depth_msg) {
                image->set_depth_representation_info(*depth_msg);
              }
            }
          }

          remove_top_level_image(image);
        }
        else {
          // 'image' is a normal image, keep it as a top-level image
        }
      }
    }
  }


  // --- check that HEVC images have an hvcC property

  for (auto& pair : m_all_images) {
    auto& image = pair.second;

    std::shared_ptr<Box_infe> infe = m_heif_file->get_infe_box(image->get_id());
    if (infe->get_item_type() == "hvc1") {

      auto ipma = m_heif_file->get_ipma_box();
      auto ipco = m_heif_file->get_ipco_box();

      if (!ipco->get_property_for_item_ID(image->get_id(), ipma, fourcc("hvcC"))) {
        return Error(heif_error_Invalid_input,
                     heif_suberror_No_hvcC_box,
                     "No hvcC property in hvc1 type image");
      }
    }
  }


  // --- read through properties for each image and extract image resolutions

  for (auto& pair : m_all_images) {
    auto& image = pair.second;

    std::vector<Box_ipco::Property> properties;

    Error err = m_heif_file->get_properties(pair.first, properties);
    if (err) {
      return err;
    }

    bool ispe_read = false;
    bool primary_colr_set = false;
    for (const auto& prop : properties) {
      auto ispe = std::dynamic_pointer_cast<Box_ispe>(prop.property);
      if (ispe) {
        uint32_t width = ispe->get_width();
        uint32_t height = ispe->get_height();


        // --- check whether the image size is "too large"

        if (width  >= static_cast<uint32_t>(MAX_IMAGE_WIDTH) ||
            height >= static_cast<uint32_t>(MAX_IMAGE_HEIGHT)) {
          std::stringstream sstr;
          sstr << "Image size " << width << "x" << height << " exceeds the maximum image size "
               << MAX_IMAGE_WIDTH << "x" << MAX_IMAGE_HEIGHT << "\n";

          return Error(heif_error_Memory_allocation_error,
                       heif_suberror_Security_limit_exceeded,
                       sstr.str());
        }

        image->set_resolution(width, height);
        image->set_ispe_resolution(width, height);
        ispe_read = true;
      }

      if (ispe_read) {
        auto clap = std::dynamic_pointer_cast<Box_clap>(prop.property);
        if (clap) {
          image->set_resolution( clap->get_width_rounded(),
                                 clap->get_height_rounded() );
        }

        auto irot = std::dynamic_pointer_cast<Box_irot>(prop.property);
        if (irot) {
          if (irot->get_rotation()==90 ||
              irot->get_rotation()==270) {
            // swap width and height
            image->set_resolution( image->get_height(),
                                   image->get_width() );
          }
        }
      }

      auto colr = std::dynamic_pointer_cast<Box_colr>(prop.property);
      if (colr) {
        auto profile = colr->get_color_profile();

        image->set_color_profile(profile);

        // if this is a grid item we assign the first one's color profile
        // to the main image which is supposed to be a grid

        // TODO: this condition is not correct. It would also classify a secondary image as a 'grid item'.
        // We have to set the grid-image color profile in another way...
        const bool is_grid_item = !image->is_primary() && !image->is_alpha_channel() && !image->is_depth_channel();

        if (primary_is_grid &&
            !primary_colr_set &&
            is_grid_item) {
          m_primary_image->set_color_profile(profile);
          primary_colr_set = true;
        }
      }
    }
  }


  // --- read metadata and assign to image

  for (heif_item_id id : image_IDs) {
    std::string item_type    = m_heif_file->get_item_type(id);
    std::string content_type = m_heif_file->get_content_type(id);
    if (item_type == "Exif" ||
        (item_type=="mime" && content_type=="application/rdf+xml")) {
      std::shared_ptr<ImageMetadata> metadata = std::make_shared<ImageMetadata>();
      metadata->item_id = id;
      metadata->item_type = item_type;
      metadata->content_type = content_type;

      Error err = m_heif_file->get_compressed_image_data(id, &(metadata->m_data));
      if (err) {
        return err;
      }

      //std::cerr.write((const char*)data.data(), data.size());


      // --- assign metadata to the image

      if (iref_box) {
        std::vector<Box_iref::Reference> references = iref_box->get_references_from(id);
        for (const auto& ref : references) {
          if (ref.header.get_short_type() == fourcc("cdsc")) {
            std::vector<uint32_t> refs = ref.to_item_ID;
            if (refs.size() != 1) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Unspecified,
                           "Exif data not correctly assigned to image");
            }

            uint32_t exif_image_id = refs[0];
            auto img_iter = m_all_images.find(exif_image_id);
            if (img_iter == m_all_images.end()) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Nonexisting_item_referenced,
                           "Exif data assigned to non-existing image");
            }

            img_iter->second->add_metadata(metadata);
          }
        }
      }
    }
  }

  return Error::Ok;
}