..\..\glslc.exe raytrace.rchit -o raytrace_rchit.spv --target-env=vulkan1.2 -g
..\..\glslc.exe raytrace.rgen -o raytrace_rgen.spv --target-env=vulkan1.2 -g
..\..\glslc.exe raytrace.rmiss -o raytrace_rmiss.spv --target-env=vulkan1.2 -g
..\..\glslc.exe full_quad.vert -o full_quad_vert.spv --target-env=vulkan1.2 -g
..\..\glslc.exe full_quad.frag -o full_quad_frag.spv --target-env=vulkan1.2 -g
..\..\glslc.exe rasterizer.vert -o rasterizer_vert.spv --target-env=vulkan1.2 -g
..\..\glslc.exe rasterizer.frag -o rasterizer_frag.spv --target-env=vulkan1.2 -g
..\..\glslc.exe pathtrace.rchit -o pathtrace_rchit.spv --target-env=vulkan1.2 -g
..\..\glslc.exe pathtrace.rgen -o pathtrace_rgen.spv --target-env=vulkan1.2 -g
..\..\glslc.exe pathtrace.rmiss -o pathtrace_rmiss.spv --target-env=vulkan1.2 -g
..\..\glslc.exe pathtrace_shadow.rmiss -o pathtrace_shadow_rmiss.spv --target-env=vulkan1.2 -g
pause