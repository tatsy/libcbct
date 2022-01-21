#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
#define LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H

#include "BaseImporter.h"

class LIBCBCT_API ImageSequenceImporter : public BaseImporter {
public:
    ImageSequenceImporter() = default;
    virtual ~ImageSequenceImporter() = default;

    void read(const std::string &format, FloatVolume &sinogram) const override;
};

#endif  // LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
