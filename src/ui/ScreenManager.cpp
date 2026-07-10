#include "ScreenManager.h"

ScreenManager::ScreenManager(Renderer& renderer, IInput& input, Screen** screens, int count)
    : _renderer(renderer), _input(input), _screens(screens), _count(count), _index(0) {}

void ScreenManager::begin() {
    _input.begin();
    if (_count > 0) {
        _screens[_index]->onEnter();
        _screens[_index]->render(_renderer);  // screen's render() calls beginFrame() itself
    }
}

void ScreenManager::tick() {
    InputEvent event = _input.poll();

    if (event == InputEvent::Next) {
        switchTo((_index + 1) % _count);
        return;
    } else if (event == InputEvent::Back) {
        switchTo(0);
        return;
    } else if (event == InputEvent::Select) {
        _screens[_index]->handleInput(event);
    }

    if (_screens[_index]->update()) {
        _screens[_index]->render(_renderer);
    }
}

void ScreenManager::switchTo(int newIndex) {
    if (newIndex == _index || _count == 0) return;
    _screens[_index]->onExit();
    _index = newIndex;
    _screens[_index]->onEnter();
    _screens[_index]->render(_renderer);  // screen's render() calls beginFrame() itself
}