#pragma once
#include <memory>
#include <vector>
#include <set>
#include <string>
#include <glm/glm.hpp>

const std::string kEvent_PositionChanged = "POSITION_CHANGED";
const std::string kEvent_OrientationChanged = "ORIENTATION_CHANGED";
const std::string kEvent_ProjectionChanged = "PROJECTION_CHANGED";

// forward declearation
class Subject;

// Observer API
class Observer : public std::enable_shared_from_this<Observer> {
public:
    virtual ~Observer() = default;
    virtual void OnNotify(const std::shared_ptr<Subject>& node, const std::string& event) = 0;
};

// Subject API 
class Subject : public std::enable_shared_from_this<Subject> {
public:
    virtual ~Subject() = default;

    void AddObserver(const std::shared_ptr<Observer>& observer);
    void RemoveObserver(const std::shared_ptr<Observer>& observer);

protected:
    void Notify(const std::string& event);

private:
    std::set<std::weak_ptr<Observer>, std::owner_less<std::weak_ptr<Observer>>> mObservers;
};

