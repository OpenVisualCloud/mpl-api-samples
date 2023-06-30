##################################
#define options
option(AOT_FOR_GPU "Ahead-Of-Time Compilation, only for GPU" OFF)
set(AOT_GPU_DEVICE_ARCH dg2 CACHE STRING "GPU device architecture parameters passed to OCLOC for AOT")

#################################
#config
execute_process(COMMAND sycl-ls 
                COMMAND grep "Graphics"
                OUTPUT_VARIABLE FIND_GPU_DEVICE)
if(NOT FIND_GPU_DEVICE)
  set(AOT_FOR_GPU OFF)
  message(STATUS "Cannot find a GPU device, Turn off Ahead-of-Time compilation for GPU")
endif()

################################
#message
if(FIND_GPU_DEVICE)
  message(STATUS "Found GPU device enabled for sycl")
endif()
message(STATUS "AOT for GPU: ${AOT_FOR_GPU}")
if(AOT_FOR_GPU)
  message(STATUS "GPU device architecture parameters for AOT: ${AOT_GPU_DEVICE_ARCH}")
endif()

###############################
#build
set(AOT_COMPILE_FLAG -fsycl-targets=spir64_gen)
set(AOT_LINK_FLAG -fsycl-targets=spir64_gen -Xsycl-target-backend=spir64_gen "-device ${AOT_GPU_DEVICE_ARCH}")

file(GLOB all_files
    "*.h"
    "*.cpp")

add_library(${LIB_NAME} SHARED ${all_files})

target_include_directories(${LIB_NAME} PUBLIC  
$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api> 
$<INSTALL_INTERFACE:include>
)

set_target_properties(${LIB_NAME} PROPERTIES
    PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/api/impl_api.h
    INTERPROCEDURAL_OPTIMIZATION TRUE                                             #[[enable IPO]]
)

target_compile_options(${LIB_NAME} PRIVATE -sycl-std=2020                         #[[basic]]
    -fvisibility-inlines-hidden -fvisibility=hidden                               #[[visibility hidden]]
    -Wall -Wextra -Wdocumentation -Wdocumentation-pedantic -pedantic-errors       #[[warning]]
    -fp-model=fast  -fp-speculation=fast -fsycl-dead-args-optimization            #[[optimization]] 
    $<$<BOOL:${AOT_FOR_GPU}>:${AOT_COMPILE_FLAG}>                                 #[[Ahead of Time Compilation]] 
    "$<$<BOOL:${VTUNEINFO}>:${VTUNE_BUILD_FLAG}>"                                 #[[add vtune debug info]]
)

target_compile_definitions(${LIB_NAME} PRIVATE 
    $<$<CONFIG:Debug>:DEBUG>
    $<$<BOOL:${AOT_FOR_GPU}>:ONLY_FOR_GPU>
)

target_link_options(${LIB_NAME} PRIVATE $<$<BOOL:${AOT_FOR_GPU}>:${AOT_LINK_FLAG}>)




