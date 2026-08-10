add_executable(ts-audio-tests
    main.cpp
)

target_compile_features(ts-audio-tests PRIVATE cxx_std_20)
target_compile_options(ts-audio-tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
target_link_libraries(ts-audio-tests PRIVATE ts-audio)

add_test(NAME ts-audio COMMAND ts-audio-tests)
