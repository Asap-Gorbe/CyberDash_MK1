#include "NavigationService.h"
#include "app/AppState.h"

void NavigationService::begin() {
    _source.begin();
}

namespace {
    bool navDataChanged(const NavData& a, const NavData& b) {
        return a.valid != b.valid ||
               a.eta != b.eta ||
               a.minutesRemaining != b.minutesRemaining ||
               a.distanceRemainingM != b.distanceRemainingM ||
               a.destination != b.destination ||
               a.currentStreet != b.currentStreet ||
               a.nextTurnDistanceM != b.nextTurnDistanceM;
    }
}  // namespace

void NavigationService::tick() {
    NavData data = _source.poll();
    if (!data.valid) return;  // nothing to publish yet

    if (!navDataChanged(data, _lastPublished)) return;  // no real change — don't bump .version for nothing

    _lastPublished = data;

    AppState::NavSnapshot snapshot;
    snapshot.eta = data.eta;
    snapshot.minutesRemaining = data.minutesRemaining;
    snapshot.distanceRemainingM = data.distanceRemainingM;
    snapshot.destination = data.destination;
    snapshot.currentStreet = data.currentStreet;
    snapshot.nextTurnDistanceM = data.nextTurnDistanceM;
    snapshot.valid = data.valid;
    // .version is assigned inside AppState::setNav() itself, not here

    AppState::setNav(snapshot);
}