// Ejercita el mismo recorrido de eventos SDL que usa el juego, sin Android ni GPU.
#include "SDLPort/TouchControls.hpp"
#include "SDLPort/DebugPanel.hpp"
#include "SDLPort/ScreenLayout.hpp"
#include "Player.hpp"
#include "Globals.hpp"
#include "Logdatei.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

static Uint8 keyboard[MAX_KEYS] = {};
const Uint8 *TastaturPuffer = keyboard;
GameStateEnum SpielZustand = GameStateEnum::GAMELOOP;
DebugPanelClass DebugPanel;
void DebugPanelClass::SetOpen(bool open) { open_ = open; }
bool DebugPanelClass::HandleTouch(float, float, bool, const SDL_Rect &) { SetOpen(false); return true; }
Logdatei::Logdatei(const std::string &) {}
Logdatei::~Logdatei() = default;
void Logdatei::flush() {}
Logdatei Protokoll("");

static SDL_Rect area{0, 0, 1200, 600};
static void update() { TouchControls.Update(area, 1200, 600); }
static void event(Uint32 type, SDL_TouchID device, SDL_FingerID id, float x, float y) {
    SDL_TouchFingerEvent f{};
    f.type = type; f.touchId = device; f.fingerId = id; f.x = x / 1200.0f; f.y = y / 600.0f;
    TouchControls.HandleFingerEvent(f, area, 1200, 600);
}
static void down(SDL_TouchID d, SDL_FingerID id, float x, float y) { event(SDL_FINGERDOWN, d, id, x, y); }
static void move(SDL_TouchID d, SDL_FingerID id, float x, float y) { event(SDL_FINGERMOTION, d, id, x, y); }
static void up(SDL_TouchID d, SDL_FingerID id, float x, float y) { event(SDL_FINGERUP, d, id, x, y); }

#undef main
int main() {
    // Ajuste de imagen en 4:3 sin recorte, incluidas ventanas cuadradas.
    for (const auto size : {std::pair<int,int>{2400,1080}, {1920,1080}, {800,600}, {800,800}, {600,900}}) {
        const auto r = ScreenLayout::FitGame(size.first, size.second);
        assert(r.x >= 0 && r.y >= 0 && r.x + r.w <= size.first && r.y + r.h <= size.second);
        assert(std::abs(r.w * 3 - r.h * 4) <= 4);
    }
    TouchControls.Init();
    assert(TouchControls.IsActive());
    // Un deslizamiento completo antes del siguiente frame conserva su punto de inicio.
    down(0,9,100,330); move(0,9,170,330); update();
    assert(TouchControls.PlayerAction(AKTION_RECHTS));
    up(0,9,170,330); update();
    // El joystick en el lateral y dos botones: ids y orden de liberacion distintos.
    down(0,10,204,444); update();
    move(0,10,264,410); down(0,11,900,516); down(-2,12,1080,480); update();
    assert(TouchControls.PlayerAction(AKTION_RECHTS));
    assert(TouchControls.PlayerAction(AKTION_OBEN));
    assert(TouchControls.PlayerAction(AKTION_SHOOT));
    assert(TouchControls.PlayerAction(AKTION_JUMP));
    assert(!keyboard[SDL_SCANCODE_LCTRL]); // las teclas del jugador no se reasignan
    move(0,10,660,444); update(); // sigue el dedo fuera de la zona original
    assert(TouchControls.PlayerAction(AKTION_RECHTS));
    up(0,10,660,444); update();
    assert(!TouchControls.PlayerAction(AKTION_RECHTS));
    assert(TouchControls.PlayerAction(AKTION_SHOOT) && TouchControls.PlayerAction(AKTION_JUMP));
    up(-2,12,1080,480); up(0,11,900,516); update();
    assert(!TouchControls.PlayerAction(AKTION_JUMP) && !TouchControls.PlayerAction(AKTION_SHOOT));

    struct Case { float x, y; int action; };
    const Case cases[] = {{1080,480,AKTION_JUMP},{900,516,AKTION_SHOOT},
        {1080,348,AKTION_BLITZ},{948,348,AKTION_POWERLINE},{816,348,AKTION_GRANATE},
        {684,348,AKTION_SMARTBOMB},{684,486,AKTION_WAFFEN_CYCLE}};
    for (auto c : cases) {
        down(0,1,c.x,c.y); update(); assert(TouchControls.PlayerAction(c.action));
        up(0,1,c.x,c.y); update(); assert(!TouchControls.PlayerAction(c.action));
    }
    // Toque completo entre frames: se conserva durante un frame y despues se suelta.
    down(-2,42,684,486); up(-2,42,684,486);
    update(); assert(TouchControls.PlayerAction(AKTION_WAFFEN_CYCLE));
    update(); assert(!TouchControls.PlayerAction(AKTION_WAFFEN_CYCLE));

    SpielZustand = GameStateEnum::MAINMENU;
    keyboard[SDL_SCANCODE_LCTRL] = 1;
    down(0,1,204,444); move(0,1,204,390); down(0,2,900,516); update();
    assert(keyboard[SDL_SCANCODE_UP] && keyboard[SDL_SCANCODE_RETURN]);
    assert(keyboard[SDL_SCANCODE_LCTRL]);
    up(0,1,204,390); up(0,2,900,516);
    std::memset(keyboard, 0, sizeof(keyboard)); update();

    SpielZustand = GameStateEnum::GAMELOOP;
    down(0,1,900,516); update(); assert(TouchControls.PlayerAction(AKTION_SHOOT));
    TouchControls.CancelTouches(); DebugPanel.SetOpen(true); update();
    assert(!TouchControls.PlayerAction(AKTION_SHOOT));
    DebugPanel.SetOpen(false); update();
    assert(!TouchControls.PlayerAction(AKTION_SHOOT)); // dedo previo sigue apoyado
    up(0,1,900,516); update();
    down(0,2,900,516); update(); assert(TouchControls.PlayerAction(AKTION_SHOOT));
    TouchControls.ResetTouches(); update(); assert(!TouchControls.PlayerAction(AKTION_SHOOT));
    // Tras volver de segundo plano no se espera un UP que Android puede haber omitido.
    down(0,3,900,516); update(); assert(TouchControls.PlayerAction(AKTION_SHOOT));
    up(0,3,900,516); update();

    // El boton DBG consume la pulsacion y no se filtra al juego al cerrar el panel.
    down(0,7,1140,42); assert(DebugPanel.IsOpen());
    up(0,7,1140,42); down(0,8,400,200); assert(!DebugPanel.IsOpen());
    update(); assert(!TouchControls.PlayerAction(AKTION_SHOOT));
    up(0,8,400,200); update();
    down(0,9,900,516); update(); assert(TouchControls.PlayerAction(AKTION_SHOOT));
    up(0,9,900,516); update();

    TouchControls.ToggleEditing();
    down(0,1,1080,480); move(0,1,1000,420); update();
    move(0,1,980,420); up(0,1,980,420);
    TouchControls.ToggleEditing();
    down(0,1,980,420); update(); assert(TouchControls.PlayerAction(AKTION_JUMP));
    up(0,1,980,420); update();
    TouchControls.Shutdown();
    std::cout << "touch_controls_test: OK\n";
    return 0;
}
