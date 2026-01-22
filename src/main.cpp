#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr unsigned kWindowWidth = 1024;
constexpr unsigned kWindowHeight = 768;

sf::Color lerp(const sf::Color& a, const sf::Color& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return sf::Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

bool loadFont(sf::Font& font) {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
    if (font.openFromFile("assets/DejaVuSans.ttf")) return true;
    if (font.openFromFile("assets/arial.ttf")) return true;
#else
    if (font.loadFromFile("assets/DejaVuSans.ttf")) return true;
    if (font.loadFromFile("assets/arial.ttf")) return true;
#endif
    return false;
}
}

int main() {
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{kWindowWidth, kWindowHeight}), "Game of Life");
    window.setFramerateLimit(60);

    sf::Font font;
    const bool hasFont = loadFont(font);

    enum class State { Menu, Running };
    State state = State::Menu;

    sf::RectangleShape button(sf::Vector2f{380.f, 120.f});
    button.setOrigin(sf::Vector2f{190.f, 60.f});
    button.setPosition(sf::Vector2f{kWindowWidth * 0.5f, kWindowHeight * 0.55f});
    button.setFillColor(sf::Color::Black);

    const sf::Color neonIdle{0u, 255u, 190u, 255u};
    const sf::Color neonHover{130u, 255u, 255u, 255u};

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
    sf::Text label(font);
#else
    sf::Text label;
    if (hasFont) label.setFont(font);
#endif
    label.setString("START");
    label.setCharacterSize(54);
    label.setFillColor(neonIdle);

    auto centerLabel = [&]() {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        const auto bounds = label.getLocalBounds();
        label.setOrigin(sf::Vector2f{bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y * 0.5f});
#else
        const auto bounds = label.getLocalBounds();
        label.setOrigin(sf::Vector2f{bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.5f});
#endif
        label.setPosition(button.getPosition());
    };
    centerLabel();

    bool wasMouseDown = false;

    while (window.isOpen()) {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
        }
#else
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }
#endif

        const auto mouse = sf::Mouse::getPosition(window);
        const float mouseX = static_cast<float>(mouse.x);
        const float mouseY = static_cast<float>(mouse.y);
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        const bool hovered = button.getGlobalBounds().contains(sf::Vector2f{mouseX, mouseY});
#else
        const bool hovered = button.getGlobalBounds().contains(mouseX, mouseY);
#endif

        const bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const bool clicked = hovered && mouseDown && !wasMouseDown;
        wasMouseDown = mouseDown;

        if (state == State::Menu && clicked) {
            state = State::Running;
        }

        window.clear(sf::Color::Black);

        if (state == State::Menu) {
            const sf::Color neon = hovered ? neonHover : neonIdle;

            for (int i = 0; i < 3; ++i) {
                const float grow = 6.f + static_cast<float>(i) * 6.f;
                sf::RectangleShape glow(button);
                glow.setSize(button.getSize() + sf::Vector2f{grow * 2.f, grow * 2.f});
                glow.setOrigin(button.getOrigin() + sf::Vector2f{grow, grow});
                glow.setFillColor(sf::Color::Transparent);
                glow.setOutlineThickness(2.f + static_cast<float>(i) * 2.f);
                glow.setOutlineColor(sf::Color{neon.r, neon.g, neon.b, static_cast<std::uint8_t>(60u / (i + 1))});
                window.draw(glow);
            }

            button.setOutlineThickness(3.f);
            button.setOutlineColor(neon);
            window.draw(button);

            label.setFillColor(lerp(neon, sf::Color::White, hovered ? 0.15f : 0.f));
            centerLabel();
            window.draw(label);
        } else {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
            sf::Text running(font);
#else
            sf::Text running;
            if (hasFont) running.setFont(font);
#endif
            running.setString("Running... (close the window to exit)");
            running.setCharacterSize(28);
            running.setFillColor(sf::Color{160u, 255u, 220u, 255u});
            running.setPosition(sf::Vector2f{30.f, 30.f});

            if (hasFont) {
                window.draw(running);
            }
        }

        window.display();
    }

    return 0;
}

