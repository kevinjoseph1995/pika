# C++ standard
set(CMAKE_CXX_STANDARD 23)

option(PIKA_ENABLE_BACKTRACE "Enable backtrace" OFF)

add_library(pika SHARED impl/shared_buffer.cpp
                        impl/error.cpp
                        impl/process_fork.cpp
                        impl/ring_buffer.cpp
                        impl/synchronization_primitives.cpp
                        impl/channel_interface.cpp
                        impl/inter_process_channel.cpp
)
target_include_directories(pika PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/impl)
target_include_directories(pika PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pika PUBLIC fmt $<$<STREQUAL:${PIKA_ENABLE_BACKTRACE},ON>:Backward::Backward>)
target_compile_definitions(pika PRIVATE $<$<STREQUAL:${PIKA_ENABLE_BACKTRACE},ON>:ENABLE_BACKTRACE=1>)

target_compile_options(pika PUBLIC
        -Wall -Wextra -Werror -fno-exceptions -Wconversion -march=native
        $<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined;-fsanitize=signed-integer-overflow;-fsanitize=null;-fsanitize=float-cast-overflow;-fsanitize=alignment > )
target_link_options(pika PUBLIC   $<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined > )
