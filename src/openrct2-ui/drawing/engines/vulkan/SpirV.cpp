#ifndef DISABLE_VULKAN
    #include "SpirV.h"
    
    #include <openrct2/Context.h>
    #include <openrct2/PlatformEnvironment.h>
    #include <openrct2/core/FileStream.h>
    #include <openrct2/core/Path.hpp>

using namespace std;

vector<uint32_t> OpenRCT2::Ui::Vulkan::ReadSpirVFile(const string& filename)
{
    auto& env = OpenRCT2::GetContext()->GetPlatformEnvironment();
    auto shadersPath = env.GetDirectoryPath(OpenRCT2::DirBase::openrct2, OpenRCT2::DirId::shaders);

    auto path = OpenRCT2::Path::Combine(shadersPath, filename);

    auto fs = OpenRCT2::FileStream(path, OpenRCT2::FileMode::open);

    uint64_t fileLength = fs.GetLength();

    // limit to 1MB for now
    if (fileLength > (1 << 20))
    {
        throw IOException("Spir-V shader file too large");
    }

    if (fileLength % sizeof(uint32_t) != 0)
    {
        throw IOException("Spir-V shader file is not in correct format, only glslc outputs are supported");
    }

    auto fileData = std::vector<uint32_t>(fileLength / sizeof(uint32_t), 0);
    fs.Read(static_cast<void*>(fileData.data()), fileLength);
    return fileData;
}
#endif
