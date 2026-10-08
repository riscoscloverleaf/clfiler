cmake_minimum_required(VERSION 3.15)
set(APP_MAIN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${APP_MAIN_ROOT}/cmake/Modules)
# this one is important
set(CMAKE_SYSTEM_NAME GNU)

set(CMAKE_SYSTEM_PROCESSOR arm)

set(RISCOS_GCCSDK "$ENV{HOME}/gccsdk" CACHE PATH "Path to the RISC OS GCCSDK")
set(riscosgcc ${RISCOS_GCCSDK})
set(CMAKE_C_COMPILER ${riscosgcc}/env/arm-unknown-riscos-gcc)
set(CMAKE_CXX_COMPILER ${riscosgcc}/env/arm-unknown-riscos-g++)
set(CMAKE_ASM_ASASM_COMPILER ${riscosgcc}/cross/bin/asasm)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

#set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> <FLAGS> -o <OBJECT> <SOURCE>")
#set(CMAKE_ASM_FLAGS " -cpu XScale -fpu FPA10")
#set(CMAKE_ASM_ASASM_FLAGS "" CACHE INTERNAL "")
string(APPEND CMAKE_ASM_ASASM_FLAGS " -cpu XScale -fpu FPA10")

project(clfiler C CXX ASM_ASASM)

include_directories("libs/eventbus/include" "libs/tbx" "libs/utf8" "libs/librufl/include" "libs/cloverleaf" "libs/inipp")
set(CURL_INCLUDE_DIR ${riscosgcc}/env/include)
set(CURL_LIBRARY ${riscosgcc}/env/lib)

add_subdirectory("libs/tbx/tbx")
add_subdirectory("libs/librufl/src")

#set(ARM_SOURCES,
add_executable(clfiler
        libs/cloverleaf/cloverleaf/awrender.s
        libs/cloverleaf/cloverleaf/flex.c
        libs/cloverleaf/cloverleaf/CLException.cpp
        libs/cloverleaf/cloverleaf/IdleTask.cpp
        libs/cloverleaf/cloverleaf/CLGraphics.cpp
        libs/cloverleaf/cloverleaf/CLImage.cpp
        libs/cloverleaf/cloverleaf/CLImageJPGLoader.cpp
        libs/cloverleaf/cloverleaf/CLImagePNGLoader.cpp
        libs/cloverleaf/cloverleaf/CLImageSpriteLoader.cpp
        libs/cloverleaf/cloverleaf/CLImageMagickLoader.cpp
        libs/cloverleaf/cloverleaf/CLFFmpegLoader.cpp
        libs/cloverleaf/cloverleaf/CLImageCache.cpp
        libs/cloverleaf/cloverleaf/Logger.cpp
        libs/cloverleaf/cloverleaf/CLUtf8.cpp
        libs/cloverleaf/cloverleaf/rosprite.cpp
        libs/cloverleaf/cloverleaf/CLClipboard.cpp
        libs/cloverleaf/cloverleaf/CLUtils.cpp
        model/AppDataModel.cpp
        model/AppSettings.cpp
        model/DrivesDetect.cpp
        ui/AppIconbar.cpp
        ui/FSRemoveUI.cpp
        ui/CLDirectoryWindow.cpp
        ui/CLImageViewWindow.cpp
        ui/CLDirSizeScanWindow.cpp
        ui/CLDirectoryMenu.cpp
        ui/CLSplash.cpp
        ui/CLAskDonate.cpp
        ui/CLDisclaimer.cpp
        ui/CopyMoveConfirm.cpp
        ui/CLBlocksView.cpp
        ui/ProgInfo.cpp
        main.cpp
        utils.cpp
)

target_link_libraries (clfiler z png jpeg tbx rufl OSLib32-soft OSLibSupport32-soft FlexLib32)
target_compile_definitions(clfiler PUBLIC -DDONATE=0)

add_custom_target(RunImage COMMAND ${riscosgcc}/cross/bin/elf2aif clfiler !RunImage,ff8
        DEPENDS clfiler)

