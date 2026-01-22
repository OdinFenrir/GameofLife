#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{900u, 600u}), "AntFarm");
    window.setFramerateLimit(60);

    sf::CircleShape ant(6.f);
    ant.setPosition(sf::Vector2f{100.f, 300.f});

    sf::Vector2f vel{120.f, 0.f};
    sf::Clock clock;

    while (window.isOpen()) {
        // SFML 3 event loop (pollEvent returns an optional-like value)
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        float dt = clock.restart().asSeconds();

        auto p = ant.getPosition();
        p += vel * dt;

        if (p.x < 0.f) { p.x = 0.f; vel.x = -vel.x; }
        if (p.x > 900.f - ant.getRadius() * 2.f) { p.x = 900.f - ant.getRadius() * 2.f; vel.x = -vel.x; }

        ant.setPosition(p);

        window.clear();
        window.draw(ant);
        window.display();
    }

    return 0;
}
