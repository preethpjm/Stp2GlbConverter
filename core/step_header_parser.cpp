#include "step_header_parser.h"
#include <fstream>
#include <regex>
#include <Utils/logger.h>

static std::vector<std::string> ExtractQuotedStrings(const std::string& s) {
    std::vector<std::string> out;
    std::regex quoted("'([^']*)'");
    auto begin = std::sregex_iterator(s.begin(), s.end(), quoted);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) out.push_back((*it)[1].str());
    return out;
}

DocumentMetadata StepHeaderParser::Parse(const std::string& stepFilePath) {
    DocumentMetadata meta;

    std::ifstream file(stepFilePath);
    if (!file.is_open()) {
        Logger::Warning("Could not open file for header parsing: " + stepFilePath);
        return meta;
    }

    std::string headerText, line;
    bool inHeader = false;
    size_t bytesRead = 0;
    const size_t MAX_HEADER_BYTES = 8192; // HEADER; is always small and near the top

    while (std::getline(file, line)) {
        bytesRead += line.size();
        if (line.find("HEADER;") != std::string::npos) { inHeader = true; continue; }
        if (inHeader) {
            headerText += line + "\n";
            if (line.find("ENDSEC;") != std::string::npos) break;
        }
        if (bytesRead > MAX_HEADER_BYTES) break;
    }

    // FILE_DESCRIPTION(('desc1','desc2',...),'impl_level');
    {
        std::smatch m;
        std::regex re("FILE_DESCRIPTION\\s*\\((.*?)\\)\\s*;", std::regex::icase);
        if (std::regex_search(headerText, m, re)) {
            meta.descriptionNotes = ExtractQuotedStrings(m[1].str());
        }
    }

    // FILE_NAME('name','timestamp',('author'),('org'),'system','preprocessor','auth_org');
    // NOTE: author/org are technically lists in STEP; this positional extraction
    // assumes the common single-author/single-org case (true for every file we've
    // tested against). A file with multiple listed authors would shift later fields.
    {
        std::smatch m;
        std::regex re("FILE_NAME\\s*\\((.*)\\)\\s*;", std::regex::icase);
        if (std::regex_search(headerText, m, re)) {
            auto fields = ExtractQuotedStrings(m[1].str());
            if (fields.size() > 0) meta.originalFileName  = fields[0];
            if (fields.size() > 1) meta.timestamp          = fields[1];
            if (fields.size() > 2) meta.author             = fields[2];
            if (fields.size() > 3) meta.organization       = fields[3];
            if (fields.size() > 4) meta.originatingSystem  = fields[4];
            if (fields.size() > 5) meta.preprocessorVersion = fields[5];
        }
    }

    // FILE_SCHEMA(('SCHEMA_NAME { ... }'));
    {
        std::smatch m;
        std::regex re("FILE_SCHEMA\\s*\\((.*?)\\)\\s*;", std::regex::icase);
        if (std::regex_search(headerText, m, re)) {
            auto fields = ExtractQuotedStrings(m[1].str());
            if (!fields.empty()) meta.schema = fields[0];
        }
    }

    Logger::Info("Header parsed — system: " + meta.originatingSystem +
                 ", org: " + meta.organization + ", schema: " + meta.schema);

    return meta;
}