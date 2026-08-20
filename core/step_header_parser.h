#pragma once
#include <string>
#include <vector>

// Document-level provenance metadata, read from the STEP file's HEADER;
// section. Pure text parsing — no OCCT dependency, so unlike the entity
// extraction below, this can't be broken by an OCCT version difference.
struct DocumentMetadata {
    std::string originalFileName;
    std::string timestamp;
    std::string author;
    std::string organization;
    std::string originatingSystem;    // e.g. "CATIA Version 5-6 Release 2017 SP6 HF31"
    std::string preprocessorVersion;  // e.g. "CATIA V5 STEP AP242"
    std::string schema;               // e.g. "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF"
    std::vector<std::string> descriptionNotes; // FILE_DESCRIPTION strings — often list
                                                 // which CAx-IF recommended practices the
                                                 // exporter used (validation props, PMI, etc.)
};

class StepHeaderParser {
public:
    static DocumentMetadata Parse(const std::string& stepFilePath);
};