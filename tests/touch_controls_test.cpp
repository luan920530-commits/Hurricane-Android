// Ejecuta la logica real del mando con SDL simulado, sin Android ni OpenGL.
#include "SDLPort/TouchControls.hpp"
#include "SDLPort/DebugPanel.hpp"
#include "Player.hpp"
#include "Globals.hpp"
#include "Logdatei.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

static std::map<SDL_TouchID, std::vector<SDL_Finger>> devices;
extern "C" int SDL_GetNumTouchDevices() { return static_cast<int>(devices.size()); }
extern "C" SDL_TouchID SDL_GetTouchDevice(int i) { auto it = devices.begin(); std::advance(it, i); return it->first; }
extern "C" int SDL_GetNumTouchFingers(SDL_TouchID id) { return static_cast<int>(devices[id].size()); }
extern "C" SDL_Finger *SDL_GetTouchFinger(SDL_TouchID id, int i) { return &devices[id][i]; }

static Uint8 keyboard[MAX_KEYS] = {};
const Uint8 *TastaturPuffer = keyboard;
GameStateEnum SpielZustand = GameStateEnum::GAMELOOP;
DebugPanelClass DebugPanel;
void DebugPanelClass::SetOpen(bool open) { open_ = open; }
Logdatei::Logdatei(const std::string &) {}
Logdatei::~Logdatei() = default;
void Logdatei::flush() {}
Logdatei Protokoll("");

static SDL_Rect area{200, 0, 800, 600}; // ventana panoramica con bandas laterales
static SDL_Finger finger(SDL_FingerID id, float x, float y) {
    return SDL_Finger{id, x / 1200.0f, y / 600.0f, 1.0f};
}
static void update() { TouchControls.Update(area, 1200, 600); }
static void event(Uint32 type, SDL_TouchID device, SDL_FingerID id, float x, float y) {
    SDL_TouchFingerEvent f{};
    f.type = type; f.touchId = device; f.fingerId = id; f.x = x / 1200.0f; f.y = y / 600.0f;
    TouchControls.HandleFingerEvent(f, area, 1200, 600);
}

#undef main
int main() {
    TouchControls.Init();
    assert(TouchControls.IsActive()); // disponible antes de que SDL anuncie un dispositivo
    devices[0] = {finger(10, 336, 444)}; // id de dispositivo 0 es valido
    update();
    devices[0] = {finger(10, 396, 410), finger(11, 800, 516)}; // derecha + arriba + fuego
    update();
    assert(TouchControls.PlayerAction(AKTION_RECHTS));
    assert(TouchControls.PlayerAction(AKTION_OBEN));
    assert(TouchControls.PlayerAction(AKTION_SHOOT));
    assert(!TouchControls.PlayerAction(AKTION_JUMP));
    assert(keyboard[SDL_SCANCODE_LCTRL] == 0); // no interfiere con reasignaciones

    // Reordenar dedos y sacar el joystick de su zona no cambia su identidad.
    devices[0] = {finger(11, 800, 516), finger(10, 660, 444), finger(12, 920, 480)};
    update();
    assert(TouchControls.PlayerAction(AKTION_RECHTS));
    assert(TouchControls.PlayerAction(AKTION_JUMP));
    assert(TouchControls.PlayerAction(AKTION_SHOOT));
    // El dedo de movimiento se suelta antes que los botones, sin dejarlo atascado.
    devices[0] = {finger(11, 800, 516), finger(12, 920, 480)};
    update();
    assert(!TouchControls.PlayerAction(AKTION_RECHTS));
    assert(TouchControls.PlayerAction(AKTION_JUMP));
    devices[0].clear(); update();
    assert(!TouchControls.PlayerAction(AKTION_JUMP));
    assert(!TouchControls.PlayerAction(AKTION_SHOOT));

    // Cada accion llega a su accion correspondiente independientemente del teclado.
    struct Case { float x, y; int action; };
    const Case cases[] = {{920,480,AKTION_JUMP},{800,516,AKTION_SHOOT},
        {920,348,AKTION_BLITZ},{832,348,AKTION_POWERLINE}, {744,348,AKTION_GRANATE},
        {656,348,AKTION_SMARTBOMB},{656,486,AKTION_WAFFEN_CYCLE}};
    for (auto c : cases) {
        devices[0] = {finger(1,c.x,c.y)}; update(); assert(TouchControls.PlayerAction(c.action));
        devices[0].clear(); update(); assert(!TouchControls.PlayerAction(c.action));
    }
    // Otro dispositivo y una pulsacion completa entre frames.
    devices.clear(); devices[-2] = {finger(42, 920, 480)}; update();
    assert(TouchControls.PlayerAction(AKTION_JUMP));
    devices[-2].clear(); update();
    event(SDL_FINGERDOWN, -2, 42, 656, 486);
    event(SDL_FINGERUP, -2, 42, 656, 486);
    update(); assert(TouchControls.PlayerAction(AKTION_WAFFEN_CYCLE));
    update(); assert(!TouchControls.PlayerAction(AKTION_WAFFEN_CYCLE));

    // En menus: direccion y confirmar; tambien preservar las teclas fisicas.
    SpielZustand = GameStateEnum::MAINMENU;
    keyboard[SDL_SCANCODE_LCTRL] = 1;
    devices[-2] = {finger(1,336,444)}; update();
    devices[-2] = {finger(1,336,390), finger(2,800,516)}; update();
    assert(keyboard[SDL_SCANCODE_UP] && keyboard[SDL_SCANCODE_RETURN]);
    assert(keyboard[SDL_SCANCODE_LCTRL]);
    devices[-2].clear(); std::memset(keyboard, 0, sizeof(keyboard)); update();

    // Panel, suspension de app y edicion no dejan acciones pulsadas.
    SpielZustand = GameStateEnum::GAMELOOP;
    devices[-2] = {finger(1,800,516)}; update();
    TouchControls.CancelTouches(); DebugPanel.SetOpen(true); update();
    assert(!TouchControls.PlayerAction(AKTION_SHOOT));
    DebugPanel.SetOpen(false); update(); // dedo de cierre todavia apoyado
    assert(!TouchControls.PlayerAction(AKTION_SHOOT));
    devices[-2].clear(); update();
    TouchControls.ToggleEditing();
    event(SDL_FINGERDOWN, -2, 1, 920, 480);
    event(SDL_FINGERMOTION, -2, 1, 840, 420);
    update(); // no debe cancelar el arrastre
    event(SDL_FINGERMOTION, -2, 1, 820, 420);
    event(SDL_FINGERUP, -2, 1, 820, 420);
    TouchControls.ToggleEditing();
    devices[-2] = {finger(1,820,420)}; update();
    assert(TouchControls.PlayerAction(AKTION_JUMP));
    devices[-2].clear(); update();
    TouchControls.Shutdown();
    std::cout << "touch_controls_test: OK\n";
    return 0;
}
