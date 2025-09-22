#include "geometry/BasicGeometry.h"

BasicGeometry::~BasicGeometry() {
    if (initialized) {
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
        glDeleteBuffers(1, &mEBO);
    }
}

void BasicGeometry::Draw() const
{
    if (!initialized) return;

    // bind texture
    mpMaterial->OnBind();

    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, GLsizei(mIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void BasicGeometry::SetupMesh()
{
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(Vertex), &mVertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(unsigned int), &mIndices[0], GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    if (mbHasUV)
    {
        // uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    }

    glBindVertexArray(0);
    initialized = true;
}

std::optional<te::AaBB> BasicGeometry::GetAABB(bool update)
{
    if (update || !mAabb)
    {
        int vSize = mVertices.size();
        if (!(mAabb) && (vSize > 0))
        {
            mAabb = te::AaBB();
        }
        if (vSize > 0)
        {
            te::AaBB abab;
            toAabb(abab, mVertices.data(), vSize, sizeof(Vertex)/*GetVertexStride()*/ , /*GetPositionOffset()*/ 0 );
            mAabb = abab;
        }
    }

    return mAabb;
}

std::optional<te::AaBB> BasicGeometry::GetLocalAABB()
{
    if (!mAabb)
    {
        GetAABB(true);
        mAabb = mAabb.value().ApplyTransform(mLocalTransform);
    }

    return mAabb;
}

std::optional<te::AaBB> BasicGeometry::GetWorldAABB()
{
    if (!(mAabb))
    {
        GetAABB(true);
    }
    auto worldAABB = mAabb.value().ApplyTransform(mWorldTransform);
    return worldAABB;
}

void BasicGeometry::SetLocalTransform(const glm::mat4& trn)
{
    mLocalTransform = trn;
}

void BasicGeometry::SetWorldTransform(const glm::mat4& trn)
{
    mWorldTransform = trn;
}

Box::Box(float width, float height, float depth)
    : m_Width(width), m_Height(height), m_Depth(depth)
{
    CreateBox();

    //material
    auto pPhongMaterial = std::make_shared<PhongMaterial>();
    pPhongMaterial->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    SetMaterial(pPhongMaterial);
}

void Box::SetPosition(const glm::vec3& pos)
{
    m_Pos = pos;
    CreateBox();
}

void Box::SetSize(float width, float height, float depth)
{
    m_Width = width;
    m_Height = height;
    m_Depth = depth;
    CreateBox();
}

void Box::SetWidth(float width)
{
    m_Width = width;
    CreateBox();
}

void Box::SetHeight(float height)
{
    m_Height = height;
    CreateBox();
}

void Box::SetDepth(float depth)
{
    m_Depth = depth;
    CreateBox();
}

glm::vec3 Box::GetPosition() const noexcept
{
    return m_Pos;
}

void Box::CreateBox()
{
    mVertices.clear();
    mIndices.clear();
    
    // calculate half size
    float w = m_Width * 0.5f;
    float h = m_Height * 0.5f;
    float d = m_Depth * 0.5f;
    
    // define 8 vertices (relative to center point)
    std::vector<glm::vec3> positions = {
        // front (Z+)
        glm::vec3(-w, -h,  d) + m_Pos,  // 0: left bottom
        glm::vec3( w, -h,  d) + m_Pos,  // 1: right bottom
        glm::vec3( w,  h,  d) + m_Pos,  // 2: right top
        glm::vec3(-w,  h,  d) + m_Pos,  // 3: left top
        
        // back (Z-)
        glm::vec3(-w, -h, -d) + m_Pos,  // 4: left bottom
        glm::vec3( w, -h, -d) + m_Pos,  // 5: right bottom
        glm::vec3( w,  h, -d) + m_Pos,  // 6: right top
        glm::vec3(-w,  h, -d) + m_Pos   // 7: left top
    };
    
    // define normals
    std::vector<glm::vec3> normals = {
        glm::vec3( 0,  0,  1),  // front
        glm::vec3( 0,  0, -1),  // back
        glm::vec3( 1,  0,  0),  // right
        glm::vec3(-1,  0,  0),  // left
        glm::vec3( 0,  1,  0),  // top
        glm::vec3( 0, -1,  0)   // bottom
    };
    
    // define texture coordinates
    std::vector<glm::vec2> texCoords = {
        glm::vec2(0.0f, 0.0f),  // left bottom
        glm::vec2(1.0f, 0.0f),  // right bottom
        glm::vec2(1.0f, 1.0f),  // right top
        glm::vec2(0.0f, 1.0f)   // left top
    };
    
    // create vertices data
    // front (0, 1, 2, 3)
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[i];
        vertex.normal = normals[0];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // back (4, 5, 6, 7) - note that the vertex order should be counter-clockwise
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[7 - i];  // reverse
        vertex.normal = normals[1];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // right (1, 5, 6, 2)
    std::vector<int> rightFace = {1, 5, 6, 2};
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[rightFace[i]];
        vertex.normal = normals[2];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // left (4, 0, 3, 7)
    std::vector<int> leftFace = {4, 0, 3, 7};
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[leftFace[i]];
        vertex.normal = normals[3];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // top (3, 2, 6, 7)
    std::vector<int> topFace = {3, 2, 6, 7};
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[topFace[i]];
        vertex.normal = normals[4];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // bottom (4, 7, 6, 5) - note that the vertex order should be counter-clockwise
    std::vector<int> bottomFace = {4, 7, 6, 5};
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[bottomFace[i]];
        vertex.normal = normals[5];
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }
    
    // create index data (each face has 2 triangles)
    for (int face = 0; face < 6; ++face) {
        int baseIndex = face * 4;
        
        // first triangle
        mIndices.push_back(baseIndex + 0);
        mIndices.push_back(baseIndex + 1);
        mIndices.push_back(baseIndex + 2);
        
        // second triangle
        mIndices.push_back(baseIndex + 0);
        mIndices.push_back(baseIndex + 2);
        mIndices.push_back(baseIndex + 3);
    }
    
    // set UV flag
    mbHasUV = true;
    
    // set mesh
    SetupMesh();
}

//plane geometry
Plane::Plane(float width, float height)
    : m_Width(width), m_Height(height)
{
    CreatePlane();

    //material
    auto pPhongMaterial = std::make_shared<PhongMaterial>();
    pPhongMaterial->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    SetMaterial(pPhongMaterial);
}

void Plane::SetPosition(const glm::vec3& pos)
{
    m_Pos = pos;
    CreatePlane();
}

void Plane::SetSize(float width, float height)
{
    m_Width = width;
    m_Height = height;
    CreatePlane();
}

void Plane::SetWidth(float width)
{
    m_Width = width;
    CreatePlane();
}

void Plane::SetHeight(float height)
{
    m_Height = height;
    CreatePlane();
}

glm::vec3 Plane::GetPosition() const noexcept
{
    return m_Pos;
}

void Plane::CreatePlane()
{
    mVertices.clear();
    mIndices.clear();

    // calculate half size
    float w = m_Width * 0.5f;
    float h = m_Height * 0.5f;

    // define 8 vertices (relative to center point)
    std::vector<glm::vec3> positions = {
        // front (Z+)
        glm::vec3(-w, -h,  0) + m_Pos,  // 0: left bottom
        glm::vec3(w, -h,  0) + m_Pos,  // 1: right bottom
        glm::vec3(w,  h,  0) + m_Pos,  // 2: right top
        glm::vec3(-w,  h,  0) + m_Pos,  // 3: left top
    };

    // define normals
    glm::vec3 normal = glm::cross((positions[1]- positions[0]), (positions[2] - positions[0]));

    // define texture coordinates
    std::vector<glm::vec2> texCoords = {
        glm::vec2(0.0f, 0.0f),  // left bottom
        glm::vec2(1.0f, 0.0f),  // right bottom
        glm::vec2(1.0f, 1.0f),  // right top
        glm::vec2(0.0f, 1.0f)   // left top
    };

    // create vertices data
    // front (0, 1, 2, 3)
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = positions[i];
        vertex.normal = normal;
        vertex.texCoords = texCoords[i];
        mVertices.push_back(vertex);
    }

    // create index data (each face has 2 triangles)
    // first triangle
    mIndices.push_back(0);
    mIndices.push_back(1);
    mIndices.push_back(2);

    // second triangle
    mIndices.push_back(0);
    mIndices.push_back(2);
    mIndices.push_back(3);

    // set UV flag
    mbHasUV = true;

    // set mesh
    SetupMesh();
}