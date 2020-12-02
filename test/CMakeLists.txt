set(files
    transport_test.cpp
    packet_test.cpp
    master_tcp_test.cpp
    slave_tcp_test.cpp
    master_usb_test.cpp
    )

foreach(file ${files})
    get_filename_component(target_name ${file} NAME_WE)
    add_executable(${target_name} ${file})
    target_link_libraries(${target_name} hdcp)
    target_compile_options(${target_name} PUBLIC -Wall -Wextra -g)
endforeach()
