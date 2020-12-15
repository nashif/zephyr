# SPDX-License-Identifier: Apache-2.0

set(find_program_clang_args )
set(find_program_binutils_args )

find_program(CMAKE_AR      llvm-ar      ${find_program_clang_args}   )
find_program(CMAKE_NM      llvm-nm      ${find_program_clang_args}   )
find_program(CMAKE_OBJDUMP llvm-objdump ${find_program_clang_args}   )
find_program(CMAKE_RANLIB  llvm-ranlib  ${find_program_clang_args}   )
find_program(CMAKE_OBJCOPY objcopy      ${find_program_binutils_args})
find_program(CMAKE_READELF readelf      ${find_program_binutils_args})

find_program(CMAKE_GDB     gdb-oneapi)

# Use the gnu binutil abstraction
include(${ZEPHYR_BASE}/cmake/bintools/llvm/target_bintools.cmake)

