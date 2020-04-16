set(files
    packet_test.cpp
    application_test.cpp
    )

foreach(file ${files})
    get_filename_component(target_name ${file} NAME_WE)
    add_executable(${target_name} ${file})
    target_link_libraries(${target_name} hdcp)
    target_compile_options(${target_name} PUBLIC -Werror -Wall -Wextra -g -fno-inline)
endforeach()
