vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO PCRE2Project/pcre2
    REF "pcre2-${VERSION}"
    SHA512 8257285809a9a4afb441869936740ae5ff04cc4bd95f86c8e4b9fd41f3b6a194e88d0aaa1c2cfc01b8845984993736fa5e77201c82021842acc87451d8f62f8b
    HEAD_REF main
    PATCHES
        pcre2-10.35_fix-uwp.patch
        no-static-suffix.patch
        fix-msvc-pdb-install.patch
)

if("jit" IN_LIST FEATURES)
    # The sljit commit pcre2 10.49 records in deps/sljit (GitHub's source snapshots leave
    # submodules out). Named here rather than asked of GitHub's API, which refuses requests
    # without a login once a runner has made too many.
    if(VCPKG_USE_HEAD_VERSION)
        message(FATAL_ERROR "This port names the sljit commit of pcre2 ${VERSION}; it cannot build the head version.")
    endif()
    set(sljit_ref de0259c7aaf36aa40cba8014f3fad3edde9307f9)

    vcpkg_from_github(
        OUT_SOURCE_PATH SLJIT_SOURCE_PATH
        REPO zherczeg/sljit
        REF "${sljit_ref}"
        SHA512 e5cc01f941877558f19859aedc9079c155f752a92e6d94fc5fb716e483b8fcb3a8495a5974bbb545d6d9a947ca97ebb36bfe278c4dc083afeb48b8ccab36895b
    )

    file(REMOVE_RECURSE "${SOURCE_PATH}/deps/sljit")
    file(MAKE_DIRECTORY "${SOURCE_PATH}/deps")
    file(RENAME "${SLJIT_SOURCE_PATH}" "${SOURCE_PATH}/deps/sljit")
endif()

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" BUILD_STATIC_LIBS)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" INSTALL_MSVC_PDB)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        jit   PCRE2_SUPPORT_JIT
)

if(VCPKG_TARGET_IS_ANDROID)
    list(APPEND FEATURE_OPTIONS -DHAVE_VSCRIPT_GNU=FALSE)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
        -DPCRE2_BUILD_PCRE2_8=ON
        -DPCRE2_BUILD_PCRE2_16=ON
        -DPCRE2_BUILD_PCRE2_32=ON
        -DPCRE2_SUPPORT_UNICODE=ON
        -DPCRE2_BUILD_TESTS=OFF
        -DPCRE2_BUILD_PCRE2GREP=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Readline=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Editline=ON
        -DINSTALL_MSVC_PDB=${INSTALL_MSVC_PDB}
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})

if(BUILD_STATIC_LIBS)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/pcre2.h" "defined(PCRE2_STATIC)" "1")
else()
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/pcre2.h" "defined(PCRE2_STATIC)" "0")
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/share/man"
    "${CURRENT_PACKAGES_DIR}/share/doc"
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share")

file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/tools/pcre2")
file(RENAME "${CURRENT_PACKAGES_DIR}/bin/pcre2-config" "${CURRENT_PACKAGES_DIR}/tools/pcre2/pcre2-config")
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/tools/pcre2/pcre2-config" "${CURRENT_PACKAGES_DIR}" [[$(cd "$(dirname "$0")/../.."; pwd -P)]])
if(NOT VCPKG_BUILD_TYPE)
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/tools/pcre2/debug")
    file(RENAME "${CURRENT_PACKAGES_DIR}/debug/bin/pcre2-config" "${CURRENT_PACKAGES_DIR}/tools/pcre2/debug/pcre2-config")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/tools/pcre2/debug/pcre2-config" "${CURRENT_PACKAGES_DIR}/debug" [[$(cd "$(dirname "$0")/../../../debug"; pwd -P)]])
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/tools/pcre2/debug/pcre2-config" [[${prefix}/include]] [[${prefix}/../include]])
endif()
vcpkg_clean_executables_in_bin(FILE_NAMES none)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
set(copyright_files "${SOURCE_PATH}/LICENCE.md")
if(PCRE2_SUPPORT_JIT)
    list(APPEND copyright_files "${SOURCE_PATH}/deps/sljit/LICENSE")
endif()
vcpkg_install_copyright(FILE_LIST ${copyright_files})
