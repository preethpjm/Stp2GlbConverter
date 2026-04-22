#pragma once
#include "core/data_model.h"
#include <string>

class JsonExporter {
public:
    static bool Export(const ModelData& model, const std::string& jsonPath);
};