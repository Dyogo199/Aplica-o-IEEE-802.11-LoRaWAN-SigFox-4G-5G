cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow_access_point C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# === Modo background ===
add_executable(picow_access_point_background
    picow_access_point.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
)

target_include_directories(picow_access_point_background PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # lwipopts.h
    ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
    ${CMAKE_CURRENT_LIST_DIR}/dnsserver
)

target_link_libraries(picow_access_point_background
    pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    pico_stdio_usb
    hardware_adc
    hardware_pwm
)

pico_enable_stdio_usb(picow_access_point_background 1)
pico_enable_stdio_uart(picow_access_point_background 0)

pico_configure_ip4_address(picow_access_point_background PRIVATE
    CYW43_DEFAULT_IP_AP_ADDRESS 192.168.4.1
)

pico_add_extra_outputs(picow_access_point_background)

# === Modo poll ===
add_executable(picow_access_point_poll
    picow_access_point.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
)

target_include_directories(picow_access_point_poll PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # lwipopts.h
    ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
    ${CMAKE_CURRENT_LIST_DIR}/dnsserver
)

target_link_libraries(picow_access_point_poll
    pico_cyw43_arch_lwip_poll
    pico_stdlib
    pico_stdio_usb
    hardware_adc
    hardware_pwm
)

pico_enable_stdio_usb(picow_access_point_poll 1)
pico_enable_stdio_uart(picow_access_point_poll 0)

pico_configure_ip4_address(picow_access_point_poll PRIVATE
    CYW43_DEFAULT_IP_AP_ADDRESS 192.168.4.1
)

pico_add_extra_outputs(picow_access_point_poll)
