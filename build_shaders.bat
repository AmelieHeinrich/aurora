@echo off

set rootDir=%cd%

pushd shaders
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/triangle_vert.vert -o triangle_vert.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/triangle_frag.frag -o triangle_frag.frag.spv
popd

Xcopy shaders build\shaders\ /y