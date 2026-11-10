// Stream chat overlay: a scrolling fake chat panel drawn on the OBS overlay
// window. You see it on your desktop; OBS capture sources don't.
//
// Swap makeFakeMessage() for a real producer (Twitch IRC, YouTube live chat,
// websocket, whatever) and everything else — the rolling queue, the panel,
// the drawing — stays the same.

#include <obs_overlay/client.h>

#include <Windows.h>

#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct ChatMessage {
    std::string user;
    uint32_t    userColor;
    std::string text;
};

constexpr float kPanelW      = 420.f;
constexpr float kPanelMargin = 30.f;
constexpr float kLineH       = 24.f;
constexpr int   kMaxLines    = 18;
constexpr float kTitleH      = 32.f;
constexpr float kPadX        = 12.f;
constexpr float kPadY        = 10.f;
constexpr float kTextSize    = 15.f;
constexpr float kTitleSize   = 17.f;

constexpr uint32_t kColPanel    = 0xC0101014u;
constexpr uint32_t kColTitleBar = 0xE01A1A20u;
constexpr uint32_t kColBorder   = 0x60FFFFFFu;
constexpr uint32_t kColText     = 0xFFEFEFEFu;
constexpr uint32_t kColTitleTx  = 0xFFE0E0E0u;

const std::vector<std::pair<const char*, uint32_t>> kUsers = {
    { "shroud",       0xFFFF6B6Bu },
    { "pokimane",     0xFFFFB6C1u },
    { "xqc",          0xFF4A90E2u },
    { "ninja",        0xFF9B59B6u },
    { "timthetatman", 0xFFF39C12u },
    { "asmongold",    0xFFE74C3Cu },
    { "hasanabi",     0xFF2ECC71u },
    { "sodapoppin",   0xFF1ABC9Cu },
    { "drdisrespect", 0xFFE67E22u },
    { "ludwig",       0xFF95A5A6u },
    { "valkyrae",     0xFFFF69B4u },
    { "mizkif",       0xFF3498DBu },
    { "lirik",        0xFF00CED1u },
    { "tfue",         0xFFFF4500u },
    { "summit1g",     0xFFADFF2Fu },
};

const std::vector<const char*> kPhrases = {
    "pog",
    "LUL",
    "W stream",
    "let's go!",
    "KEKW",
    "that was insane",
    "GG",
    "no way lmao",
    "clip it",
    "first",
    "hi chat",
    "PogChamp",
    "Kappa",
    "nice play",
    "monkaS",
    "Pepega",
    "EZ Clap",
    "based",
    "cringe",
    "copium",
    "L + ratio",
    "W take",
    "banger stream fr",
    "sheeesh",
    "bruh",
    "that aim is sus",
    "clean",
    "insane",
    "poggers",
    "FeelsBadMan",
    "FeelsGoodMan",
    "respect",
    "so clean",
    "wake up chat",
    "mods asleep",
    "actual aimbot",
    "where's the raid at",
    "first time catching live",
    "love this game",
    "music is fire",
    "HYPE",
    "KEKWait",
    "Jebaited",
    "5Head",
    "2Head",
};

ChatMessage makeFakeMessage(std::mt19937& rng) {
    std::uniform_int_distribution<size_t> u(0, kUsers.size()   - 1);
    std::uniform_int_distribution<size_t> p(0, kPhrases.size() - 1);
    const auto& user = kUsers[u(rng)];
    return { user.first, user.second, kPhrases[p(rng)] };
}

// rough proportional-font advance so username and message line up inline.
// fine for an example; measure through DirectWrite if you want pixel-perfect.
float estimateAdvance(std::string_view s, float size) {
    return static_cast<float>(s.size()) * size * 0.52f;
}

}  // namespace

int main() {
    obs_overlay::Client c;
    if (!c.connect()) {
        printf("connect failed\n");
        return 1;
    }

    obs_overlay::PluginInfo info{};
    c.getPluginInfo(info);
    const float screenW = info.width  > 0 ? static_cast<float>(info.width)  : 1920.f;
    const float screenH = info.height > 0 ? static_cast<float>(info.height) : 1080.f;

    const float panelW = kPanelW;
    const float panelH = kTitleH + kLineH * kMaxLines + kPadY * 2.f;
    const float panelX = screenW - panelW - kPanelMargin;
    const float panelY = screenH * 0.18f;

    std::deque<ChatMessage>            messages;
    std::mt19937                       rng{ std::random_device{}() };
    std::uniform_int_distribution<int> delay(350, 1600);

    DWORD       nextSpawn = GetTickCount();
    const DWORD startTick = GetTickCount();
    const DWORD runForMs  = 60'000;

    while (GetTickCount() - startTick < runForMs) {
        const DWORD now = GetTickCount();
        if (now >= nextSpawn) {
            messages.push_back(makeFakeMessage(rng));
            while (messages.size() > static_cast<size_t>(kMaxLines)) {
                messages.pop_front();
            }
            nextSpawn = now + static_cast<DWORD>(delay(rng));
        }

        c.clear();

        c.filledRect(panelX, panelY, panelW, panelH, kColPanel);
        c.filledRect(panelX, panelY, panelW, kTitleH, kColTitleBar);
        c.text      (panelX + kPadX, panelY + 6.f, kTitleSize, kColTitleTx, "stream chat");
        c.rect      (panelX, panelY, panelW, panelH, 1.f, kColBorder);

        float y = panelY + kTitleH + kPadY;
        for (const auto& m : messages) {
            std::string tag = m.user;
            tag.push_back(':');
            c.text(panelX + kPadX, y, kTextSize, m.userColor, tag);

            const float xMsg = panelX + kPadX + estimateAdvance(tag, kTextSize) + 6.f;
            c.text(xMsg, y, kTextSize, kColText, m.text);

            y += kLineH;
        }

        if (!c.present()) break;
        Sleep(33);
    }

    c.clear();
    c.present();
    c.disconnect();
    return 0;
}
