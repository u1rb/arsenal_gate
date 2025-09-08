include(ExternalProject)

set(LIBRDKAFKA_PREFIX ${CMAKE_BINARY_DIR}/external/librdkafka)
set(LIBRDKAFKA_INSTALL_DIR ${LIBRDKAFKA_PREFIX}/install)

ExternalProject_Add(
    librdkafka_external
    URL https://github.com/confluentinc/librdkafka/archive/refs/tags/v2.11.1.zip
    URL_HASH SHA256=4a63e4422e5f5bbbb47f0ac1200e2ebd1f91b7b23f0de1bc625810c943fb870e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${LIBRDKAFKA_PREFIX}
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env 
        HOME=${LIBRDKAFKA_PREFIX}/home
        ${LIBRDKAFKA_PREFIX}/src/librdkafka_external/configure 
        --prefix=${LIBRDKAFKA_INSTALL_DIR}
        --install-deps 
        --source-deps-only
        --disable-lz4-ext
        --disable-ssl
        --disable-sasl
        --disable-curl
        --enable-static
    BUILD_COMMAND ${CMAKE_COMMAND} -E env 
        HOME=${LIBRDKAFKA_PREFIX}/home
        make -j${CMAKE_BUILD_PARALLEL_LEVEL}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ${CMAKE_COMMAND} -E env 
        HOME=${LIBRDKAFKA_PREFIX}/home
        make install DESTDIR=
)

set(LIBRDKAFKA_INCLUDE_DIR ${LIBRDKAFKA_INSTALL_DIR}/include)
set(LIBRDKAFKA_LIBRARY_DIR ${LIBRDKAFKA_INSTALL_DIR}/lib)

file(MAKE_DIRECTORY ${LIBRDKAFKA_INCLUDE_DIR})

add_library(rdkafka::rdkafka STATIC IMPORTED GLOBAL)
set_target_properties(rdkafka::rdkafka PROPERTIES
    IMPORTED_LOCATION ${LIBRDKAFKA_LIBRARY_DIR}/librdkafka.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBRDKAFKA_INCLUDE_DIR}
)
add_dependencies(rdkafka::rdkafka librdkafka_external)

add_library(rdkafka::rdkafka++ STATIC IMPORTED GLOBAL)
set_target_properties(rdkafka::rdkafka++ PROPERTIES
    IMPORTED_LOCATION ${LIBRDKAFKA_LIBRARY_DIR}/librdkafka++.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBRDKAFKA_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES rdkafka::rdkafka
)
add_dependencies(rdkafka::rdkafka++ librdkafka_external)

find_package(Threads REQUIRED)

# Link with the static zlib and zstd that librdkafka built
set_property(TARGET rdkafka::rdkafka APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES 
    ${LIBRDKAFKA_PREFIX}/src/librdkafka_external/mklove/deps/dest/usr/lib/libzstd.a
    ${LIBRDKAFKA_PREFIX}/src/librdkafka_external/mklove/deps/dest/usr/lib/libz.a
    Threads::Threads
    m
    dl
    rt
)