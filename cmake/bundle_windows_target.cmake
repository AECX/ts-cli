# ts_bundle_windows(<target>)
#
# After each build of <target>, assembles build/<config>/dist/<target>/: a
# self-contained folder holding the executable and every DLL it imports
# from TS_WINDOWS_RUNTIME_PREFIX, ready to zip. See
# cmake/bundle_windows.cmake for the assembly itself.

set(TS_BUNDLE_WINDOWS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/bundle_windows.cmake")

function(ts_bundle_windows target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:${target}>
            -DBUNDLE_DIR=${PROJECT_BINARY_DIR}/dist/${target}
            -DRUNTIME_PREFIX=${TS_WINDOWS_RUNTIME_PREFIX}
            -DOBJDUMP=${CMAKE_OBJDUMP}
            -P ${TS_BUNDLE_WINDOWS_SCRIPT}
        COMMENT "Bundling ${target} with its DLLs into dist/${target}"
        VERBATIM
    )
endfunction()
