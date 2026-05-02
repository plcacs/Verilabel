    ExifData::const_iterator isoSpeed(const ExifData& ed)
    {
        static const char* keys[] = {
            "Exif.Photo.ISOSpeedRatings",
            "Exif.Image.ISOSpeedRatings",
            "Exif.CanonSi.ISOSpeed",
            "Exif.CanonCs.ISOSpeed",
            "Exif.Nikon1.ISOSpeed",
            "Exif.Nikon2.ISOSpeed",
            "Exif.Nikon3.ISOSpeed",
            "Exif.NikonIi.ISO",
            "Exif.NikonIi.ISO2",
            "Exif.MinoltaCsNew.ISOSetting",
            "Exif.MinoltaCsOld.ISOSetting",
            "Exif.MinoltaCs5D.ISOSpeed",
            "Exif.MinoltaCs7D.ISOSpeed",
            "Exif.Sony1Cs.ISOSetting",
            "Exif.Sony2Cs.ISOSetting",
            "Exif.Sony1Cs2.ISOSetting",
            "Exif.Sony2Cs2.ISOSetting",
            "Exif.Sony1MltCsA100.ISOSetting",
            "Exif.Pentax.ISO",
            "Exif.PentaxDng.ISO",
            "Exif.Olympus.ISOSpeed",
            "Exif.Samsung2.ISO",
            "Exif.Casio.ISO",
            "Exif.Casio2.ISO",
            "Exif.Casio2.ISOSpeed"
        };

        struct SensKeyNameList {
            int count;
            const char* keys[3];
        };

        // covers Exif.Phot.SensitivityType values 1-7. Note that SOS, REI and
        // ISO do differ in their meaning. Values coming first in a list (and
        // existing as a tag) are picked up first and used as the "ISO" value.
        static const SensKeyNameList sensitivityKey[] = {
            { 1, { "Exif.Photo.StandardOutputSensitivity" }},
            { 1, { "Exif.Photo.RecommendedExposureIndex" }},
            { 1, { "Exif.Photo.ISOSpeed" }},
            { 2, { "Exif.Photo.RecommendedExposureIndex", "Exif.Photo.StandardOutputSensitivity" }},
            { 2, { "Exif.Photo.ISOSpeed", "Exif.Photo.StandardOutputSensitivity" }},
            { 2, { "Exif.Photo.ISOSpeed", "Exif.Photo.RecommendedExposureIndex" }},
            { 3, { "Exif.Photo.ISOSpeed", "Exif.Photo.RecommendedExposureIndex", "Exif.Photo.StandardOutputSensitivity" }}
        };

        static const char* sensitivityType[] = {
            "Exif.Photo.SensitivityType"
        };

        // Find the first ISO value which is not "0"
        const int cnt = EXV_COUNTOF(keys);
        ExifData::const_iterator md = ed.end();
        long iso_val = -1;
        for (int idx = 0; idx < cnt; ) {
            md = findMetadatum(ed, keys + idx, cnt - idx);
            if (md == ed.end()) break;
            std::ostringstream os;
            md->write(os, &ed);
            bool ok = false;
            iso_val = parseLong(os.str(), ok);
            if (ok && iso_val > 0) break;
            while (strcmp(keys[idx++], md->key().c_str()) != 0 && idx < cnt) {}
            md = ed.end();
        }

        // there is either a possible ISO "overflow" or no legacy
        // ISO tag at all. Check for SensitivityType tag and the referenced
        // ISO value (see EXIF 2.3 Annex G)
        long iso_tmp_val = -1;
        while (iso_tmp_val == -1 && (iso_val == 65535 || md == ed.end())) {
            ExifData::const_iterator md_st = findMetadatum(ed, sensitivityType, 1);
            // no SensitivityType? exit with existing data
            if (md_st == ed.end())
                break;
            // otherwise pick up actual value and grab value accordingly
            std::ostringstream os;
            md_st->write(os, &ed);
            bool ok = false;
            long st_val = parseLong(os.str(), ok);
            // SensivityType out of range or cannot be parsed properly
            if (!ok || st_val < 1 || st_val > 7)
                break;
            // pick up list of ISO tags, and check for at least one of
            // them available.
            const SensKeyNameList *sensKeys = &sensitivityKey[st_val - 1];
            md_st = ed.end();
            for (int idx = 0; idx < sensKeys->count; md_st = ed.end()) {
                md_st = findMetadatum(ed, const_cast<const char**>(sensKeys->keys), sensKeys->count);
                if (md_st == ed.end())
                    break;
                std::ostringstream os_iso;
                md_st->write(os_iso, &ed);
                ok = false;
                iso_tmp_val = parseLong(os_iso.str(), ok);
                // something wrong with the value
                if (ok || iso_tmp_val > 0) {
                    md = md_st;
                    break;
                }
                while (strcmp(sensKeys->keys[idx++], md_st->key().c_str()) != 0 && idx < cnt) {}
            }
            break;
        }

        return md;
    }