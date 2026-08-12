#include "GltfInternal.hpp"

namespace heritage::graphics::gltf_internal {

bool parseEmbeddedImage(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int imageIndex,
    ImageSource& image,
    std::string& error)
{
    const JsonValue* images = root.find("images");
    if (!images || !images->isArray()
        || imageIndex < 0
        || static_cast<std::size_t>(imageIndex) >= images->arrayValue.size())
    {
        error = "Invalid GLB image index.";
        return false;
    }

    const JsonValue& value = images->arrayValue[static_cast<std::size_t>(imageIndex)];
    if (const JsonValue* bufferViewValue = value.find("bufferView"); bufferViewValue)
    {
        const auto bufferViewInfo = getBufferViewInfo(root, bufferViewValue->asInt(-1));
        if (!bufferViewInfo)
        {
            error = "GLB image references an invalid bufferView.";
            return false;
        }
        if (bufferViewInfo->buffer != 0
            || bufferViewInfo->byteOffset + bufferViewInfo->byteLength > binaryChunk.size())
        {
            error = "GLB image exceeds the binary buffer.";
            return false;
        }

        auto embedded = std::make_shared<EmbeddedTextureData>();
        embedded->key = heritage::paths::toUtf8(assetPath) + "#image" + std::to_string(imageIndex);
        embedded->mimeType = value.find("mimeType") ? value.find("mimeType")->asString() : std::string();
        embedded->bytes.assign(
            binaryChunk.begin() + static_cast<std::ptrdiff_t>(bufferViewInfo->byteOffset),
            binaryChunk.begin() + static_cast<std::ptrdiff_t>(bufferViewInfo->byteOffset + bufferViewInfo->byteLength));
        image.texture.embedded = std::move(embedded);
        image.texture.flipVerticalOnDecode = false;
        return true;
    }

    if (const JsonValue* uriValue = value.find("uri"); uriValue && uriValue->isString())
    {
        image.texture.filePath =
            (assetPath.parent_path() / heritage::paths::fromUtf8(uriValue->asString())).lexically_normal();
        image.texture.flipVerticalOnDecode = false;
        return true;
    }

    error = "GLB image does not contain supported image data.";
    return false;
}

std::optional<ImageSource> buildTextureSource(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int textureIndex,
    std::string& error)
{
    const JsonValue* textures = root.find("textures");
    if (!textures || !textures->isArray()
        || textureIndex < 0
        || static_cast<std::size_t>(textureIndex) >= textures->arrayValue.size())
    {
        error = "Invalid GLB texture index.";
        return std::nullopt;
    }

    const JsonValue& textureValue = textures->arrayValue[static_cast<std::size_t>(textureIndex)];
    const int sourceIndex = textureValue.find("source") ? textureValue.find("source")->asInt(-1) : -1;
    if (sourceIndex < 0)
    {
        error = "GLB texture does not declare an image source.";
        return std::nullopt;
    }

    ImageSource image;
    if (!parseEmbeddedImage(root, binaryChunk, assetPath, sourceIndex, image, error))
        return std::nullopt;
    return image;
}

std::string materialKey(const JsonValue& root, int materialIndex)
{
    const JsonValue* materials = root.find("materials");
    std::string name;
    if (materials && materials->isArray()
        && materialIndex >= 0
        && static_cast<std::size_t>(materialIndex) < materials->arrayValue.size())
    {
        const JsonValue& value = materials->arrayValue[static_cast<std::size_t>(materialIndex)];
        if (const JsonValue* nameValue = value.find("name"); nameValue && nameValue->isString())
            name = nameValue->asString();
    }

    if (name.empty())
        name = "glTF Material " + std::to_string(materialIndex);
    else
        name += " [glTF #" + std::to_string(materialIndex) + "]";
    return name;
}

MaterialDefinition buildMaterialDefinition(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int materialIndex,
    std::string& warning)
{
    MaterialDefinition material;
    material.name = materialKey(root, materialIndex);
    material.sourceDirectory = assetPath.parent_path();

    const JsonValue* materials = root.find("materials");
    if (!materials || !materials->isArray()
        || materialIndex < 0
        || static_cast<std::size_t>(materialIndex) >= materials->arrayValue.size())
    {
        return material;
    }

    const JsonValue& value = materials->arrayValue[static_cast<std::size_t>(materialIndex)];
    if (const JsonValue* pbr = value.find("pbrMetallicRoughness"); pbr && pbr->isObject())
    {
        const auto baseColorFactor = readVec4(pbr->find("baseColorFactor"), { 1.0f, 1.0f, 1.0f, 1.0f });
        material.baseColor = { baseColorFactor[0], baseColorFactor[1], baseColorFactor[2] };
        material.opacity = std::clamp(baseColorFactor[3], 0.0f, 1.0f);
        material.metallic = std::clamp(
            static_cast<float>(pbr->find("metallicFactor") ? pbr->find("metallicFactor")->asDouble(1.0) : 1.0),
            0.0f,
            1.0f);
        material.roughness = std::clamp(
            static_cast<float>(pbr->find("roughnessFactor") ? pbr->find("roughnessFactor")->asDouble(1.0) : 1.0),
            0.04f,
            1.0f);

        if (const JsonValue* baseColorTexture = pbr->find("baseColorTexture");
            baseColorTexture && baseColorTexture->isObject())
        {
            std::string error;
            const auto texture = buildTextureSource(
                root,
                binaryChunk,
                assetPath,
                baseColorTexture->find("index") ? baseColorTexture->find("index")->asInt(-1) : -1,
                error);
            if (texture)
                material.baseColorMap = texture->texture;
            else if (!error.empty())
                warning = error;
        }

        if (const JsonValue* metallicRoughnessTexture = pbr->find("metallicRoughnessTexture");
            metallicRoughnessTexture && metallicRoughnessTexture->isObject())
        {
            std::string error;
            const auto texture = buildTextureSource(
                root,
                binaryChunk,
                assetPath,
                metallicRoughnessTexture->find("index")
                    ? metallicRoughnessTexture->find("index")->asInt(-1)
                    : -1,
                error);
            if (texture)
            {
                material.roughnessMap = texture->texture;
                material.metallicMap = texture->texture;
                material.roughnessMap.channel = TextureChannel::G;
                material.metallicMap.channel = TextureChannel::B;
            }
            else if (!error.empty())
            {
                warning = error;
            }
        }
    }

    // glTF KHR_materials_specular extends the metallic-roughness workflow
    // without replacing base color / roughness / metallic. This lets Heritage
    // keep the normal PBR material while exposing explicit dielectric specular
    // strength and color from Blender/glTF.
    if (const JsonValue* extensions = value.find("extensions");
        extensions && extensions->isObject())
    {
        if (const JsonValue* specular = extensions->find("KHR_materials_specular");
            specular && specular->isObject())
        {
            material.specularFactor = std::clamp(
                static_cast<float>(specular->find("specularFactor")
                    ? specular->find("specularFactor")->asDouble(1.0)
                    : 1.0),
                0.0f,
                1.0f);

            const auto colorFactor = readVec3(
                specular->find("specularColorFactor"),
                { 1.0f, 1.0f, 1.0f });
            // Heritage's lightweight shader stores dielectric F0 directly.
            // glTF's default dielectric reflectance is approximately 0.04.
            material.specularColor = {
                0.04f * std::clamp(colorFactor[0], 0.0f, 1.0f),
                0.04f * std::clamp(colorFactor[1], 0.0f, 1.0f),
                0.04f * std::clamp(colorFactor[2], 0.0f, 1.0f) };

            if (const JsonValue* specularTexture = specular->find("specularTexture");
                specularTexture && specularTexture->isObject())
            {
                std::string error;
                const auto texture = buildTextureSource(
                    root,
                    binaryChunk,
                    assetPath,
                    specularTexture->find("index")
                        ? specularTexture->find("index")->asInt(-1)
                        : -1,
                    error);
                if (texture)
                {
                    material.specularFactorMap = texture->texture;
                    material.specularFactorMap.channel = TextureChannel::A;
                }
                else if (!error.empty())
                {
                    warning = error;
                }
            }

            if (const JsonValue* specularColorTexture = specular->find("specularColorTexture");
                specularColorTexture && specularColorTexture->isObject())
            {
                std::string error;
                const auto texture = buildTextureSource(
                    root,
                    binaryChunk,
                    assetPath,
                    specularColorTexture->find("index")
                        ? specularColorTexture->find("index")->asInt(-1)
                        : -1,
                    error);
                if (texture)
                    material.specularMap = texture->texture;
                else if (!error.empty())
                    warning = error;
            }
        }
    }

    if (const JsonValue* normalTexture = value.find("normalTexture");
        normalTexture && normalTexture->isObject())
    {
        std::string error;
        const auto texture = buildTextureSource(
            root,
            binaryChunk,
            assetPath,
            normalTexture->find("index") ? normalTexture->find("index")->asInt(-1) : -1,
            error);
        if (texture)
            material.normalMap = texture->texture;
        else if (!error.empty())
            warning = error;
    }

    if (const JsonValue* occlusionTexture = value.find("occlusionTexture");
        occlusionTexture && occlusionTexture->isObject())
    {
        std::string error;
        const auto texture = buildTextureSource(
            root,
            binaryChunk,
            assetPath,
            occlusionTexture->find("index") ? occlusionTexture->find("index")->asInt(-1) : -1,
            error);
        if (texture)
            material.ambientOcclusionMap = texture->texture;
        else if (!error.empty())
            warning = error;
    }

    material.emissiveColor = { 0.0f, 0.0f, 0.0f };
    if (const JsonValue* emissiveFactor = value.find("emissiveFactor"); emissiveFactor)
    {
        const auto e = readVec3(emissiveFactor, { 0.0f, 0.0f, 0.0f });
        material.emissiveColor = { e[0], e[1], e[2] };
    }

    if (const JsonValue* emissiveTexture = value.find("emissiveTexture");
        emissiveTexture && emissiveTexture->isObject())
    {
        std::string error;
        const auto texture = buildTextureSource(
            root,
            binaryChunk,
            assetPath,
            emissiveTexture->find("index") ? emissiveTexture->find("index")->asInt(-1) : -1,
            error);
        if (texture)
            material.emissiveMap = texture->texture;
        else if (!error.empty())
            warning = error;
    }

    if (const JsonValue* alphaMode = value.find("alphaMode"); alphaMode && alphaMode->isString())
    {
        if (alphaMode->asString() == "MASK")
        {
            const float cutoff = static_cast<float>(
                value.find("alphaCutoff") ? value.find("alphaCutoff")->asDouble(0.5) : 0.5);
            if (material.opacity < cutoff)
                material.opacity = 0.0f;
        }
    }

    return material;
}

void collectExternalTextureDependencies(
    const MaterialDefinition& material,
    std::unordered_set<std::string>& deduplicated,
    std::vector<std::filesystem::path>& output)
{
    const MaterialTextureReference* references[] = {
        &material.baseColorMap,
        &material.normalMap,
        &material.roughnessMap,
        &material.metallicMap,
        &material.specularMap,
        &material.specularFactorMap,
        &material.ambientOcclusionMap,
        &material.emissiveMap,
        &material.opacityMap };

    for (const MaterialTextureReference* reference : references)
    {
        if (!reference || reference->filePath.empty())
            continue;
        const std::string key = heritage::paths::toUtf8(reference->filePath);
        if (deduplicated.insert(key).second)
            output.push_back(reference->filePath);
    }
}

int attributeAccessorIndex(const JsonValue* attributes, const char* name)
{
    if (!attributes || !attributes->isObject())
        return -1;
    const JsonValue* value = attributes->find(name);
    return value ? value->asInt(-1) : -1;
}

void appendPrimitive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    const JsonValue& primitive,
    int nodeIndex,
    int skinIndex,
    Mesh& mesh,
    std::unordered_set<std::string>& dependencySet)
{
    const int mode = primitive.find("mode") ? primitive.find("mode")->asInt(4) : 4;
    if (mode != 4)
    {
        std::cerr << "GLB primitive warning: only TRIANGLES mode is currently supported." << '\n';
        return;
    }

    const JsonValue* attributes = primitive.find("attributes");
    const int positionAccessorIndex = attributeAccessorIndex(attributes, "POSITION");
    if (positionAccessorIndex < 0)
    {
        std::cerr << "GLB primitive warning: POSITION accessor is missing." << '\n';
        return;
    }

    std::string error;
    AccessorInfo positionAccessor;
    {
        const auto accessorInfo = getAccessorInfo(root, positionAccessorIndex);
        if (!accessorInfo)
        {
            std::cerr << "GLB primitive warning: invalid POSITION accessor." << '\n';
            return;
        }
        positionAccessor = *accessorInfo;
    }

    std::vector<float> positions;
    if (!readFloatAccessor(root, binaryChunk, positionAccessorIndex, 3, positions, error))
    {
        std::cerr << "GLB primitive warning: " << error << '\n';
        return;
    }

    const std::size_t vertexCount = positionAccessor.count;
    std::vector<float> normals;
    const int normalAccessorIndex = attributeAccessorIndex(attributes, "NORMAL");
    if (normalAccessorIndex >= 0)
    {
        if (!readFloatAccessor(root, binaryChunk, normalAccessorIndex, 3, normals, error)
            || normals.size() / 3 != vertexCount)
        {
            normals.clear();
        }
    }

    std::vector<float> texcoords;
    const int texcoordAccessorIndex = attributeAccessorIndex(attributes, "TEXCOORD_0");
    if (texcoordAccessorIndex >= 0)
    {
        if (!readFloatAccessor(root, binaryChunk, texcoordAccessorIndex, 2, texcoords, error)
            || texcoords.size() / 2 != vertexCount)
        {
            texcoords.clear();
        }
    }

    std::vector<float> tangents;
    const int tangentAccessorIndex = attributeAccessorIndex(attributes, "TANGENT");
    if (tangentAccessorIndex >= 0)
    {
        if (!readFloatAccessor(root, binaryChunk, tangentAccessorIndex, 4, tangents, error)
            || tangents.size() / 4 != vertexCount)
        {
            tangents.clear();
        }
    }

    std::vector<float> colors;
    bool primitiveHasVertexColors = false;
    const int colorAccessorIndex = attributeAccessorIndex(attributes, "COLOR_0");
    if (colorAccessorIndex >= 0)
    {
        const auto colorAccessor = getAccessorInfo(root, colorAccessorIndex);
        if (colorAccessor)
        {
            const std::size_t colorComponents = componentCount(colorAccessor->type);
            if (colorComponents == 3 || colorComponents == 4)
            {
                std::vector<float> sourceColors;
                if (readFloatAccessor(
                        root,
                        binaryChunk,
                        colorAccessorIndex,
                        colorComponents,
                        sourceColors,
                        error)
                    && sourceColors.size() / colorComponents == vertexCount)
                {
                    colors.resize(vertexCount * 4, 1.0f);
                    for (std::size_t colorIndex = 0; colorIndex < vertexCount; ++colorIndex)
                    {
                        colors[colorIndex * 4 + 0] = sourceColors[colorIndex * colorComponents + 0];
                        colors[colorIndex * 4 + 1] = sourceColors[colorIndex * colorComponents + 1];
                        colors[colorIndex * 4 + 2] = sourceColors[colorIndex * colorComponents + 2];
                        if (colorComponents == 4)
                            colors[colorIndex * 4 + 3] = sourceColors[colorIndex * colorComponents + 3];
                    }
                    primitiveHasVertexColors = true;
                }
            }
        }
    }

    std::vector<float> joints;
    const int jointsAccessorIndex = attributeAccessorIndex(attributes, "JOINTS_0");
    if (jointsAccessorIndex >= 0)
    {
        if (!readFloatAccessor(root, binaryChunk, jointsAccessorIndex, 4, joints, error)
            || joints.size() / 4 != vertexCount)
        {
            joints.clear();
        }
    }

    std::vector<float> weights;
    const int weightsAccessorIndex = attributeAccessorIndex(attributes, "WEIGHTS_0");
    if (weightsAccessorIndex >= 0)
    {
        if (!readFloatAccessor(root, binaryChunk, weightsAccessorIndex, 4, weights, error)
            || weights.size() / 4 != vertexCount)
        {
            weights.clear();
        }
    }

    std::vector<unsigned int> indices;
    const int indexAccessorIndex = primitive.find("indices") ? primitive.find("indices")->asInt(-1) : -1;
    if (indexAccessorIndex >= 0)
    {
        if (!readIndexAccessor(root, binaryChunk, indexAccessorIndex, indices, error))
        {
            std::cerr << "GLB primitive warning: " << error << '\n';
            return;
        }
    }
    else
    {
        indices.resize(vertexCount);
        for (std::size_t i = 0; i < vertexCount; ++i)
            indices[i] = static_cast<unsigned int>(i);
    }

    const int materialIndex = primitive.find("material") ? primitive.find("material")->asInt(-1) : -1;
    std::string materialName;
    if (materialIndex >= 0)
    {
        materialName = materialKey(root, materialIndex);
        if (mesh.materials.find(materialName) == mesh.materials.end())
        {
            std::string warning;
            MaterialDefinition material =
                buildMaterialDefinition(root, binaryChunk, assetPath, materialIndex, warning);
            if (!warning.empty())
                std::cerr << "GLB material warning: " << warning << '\n';
            collectExternalTextureDependencies(material, dependencySet, mesh.sourceDependencies);
            mesh.materials.emplace(materialName, std::move(material));
        }
    }

    const unsigned int baseVertex =
        static_cast<unsigned int>(mesh.vertices.size() / mesh.vertexStrideFloats);
    for (std::size_t i = 0; i < vertexCount; ++i)
    {
        const std::array<float, 3> sourceNormal =
            normals.empty()
            ? std::array<float, 3>{ 0.0f, 1.0f, 0.0f }
            : std::array<float, 3>{
                normals[i * 3 + 0],
                normals[i * 3 + 1],
                normals[i * 3 + 2] };
        // glTF TEXCOORD_0 is authoritative. Do not rotate, mirror or otherwise
        // reinterpret authored UV coordinates. Blender and conforming glTF
        // viewers use these values exactly as exported.
        const std::array<float, 2> sourceTexcoord =
            texcoords.empty()
            ? std::array<float, 2>{ 0.0f, 0.0f }
            : std::array<float, 2>{
                texcoords[i * 2 + 0],
                texcoords[i * 2 + 1] };
        const std::array<float, 4> sourceTangent =
            tangents.empty()
            ? std::array<float, 4>{ 1.0f, 0.0f, 0.0f, 1.0f }
            : std::array<float, 4>{
                tangents[i * 4 + 0],
                tangents[i * 4 + 1],
                tangents[i * 4 + 2],
                tangents[i * 4 + 3] };
        std::array<float, 4> sourceJoints = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (!joints.empty())
        {
            sourceJoints = {
                joints[i * 4 + 0],
                joints[i * 4 + 1],
                joints[i * 4 + 2],
                joints[i * 4 + 3] };
        }
        std::array<float, 4> sourceWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
        if (!weights.empty())
        {
            sourceWeights = {
                weights[i * 4 + 0],
                weights[i * 4 + 1],
                weights[i * 4 + 2],
                weights[i * 4 + 3] };
        }
        const std::array<float, 4> sourceColor =
            colors.empty()
            ? std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }
            : std::array<float, 4>{
                colors[i * 4 + 0],
                colors[i * 4 + 1],
                colors[i * 4 + 2],
                colors[i * 4 + 3] };

        mesh.vertices.insert(
            mesh.vertices.end(),
            {
                positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2],
                sourceNormal[0], sourceNormal[1], sourceNormal[2],
                sourceTexcoord[0], sourceTexcoord[1],
                sourceTangent[0], sourceTangent[1], sourceTangent[2], sourceTangent[3],
                sourceJoints[0], sourceJoints[1], sourceJoints[2], sourceJoints[3],
                sourceWeights[0], sourceWeights[1], sourceWeights[2], sourceWeights[3],
                sourceColor[0], sourceColor[1], sourceColor[2], sourceColor[3] });
    }

    MeshDrawRange range;
    range.firstIndex = mesh.indices.size();
    range.indexCount = indices.size();
    range.materialName = materialName;
    range.nodeIndex = nodeIndex;
    range.skinIndex = skinIndex;
    range.hasVertexColors = primitiveHasVertexColors;
    mesh.drawRanges.push_back(range);

    for (unsigned int index : indices)
        mesh.indices.push_back(baseVertex + index);

    if (skinIndex >= 0 && !joints.empty() && !weights.empty())
        mesh.hasSkinning = true;
    if (primitiveHasVertexColors)
        mesh.hasVertexColors = true;
}

void appendNodeRecursive(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    const std::filesystem::path& assetPath,
    int nodeIndex,
    Mesh& mesh,
    std::unordered_set<std::string>& dependencySet)
{
    const JsonValue* nodes = root.find("nodes");
    if (!nodes || !nodes->isArray()
        || nodeIndex < 0
        || static_cast<std::size_t>(nodeIndex) >= nodes->arrayValue.size())
    {
        return;
    }

    const JsonValue& node = nodes->arrayValue[static_cast<std::size_t>(nodeIndex)];
    if (const JsonValue* meshIndexValue = node.find("mesh"); meshIndexValue)
    {
        const JsonValue* meshes = root.find("meshes");
        const int meshIndex = meshIndexValue->asInt(-1);
        if (meshes && meshes->isArray()
            && meshIndex >= 0
            && static_cast<std::size_t>(meshIndex) < meshes->arrayValue.size())
        {
            const int skinIndex = node.find("skin") ? node.find("skin")->asInt(-1) : -1;
            const JsonValue& meshValue = meshes->arrayValue[static_cast<std::size_t>(meshIndex)];
            if (const JsonValue* primitives = meshValue.find("primitives"); primitives && primitives->isArray())
            {
                for (const JsonValue& primitive : primitives->arrayValue)
                    appendPrimitive(root, binaryChunk, assetPath, primitive, nodeIndex, skinIndex, mesh, dependencySet);
            }
        }
    }

    if (const JsonValue* children = node.find("children"); children && children->isArray())
    {
        for (const JsonValue& child : children->arrayValue)
            appendNodeRecursive(
                root,
                binaryChunk,
                assetPath,
                child.asInt(-1),
                mesh,
                dependencySet);
    }
}

void buildSkins(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    Mesh& mesh)
{
    const JsonValue* skins = root.find("skins");
    if (!skins || !skins->isArray())
        return;

    mesh.skins.reserve(skins->arrayValue.size());
    for (std::size_t skinIndex = 0; skinIndex < skins->arrayValue.size(); ++skinIndex)
    {
        const JsonValue& skinValue = skins->arrayValue[skinIndex];
        MeshSkin skin;
        skin.joints = readIntArray(skinValue.find("joints"));
        skin.inverseBindMatrices.assign(
            skin.joints.size(),
            { 1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              0.0f, 0.0f, 0.0f, 1.0f });

        if (const JsonValue* inverseBind = skinValue.find("inverseBindMatrices"); inverseBind)
        {
            std::vector<std::array<float, 16>> matrices;
            std::string error;
            if (readMat4Accessor(root, binaryChunk, inverseBind->asInt(-1), matrices, error))
            {
                const std::size_t count = std::min(skin.inverseBindMatrices.size(), matrices.size());
                for (std::size_t i = 0; i < count; ++i)
                    skin.inverseBindMatrices[i] = matrices[i];
            }
            else if (!error.empty())
            {
                std::cerr << "GLB skin warning: " << error << '\n';
            }
        }

        mesh.skins.push_back(std::move(skin));
    }
}

void buildAnimations(
    const JsonValue& root,
    const std::vector<std::uint8_t>& binaryChunk,
    Mesh& mesh)
{
    const JsonValue* animations = root.find("animations");
    if (!animations || !animations->isArray())
        return;

    for (std::size_t animationIndex = 0; animationIndex < animations->arrayValue.size(); ++animationIndex)
    {
        const JsonValue& animationValue = animations->arrayValue[animationIndex];
        const JsonValue* samplers = animationValue.find("samplers");
        const JsonValue* channels = animationValue.find("channels");
        if (!samplers || !samplers->isArray() || !channels || !channels->isArray())
            continue;

        AnimationClip clip;
        if (const JsonValue* nameValue = animationValue.find("name"); nameValue && nameValue->isString())
            clip.name = nameValue->asString();
        if (clip.name.empty())
            clip.name = "Animation " + std::to_string(animationIndex);

        for (const JsonValue& channelValue : channels->arrayValue)
        {
            const int samplerIndex = channelValue.find("sampler") ? channelValue.find("sampler")->asInt(-1) : -1;
            const JsonValue* target = channelValue.find("target");
            const int nodeIndex = target && target->find("node") ? target->find("node")->asInt(-1) : -1;
            const std::string path = target && target->find("path") ? target->find("path")->asString() : std::string();
            if (samplerIndex < 0
                || static_cast<std::size_t>(samplerIndex) >= samplers->arrayValue.size()
                || nodeIndex < 0)
            {
                continue;
            }
            if (path != "translation" && path != "rotation" && path != "scale")
                continue;

            const JsonValue& samplerValue = samplers->arrayValue[static_cast<std::size_t>(samplerIndex)];
            const int inputAccessor = samplerValue.find("input") ? samplerValue.find("input")->asInt(-1) : -1;
            const int outputAccessor = samplerValue.find("output") ? samplerValue.find("output")->asInt(-1) : -1;
            if (inputAccessor < 0 || outputAccessor < 0)
                continue;

            std::string interpolation = samplerValue.find("interpolation")
                ? samplerValue.find("interpolation")->asString()
                : std::string("LINEAR");

            std::vector<float> times;
            std::string error;
            if (!readFloatAccessor(root, binaryChunk, inputAccessor, 1, times, error))
            {
                if (!error.empty())
                    std::cerr << "GLB animation warning: " << error << '\n';
                continue;
            }

            const auto outputInfo = getAccessorInfo(root, outputAccessor);
            if (!outputInfo)
                continue;
            const std::size_t components = componentCount(outputInfo->type);
            if ((path == "rotation" && components < 4) || (path != "rotation" && components < 3))
                continue;

            std::vector<float> values;
            if (!readFloatAccessor(
                    root,
                    binaryChunk,
                    outputAccessor,
                    path == "rotation" ? 4u : 3u,
                    values,
                    error))
            {
                if (!error.empty())
                    std::cerr << "GLB animation warning: " << error << '\n';
                continue;
            }
            if (times.empty())
                continue;

            const std::size_t keyMultiplier = interpolation == "CUBICSPLINE" ? 3u : 1u;
            const std::size_t expectedValueCount =
                times.size() * keyMultiplier * (path == "rotation" ? 4u : 3u);
            if (values.size() != expectedValueCount)
            {
                std::cerr
                    << "GLB animation warning: sampler output count does not match input keys for "
                    << clip.name << '\n';
                continue;
            }

            AnimationChannel channel;
            channel.nodeIndex = nodeIndex;
            channel.componentCount = (path == "rotation") ? 4u : 3u;
            channel.times = std::move(times);
            channel.values = std::move(values);
            channel.interpolation = interpolation == "STEP"
                ? AnimationInterpolation::Step
                : (interpolation == "CUBICSPLINE"
                    ? AnimationInterpolation::CubicSpline
                    : AnimationInterpolation::Linear);
            channel.path = path == "translation"
                ? AnimationTargetPath::Translation
                : (path == "rotation" ? AnimationTargetPath::Rotation : AnimationTargetPath::Scale);

            clip.durationSeconds = std::max(clip.durationSeconds, channel.times.back());
            clip.channels.push_back(std::move(channel));
        }

        if (!clip.channels.empty())
            mesh.animations.push_back(std::move(clip));
    }
}



} // namespace heritage::graphics::gltf_internal
