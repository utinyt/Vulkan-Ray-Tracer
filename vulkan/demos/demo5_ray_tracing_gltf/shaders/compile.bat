..\..\glslc.exe raytrace.rchit -o raytrace_rchit.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rgen -o raytrace_rgen.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace.rmiss -o raytrace_rmiss.spv --target-env=vulkan1.2
..\..\glslc.exe raytrace_shadow.rmiss -o raytrace_shadow_rmiss.spv --target-env=vulkan1.2
..\..\glslc.exe full_quad.vert -o full_quad_vert.spv
..\..\glslc.exe full_quad.frag -o full_quad_frag.spv
..\..\glslc.exe rasterizer.vert -o rasterizer_vert.spv --target-env=vulkan1.2
..\..\glslc.exe rasterizer.frag -o rasterizer_frag.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rchit -o pathtrace_rchit.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rgen -o pathtrace_rgen.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rmiss -o pathtrace_rmiss.spv --target-env=vulkan1.2
pause