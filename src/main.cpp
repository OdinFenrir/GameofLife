#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "Constants.h"
#include "Types.h"

namespace {

sf::Color lerpColor(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return sf::Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

bool loadFont(sf::Font& font) {
    if (font.openFromFile("assets/DejaVuSans.ttf")) return true;
    if (font.openFromFile("assets/arial.ttf")) return true;
    return false;
}

}

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke") smokeTest = true;
    }

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{Config::Window::Width, Config::Window::Height}), "Game of Life");
    window.setFramerateLimit(60);

    sf::Font font;
    const bool hasFont = loadFont(font);

    enum class ApplicationState { Menu, Simulation };
    ApplicationState state = smokeTest ? ApplicationState::Simulation : ApplicationState::Menu;

    Simulation sim{Config::Window::GridWidth, Config::Window::GridHeight};
    sim.reset();

    float viewScale = 1.0f;

    sf::Texture gridTexture(sf::Vector2u{Config::Window::GridWidth, Config::Window::GridHeight});
    sf::Sprite gridSprite(gridTexture);
    auto updateSpriteScale = [&]() {
        gridSprite.setScale(sf::Vector2f{
            static_cast<float>(Config::Window::CellSize) * viewScale,
            static_cast<float>(Config::Window::CellSize) * viewScale
        });
    };
    updateSpriteScale();

    const std::size_t pixelCount = static_cast<std::size_t>(Config::Window::GridWidth) * Config::Window::GridHeight;
    std::vector<std::uint8_t> pixels(pixelCount * 4, 0);
    std::vector<float> trail(pixelCount, 0.0f);

    sf::RectangleShape button(sf::Vector2f{420.0f, 140.0f});
    button.setOrigin(sf::Vector2f{210.0f, 70.0f});
    button.setPosition(sf::Vector2f{Config::Window::Width * 0.5f, Config::Window::Height * 0.55f});
    button.setFillColor(sf::Color::Black);

    const sf::Color& neonIdle = Config::Colors::NeonIdle;
    const sf::Color& neonHover = Config::Colors::NeonHover;

    sf::Text startLabel(font);
    sf::Text hud(font);

    startLabel.setString("START");
    startLabel.setCharacterSize(58);
    startLabel.setFillColor(neonIdle);

    auto centerLabel = [&]() {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        const auto bounds = startLabel.getLocalBounds();
        startLabel.setOrigin(sf::Vector2f{bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y * 0.5f});
#else
        const auto bounds = startLabel.getLocalBounds();
        startLabel.setOrigin(sf::Vector2f{bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.5f});
#endif
        startLabel.setPosition(button.getPosition());
    };
    centerLabel();

    bool paused = false;
    bool showNutrients = true;
    bool showTrails = true;
    bool enableMovement = true;

    float speedMultiplier = 1.0f;
    float viewOffsetX = 0.0f;
    float viewOffsetY = 0.0f;

    float accumulator = 0.0f;
    sf::Clock clock;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        accumulator += dt;

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                continue;
            }

            if (const auto* keyPress = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPress->code == sf::Keyboard::Key::Escape) {
                    if (state == ApplicationState::Simulation) {
                        state = ApplicationState::Menu;
                    } else {
                        window.close();
                    }
                }

                if (state == ApplicationState::Simulation) {
                    if (keyPress->code == sf::Keyboard::Key::Space) paused = !paused;
                    if (keyPress->code == sf::Keyboard::Key::R) {
                        sim.reset();
                        std::fill(trail.begin(), trail.end(), 0.0f);
                    }
                    if (keyPress->code == sf::Keyboard::Key::C) {
                        sim.clear();
                        std::fill(trail.begin(), trail.end(), 0.0f);
                    }
                    if (keyPress->code == sf::Keyboard::Key::N) showNutrients = !showNutrients;
                    if (keyPress->code == sf::Keyboard::Key::T) showTrails = !showTrails;
                    if (keyPress->code == sf::Keyboard::Key::M) enableMovement = !enableMovement;

                    if (keyPress->code == sf::Keyboard::Key::Equal || keyPress->code == sf::Keyboard::Key::Add) {
                        speedMultiplier = std::min(4.0f, speedMultiplier * 1.5f);
                    }
                    if (keyPress->code == sf::Keyboard::Key::Hyphen) {
                        speedMultiplier = std::max(0.25f, speedMultiplier / 1.5f);
                    }

                    const float panSpeed = 20.0f / viewScale;
                    if (keyPress->code == sf::Keyboard::Key::Left) viewOffsetX -= panSpeed;
                    if (keyPress->code == sf::Keyboard::Key::Right) viewOffsetX += panSpeed;
                    if (keyPress->code == sf::Keyboard::Key::Up) viewOffsetY -= panSpeed;
                    if (keyPress->code == sf::Keyboard::Key::Down) viewOffsetY += panSpeed;
                    if (keyPress->code == sf::Keyboard::Key::Num1) {
                        sim.clear();
                        sim.spawnCellCluster(Config::Window::GridWidth / 2, Config::Window::GridHeight / 2, 0);
                        viewOffsetX = 0; viewOffsetY = 0; viewScale = 1.0f;
                        updateSpriteScale();
                    }
                    if (keyPress->code == sf::Keyboard::Key::Num2) {
                        sim.clear();
                        for (int i = 0; i < 5; ++i) {
                            sim.spawnCellCluster(
                                Config::Window::GridWidth / 2 + (i - 2) * 10,
                                Config::Window::GridHeight / 2,
                                static_cast<std::uint8_t>(i % 2)
                            );
                        }
                        viewOffsetX = 0; viewOffsetY = 0; viewScale = 1.0f;
                        updateSpriteScale();
                    }
                    if (keyPress->code == sf::Keyboard::Key::Num3) {
                        sim.clear();
                        for (int dx = -2; dx <= 2; ++dx) {
                            for (int dy = -2; dy <= 2; ++dy) {
                                if ((dx + dy) % 2 == 0) {
                                    sim.spawnCellCluster(
                                        Config::Window::GridWidth / 2 + dx * 5,
                                        Config::Window::GridHeight / 2 + dy * 5,
                                        0
                                    );
                                }
                            }
                        }
                        viewOffsetX = 0; viewOffsetY = 0; viewScale = 1.0f;
                        updateSpriteScale();
                    }
                }
            }

            if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>()) {
                const sf::Vector2f mousePosition{
                    static_cast<float>(mousePress->position.x),
                    static_cast<float>(mousePress->position.y)
                };

                if (state == ApplicationState::Menu) {
                    if (button.getGlobalBounds().contains(mousePosition)) {
                        state = ApplicationState::Simulation;
                    }
                } else {
                    const float scaledCellSize = std::max(0.01f, static_cast<float>(Config::Window::CellSize) * viewScale);
                    const float adjustedX = (static_cast<float>(mousePress->position.x) - viewOffsetX) / scaledCellSize;
                    const float adjustedY = (static_cast<float>(mousePress->position.y) - viewOffsetY) / scaledCellSize;
                    const int cellX = static_cast<int>(std::floor(adjustedX));
                    const int cellY = static_cast<int>(std::floor(adjustedY));

                    if (cellX >= 0 && cellY >= 0 &&
                        cellX < static_cast<int>(Config::Window::GridWidth) &&
                        cellY < static_cast<int>(Config::Window::GridHeight)) {

                        const bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                              sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                        if (mousePress->button == sf::Mouse::Button::Left) {
                            sim.spawnCellCluster(
                                static_cast<unsigned>(cellX),
                                static_cast<unsigned>(cellY),
                                shiftHeld ? 1u : 0u
                            );
                        } else if (mousePress->button == sf::Mouse::Button::Right) {
                            sim.addNutrientPatch(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), 1.8f, 12);
                        }
                    }
                }
            }

            if (const auto* mouseWheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (state == ApplicationState::Simulation) {
                    const float zoomFactor = (mouseWheel->delta > 0) ? 1.2f : 0.8f;
                    viewScale = std::clamp(viewScale * zoomFactor, 0.5f, 4.0f);
                    updateSpriteScale();
                }
            }
        }

        if (state == ApplicationState::Simulation && !paused) {
            const float adjustedTickDelta = Config::Timing::TickDeltaTime / speedMultiplier;
            int steps = 0;
            while (accumulator >= adjustedTickDelta && steps < Config::Timing::MaxStepsPerFrame) {
                sim.step(enableMovement);
                accumulator -= adjustedTickDelta;
                ++steps;
            }
            accumulator = std::min(accumulator, adjustedTickDelta);
        } else {
            accumulator = 0.0f;
        }

        window.clear(sf::Color::Black);

        if (state == ApplicationState::Menu) {
            const auto mousePos = sf::Mouse::getPosition(window);
            const sf::Vector2f mouseF{static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)};
            const bool hovered = button.getGlobalBounds().contains(mouseF);

            const sf::Color neon = hovered ? neonHover : neonIdle;

            for (int i = 0; i < 3; ++i) {
                const float grow = 7.0f + static_cast<float>(i) * 7.0f;
                sf::RectangleShape glow(button);
                glow.setSize(button.getSize() + sf::Vector2f{grow * 2.0f, grow * 2.0f});
                glow.setOrigin(button.getOrigin() + sf::Vector2f{grow, grow});
                glow.setFillColor(sf::Color::Transparent);
                glow.setOutlineThickness(2.0f + static_cast<float>(i) * 2.0f);
                glow.setOutlineColor(sf::Color{neon.r, neon.g, neon.b, static_cast<std::uint8_t>(70u / (i + 1))});
                window.draw(glow);
            }

            button.setOutlineThickness(3.0f);
            button.setOutlineColor(neon);
            window.draw(button);

            startLabel.setFillColor(lerpColor(neon, sf::Color::White, hovered ? 0.15f : 0.0f));
            centerLabel();
            if (hasFont) window.draw(startLabel);
        } else {
            const sf::Color& species0Young = Config::Colors::Species0Young;
            const sf::Color& species0Old = Config::Colors::Species0Old;
            const sf::Color& species1Young = Config::Colors::Species1Young;
            const sf::Color& species1Old = Config::Colors::Species1Old;
            const auto& cells = sim.getCells();
            const auto& nutrients = sim.getNutrient();

            for (unsigned y = 0; y < Config::Window::GridHeight; ++y) {
                for (unsigned x = 0; x < Config::Window::GridWidth; ++x) {
                    const unsigned i = x + y * Config::Window::GridWidth;

                    const float normalizedNutrient = nutrients[i] / Config::Nutrients::Maximum;
                    const float backgroundIntensity = showNutrients ? clamp01(normalizedNutrient) : 0.0f;

                    float red = 0.02f + 0.08f * backgroundIntensity;
                    float green = 0.02f + 0.10f * backgroundIntensity;
                    float blue = 0.04f + 0.20f * backgroundIntensity;

                    if (showTrails) {
                        trail[i] *= Config::Cell::TrailDecay;
                    } else {
                        trail[i] = 0.0f;
                    }

                    const auto& cell = cells[i];
                    if (cell.alive) {
                        const std::uint8_t speciesId = cell.species ? 1u : 0u;
                        const float energyNormalized = clamp01(cell.energy / Config::Cell::EnergyDisplayDivisor);
                        const float maxAgeEffective = std::max(1.0f,
                            static_cast<float>(Config::SpeciesBase::Data[speciesId].maximumAge) * cell.ageMultiplier);
                        const float ageNormalized = clamp01(static_cast<float>(cell.age) / maxAgeEffective);

                        const float effectiveHarvest = Config::SpeciesBase::Data[speciesId].harvestFraction * cell.harvestMultiplier;
                        const float effectiveUpkeep = Config::SpeciesBase::Data[speciesId].upkeepCost * cell.upkeepMultiplier;
                        const sf::Color youngColor = speciesId ? species1Young : species0Young;
                        const sf::Color oldColor = speciesId ? species1Old : species0Old;
                        const sf::Color cellColor = lerpColor(youngColor, oldColor, ageNormalized);

                        const float brightness = Config::Rendering::CellColorBrightnessMin +
                                                 Config::Rendering::CellColorBrightnessMax * energyNormalized;

                        float blendFactor = brightness;
                        if (showTrails) {
                            trail[i] = std::max(trail[i], brightness);
                            blendFactor = trail[i];
                        }

                        red = std::min(1.0f, red + (cellColor.r / 255.0f) * blendFactor);
                        green = std::min(1.0f, green + (cellColor.g / 255.0f) * blendFactor);
                        blue = std::min(1.0f, blue + (cellColor.b / 255.0f) * blendFactor);
                    }

                    const std::uint8_t corpseSpecies = cell.corpseSpecies;
                    if (corpseSpecies == 1) {
                        red = std::min(1.0f, red + 0.15f);
                        green = std::min(1.0f, green + 0.35f);
                        blue = std::min(1.0f, blue + 0.35f);
                    } else if (corpseSpecies == 2) {
                        red = std::min(1.0f, red + 0.35f);
                        green = std::min(1.0f, green + 0.10f);
                        blue = std::min(1.0f, blue + 0.10f);
                    }

                    const std::size_t pixelIndex = static_cast<std::size_t>(i) * 4;
                    pixels[pixelIndex + 0] = static_cast<std::uint8_t>(std::lround(red * 255.0f));
                    pixels[pixelIndex + 1] = static_cast<std::uint8_t>(std::lround(green * 255.0f));
                    pixels[pixelIndex + 2] = static_cast<std::uint8_t>(std::lround(blue * 255.0f));
                    pixels[pixelIndex + 3] = 255;
                }
            }

            gridTexture.update(pixels.data());
            gridSprite.setPosition(sf::Vector2f{viewOffsetX, viewOffsetY});
            window.draw(gridSprite);

            if (hasFont) {
                const auto& aliveCounts = sim.getAliveCountBySpecies();
                const float inv0 = (aliveCounts[0] > 0) ? (1.0f / static_cast<float>(aliveCounts[0])) : 0.0f;
                const float inv1 = (aliveCounts[1] > 0) ? (1.0f / static_cast<float>(aliveCounts[1])) : 0.0f;

                std::ostringstream line1;
                line1.setf(std::ios::fixed);
                line1 << std::setprecision(2);
                line1 << "Cells " << (aliveCounts[0] + aliveCounts[1])
                      << " | S0 " << aliveCounts[0]
                      << " | S1 " << aliveCounts[1]
                      << " | Hmul " << (sim.getHarvestMultiplierSum()[0] * inv0) << "/" << (sim.getHarvestMultiplierSum()[1] * inv1)
                      << " | Umul " << (sim.getUpkeepMultiplierSum()[0] * inv0) << "/" << (sim.getUpkeepMultiplierSum()[1] * inv1)
                      << " | Speed " << static_cast<int>(Config::Timing::TicksPerSecond * speedMultiplier) << " tps";

                std::ostringstream line2;
                line2 << "[Space] pause  [R] reset  [C] clear  LMB seed  Shift+LMB seed S1  RMB nutrients  "
                      << "[M] move:" << (enableMovement ? "on" : "off") << "  "
                      << "[N] nutrients:" << (showNutrients ? "on" : "off") << "  "
                      << "[T] trails:" << (showTrails ? "on" : "off") << "  "
                      << "[Esc] menu";
                if (paused) line2 << "   (PAUSED)";

                std::ostringstream line3;
                line3 << "[+/-] speed:" << std::fixed << std::setprecision(1) << speedMultiplier << "x  "
                      << "[Arrows] pan  [Wheel] zoom:" << std::fixed << std::setprecision(1) << viewScale << "x  "
                      << "[1-3] presets";

                hud.setString(line1.str() + "\n" + line2.str() + "\n" + line3.str());
                hud.setCharacterSize(18);
                hud.setLineSpacing(1.1f);
                hud.setFillColor(Config::Colors::HudText);
                hud.setPosition(sf::Vector2f{18.0f, 14.0f});
                window.draw(hud);
            }
        }

        window.display();

        if (smokeTest) {
            static int frameCount = 0;
            if (++frameCount > 180) window.close();
        }
    }

    return 0;
}
