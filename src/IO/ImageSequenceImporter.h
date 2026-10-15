#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
#define LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H

#include <optional>

#include "BaseImporter.h"

namespace libcbct {

// Uniform full-turn acquisition. The first file defines angle zero.
struct ProjectionSequenceOptions {
    int numberOfProjections = 0;  // Effective count, excluding the duplicate endpoint.
    bool clockwise = false;
    bool isEndpointDuplicate = false;  // The final file repeats angle zero.
};

class LIBCBCT_API ImageSequenceImporter : public BaseImporter {
public:
    explicit ImageSequenceImporter(const std::string &folder, const std::string &extension, bool reverseOrder = false)
        : BaseImporter{}
        , folder{ folder }
        , extension{ extension }
        , reverseOrder{ reverseOrder } {
    }

    explicit ImageSequenceImporter(const std::string &folder, const std::string &extension,
                                   const ProjectionSequenceOptions &projections)
        : BaseImporter{}
        , folder{ folder }
        , extension{ extension }
        , projections{ projections } {
    }
    virtual ~ImageSequenceImporter() = default;

    VolumeF32 read() const override;

private:
    std::string folder;
    std::string extension;
    bool reverseOrder = false;
    std::optional<ProjectionSequenceOptions> projections;
};

}  // namespace libcbct

#endif  // LIBCBCT_IMAGE_SEQUENCE_IMPORTER_H
