cmake_minimum_required(VERSION 3.20.5)
set(CMAKE_CXX_COMPILER icpx)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
project(impl LANGUAGES CXX VERSION 23.06)


find_package(IntelDPCPP REQUIRED)

####################################################################
#
#   build options
#     you can change it externally with -D<name>=<value>, The default value is:
#       Global：
#         CMAKE_BUILD_TYPE        = Release
#         CMAKE_INSTALL_PREFIX    = /usr/
#         VTUNEINFO               = OFF
#
#      GPU:
#         AOT_FOR_GPU             = OFF
#         AOT_GPU_DEVICE_ARCH     = dg2
#
#
######################################################################
# build option
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

#set(CMAKE_INSTALL_PREFIX /usr/local)
option(VTUNEINFO "add vtune debug info for profiling, force build with RelWithDebInfo" OFF)

if(VTUNEINFO)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  set(VTUNE_BUILD_FLAG "-gline-tables-only;-fdebug-info-for-profiling")
endif()
########################################################################
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "vtune info: ${VTUNEINFO}")

set(MYPROJECT_OUTPATH ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${MYPROJECT_OUTPATH}) 
set(LIBRARY_OUTPUT_PATH ${MYPROJECT_OUTPATH}/src) 

set(LIB_NAME    Impl)
set(TARGET_CP   composition)
set(TARGET_AB   alphablending)
set(TARGET_MV   multiview)
set(TARGET_RP   replay)
set(TARGET_CV   convert)
set(TARGET_CSC  csc)
set(TARGET_RS   resize)


#remove icpx debug remark
add_compile_options($<$<CONFIG:Debug>:-Rno-debug-disables-optimization>)
add_link_options($<$<CONFIG:Debug>:-Rno-debug-disables-optimization>)


add_subdirectory(./src)
add_subdirectory(./samples)

######################################################
#install 
include(CMakePackageConfigHelpers)

install(TARGETS ${LIB_NAME}
        EXPORT MyMathTargets
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
        )

install(EXPORT MyMathTargets
        FILE ImplTargets.cmake
        NAMESPACE Impl::
        DESTINATION lib/cmake/Impl
        )

configure_package_config_file(${CMAKE_SOURCE_DIR}/config/ImplConfig.cmake.in ImplConfig.cmake
                                INSTALL_DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/Impl)

write_basic_package_version_file(
        ImplConfigVersion.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY ExactVersion
        )

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ImplConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/ImplConfigVersion.cmake"
        DESTINATION lib/cmake/Impl
        )

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "impl")
set(CPACK_DEBIAN_PACKAGE_NAME "impl")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_PACKAGE_CONTACT "kelvin.hu@intel.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "kelvin.hu@intel.com")
include(CPack)
