# support for asasm (RISC OS GCC SDK)
set(ASM_DIALECT "_ASASM")
set(CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT "<CMAKE_ASM${ASM_DIALECT}_COMPILER> <FLAGS> -o <OBJECT> <SOURCE>")
include(CMakeASMInformation)
# CMake >= 4.0 would otherwise require a CMakeASM_ASASMLinkerInformation.cmake
# module that does not exist for this custom dialect. The final executable is
# linked by the C/C++ compiler, so ASM linker-information is not needed.
set(CMAKE_ASM${ASM_DIALECT}_USE_LINKER_INFORMATION FALSE)
set(ASM_DIALECT)
