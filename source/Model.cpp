#include "Model.h"
#include "materials/BlinnPhongMaterial.h"
#include "filesystem.h"

namespace
{
    void loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::vector<std::shared_ptr<Texture2D>>& outTextures)
    {
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
            texture->SetTypeName(typeName);
            std::vector<std::string> paths;
            paths.push_back(str.C_Str());
            texture->SetTexturePaths(paths);
            outTextures.push_back(texture);
        }
    }
}

Model::Model()
{
    mMeshList.clear();
}

Model::~Model()
{
    mMeshList.clear();
}

// loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
void Model::loadModel(std::string const& path)
{
    auto modelFullFile = FileSystem::getPath(path);

    // read file via ASSIMP
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelFullFile, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    // check for errors
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
    {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return;
    }
    // retrieve the directory path of the filepath
    mDirectory = modelFullFile.substr(0, modelFullFile.find_last_of('/'));

    // process ASSIMP's root node recursively
    processNode(scene->mRootNode, scene);
}

// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
void Model::processNode(aiNode* node, const aiScene* scene)
{
    // process each mesh located at the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        // the node object only contains indices to index the actual objects in the scene. 
        // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        mMeshList.push_back(processMesh(mesh, scene));
    }
    // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

std::shared_ptr<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    // data to fill
    std::shared_ptr<Mesh> pMesh = std::make_shared<Mesh>();
    auto& vertices = pMesh->VerticesRef();
    auto& indices = pMesh->IndicesRef();

    // walk through each of the mesh's vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;
        // normals
        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
        }
        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.texCoords = vec;

            //todo
            //// tangent
            //vector.x = mesh->mTangents[i].x;
            //vector.y = mesh->mTangents[i].y;
            //vector.z = mesh->mTangents[i].z;
            //vertex.Tangent = vector;
            //// bitangent
            //vector.x = mesh->mBitangents[i].x;
            //vector.y = mesh->mBitangents[i].y;
            //vector.z = mesh->mBitangents[i].z;
            //vertex.Bitangent = vector;
        }
        else
            vertex.texCoords = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }
    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    // process materials
    pMesh->SetMaterial(processMaterial(mesh, scene));

    // return a mesh object created from the extracted mesh data
    return pMesh;
}

std::shared_ptr<MaterialBase> Model::processMaterial(aiMesh* mesh, const aiScene* scene)
{
    std::vector<std::shared_ptr<Texture2D>> textures;

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
    // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
    // Same applies to other texture as the following list summarizes:
    // diffuse: texture_diffuseN
    // specular: texture_specularN
    // normal: texture_normalN
    std::shared_ptr<BlinnPhongMaterial> pMaterial = std::make_shared<BlinnPhongMaterial>();
    // 1. diffuse maps
    std::vector<std::shared_ptr<Texture2D>> diffuseMaps;
    loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", diffuseMaps);
    if (!diffuseMaps.empty())
    {
        pMaterial->SetDiffuseTexture(diffuseMaps[0]);
    }
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<std::shared_ptr<Texture2D>> specularMaps;
    loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", specularMaps);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<std::shared_ptr<Texture2D>> normalMaps;
    loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", normalMaps);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<std::shared_ptr<Texture2D>> heightMaps;
    loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", heightMaps);
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    return pMaterial;
}