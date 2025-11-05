#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
#define LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H

#include "BaseImporter.h"

class LIBCBCT_API ImageSequenceImporter : public BaseImporter {
public:
    explicit ImageSequenceImporter(const std::string &folder, const std::string &extension, bool reverseOrder = false)
        : BaseImporter{}
        , folder{ folder }
        , extension{ extension }
        , reverseOrder{ reverseOrder } {
    }
    virtual ~ImageSequenceImporter() = default;

    VolumeF32 read() const override;

private:
    std::string folder;
    std::string extension;
    bool reverseOrder = false;
};

#endif  // LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
