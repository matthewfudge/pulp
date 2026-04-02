# Sanitizers.cmake — Add sanitizer support to Pulp builds
#
# Usage:
#   cmake -B build -DPULP_SANITIZER=address   # AddressSanitizer
#   cmake -B build -DPULP_SANITIZER=thread    # ThreadSanitizer
#   cmake -B build -DPULP_SANITIZER=undefined # UndefinedBehaviorSanitizer
#   cmake -B build -DPULP_SANITIZER=memory    # MemorySanitizer (Clang only)
#   cmake -B build -DPULP_SANITIZER=realtime  # RealtimeSanitizer (Clang 18+)

set(PULP_SANITIZER "" CACHE STRING "Enable sanitizer: address, thread, undefined, memory, realtime")

if(PULP_SANITIZER)
    message(STATUS "Pulp: Sanitizer enabled: ${PULP_SANITIZER}")

    if(PULP_SANITIZER STREQUAL "address")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    elseif(PULP_SANITIZER STREQUAL "thread")
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
    elseif(PULP_SANITIZER STREQUAL "undefined")
        add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
        add_link_options(-fsanitize=undefined)
    elseif(PULP_SANITIZER STREQUAL "memory")
        add_compile_options(-fsanitize=memory -fno-omit-frame-pointer)
        add_link_options(-fsanitize=memory)
    elseif(PULP_SANITIZER STREQUAL "realtime")
        # RealtimeSanitizer (RTSan) — detects real-time safety violations
        # such as memory allocation, mutex locks, or syscalls in audio callbacks.
        # Requires: Clang 18+ (LLVM) — not available in Apple Clang as of Xcode 16.
        # See: https://clang.llvm.org/docs/RealtimeSanitizer.html
        #
        # Platform support:
        #   Linux (x86_64, aarch64): Fully supported with Clang 18+
        #   macOS: Requires upstream LLVM Clang 18+, NOT Apple Clang
        #   Windows: Not supported
        add_compile_options(-fsanitize=realtime -fno-omit-frame-pointer)
        add_link_options(-fsanitize=realtime)
    else()
        message(WARNING "Unknown sanitizer: ${PULP_SANITIZER}")
    endif()

    # Force debug info for meaningful stack traces
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
    endif()
endif()
