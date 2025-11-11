#pragma once
#include "mesh/Mesh.h"
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Texture2D;

class ModelLoader
{
    public:
        ModelLoader();
        ~ModelLoader();

        std::vector<std::shared_ptr<Mesh>>& GetMeshList() noexcept
        {
           return mMeshList;
        }

        void loadModel(const std::string& path);

    private:
        // model data
        std::vector<std::shared_ptr<Mesh>> mMeshList;
        std::string mDirectory;

        void processNode(aiNode *node, const aiScene *scene);
        std::shared_ptr<Mesh> processMesh(aiMesh *mesh, const aiScene *scene);

        std::shared_ptr<MaterialBase> processMaterial(aiMesh* mesh, const aiScene* scene);
};