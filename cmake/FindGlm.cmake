#
# - Find glm libraries
#
#  GLM_INCLUDE_DIRS - where to find speexdsp headers.
#  GLM_LIBRARIES    - List of libraries when using speexdsp.
#  GLM_FOUND        - True if speexdsp is found.

find_package(PkgConfig QUIET)
pkg_search_module(GLM QUIET glm)

find_path(GLM_INCLUDE_DIR
  NAMES
    glm/gl;m.hpp
  HINTS
    ${GLM_INCLUDE_DIRS}
)

find_library(GLM_LIBRARY
  NAMES
    glm
  HINTS
    ${GLM_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLM
  REQUIRED_VARS   GLM_LIBRARY GLM_INCLUDE_DIR
  VERSION_VAR     GLM_VERSION)

if(GLM_FOUND)
  set(GLM_LIBRARIES ${GLM_LIBRARY})
  set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})
else()
  set(GLM_LIBRARIES)
  set(GLM_INCLUDE_DIRS)
endif()

mark_as_advanced(GLM_LIBRARIES GLM_INCLUDE_DIRS)
