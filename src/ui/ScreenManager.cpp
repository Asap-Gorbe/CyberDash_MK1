#include "ScreenManager.h"

ScreenManager::ScreenManager(Renderer& renderer, IInput& input, Screen** screens, int count)
    : _renderer(renderer), _input(input), _screens(screens), _count(count), _index(0) {}

void ScreenManager::begin() {
    _input.begin();
    _renderer.clear();
    if (_count > 0) {
        _screens[_index]->onEnter();
        _screens[_index]->render(_renderer);  // draw once immediately, don't wait for a dirty tick
    }
}

void ScreenManager::tick() {
    InputEvent event = _input.poll();

    if (event == InputEvent::Next) {
        switchTo((_index + 1) % _count);
        return;  // switchTo() already rendered the new screen once
    } else if (event == InputEvent::Back) {
        switchTo(0);
        return;
    } else if (event == InputEvent::Select) {
        _screens[_index]->handleInput(event);
    }

    // Only redraw if the active screen reports its content actually
    // changed — not on every tick. This is what stops the constant
    // full-screen ANSI clear/redraw storm.
    if (_screens[_index]->update()) {
        _screens[_index]->render(_renderer);
    }
}

void ScreenManager::switchTo(int newIndex) {
    if (newIndex == _index || _count == 0) return;
    _screens[_index]->onExit();
    _index = newIndex;
    _renderer.clear();
    _screens[_index]->onEnter();
    _screens[_index]->render(_renderer);  // draw once immediately on switch
}