#
# - Find vulkan headers
#
#  VULKAN_HEADERS_INCLUDE_DIRS - where to find vulkan headers.
#  VULKAN_HEADERS_FOUND        - True if vulkan headers is found.

find_package(PkgConfig QUIET)
pkg_search_module(VULKAN_HEADERS QUIET vulkan-headers)

find_path(VULKAN_HEADERS_INCLUDE_DIR
  NAMES
    vulkan/vulkan_raii.hpp
  HINTS
    ${VULKAN_HEADERS_INCLUDE_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VULKAN_HEADERS
  REQUIRED_VARS   VULKAN_HEADERS_LIBRARY VULKAN_HEADERS_INCLUDE_DIR
  VERSION_VAR     VULKAN_HEADERS_VERSION)

if(VULKAN_HEADERS_FOUND)
  set(VULKAN_HEADERS_INCLUDE_DIRS ${VULKAN_HEADERS_INCLUDE_DIR})
else()
  set(VULKAN_HEADERS_INCLUDE_DIRS)
endif()

mark_as_advanced(VULKAN_HEADERS_INCLUDE_DIRS)
