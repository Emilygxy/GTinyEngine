#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

enum class Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;

/**
 * @enum EProjectionMode
 * @brief Enumeration representing different projection modes.
 */
enum class CProjectionMode
{
    PERSPECTIVE = 0, ///< Perspective projection mode.
    ORTHOGRAPHIC    ///< Orthographic projection mode.
};

class Camera;

class Camera_Event : public std::enable_shared_from_this<Camera_Event>
{
public:
    Camera_Event() = delete;
    Camera_Event(const std::shared_ptr<Camera>& camera);
    ~Camera_Event();
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float deta);
private:
    std::shared_ptr<Camera> mpCamera;
};

class Camera : public std::enable_shared_from_this<Camera>
{
public:
    friend class Camera_Event;

    Camera() = delete;
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);
    ~Camera() {}

    Camera(const Camera& rhs);
    Camera& operator=(const Camera& rhs);

    // If you invoke these function, the camera no longer updates the projection matrix
    void SetProjectionMatrix(glm::mat4 projectionMatrix);
    void SetViewMatrix(glm::mat4 viewMatrix);

    glm::mat4 GetProjectionMatrix() const noexcept;
    glm::mat4 GetViewMatrix() const noexcept;
    glm::vec3 GetEye() const noexcept;
    void SetEye(glm::vec3 position);
    glm::vec3 GetTarget() const noexcept;
    void SetTarget(glm::vec3 target);
    glm::vec3 GetUp() const noexcept;
    void SetUp(glm::vec3 up);
    glm::vec3 GetRight() const noexcept;
    void SetRight(glm::vec3 right);
    float GetFov() const noexcept;
    void SetFov(float fov);

    glm::vec3 GetFront() const noexcept;
    void SetFront(glm::vec3 up);

    void SetAspectRatio(float aspectRatio);
    float GetAspectRatio() const noexcept;
    void SetNearPlane(float nearPlane);
    float GetNearPlane() const noexcept;
    void SetFarPlane(float farPlane);
    float GetFarPlane() const noexcept;

    void SetOrthoScale(float scale);
    float GetOrthoScale() const noexcept;

    void SetLookAt(
		float posX, float posY, float posZ,
		float atX, float atY, float atZ,
		float upX, float upY, float upZ);
    
    CProjectionMode GetProjectionMode() const noexcept { return mProjectionMode; }
	void SetProjectionMode(CProjectionMode mode);

    void SetPerspective(float fov, float asp, float zNear, float zFar);
    void SetPerspective(float left, float right, float bottom, float up, float zNear, float zFar);

private:
    void updateCameraVectors(); 
    void SyncProjectMatrix();
    void SyncViewMatrix();

    glm::vec3 mPosition;
    glm::vec3 mTarget;

    glm::vec3 mFront; // vector of front
    glm::vec3 mUp; // vector of up
    glm::vec3 mRight; // vector of right
    glm::vec3 mWorldUp;

    float mYaw;
    float mPitch;
    float mMovementSpeed;
    float mMouseSensitivity;
    glm::vec4 mFov;

    float mAspectRatio; // screen rate
    float mNearPlane;
    float mFarPlane;
    float mDistance; // the length of frustom = far-near

    float mOrthoScale;

    glm::mat4 mViewMatrix;
    glm::mat4 mProjectionMatrix;

    CProjectionMode mProjectionMode{ CProjectionMode::PERSPECTIVE };

    mutable bool mViewDirty{ true };
    mutable bool mProjDirty{ true };
    bool mbUseDirectViewMatrix{ false };
    bool mbUseDirectProjMatrix{ false };

    static constexpr float sDeg2Rad = 0.0174532924F;
};