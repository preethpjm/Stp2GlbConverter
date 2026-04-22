#pragma once
#include "core/data_model.h"
#include <string>
#include <array>

class GlbExporter {
public:
    static bool Export(const ModelData& model, const std::string& glbPath);

private:
    static void ConvertTransform(const gp_Trsf& trsf, std::array<double, 16>& matrix);
};