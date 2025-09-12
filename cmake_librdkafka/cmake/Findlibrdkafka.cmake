include(ExternalProject)

if(TARGET rdkafka::rdkafka++)
  message(STATUS "Findlibrdkafka: using existing target rdkafka::rdkafka++ (skipping external build)")
  return()
endif()

# Paths
set(RDK_PREFIX        "${CMAKE_BINARY_DIR}/external/librdkafka")
set(RDK_INSTALL_DIR   "${RDK_PREFIX}/install")
set(RDK_INCLUDE_DIR   "${RDK_INSTALL_DIR}/include")
set(RDK_LIBRARY_DIR   "${RDK_INSTALL_DIR}/lib")
set(RDK_DEPS_LIB_DIR  "${RDK_PREFIX}/src/librdkafka_external/mklove/deps/dest/usr/lib")

file(MAKE_DIRECTORY "${RDK_INCLUDE_DIR}" "${RDK_LIBRARY_DIR}")

# Build environment and commands
set(RDK_ENV            ${CMAKE_COMMAND} -E env HOME=${RDK_PREFIX}/home)
set(RDK_CONFIGURE_CMD  ${RDK_ENV} ${RDK_PREFIX}/src/librdkafka_external/configure
                        --prefix=${RDK_INSTALL_DIR}
                        --install-deps --source-deps-only
                        --disable-lz4-ext --disable-ssl --disable-sasl --disable-curl
                        --enable-static)
set(RDK_BUILD_CMD      ${RDK_ENV} make -j${CMAKE_BUILD_PARALLEL_LEVEL})
set(RDK_INSTALL_CMD    ${RDK_ENV} make install DESTDIR=)

# External project
ExternalProject_Add(
  librdkafka_external
  URL                          https://github.com/confluentinc/librdkafka/archive/refs/tags/v2.11.1.zip
  URL_HASH                     SHA256=4a63e4422e5f5bbbb47f0ac1200e2ebd1f91b7b23f0de1bc625810c943fb870e
  DOWNLOAD_EXTRACT_TIMESTAMP   TRUE
  PREFIX                       ${RDK_PREFIX}
  CONFIGURE_COMMAND            ${RDK_CONFIGURE_CMD}
  BUILD_COMMAND                ${RDK_BUILD_CMD}
  BUILD_IN_SOURCE              1
  INSTALL_COMMAND              ${RDK_INSTALL_CMD}
  BUILD_BYPRODUCTS
    ${RDK_LIBRARY_DIR}/librdkafka.a
    ${RDK_LIBRARY_DIR}/librdkafka++.a
    ${RDK_DEPS_LIB_DIR}/libzstd.a
    ${RDK_DEPS_LIB_DIR}/libz.a
)

find_package(Threads REQUIRED)

# Imported library targets
add_library(rdkafka::rdkafka STATIC IMPORTED GLOBAL)
set_target_properties(rdkafka::rdkafka PROPERTIES
  IMPORTED_LOCATION             "${RDK_LIBRARY_DIR}/librdkafka.a"
  INTERFACE_INCLUDE_DIRECTORIES "${RDK_INCLUDE_DIR}"
)
add_dependencies(rdkafka::rdkafka librdkafka_external)
set_property(TARGET rdkafka::rdkafka APPEND PROPERTY INTERFACE_LINK_LIBRARIES
  "${RDK_DEPS_LIB_DIR}/libzstd.a"
  "${RDK_DEPS_LIB_DIR}/libz.a"
  Threads::Threads
  m dl rt
)

add_library(rdkafka::rdkafka++ STATIC IMPORTED GLOBAL)
set_target_properties(rdkafka::rdkafka++ PROPERTIES
  IMPORTED_LOCATION             "${RDK_LIBRARY_DIR}/librdkafka++.a"
  INTERFACE_INCLUDE_DIRECTORIES "${RDK_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES      rdkafka::rdkafka
)
add_dependencies(rdkafka::rdkafka++ librdkafka_external)
