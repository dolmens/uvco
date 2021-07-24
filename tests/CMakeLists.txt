include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(uvco_test
    uvco_type_test.cc
    )

target_include_directories(uvco_test PRIVATE
    ${CppcoroIncludeDir}
    )

target_link_libraries(uvco_test PRIVATE
    gtest_main
    uvco
    )

include(GoogleTest)
gtest_discover_tests(uvco_test)
