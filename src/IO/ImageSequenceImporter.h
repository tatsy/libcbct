#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
#define LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H

#include "BaseImporter.h"

class LIBCBCT_API ImageSequenceImporter : public BaseImporter {
public:
    explicit ImageSequenceImporter(bool reverseOrder = false)
        : BaseImporter{}
        , reverseOrder{ reverseOrder } {
    }
    virtual ~ImageSequenceImporter() = default;

    void read(const std::string &format, FloatVolume &sinogram) const override;

private:
    bool reverseOrder = false;
};

#endif  // LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
