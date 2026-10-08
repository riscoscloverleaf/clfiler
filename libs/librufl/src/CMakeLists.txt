# this one is important
SET(CMAKE_SYSTEM_NAME GNU)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(RISCOS_GCCSDK "$ENV{HOME}/gccsdk" CACHE PATH "Path to the RISC OS GCCSDK")
set(riscosgcc ${RISCOS_GCCSDK})
set(CMAKE_C_COMPILER ${riscosgcc}/env/arm-unknown-riscos-gcc)
set(CMAKE_CXX_COMPILER ${riscosgcc}/env/arm-unknown-riscos-g++)
set(CMAKE_AR ${riscosgcc}/cross/bin/arm-unknown-riscos-ar)

add_library(rufl
        rufl_character_set_test.c
        rufl_decompose.c
        rufl_dump_state.c
        rufl_find.c
        rufl_init.c
        rufl_invalidate_cache.c
        rufl_metrics.c
        rufl_paint.c
        rufl_quit.c
        rufl_glyph_map.c
        )