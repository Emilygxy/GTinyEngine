#include "Camera.h"
#include <iostream>
#include <glm/ext.hpp>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
{
    mPosition = position;
    mWorldUp = up;
    mYaw = yaw;
    mPitch = pitch;

    updateCameraVectors();
}

Camera::Camera(const Camera& rhs)
{
    mPosition = rhs.mPosition;
    mTarget = rhs.mTarget;
    mUp = rhs.mUp;
    mRight = rhs.mRight;
    mWorldUp = rhs.mWorldUp;
    mYaw = rhs.mYaw;
    mPitch = rhs.mPitch;
    mMovementSpeed = rhs.mMovementSpeed;
    mMouseSensitivity = rhs.mMouseSensitivity;
    mFov = rhs.mFov;
    mAspectRatio = rhs.mAspectRatio;
    mNearPlane = rhs.mNearPlane;
    mFarPlane = rhs.mFarPlane;
    mDistance = rhs.mDistance;
    mOrthoScale = rhs.mOrthoScale;
    mViewMatrix = rhs.mViewMatrix;
    mProjectionMatrix = rhs.mProjectionMatrix;
    mProjectionMode = rhs.mProjectionMode;
    mProjDirty = rhs.mProjDirty;
    mViewDirty = rhs.mViewDirty;
    mbUseDirectProjMatrix = rhs.mbUseDirectProjMatrix;
    mbUseDirectViewMatrix = rhs.mbUseDirectViewMatrix;

    updateCameraVectors();
}

Camera& Camera::operator=(const Camera& rhs)
{
    mPosition = rhs.mPosition;
    mTarget = rhs.mTarget;
    mUp = rhs.mUp;
    mRight = rhs.mRight;
    mWorldUp = rhs.mWorldUp;
    mYaw = rhs.mYaw;
    mPitch = rhs.mPitch;
    mMovementSpeed = rhs.mMovementSpeed;
    mMouseSensitivity = rhs.mMouseSensitivity;
    mFov = rhs.mFov;
    mAspectRatio = rhs.mAspectRatio;
    mNearPlane = rhs.mNearPlane;
    mFarPlane = rhs.mFarPlane;
    mDistance = rhs.mDistance;
    mOrthoScale = rhs.mOrthoScale;
    mViewMatrix = rhs.mViewMatrix;
    mProjectionMatrix = rhs.mProjectionMatrix;
    mProjectionMode = rhs.mProjectionMode;
    mProjDirty = rhs.mProjDirty;
    mViewDirty = rhs.mViewDirty;
    mbUseDirectProjMatrix = rhs.mbUseDirectProjMatrix;
    mbUseDirectViewMatrix = rhs.mbUseDirectViewMatrix;  

    updateCameraVectors();
    
    return *this;
}

glm::mat4 Camera::GetViewMatrix() const noexcept
{
    const_cast<Camera*>(this)->SyncViewMatrix();
    return mViewMatrix;
}

void Camera::SetViewMatrix(glm::mat4 viewMatrix)
{
    mViewMatrix = viewMatrix;
    mbUseDirectViewMatrix = true;
}

glm::mat4 Camera::GetProjectionMatrix() const noexcept
{
    const_cast<Camera*>(this)->SyncProjectMatrix();
    return mProjectionMatrix;
}

void Camera::SetProjectionMatrix(glm::mat4 projectionMatrix)
{
    mProjectionMatrix = projectionMatrix;
    mbUseDirectProjMatrix = true;
}   

glm::vec3 Camera::GetEye() const noexcept
{
    return mPosition;
}

void Camera::SetEye(glm::vec3 position)
{
    mPosition = position;
}

glm::vec3 Camera::GetTarget() const noexcept
{
    return mTarget;
}

void Camera::SetTarget(glm::vec3 target)
{
    mTarget = target;
}

glm::vec3 Camera::GetUp() const noexcept
{
    return mUp;
}

void Camera::SetUp(glm::vec3 up)
{
    mUp = up;
}

glm::vec3 Camera::GetRight() const noexcept
{
    return mRight;
}

void Camera::SetRight(glm::vec3 right)
{
    mRight = right;
}

float Camera::GetFov() const noexcept
{
    return mFov.x;
}

void Camera::SetFov(float fov)
{
    mFov.x = mFov.y = mFov.z = mFov.w = fov;
    mProjDirty = true;
}

glm::vec3 Camera::GetFront() const noexcept
{
    return mFront;
}

void Camera::SetFront(glm::vec3 front)
{
    mFront = front;
}

void Camera::SetAspectRatio(float aspectRatio)
{
    mAspectRatio = aspectRatio;
    mProjDirty = true;
}

float Camera::GetAspectRatio() const noexcept
{
    return mAspectRatio;
}

void Camera::SetNearPlane(float nearPlane)
{
    mNearPlane = nearPlane;
    mProjDirty = true;
}

float Camera::GetNearPlane() const noexcept
{
    return mNearPlane;
}

void Camera::SetFarPlane(float farPlane)
{
    mFarPlane = farPlane;
    mProjDirty = true;
}

float Camera::GetFarPlane() const noexcept
{
    return mFarPlane;
}

void Camera::SetOrthoScale(float scale)
{
    mOrthoScale = scale;
    mProjDirty = true;
}

float Camera::GetOrthoScale() const noexcept
{
    return mOrthoScale;
}

void Camera::SetLookAt(float posX, float posY, float posZ, float atX, float atY, float atZ, float upX, float upY, float upZ)
{
    mPosition = glm::vec3(posX, posY, posZ);
    mTarget = glm::vec3(atX, atY, atZ);
    mUp = glm::vec3(upX, upY, upZ);

    mViewDirty = true;
    mProjDirty = true;
}

void Camera::SyncProjectMatrix()
{
    if (mProjDirty)
    {
        if (mProjectionMode == CProjectionMode::PERSPECTIVE)
        {
            mProjectionMatrix = glm::perspective(mFov.x * sDeg2Rad, mAspectRatio, mNearPlane, mFarPlane);
        }
        else if (mProjectionMode == CProjectionMode::ORTHOGRAPHIC)
        {
            auto camW = mOrthoScale * mAspectRatio;
            auto camH = mOrthoScale;

            const auto left = -camW * 0.5f;
            const auto right = camW * 0.5f;
            const auto bottom = -camH * 0.5f;
            const auto top = camH * 0.5f;

            mProjectionMatrix = glm::ortho(left, right, bottom, top, mNearPlane, mFarPlane);
        }
        else
        {
            std::cout<<"Unkown projection mode"<<std::endl;
        }

        mbUseDirectProjMatrix = false;
        mProjDirty = false;
    }
}

void Camera::SyncViewMatrix()
{
    static const auto Zero = glm::zero<glm::vec3>();
    if (mViewDirty)
    {
        auto view = mTarget - mPosition;
        if (glm::equal(glm::length(view), (float)0.0, glm::epsilon<float>()))
        {
            mViewDirty = false;
            return;
        }

        auto zAxis = glm::normalize(mTarget - mPosition);
        auto xAxis = glm::cross(zAxis, mUp);
        if (glm::equal(glm::length(xAxis), (float)0.0, glm::epsilon<float>()))
        {
            xAxis = { 1.0f, 0.0f, 0.0f };
            mUp = glm::normalize(glm::cross(zAxis, xAxis));
        }

        mViewMatrix = glm::lookAt(mPosition, mTarget, mUp);

        mViewDirty = false;
    }
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(mYaw)) * cos(glm::radians(mPitch));
    front.y = sin(glm::radians(mPitch));
    front.z = sin(glm::radians(mYaw)) * cos(glm::radians(mPitch));

    mFront = glm::normalize(front);
    mRight = glm::normalize(glm::cross(mFront, mWorldUp));
    mUp = glm::normalize(glm::cross(mRight, mFront));

    mPosition = mTarget - mFront * mDistance;
    mViewMatrix = glm::lookAt(mPosition, mTarget, mUp);
    mProjectionMatrix = glm::perspective(glm::radians(mFov.x), mAspectRatio, mNearPlane, mFarPlane);

    //mCounter++;
}

void Camera::SetProjectionMode(CProjectionMode mode)
{
    if (mProjectionMode != mode)
    {
        mProjectionMode = mode;
        mProjDirty = true;
    }
}

void Camera::SetPerspective(float fov, float asp, float zNear, float zFar)
{
    mFov.x = mFov.y = mFov.z = mFov.w = fov;
    mAspectRatio = asp;
    mNearPlane = zNear;
    mFarPlane = zFar;
    mViewDirty = true;
    mProjDirty = true;
}

void Camera::SetPerspective(float left, float right, float bottom, float up, float zNear, float zFar)
{
    mFov.x = left;
    mFov.y = right;
    mFov.z = bottom;
    mFov.w = up;

    mNearPlane = zNear;
    mFarPlane = zFar;

    mViewDirty = true;
    mProjDirty = true;
}

Camera_Event::Camera_Event(const std::shared_ptr<Camera>& camera)
: mpCamera(camera)
{
    
}

Camera_Event::~Camera_Event() 
{
    mpCamera = nullptr;
}

void Camera_Event::ProcessKeyboard(Camera_Movement direction, float deltaTime)
{
    float velocity = mpCamera->mMovementSpeed * deltaTime;
    if (direction == Camera_Movement::FORWARD)
        mpCamera->mPosition += mpCamera->GetFront() * velocity;
    if (direction == Camera_Movement::BACKWARD)
        mpCamera->mPosition -= mpCamera->GetFront() * velocity;
    if (direction == Camera_Movement::LEFT)
        mpCamera->mPosition -= mpCamera->GetRight() * velocity;
    if (direction == Camera_Movement::RIGHT)
        mpCamera->mPosition += mpCamera->GetRight() * velocity;

    mpCamera->updateCameraVectors();
}

void Camera_Event::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= mpCamera->mMouseSensitivity;
    yoffset *= mpCamera->mMouseSensitivity;

    mpCamera->mYaw += xoffset;
    mpCamera->mPitch += yoffset;

    if (constrainPitch)
    {
        if (mpCamera->mPitch > 89.0f)
        {

        }
    }

    mpCamera->updateCameraVectors();
}
