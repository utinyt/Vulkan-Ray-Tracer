..\..\glslc.exe raytrace.rchit -o raytrace_rchit.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rgen -o raytrace_rgen.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rmiss -o raytrace_rmiss.spv --target-env=vulkan1.2
..\..\glslc.exe full_quad.vert -o full_quad_vert.spv
..\..\glslc.exe full_quad.frag -o full_quad_frag.spv
pause