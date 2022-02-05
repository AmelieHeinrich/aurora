@echo off

set rootDir=%cd%
if not exist build (
    mkdir build
)

set debug=true

if %debug%==true (
    echo Compiling in debug mode.
    echo.

    set debugFlags = -DAURORA_DEBUG
    set entryPoint = /link /subsystem:CONSOLE
    set link_path=/link /LIBPATH:%rootDir%/bin/dbg    
) else (
    echo Compiling in release mode.
    echo.

    set debugFlags=-DNDEBUG -O2 -Oi -fp:fast
    set entryPoint = /link /subsystem:WINDOWS
    set link_path=/link /LIBPATH:%rootDir%/bin/dbg
)

set output=aurora
set flags=-nologo -FC -Zi -WX -W4 /MP -DVK_NO_PROTOTYPES -DVK_USE_PLATFORM_WIN32_KHR
set disabledWarnings=-wd4100 -wd4201 -wd4018 -wd4099 -wd4189 -wd4505 -wd4530 -wd4840 -wd4324 -wd4459 -wd4702 -wd4244 -wd4310 -wd4611 -wd4996
set source= %rootDir%/src/*.c
set links=user32.lib shlwapi.lib volk.lib
set includeDirs= -I%rootDir%/src -I%rootDir%/ext -I%VULKAN_SDK%/Include

pushd build
if not exist volk.lib (
    cl -I%VULKAN_SDK%/Include %flags% -Fe%output% %rootDir%/ext/*.c /incremental /c
    lib %rootDir%/build/volk.obj
)
cl %disabledWarnings% %includeDirs% %debugFlags% %flags% -Fe%output% %source% /incremental %links% %entryPoint% %link_path%
popd

echo.
echo Build finished.