@echo off

set rootDir=%cd%

pushd shaders
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/deferred.vert -o deferred.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/deferred.frag -o deferred.frag.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/gbuffer.vert -o gbuffer.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/gbuffer.frag -o gbuffer.frag.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/fxaa.vert -o fxaa.vert.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/fxaa.frag -o fxaa.frag.spv
call %VULKAN_SDK%/bin/glslc.exe -g -O %rootDir%/shaders/equirectangular_cubemap.comp -o equirectangular_cubemap.comp.spv
popd

Xcopy shaders build\shaders\ /y