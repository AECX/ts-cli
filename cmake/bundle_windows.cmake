# Assembles a self-contained Windows folder for one executable (run with
# cmake -P; cmake/bundle_windows_target.cmake wires it up as a post-build step).
#
# Windows has no system package manager for our libraries, so every
# executable ships next to the DLLs it imports:
#
#   <BUNDLE_DIR>/<name>.exe and every DLL it needs
#
# The folder can be zipped and run on any 64-bit Windows machine as-is.
#
# Required variables:
#   EXECUTABLE       the built .exe
#   BUNDLE_DIR       output folder (recreated from scratch)
#   RUNTIME_PREFIX   MSYS2 UCRT64 prefix holding the DLLs (bin/, lib/, share/)
#   OBJDUMP          objdump able to read PE files

foreach(variable EXECUTABLE BUNDLE_DIR RUNTIME_PREFIX OBJDUMP)
    if(NOT ${variable})
        message(FATAL_ERROR "bundle_windows.cmake: ${variable} is not set")
    endif()
endforeach()

set(BINARY_DIR "${BUNDLE_DIR}")

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
