..\..\glslc.exe full_quad.vert -o full_quad_vert.spv --target-env=vulkan1.2
..\..\glslc.exe full_quad.frag -o full_quad_frag.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rchit -o pathtrace_rchit.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rgen -o pathtrace_rgen.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace.rmiss -o pathtrace_rmiss.spv --target-env=vulkan1.2
..\..\glslc.exe pathtrace_shadow.rmiss -o pathtrace_shadow_rmiss.spv --target-env=vulkan1.2
pause