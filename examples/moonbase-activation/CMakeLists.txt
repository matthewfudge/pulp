# moonbase-activation — optional reference plugin gating audio on a Moonbase
# license, with a Pulp-native activation panel.
#
# Opt-in: this example fetches the third-party Moonbase SDK (MIT) and requires
# OpenSSL at configure time (an upstream v3.3.0 CMake requirement, even though we
# select OS-native crypto). It is OFF by default; enable with
#   -DPULP_BUILD_MOONBASE_EXAMPLE=ON
# See docs/guides/copyright-protection.md.

include(FetchContent)

# ── "Bolt on" OpenSSL so developers need nothing from upstream ──────────────
# Moonbase documents OpenSSL as a system requirement, and v3.3.0's CMake runs
# find_package(OpenSSL REQUIRED) and links OpenSSL::SSL/Crypto UNCONDITIONALLY
# (no MOONBASE_CRYPTO_NATIVE wiring). So this example simply uses the
# upstream-default OpenSSL crypto backend that is required and linked anyway — no
# native-framework wiring. Make OpenSSL discoverable on common toolchains;
# override OPENSSL_ROOT_DIR if find_package doesn't pick it up.
if(APPLE AND NOT OPENSSL_ROOT_DIR)
    foreach(_ssl "/opt/homebrew/opt/openssl@3" "/usr/local/opt/openssl@3")
        if(EXISTS "${_ssl}")
            set(OPENSSL_ROOT_DIR "${_ssl}" CACHE PATH "OpenSSL root for Moonbase")
            break()
        endif()
    endforeach()
endif()

# ── Fetch Moonbase (MIT). Prefer a local clone for offline builds ───────────
set(MOONBASE_LOCAL_DIR "" CACHE PATH "Path to a local moonbase-cpp checkout (offline builds)")
if(NOT MOONBASE_LOCAL_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/../moonbase-cpp/CMakeLists.txt")
    set(MOONBASE_LOCAL_DIR "${CMAKE_SOURCE_DIR}/../moonbase-cpp")
endif()

# Required Moonbase build settings: no CURL (we supply a Pulp transport over
# cpp-httplib), no upstream tests/examples. MOONBASE_USE_CURL=OFF makes the
# upstream INTERFACE target define MOONBASE_DISABLE_CURL_TRANSPORT for us, so we
# don't set that ourselves. Crypto stays the upstream OpenSSL default (see above).
set(MOONBASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MOONBASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MOONBASE_USE_CURL OFF CACHE BOOL "" FORCE)

if(MOONBASE_LOCAL_DIR)
    message(STATUS "Moonbase: using local checkout ${MOONBASE_LOCAL_DIR}")
    FetchContent_Declare(moonbase_cpp SOURCE_DIR "${MOONBASE_LOCAL_DIR}")
else()
    FetchContent_Declare(moonbase_cpp
        GIT_REPOSITORY https://github.com/Moonbase-sh/moonbase-cpp.git
        GIT_TAG v3.3.0)
endif()
FetchContent_MakeAvailable(moonbase_cpp)

# ── Tests (the validation proof) ────────────────────────────────────────────
if(PULP_BUILD_TESTS)
    add_executable(pulp-moonbase-activation-test test_moonbase_activation.cpp)
    target_link_libraries(pulp-moonbase-activation-test
        PRIVATE pulp::format pulp::view moonbase::licensing pulp-cpp-httplib
                Catch2::Catch2WithMain)
    target_include_directories(pulp-moonbase-activation-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    catch_discover_tests(pulp-moonbase-activation-test)
endif()
