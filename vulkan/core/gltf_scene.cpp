/*
* reference : https://github.com/nvpro-samples/nvpro_core/blob/master/nvh/gltfscene.cpp
*/

#include <set>
#include <iostream>
#include <sstream>
#include <glm/gtc/quaternion.hpp>
#include "gltf_scene.h"

#define EXTENSION_ATTRIB_IRAY "NV_attributes_iray"

glm::mat4 getLocalMatrix(const tinygltf::Node& tnode)
{
    glm::mat4 mtranslation{ 1 };
    glm::mat4 mscale{ 1 };
    glm::mat4 mrot{ 1 };
    glm::mat4 matrix{ 1 };
    glm::quat mrotation;

    if (!tnode.translation.empty())
        mtranslation = glm::translate(mtranslation, glm::vec3(tnode.translation[0], tnode.translation[1], tnode.translation[2]));
    if (!tnode.scale.empty())
        mscale = glm::scale(mscale, glm::vec3(tnode.scale[0], tnode.scale[1], tnode.scale[2]));
    if (!tnode.rotation.empty())
    {
        mrotation[0] = static_cast<float>(tnode.rotation[0]);
        mrotation[1] = static_cast<float>(tnode.rotation[1]);
        mrotation[2] = static_cast<float>(tnode.rotation[2]);
        mrotation[3] = static_cast<float>(tnode.rotation[3]);
        mrot = glm::mat4_cast(mrotation);
    }
    if (!tnode.matrix.empty())
    {
        for (int i = 0; i < 4; ++i)
        {
            matrix[i][0] = static_cast<float>(tnode.matrix[0 + i * 4]);
            matrix[i][1] = static_cast<float>(tnode.matrix[1 + i * 4]);
            matrix[i][2] = static_cast<float>(tnode.matrix[2 + i * 4]);
            matrix[i][3] = static_cast<float>(tnode.matrix[3 + i * 4]);
        }
    }
    return mtranslation * mrot * mscale * matrix;
}

void GltfScene::computeSceneDimensions()
{
    auto valMin = glm::vec3(FLT_MAX);
    auto valMax = glm::vec3(-FLT_MAX);
    for (const auto& node : m_nodes)
    {
        const auto& mesh = m_primMeshes[node.primMesh];

        glm::vec4 locMin = node.worldMatrix * glm::vec4(mesh.posMin, 1.0f);
        glm::vec4 locMax = node.worldMatrix * glm::vec4(mesh.posMax, 1.0f);

        valMin = { std::min(locMin.x, valMin.x), std::min(locMin.y, valMin.y), std::min(locMin.z, valMin.z) };
        valMax = { std::max(locMax.x, valMax.x), std::max(locMax.y, valMax.y), std::max(locMax.z, valMax.z) };
    }
    if (valMin == valMax)
    {
        valMin = glm::vec3(-1);
        valMin = glm::vec3(1);
    }
    m_dimensions.min = valMin;
    m_dimensions.max = valMax;
    m_dimensions.size = valMax - valMin;
    m_dimensions.center = (valMin + valMax) / 2.0f;
    m_dimensions.radius = glm::length(valMax - valMin) / 2.0f;
}

void GltfScene::importMaterials(const tinygltf::Model& tmodel)
{
    m_materials.reserve(tmodel.materials.size());

    for (auto& tmat : tmodel.materials)
    {
        GltfMaterial gmat;

        gmat.alphaCutoff = static_cast<float>(tmat.alphaCutoff);
        gmat.alphaMode = tmat.alphaMode == "MASK" ? 1 : (tmat.alphaMode == "BLEND" ? 2 : 0);
        gmat.doubleSided = tmat.doubleSided ? 1 : 0;
        gmat.emissiveFactor = glm::vec3(tmat.emissiveFactor[0], tmat.emissiveFactor[1], tmat.emissiveFactor[2]);
        gmat.emissiveTexture = tmat.emissiveTexture.index;
        gmat.normalTexture = tmat.normalTexture.index;
        gmat.normalTextureScale = static_cast<float>(tmat.normalTexture.scale);
        gmat.occlusionTexture = tmat.occlusionTexture.index;
        gmat.occlusionTextureStrength = static_cast<float>(tmat.occlusionTexture.strength);

        // PbrMetallicRoughness
        auto& tpbr = tmat.pbrMetallicRoughness;
        gmat.baseColorFactor =
            glm::vec4(tpbr.baseColorFactor[0], tpbr.baseColorFactor[1], tpbr.baseColorFactor[2], tpbr.baseColorFactor[3]);
        gmat.baseColorTexture = tpbr.baseColorTexture.index;
        gmat.metallicFactor = static_cast<float>(tpbr.metallicFactor);
        gmat.metallicRoughnessTexture = tpbr.metallicRoughnessTexture.index;
        gmat.roughnessFactor = static_cast<float>(tpbr.roughnessFactor);

        // KHR_materials_pbrSpecularGlossiness
        if (tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME) != tmat.extensions.end())
        {
            gmat.shadingModel = 1;

            const auto& ext = tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME)->second;
            getVec4(ext, "diffuseFactor", gmat.specularGlossiness.diffuseFactor);
            getFloat(ext, "glossinessFactor", gmat.specularGlossiness.glossinessFactor);
            getVec3(ext, "specularFactor", gmat.specularGlossiness.specularFactor);
            getTexId(ext, "diffuseTexture", gmat.specularGlossiness.diffuseTexture);
            getTexId(ext, "specularGlossinessTexture", gmat.specularGlossiness.specularGlossinessTexture);
        }

        // KHR_texture_transform
        if (tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME) != tpbr.baseColorTexture.extensions.end())
        {
            const auto& ext = tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME)->second;
            auto& tt = gmat.textureTransform;
            getVec2(ext, "offset", tt.offset);
            getVec2(ext, "scale", tt.scale);
            getFloat(ext, "rotation", tt.rotation);
            getInt(ext, "texCoord", tt.texCoord);

            // Computing the transformation
            auto translation = glm::mat3(1, 0, tt.offset.x, 0, 1, tt.offset.y, 0, 0, 1);
            auto rotation = glm::mat3(cos(tt.rotation), sin(tt.rotation), 0, -sin(tt.rotation), cos(tt.rotation), 0, 0, 0, 1);
            auto scale = glm::mat3(tt.scale.x, 0, 0, 0, tt.scale.y, 0, 0, 0, 1);
            tt.uvTransform = scale * rotation * translation;
        }

        // KHR_materials_unlit
        if (tmat.extensions.find(KHR_MATERIALS_UNLIT_EXTENSION_NAME) != tmat.extensions.end())
        {
            gmat.unlit.active = 1;
        }

        // KHR_materials_anisotropy
        if (tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME)->second;
            getFloat(ext, "anisotropy", gmat.anisotropy.factor);
            getVec3(ext, "anisotropyDirection", gmat.anisotropy.direction);
            getTexId(ext, "anisotropyTexture", gmat.anisotropy.texture);
        }

        // KHR_materials_clearcoat
        if (tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME)->second;
            getFloat(ext, "clearcoatFactor", gmat.clearcoat.factor);
            getTexId(ext, "clearcoatTexture", gmat.clearcoat.texture);
            getFloat(ext, "clearcoatRoughnessFactor", gmat.clearcoat.roughnessFactor);
            getTexId(ext, "clearcoatRoughnessTexture", gmat.clearcoat.roughnessTexture);
            getTexId(ext, "clearcoatNormalTexture", gmat.clearcoat.normalTexture);
        }

        // KHR_materials_sheen
        if (tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME)->second;
            getVec3(ext, "sheenColorFactor", gmat.sheen.colorFactor);
            getTexId(ext, "sheenColorTexture", gmat.sheen.colorTexture);
            getFloat(ext, "sheenRoughnessFactor", gmat.sheen.roughnessFactor);
            getTexId(ext, "sheenRoughnessTexture", gmat.sheen.roughnessTexture);
        }

        // KHR_materials_transmission
        if (tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME)->second;
            getFloat(ext, "transmissionFactor", gmat.transmission.factor);
            getTexId(ext, "transmissionTexture", gmat.transmission.texture);
        }

        // KHR_materials_ior
        if (tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME)->second;
            getFloat(ext, "ior", gmat.ior.ior);
        }

        // KHR_materials_volume
        if (tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME)->second;
            getFloat(ext, "thicknessFactor", gmat.volume.thicknessFactor);
            getTexId(ext, "thicknessTexture", gmat.volume.thicknessTexture);
            getFloat(ext, "attenuationDistance", gmat.volume.attenuationDistance);
            getVec3(ext, "attenuationColor", gmat.volume.attenuationColor);
        };


        m_materials.emplace_back(gmat);
    }

    // Make default
    if (m_materials.empty())
    {
        GltfMaterial gmat;
        gmat.metallicFactor = 0;
        m_materials.emplace_back(gmat);
    }
}

void GltfScene::importDrawableNodes(const tinygltf::Model& tmodel, GltfAttributes attributes)
{
    checkRequiredExtensions(tmodel);

    // Find the number of vertex(attributes) and index
    //uint32_t nbVert{0};
    uint32_t nbIndex{ 0 };
    uint32_t meshCnt{ 0 };  // use for mesh to new meshes
    uint32_t primCnt{ 0 };  //  "   "  "  "
    for (const auto& mesh : tmodel.meshes)
    {
        std::vector<uint32_t> vprim;
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.mode != 4)  // Triangle
                continue;
            const auto& posAccessor = tmodel.accessors[primitive.attributes.find("POSITION")->second];
            //nbVert += static_cast<uint32_t>(posAccessor.count);
            if (primitive.indices > -1)
            {
                const auto& indexAccessor = tmodel.accessors[primitive.indices];
                nbIndex += static_cast<uint32_t>(indexAccessor.count);
            }
            else
            {
                nbIndex += static_cast<uint32_t>(posAccessor.count);
            }
            vprim.emplace_back(primCnt++);
        }
        m_meshToPrimMeshes[meshCnt++] = std::move(vprim);  // mesh-id = { prim0, prim1, ... }
    }

    // Reserving memory
    m_indices.reserve(nbIndex);

    // Convert all mesh/primitives+ to a single primitive per mesh
    for (const auto& tmesh : tmodel.meshes)
    {
        for (const auto& tprimitive : tmesh.primitives)
        {
            processMesh(tmodel, tprimitive, attributes, tmesh.name);
        }
    }

    // Transforming the scene hierarchy to a flat list
    int         defaultScene = tmodel.defaultScene > -1 ? tmodel.defaultScene : 0;
    const auto& tscene = tmodel.scenes[defaultScene];
    for (auto nodeIdx : tscene.nodes)
    {
        processNode(tmodel, nodeIdx, glm::mat4(1));
    }

    computeSceneDimensions();
    //computeCamera();

    m_meshToPrimMeshes.clear();
    primitiveIndices32u.clear();
    primitiveIndices16u.clear();
    primitiveIndices8u.clear();
}

void GltfScene::processNode(const tinygltf::Model& tmodel, int& nodeIdx, const glm::mat4& parentMatrix)
{
    const auto& tnode = tmodel.nodes[nodeIdx];

    glm::mat4 matrix = getLocalMatrix(tnode);
    glm::mat4 worldMatrix = parentMatrix * matrix;

    if (tnode.mesh > -1)
    {
        const auto& meshes = m_meshToPrimMeshes[tnode.mesh];  // A mesh could have many primitives
        for (const auto& mesh : meshes)
        {
            GltfNode node;
            node.primMesh = mesh;
            node.worldMatrix = worldMatrix;
            m_nodes.emplace_back(node);
        }
    }
    else if (tnode.camera > -1)
    {
        GltfCamera camera;
        camera.worldMatrix = worldMatrix;
        camera.cam = tmodel.cameras[tmodel.nodes[nodeIdx].camera];

        // If the node has the Iray extension, extract the camera information.
        if (hasExtension(tnode.extensions, EXTENSION_ATTRIB_IRAY))
        {
            auto& iray_ext = tnode.extensions.at(EXTENSION_ATTRIB_IRAY);
            auto& attributes = iray_ext.Get("attributes");
            for (size_t idx = 0; idx < attributes.ArrayLen(); idx++)
            {
                auto& attrib = attributes.Get((int)idx);
                std::string attName = attrib.Get("name").Get<std::string>();
                auto& attValue = attrib.Get("value");
                if (attValue.IsArray())
                {
                    auto vec = getVector<float>(attValue);
                    if (attName == "iview:position")
                        camera.eye = { vec[0], vec[1], vec[2] };
                    else if (attName == "iview:interest")
                        camera.center = { vec[0], vec[1], vec[2] };
                    else if (attName == "iview:up")
                        camera.up = { vec[0], vec[1], vec[2] };
                }
            }
        }

        m_cameras.emplace_back(camera);
    }
    else if (tnode.extensions.find(KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME) != tnode.extensions.end())
    {
        GltfLight   light;
        const auto& ext = tnode.extensions.find(KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME)->second;
        auto        lightIdx = ext.Get("light").GetNumberAsInt();
        light.light = tmodel.lights[lightIdx];
        light.worldMatrix = worldMatrix;
        m_lights.emplace_back(light);
    }

    // Recursion for all children
    for (auto child : tnode.children)
    {
        processNode(tmodel, child, worldMatrix);
    }
}

void GltfScene::processMesh(const tinygltf::Model& tmodel, const tinygltf::Primitive& tmesh, GltfAttributes attributes, const std::string& name)
{
    // Only triangles are supported
    // 0:point, 1:lines, 2:line_loop, 3:line_strip, 4:triangles, 5:triangle_strip, 6:triangle_fan
    if (tmesh.mode != 4)
        return;

    GltfPrimMesh resultMesh;
    resultMesh.name = name;
    resultMesh.materialIndex = std::max(0, tmesh.material);
    resultMesh.vertexOffset = static_cast<uint32_t>(m_positions.size());
    resultMesh.firstIndex = static_cast<uint32_t>(m_indices.size());

    // Create a key made of the attributes, to see if the primitive was already
    // processed. If it is, we will re-use the cache, but allow the material and
    // indices to be different.
    std::stringstream o;
    for (auto& a : tmesh.attributes)
    {
        o << a.first << a.second;
    }
    std::string key = o.str();
    bool        primMeshCached = false;

    // Found a cache - will not need to append vertex
    auto it = m_cachePrimMesh.find(key);
    if (it != m_cachePrimMesh.end())
    {
        primMeshCached = true;
        GltfPrimMesh cacheMesh = it->second;
        resultMesh.vertexCount = cacheMesh.vertexCount;
        resultMesh.vertexOffset = cacheMesh.vertexOffset;
    }


    // INDICES
    if (tmesh.indices > -1)
    {
        const tinygltf::Accessor& indexAccessor = tmodel.accessors[tmesh.indices];
        const tinygltf::BufferView& bufferView = tmodel.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& buffer = tmodel.buffers[bufferView.buffer];

        resultMesh.indexCount = static_cast<uint32_t>(indexAccessor.count);

        switch (indexAccessor.componentType)
        {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            primitiveIndices32u.resize(indexAccessor.count);
            memcpy(primitiveIndices32u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
                indexAccessor.count * sizeof(uint32_t));
            m_indices.insert(m_indices.end(), primitiveIndices32u.begin(), primitiveIndices32u.end());
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            primitiveIndices16u.resize(indexAccessor.count);
            memcpy(primitiveIndices16u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
                indexAccessor.count * sizeof(uint16_t));
            m_indices.insert(m_indices.end(), primitiveIndices16u.begin(), primitiveIndices16u.end());
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            primitiveIndices8u.resize(indexAccessor.count);
            memcpy(primitiveIndices8u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
                indexAccessor.count * sizeof(uint8_t));
            m_indices.insert(m_indices.end(), primitiveIndices8u.begin(), primitiveIndices8u.end());
            break;
        }
        default:
            std::cerr << "Index component type " << indexAccessor.componentType << " not supported!" << std::endl;
            return;
        }
    }
    else
    {
        // Primitive without indices, creating them
        const auto& accessor = tmodel.accessors[tmesh.attributes.find("POSITION")->second];
        for (auto i = 0; i < accessor.count; i++)
            m_indices.push_back(i);
        resultMesh.indexCount = static_cast<uint32_t>(accessor.count);
    }

    if (primMeshCached == false)  // Need to add this primitive
    {

        // POSITION
        {
            bool result = getAttribute<glm::vec3>(tmodel, tmesh, m_positions, "POSITION");

            // Keeping the size of this primitive (Spec says this is required information)
            const auto& accessor = tmodel.accessors[tmesh.attributes.find("POSITION")->second];
            resultMesh.vertexCount = static_cast<uint32_t>(accessor.count);
            if (!accessor.minValues.empty())
                resultMesh.posMin = glm::vec3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
            if (!accessor.maxValues.empty())
                resultMesh.posMax = glm::vec3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
        }

        // NORMAL
        if ((attributes & GltfAttributes::Normal) == GltfAttributes::Normal)
        {
            if (!getAttribute<glm::vec3>(tmodel, tmesh, m_normals, "NORMAL"))
            {
                // Need to compute the normals
                std::vector<glm::vec3> geonormal(resultMesh.vertexCount);
                for (size_t i = 0; i < resultMesh.indexCount; i += 3)
                {
                    uint32_t    ind0 = m_indices[resultMesh.firstIndex + i + 0];
                    uint32_t    ind1 = m_indices[resultMesh.firstIndex + i + 1];
                    uint32_t    ind2 = m_indices[resultMesh.firstIndex + i + 2];
                    const auto& pos0 = m_positions[ind0 + resultMesh.vertexOffset];
                    const auto& pos1 = m_positions[ind1 + resultMesh.vertexOffset];
                    const auto& pos2 = m_positions[ind2 + resultMesh.vertexOffset];
                    const auto  v1 = glm::normalize(pos1 - pos0);  // Many normalize, but when objects are really small the
                    const auto  v2 = glm::normalize(pos2 - pos0);  // cross will go below nv_eps and the normal will be (0,0,0)
                    const auto  n = glm::cross(v2, v1);
                    geonormal[ind0] += n;
                    geonormal[ind1] += n;
                    geonormal[ind2] += n;
                }
                for (auto& n : geonormal)
                    n = glm::normalize(n);
                m_normals.insert(m_normals.end(), geonormal.begin(), geonormal.end());
            }
        }

        // TEXCOORD_0
        if ((attributes & GltfAttributes::Texcoord_0) == GltfAttributes::Texcoord_0)
        {
            if (!getAttribute<glm::vec2>(tmodel, tmesh, m_texcoords0, "TEXCOORD_0"))
            {
                // Set them all to zero
                //      m_texcoords0.insert(m_texcoords0.end(), resultMesh.vertexCount, glm::vec2(0, 0));

                // Cube map projection
                for (uint32_t i = 0; i < resultMesh.vertexCount; i++)
                {
                    const auto& pos = m_positions[resultMesh.vertexOffset + i];
                    float       absX = fabs(pos.x);
                    float       absY = fabs(pos.y);
                    float       absZ = fabs(pos.z);

                    int isXPositive = pos.x > 0 ? 1 : 0;
                    int isYPositive = pos.y > 0 ? 1 : 0;
                    int isZPositive = pos.z > 0 ? 1 : 0;

                    float maxAxis, uc, vc;

                    // POSITIVE X
                    if (isXPositive && absX >= absY && absX >= absZ)
                    {
                        // u (0 to 1) goes from +z to -z
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absX;
                        uc = -pos.z;
                        vc = pos.y;
                    }
                    // NEGATIVE X
                    if (!isXPositive && absX >= absY && absX >= absZ)
                    {
                        // u (0 to 1) goes from -z to +z
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absX;
                        uc = pos.z;
                        vc = pos.y;
                    }
                    // POSITIVE Y
                    if (isYPositive && absY >= absX && absY >= absZ)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from +z to -z
                        maxAxis = absY;
                        uc = pos.x;
                        vc = -pos.z;
                    }
                    // NEGATIVE Y
                    if (!isYPositive && absY >= absX && absY >= absZ)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from -z to +z
                        maxAxis = absY;
                        uc = pos.x;
                        vc = pos.z;
                    }
                    // POSITIVE Z
                    if (isZPositive && absZ >= absX && absZ >= absY)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absZ;
                        uc = pos.x;
                        vc = pos.y;
                    }
                    // NEGATIVE Z
                    if (!isZPositive && absZ >= absX && absZ >= absY)
                    {
                        // u (0 to 1) goes from +x to -x
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absZ;
                        uc = -pos.x;
                        vc = pos.y;
                    }

                    // Convert range from -1 to 1 to 0 to 1
                    float u = 0.5f * (uc / maxAxis + 1.0f);
                    float v = 0.5f * (vc / maxAxis + 1.0f);

                    m_texcoords0.emplace_back(u, v);
                }
            }
        }


        // TANGENT
        if ((attributes & GltfAttributes::Tangent) == GltfAttributes::Tangent)
        {
            if (!getAttribute<glm::vec4>(tmodel, tmesh, m_tangents, "TANGENT"))
            {
                std::vector<glm::vec3> tangent(resultMesh.vertexCount);
                std::vector<glm::vec3> bitangent(resultMesh.vertexCount);

                // Current implementation
                // http://foundationsofgameenginedev.com/FGED2-sample.pdf
                for (size_t i = 0; i < resultMesh.indexCount; i += 3)
                {
                    // local index
                    uint32_t i0 = m_indices[resultMesh.firstIndex + i + 0];
                    uint32_t i1 = m_indices[resultMesh.firstIndex + i + 1];
                    uint32_t i2 = m_indices[resultMesh.firstIndex + i + 2];
                    assert(i0 < resultMesh.vertexCount);
                    assert(i1 < resultMesh.vertexCount);
                    assert(i2 < resultMesh.vertexCount);


                    // global index
                    uint32_t gi0 = i0 + resultMesh.vertexOffset;
                    uint32_t gi1 = i1 + resultMesh.vertexOffset;
                    uint32_t gi2 = i2 + resultMesh.vertexOffset;

                    const auto& p0 = m_positions[gi0];
                    const auto& p1 = m_positions[gi1];
                    const auto& p2 = m_positions[gi2];

                    const auto& uv0 = m_texcoords0[gi0];
                    const auto& uv1 = m_texcoords0[gi1];
                    const auto& uv2 = m_texcoords0[gi2];

                    glm::vec3 e1 = p1 - p0;
                    glm::vec3 e2 = p2 - p0;

                    glm::vec2 duvE1 = uv1 - uv0;
                    glm::vec2 duvE2 = uv2 - uv0;

                    float r = 1.0F;
                    float a = duvE1.x * duvE2.y - duvE2.x * duvE1.y;
                    if (fabs(a) > 0)  // Catch degenerated UV
                    {
                        r = 1.0f / a;
                    }

                    glm::vec3 t = (e1 * duvE2.y - e2 * duvE1.y) * r;
                    glm::vec3 b = (e2 * duvE1.x - e1 * duvE2.x) * r;

                    tangent[i0] += t;
                    tangent[i1] += t;
                    tangent[i2] += t;

                    bitangent[i0] += b;
                    bitangent[i1] += b;
                    bitangent[i2] += b;
                }

                for (uint32_t a = 0; a < resultMesh.vertexCount; a++)
                {
                    const auto& t = tangent[a];
                    const auto& b = bitangent[a];
                    const auto& n = m_normals[resultMesh.vertexOffset + a];

                    // Gram-Schmidt orthogonalize
                    glm::vec3 tangent = glm::normalize(t - (glm::dot(n, t) * n));

                    // Calculate handedness
                    float handedness = (glm::dot(glm::cross(n, t), b) < 0.0F) ? -1.0F : 1.0F;
                    m_tangents.emplace_back(tangent.x, tangent.y, tangent.z, handedness);
                }
            }
        }

        // COLOR_0
        if ((attributes & GltfAttributes::Color_0) == GltfAttributes::Color_0)
        {
            if (!getAttribute<glm::vec4>(tmodel, tmesh, m_colors0, "COLOR_0"))
            {
                // Set them all to one
                m_colors0.insert(m_colors0.end(), resultMesh.vertexCount, glm::vec4(1, 1, 1, 1));
            }
        }
    }

    // Keep result in cache
    m_cachePrimMesh[key] = resultMesh;

    // Append prim mesh to the list of all primitive meshes
    m_primMeshes.emplace_back(resultMesh);
}

void GltfScene::checkRequiredExtensions(const tinygltf::Model& tmodel)
{
    std::set<std::string> supportedExtensions{
        KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME,
        KHR_TEXTURE_TRANSFORM_EXTENSION_NAME,
        KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME,
        KHR_MATERIALS_UNLIT_EXTENSION_NAME,
        KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME,
        KHR_MATERIALS_IOR_EXTENSION_NAME,
        KHR_MATERIALS_VOLUME_EXTENSION_NAME,
        KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME,
    };

    for (auto& e : tmodel.extensionsRequired)
    {
        if (supportedExtensions.find(e) == supportedExtensions.end())
        {
            std::cerr << "\n---------------------------------------\n" <<
                "The extension " << e.c_str() << " is REQUIRED and not supported \n";
        }
    }
}

//void GltfScene::computeCamera()
//{
//    for (auto& camera : m_cameras)
//    {
//        if (camera.eye == camera.center)  // Applying the rule only for uninitialized camera.
//        {
//            camera.worldMatrix.get_translation(camera.eye);
//            float distance = nvmath::length(m_dimensions.center - camera.eye);
//            auto  rotMat = camera.worldMatrix.get_rot_mat3();
//            camera.center = { 0, 0, -distance };
//            camera.center = camera.eye + (rotMat * camera.center);
//            camera.up = { 0, 1, 0 };
//        }
//    }
//}
