# C++ standard
set(CMAKE_CXX_STANDARD 23)

add_executable(test_pika main.cpp
                         test_mpmc_channel.cpp)
target_link_libraries(test_pika gtest_main pika fmt)
add_test(NAME test_pika COMMAND test_pika)
target_compile_options(test_pika PRIVATE -Wall -Wextra -Werror -fno-exceptions)