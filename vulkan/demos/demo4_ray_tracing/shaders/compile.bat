..\..\glslc.exe raytrace.rchit -o raytrace_rchit.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rgen -o raytrace_rgen.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rmiss -o raytrace_rmiss.spv --target-env=vulkan1.2
pause