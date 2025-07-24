#pragma once
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Vulkan
{
    std::vector<uint32_t> ReadSpirVFile(const std::string& filename);
}
