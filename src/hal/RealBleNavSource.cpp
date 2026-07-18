#include "RealBleNavSource.h"

void RealBleNavSource::begin() {
    _watch.begin();  // starts advertising; Chronos app connects to us, we don't scan for it
}

NavData RealBleNavSource::poll() {
    // ChronosESP32 needs its own housekeeping pumped regularly for BLE to
    // work at all. NavigationService already calls poll() once per loop()
    // tick (see decisions log: NavigationService concurrency), so that's
    // the natural place for it — no separate task needed.
    _watch.loop();

    Navigation nav = _watch.getNavigation();

    NavData data;
    data.active = nav.active;
    data.isNavigation = nav.isNavigation;
    data.eta = nav.eta;
    data.duration = nav.duration;
    data.distance = nav.distance;
    data.title = nav.title;
    data.directions = nav.directions;
    data.speed = nav.speed;

    // `valid` means "we have a real reading from a connected phone" — NOT
    // "a turn is in progress." Deliberately NOT gated on isNavigation: if
    // that were false when nav genuinely ends, NavigationService would
    // just stop publishing and NavScreen would show stale directions
    // forever — the exact bug class SpotifyService's "nothing playing"
    // transition already burned us on once. isNavigation/active/directions
    // are left for NavScreen to interpret, same as everything else pending
    // real BLE traffic.
    data.valid = _watch.isConnected();

    return data;
}