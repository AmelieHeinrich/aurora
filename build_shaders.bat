@echo off

set rootDir=%cd%

pushd shaders
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/geometry.vert -o geometry.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/geometry.frag -o geometry.frag.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/fxaa.vert -o fxaa.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/fxaa.frag -o fxaa.frag.spv
popd

Xcopy shaders build\shaders\ /y