#include "ImageId.hpp"

#include <memory>

std::size_t ImageIdHasher::operator()(const ImageId& k) const
{
    uint64_t imageIdToHash;
    memcpy(&imageIdToHash, &k, sizeof(imageIdToHash));
    return std::hash<uint64_t>{}(imageIdToHash);
}
