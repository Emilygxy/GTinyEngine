#include "ObserverModeObject.h"

void Subject::AddObserver(const std::shared_ptr<Observer>& observer) {
    mObservers.insert(std::weak_ptr<Observer>(observer));
}

void Subject::RemoveObserver(const std::shared_ptr<Observer>& observer)
{
    // simply handling
    for (auto it = mObservers.begin(); it != mObservers.end(); ) {
        if (auto obs = it->lock()) {
            if (obs == observer) {
                it = mObservers.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void Subject::Notify(const std::string& event)
{
    std::vector<std::shared_ptr<Observer>> activeObservers;

    for (auto it = mObservers.begin(); it != mObservers.end(); ) {
        if (auto obs = it->lock()) {
            activeObservers.push_back(obs);
            ++it;
        }
        else {
            it = mObservers.erase(it); //remove expired observer
        }
    }

    for (const auto& obs : activeObservers) {
        obs->OnNotify(shared_from_this(), event);
    }
}
