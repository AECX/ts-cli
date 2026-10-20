# Assembles a self-contained Windows folder for one executable (run with
# cmake -P; cmake/bundle_windows_target.cmake wires it up as a post-build step).
#
# Windows has no system package manager for our libraries, so every
# executable ships next to the DLLs it imports. Without GTK_DATA the folder
# is flat:
#
#   <BUNDLE_DIR>/<name>.exe and every DLL it needs
#
# GTK locates its data relative to its own DLLs, so with GTK_DATA the folder
# mirrors an install prefix:
#
#   <BUNDLE_DIR>/bin/                      <name>.exe and every DLL it needs
#   <BUNDLE_DIR>/share/icons/              Adwaita, hicolor
#   <BUNDLE_DIR>/share/glib-2.0/schemas/   compiled GTK settings schemas
#
# Either folder can be zipped and run on any 64-bit Windows machine as-is.
#
# Required variables:
#   EXECUTABLE       the built .exe
#   BUNDLE_DIR       output folder (recreated from scratch)
#   RUNTIME_PREFIX   MSYS2 UCRT64 prefix holding the DLLs (bin/, lib/, share/)
#   OBJDUMP          objdump able to read PE files
# Optional:
#   GTK_DATA         also bundle GTK's runtime data
#   COMPILE_SCHEMAS  glib-compile-schemas runnable on the build host (with GTK_DATA)

foreach(variable EXECUTABLE BUNDLE_DIR RUNTIME_PREFIX OBJDUMP)
    if(NOT ${variable})
        message(FATAL_ERROR "bundle_windows.cmake: ${variable} is not set")
    endif()
endforeach()

set(BINARY_DIR "${BUNDLE_DIR}")

if(GTK_DATA)
    if(NOT COMPILE_SCHEMAS)
        message(FATAL_ERROR "bundle_windows.cmake: GTK_DATA needs COMPILE_SCHEMAS")
    endif()

    set(BINARY_DIR "${BUNDLE_DIR}/bin")
endif()

file(REMOVE_RECURSE "${BUNDLE_DIR}")
file(MAKE_DIRECTORY "${BINARY_DIR}")

file(GLOB available_dlls RELATIVE "${RUNTIME_PREFIX}/bin" "${RUNTIME_PREFIX}/bin/*.dll")
set(available_lower "")
foreach(dll IN LISTS available_dlls)
    string(TOLOWER "${dll}" lower)
    list(APPEND available_lower "${lower}")
endforeach()

# Breadth-first walk over the PE import tables. Anything not found in the
# runtime prefix (KERNEL32.dll, the UCRT api-ms-win-* set, ...) ships with Windows.
set(pending "${EXECUTABLE}")
set(bundled "")

while(pending)
    list(POP_FRONT pending binary)

    execute_process(
        COMMAND "${OBJDUMP}" -p "${binary}"
        OUTPUT_VARIABLE imports
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "bundle_windows.cmake: could not read imports of ${binary}")
    endif()

    string(REGEX MATCHALL "DLL Name: [^\r\n]+" names "${imports}")
    foreach(entry IN LISTS names)
        string(REGEX REPLACE "^DLL Name: " "" name "${entry}")
        string(TOLOWER "${name}" lower)
        list(FIND available_lower "${lower}" index)

        if(index EQUAL -1 OR lower IN_LIST bundled)
            continue()
        endif()

        list(GET available_dlls ${index} dll)
        list(APPEND bundled "${lower}")
        list(APPEND pending "${RUNTIME_PREFIX}/bin/${dll}")
        file(COPY "${RUNTIME_PREFIX}/bin/${dll}" DESTINATION "${BINARY_DIR}")
    endforeach()
endwhile()

file(COPY "${EXECUTABLE}" DESTINATION "${BINARY_DIR}")

if(NOT GTK_DATA)
    return()
endif()

# GTK 4 renders the (symbolic SVG) Adwaita icons itself; no gdk-pixbuf
# loader modules are needed.
foreach(theme Adwaita hicolor)
    file(COPY "${RUNTIME_PREFIX}/share/icons/${theme}" DESTINATION "${BUNDLE_DIR}/share/icons")
endforeach()

# GtkEntry's emoji chooser and the file/color choosers read GSettings; GLib
# aborts when a schema they need is missing.
file(GLOB schemas "${RUNTIME_PREFIX}/share/glib-2.0/schemas/org.gtk.gtk4.Settings.*.gschema.xml")
file(MAKE_DIRECTORY "${BUNDLE_DIR}/share/glib-2.0/schemas")
file(COPY ${schemas} DESTINATION "${BUNDLE_DIR}/share/glib-2.0/schemas")

execute_process(
    COMMAND "${COMPILE_SCHEMAS}" "${BUNDLE_DIR}/share/glib-2.0/schemas"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "bundle_windows.cmake: glib-compile-schemas failed")
endif()
