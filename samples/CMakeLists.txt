
link_libraries( ${LIB_NAME} )
add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden 
-Wall -Wextra -Wdocumentation
"$<$<BOOL:${VTUNEINFO}>:${VTUNE_BUILD_FLAG}>"
)
#-Wdocumentation-pedantic -pedantic-errors
#TODO:

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)


add_executable(${TARGET_CP}     composition.cpp)
add_executable(${TARGET_AB}     alphablending.cpp)
add_executable(${TARGET_MV}     multiview.cpp)
add_executable(${TARGET_CSC}    csc.cpp)
add_executable(${TARGET_RS}     resize.cpp)


find_package(VPL REQUIRED)
set(LIBVA_SUPPORT
    ON
    CACHE BOOL "Enable hardware support.")
if(LIBVA_SUPPORT)
  find_package(PkgConfig REQUIRED)
  # note: pkg-config version for libva is *API* version
  pkg_check_modules(PKG_LIBVA libva>=1.2 libva-drm>=1.2)
  if(PKG_LIBVA_FOUND)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    add_executable(${TARGET_RP} replay.cpp)
    add_executable(${TARGET_CV} convert.cpp)

    target_compile_definitions(${TARGET_RP} PUBLIC -DLIBVA_SUPPORT)
    target_compile_definitions(${TARGET_CV} PUBLIC -DLIBVA_SUPPORT)
 
    target_link_libraries(${TARGET_RP} VPL::dispatcher ${PKG_LIBVA_LIBRARIES}
                          ${PKG_THREAD_LIBRARIES} ze_loader)
    target_link_libraries(${TARGET_CV} VPL::dispatcher ${PKG_LIBVA_LIBRARIES}
                          ${PKG_THREAD_LIBRARIES} ze_loader)
    target_include_directories(${TARGET_RP} PUBLIC ${PKG_LIBVA_INCLUDE_DIRS})
    target_include_directories(${TARGET_CV} PUBLIC ${PKG_LIBVA_INCLUDE_DIRS})
  endif()
else()
    message(STATUS "skipped ${TARGET_RP} and ${TARGET_CV} build for no hardware support")
endif()
