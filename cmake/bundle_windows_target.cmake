# ts_bundle_windows(<target> [GTK_DATA])
#
# After each build of <target>, assembles build/<config>/dist/<target>/: a
# self-contained folder holding the executable and every DLL it imports
# from TS_WINDOWS_RUNTIME_PREFIX, ready to zip. Without GTK_DATA the folder
# is flat (<target>.exe next to its DLLs). With GTK_DATA it mirrors an
# install prefix (bin/, share/), because GTK finds its icons and settings
# schemas relative to its own DLLs.
# See cmake/bundle_windows.cmake for the assembly itself.

set(TS_BUNDLE_WINDOWS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/bundle_windows.cmake")

function(ts_bundle_windows target)
    cmake_parse_arguments(PARSE_ARGV 1 BUNDLE "GTK_DATA" "" "")

    set(gtk_arguments "")
    if(BUNDLE_GTK_DATA)
        if(CMAKE_CROSSCOMPILING)
            find_program(TS_GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED)
        else()
            set(TS_GLIB_COMPILE_SCHEMAS "${TS_WINDOWS_RUNTIME_PREFIX}/bin/glib-compile-schemas.exe")
        endif()

        set(gtk_arguments -DGTK_DATA=ON -DCOMPILE_SCHEMAS=${TS_GLIB_COMPILE_SCHEMAS})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:${target}>
            -DBUNDLE_DIR=${PROJECT_BINARY_DIR}/dist/${target}
            -DRUNTIME_PREFIX=${TS_WINDOWS_RUNTIME_PREFIX}
            -DOBJDUMP=${CMAKE_OBJDUMP}
            ${gtk_arguments}
            -P ${TS_BUNDLE_WINDOWS_SCRIPT}
        COMMENT "Bundling ${target} with its DLLs into dist/${target}"
        VERBATIM
    )
endfunction()
