#include "NavigationService.h"
#include "app/AppState.h"

void NavigationService::begin() {
    _source.begin();
}

namespace {
    bool navDataChanged(const NavData& a, const NavData& b) {
        return a.valid != b.valid ||
               a.active != b.active ||
               a.isNavigation != b.isNavigation ||
               a.eta != b.eta ||
               a.duration != b.duration ||
               a.distance != b.distance ||
               a.title != b.title ||
               a.directions != b.directions ||
               a.speed != b.speed;
    }
}  // namespace

void NavigationService::tick() {
    NavData data = _source.poll();
    if (!data.valid) return;  // nothing to publish yet

    if (!navDataChanged(data, _lastPublished)) return;  // no real change — don't bump .version for nothing

    _lastPublished = data;

    AppState::NavSnapshot snapshot;
    snapshot.active = data.active;
    snapshot.isNavigation = data.isNavigation;
    snapshot.eta = data.eta;
    snapshot.duration = data.duration;
    snapshot.distance = data.distance;
    snapshot.title = data.title;
    snapshot.directions = data.directions;
    snapshot.speed = data.speed;
    snapshot.valid = data.valid;
    // .version is assigned inside AppState::setNav() itself, not here

    AppState::setNav(snapshot);
}