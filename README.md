# Vulkan Ray Tracer
This is a real-time ray tracer using C++ / Vulkan / GLSL.

## Developing Environments
* OS: Windows 10
* IDE: Visual Studio 2019
* GPU: RTX 2060 SUPER

Successful build on other environments is not guaranteed.

## Third Party
* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
* [tinygltf](https://github.com/syoyo/tinygltf)
* [stb collection](https://github.com/nothings/stb)
* [imgui](https://github.com/ocornut/imgui)

## References
A lot of code were based on these great resources:
* [Vulkan tutorial by Alexander Overvoorde](https://vulkan-tutorial.com/Introduction)
* [Vulkan samples by Sascha Willems](https://github.com/SaschaWillems/Vulkan)
* [Nvpro Core from NVIDIA DesignWorks Samples](https://github.com/nvpro-samples/nvpro_core)
* [vk_raytracing_tutorial_KHR from NVIDIA DesignWorks Samples](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)

## Updates
## Path Tracing + Next Event Estimation - (Nov.17.2021)
![path_tracing](https://github.com/utinyt/Vulkan-Ray-Tracer/blob/master/screenshots/pathtracing.png)<br>

### Next Event Estimation
![path_tracing_nee](https://github.com/utinyt/Vulkan-Ray-Tracer/blob/master/screenshots/pathtracing.gif)<br>
#### Average frame time: 19.32 (51.77 FPS)
* 1200x800 screen resolution
* 8 rays per pixel (subsequent rendered images are combined to the previous image)
* 169 instances / 169 bottom level acceleration structures

Closest hit shader shoots additional (shadow) ray to the invisible light sphere and check if current point is directly illuminated from it. This checking of 'explicit light connection' accelerates image convergence speed.<br>

#### Gltf model reference: https://sketchfab.com/SEED.EA/collections/pica-pica

## Path Tracing - (Sep.20.2021)
![path_tracing](https://github.com/utinyt/Vulkan-Ray-Tracer/blob/master/screenshots/path_tracing.gif)<br>

#### Reference: https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_gltf

## First model - (Sep.12.2021)
![bunny](https://github.com/utinyt/Vulkan-Ray-Tracer/blob/master/screenshots/bunny.png)<br>

#### Reference: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
