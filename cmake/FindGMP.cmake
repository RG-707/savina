# Try to find GMP headers and libraries
#
# Use this module as follows:
#
#     find_package(GMP [REQUIRED])
#
# Variables used by this module (they can change the default behavior and need
# to be set before calling find_package):
#
#  GMP_ROOT_DIR Set this variable either to an installation prefix or to
#                 the GMP root directory where to look for the library.
#
# Variables defined by this module:
#
#  GMP_FOUND        Found library and header
#  GMP_LIBRARY      Path to library
#  GMP_INCLUDE_DIR  Include path for headers
#  GMPXX_LIBRARY      Path to c++ library
#  GMPXX_INCLUDE_DIR  Include path for c++ headers
#

find_library(GMP_LIBRARY
  NAMES
    gmp
  HINTS 
    ${GMP_ROOT_DIR}/lib
    ${GMP_ROOT_DIR}/.libs
    ${GMP_ROOT_DIR}/
)

find_library(GMPXX_LIBRARY
  NAMES
    gmpxx
  HINTS 
    ${GMP_ROOT_DIR}/lib
    ${GMP_ROOT_DIR}/.libs
    ${GMP_ROOT_DIR}/
)

find_path(GMP_INCLUDE_DIR
  NAMES
    "gmp.h"
  HINTS
    ${GMP_ROOT_DIR}/include
    ${GMP_ROOT_DIR}/lib/
)

find_path(GMPXX_INCLUDE_DIR
  NAMES
    "gmpxx.h"
  HINTS
    ${GMP_ROOT_DIR}/include
    ${GMP_ROOT_DIR}/lib/
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  GMP
  DEFAULT_MSG
  GMP_LIBRARY
  GMPXX_LIBRARY
  GMP_INCLUDE_DIR
  GMPXX_INCLUDE_DIR
)

mark_as_advanced(
  GMP_ROOT_DIR
  GMP_LIBRARY
  GMPXX_LIBRARY
  GMP_INCLUDE_DIR
  GMPXX_INCLUDE_DIR
)
