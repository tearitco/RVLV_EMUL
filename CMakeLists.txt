cmake_minimum_required(VERSION 3.16)
project(rvemu C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  message(FATAL_ERROR "This machine builds with MinGW/clang, not MSVC")
endif()

add_executable(main
  main.c
  src/bus.c
  src/clint.c
  src/cpu.c
  src/csr.c
  src/dram.c
  src/dtb.c
  src/fpu.c
  src/mmu.c
  src/plic.c
  src/sbi.c
  src/selftest.c
  src/trap.c
  src/uart.c
  src/virtio.c
)

target_include_directories(main PRIVATE includes)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
if(WIN32)
  target_compile_definitions(main PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(main PRIVATE m)
