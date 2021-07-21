set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

add_executable(uvcosample
    main.cc
    )

target_include_directories(uvcosample PRIVATE
    ${CMAKE_SOURCE_DIR}/cppcoro/include
    )

target_link_libraries(uvcosample PRIVATE
    uvco
    )
