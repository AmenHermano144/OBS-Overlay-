// Minimal example: connect to the OBS plugin, draw a rectangle + text, hold
// for a few seconds, disconnect. Replace the drawing block with whatever
// you actually want to put on the overlay.

#include <obs_overlay/client.h>

#include <Windows.h>
#include <cstdio>

int main() {
    obs_overlay::Client c;
    if (!c.connect()) {
        printf("connect failed\n");
        return 1;
    }

    c.clear();
    c.filledRect(200.f, 200.f, 400.f, 100.f, 0xC0202020u);
    c.rect      (200.f, 200.f, 400.f, 100.f, 2.f, 0xFF4A90E2u);
    c.text      (220.f, 230.f, 24.f, 0xFFFFFFFFu, "overlay ok");
    c.present();

    Sleep(5000);

    c.clear();
    c.present();
    c.disconnect();
    return 0;
}
