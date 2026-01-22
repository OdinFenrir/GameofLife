#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr unsigned kWindowWidth = 2560;
constexpr unsigned kWindowHeight = 1440;

constexpr unsigned kCellSize = 4;
constexpr unsigned kGridWidth = kWindowWidth / kCellSize;
constexpr unsigned kGridHeight = kWindowHeight / kCellSize;

constexpr float kTickHz = 20.f;
constexpr float kTickDt = 1.f / kTickHz;

constexpr float kMaxNutrient = 3.0f;
constexpr float kDiffusion = 0.22f;     // 0..1; higher = smoother nutrient field
constexpr float kRegrowRate = 0.0025f;  // fraction toward max per tick

constexpr float kBirthNutrientMin = 1.10f;
constexpr float kInitialEnergy = 0.55f;

constexpr float kTrailDecay = 0.92f;

constexpr float kMutationChance = 0.015f; // ~1.5% flip species on birth

struct SpeciesParams {
    float harvestFrac{};
    float upkeep{};
    float birthThreshold{};
    std::uint16_t maxAge{};
    float corpseValue{};
};

constexpr std::array<SpeciesParams, 2> kSpecies{
    SpeciesParams{0.095f, 0.033f, 1.70f, static_cast<std::uint16_t>(1100), 0.70f},
    SpeciesParams{0.070f, 0.026f, 2.05f, static_cast<std::uint16_t>(1700), 0.90f},
};

unsigned wrap(int v, unsigned max) {
    const int m = static_cast<int>(max);
    int r = v % m;
    if (r < 0) r += m;
    return static_cast<unsigned>(r);
}

sf::Color lerp(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return sf::Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

float clamp01(float v) {
    return std::clamp(v, 0.f, 1.f);
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

struct Simulation {
    unsigned width{};
    unsigned height{};

    std::vector<std::uint8_t> alive;
    std::vector<std::uint8_t> nextAlive;

    std::vector<std::uint8_t> species;
    std::vector<std::uint8_t> nextSpecies;

    std::vector<float> energy;
    std::vector<float> nextEnergy;

    std::vector<std::uint16_t> age;
    std::vector<std::uint16_t> nextAge;

    std::vector<float> nutrient;
    std::vector<float> nextNutrient;

    std::vector<unsigned> xL;
    std::vector<unsigned> xR;
    std::vector<unsigned> yU;
    std::vector<unsigned> yD;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist01{0.f, 1.f};

    std::array<std::size_t, 2> aliveBySpecies{};

    explicit Simulation(unsigned w, unsigned h)
        : width(w)
        , height(h)
        , alive(w * h, 0)
        , nextAlive(w * h, 0)
        , species(w * h, 0)
        , nextSpecies(w * h, 0)
        , energy(w * h, 0.f)
        , nextEnergy(w * h, 0.f)
        , age(w * h, 0)
        , nextAge(w * h, 0)
        , nutrient(w * h, 0.f)
        , nextNutrient(w * h, 0.f)
        , xL(w)
        , xR(w)
        , yU(h)
        , yD(h) {
        for (unsigned x = 0; x < width; ++x) {
            xL[x] = (x == 0) ? (width - 1) : (x - 1);
            xR[x] = (x + 1 == width) ? 0 : (x + 1);
        }
        for (unsigned y = 0; y < height; ++y) {
            yU[y] = (y == 0) ? (height - 1) : (y - 1);
            yD[y] = (y + 1 == height) ? 0 : (y + 1);
        }
    }

    unsigned idx(unsigned x, unsigned y) const {
        return x + y * width;
    }

    int countNeighbors(unsigned x, unsigned y) const {
        const unsigned xl = xL[x];
        const unsigned xr = xR[x];
        const unsigned yu = yU[y];
        const unsigned yd = yD[y];

        return alive[idx(xl, yu)] + alive[idx(x, yu)] + alive[idx(xr, yu)] + alive[idx(xl, y)] + alive[idx(xr, y)] +
               alive[idx(xl, yd)] + alive[idx(x, yd)] + alive[idx(xr, yd)];
    }

    std::array<int, 2> neighborSpeciesCounts(unsigned x, unsigned y) const {
        const unsigned xl = xL[x];
        const unsigned xr = xR[x];
        const unsigned yu = yU[y];
        const unsigned yd = yD[y];

        std::array<unsigned, 8> ns{
            idx(xl, yu), idx(x, yu), idx(xr, yu), idx(xl, y), idx(xr, y), idx(xl, yd), idx(x, yd), idx(xr, yd),
        };

        std::array<int, 2> counts{0, 0};
        for (unsigned ni : ns) {
            if (!alive[ni]) continue;
            counts[species[ni] ? 1 : 0] += 1;
        }
        return counts;
    }

    std::uint8_t mutateIfNeeded(std::uint8_t s) {
        if (dist01(rng) < kMutationChance) return static_cast<std::uint8_t>(s ^ 1u);
        return s;
    }

    void clear() {
        std::fill(alive.begin(), alive.end(), 0);
        std::fill(species.begin(), species.end(), 0);
        std::fill(energy.begin(), energy.end(), 0.f);
        std::fill(age.begin(), age.end(), 0);
        aliveBySpecies = {0, 0};
    }

    void reset() {
        clear();

        const float base = kMaxNutrient * 0.55f;
        std::fill(nutrient.begin(), nutrient.end(), base);

        std::uniform_int_distribution<unsigned> distX(0, width - 1);
        std::uniform_int_distribution<unsigned> distY(0, height - 1);
        std::uniform_int_distribution<int> distR(40, 140);
        std::uniform_real_distribution<float> distA(0.6f, 1.6f);

        for (int blob = 0; blob < 10; ++blob) {
            const unsigned cx = distX(rng);
            const unsigned cy = distY(rng);
            const float amp = distA(rng);
            const int r = distR(rng);
            const float invR2 = 1.f / static_cast<float>(r * r);

            for (unsigned y = 0; y < height; ++y) {
                const int dy = static_cast<int>(y) - static_cast<int>(cy);
                for (unsigned x = 0; x < width; ++x) {
                    const int dx = static_cast<int>(x) - static_cast<int>(cx);
                    const float d2 = static_cast<float>(dx * dx + dy * dy);
                    const float t = 1.f - d2 * invR2;
                    if (t <= 0.f) continue;
                    nutrient[idx(x, y)] += amp * t * t;
                }
            }
        }

        for (auto& n : nutrient) n = std::clamp(n, 0.f, kMaxNutrient);

        seedCross(width / 2, height / 2, 0);
    }

    void seedCross(unsigned x, unsigned y, std::uint8_t s) {
        const std::array<std::pair<int, int>, 5> offsets{
            {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}},
        };

        for (auto [ox, oy] : offsets) {
            const unsigned xx = wrap(static_cast<int>(x) + ox, width);
            const unsigned yy = wrap(static_cast<int>(y) + oy, height);
            const unsigned i = idx(xx, yy);
            if (alive[i]) continue;
            alive[i] = 1;
            species[i] = s;
            energy[i] = kInitialEnergy;
            age[i] = 0;
            ++aliveBySpecies[s ? 1 : 0];
        }
    }

    void addNutrientPatch(unsigned x, unsigned y, float amount, int radius) {
        const float invR2 = 1.f / static_cast<float>(radius * radius);

        for (int oy = -radius; oy <= radius; ++oy) {
            for (int ox = -radius; ox <= radius; ++ox) {
                const float d2 = static_cast<float>(ox * ox + oy * oy);
                const float t = 1.f - d2 * invR2;
                if (t <= 0.f) continue;

                const unsigned xx = wrap(static_cast<int>(x) + ox, width);
                const unsigned yy = wrap(static_cast<int>(y) + oy, height);
                const unsigned i = idx(xx, yy);
                nutrient[i] = std::clamp(nutrient[i] + amount * t, 0.f, kMaxNutrient);
            }
        }
    }

    void step() {
        // Nutrient diffusion + regrowth
        for (unsigned y = 0; y < height; ++y) {
            const unsigned yu = yU[y];
            const unsigned yd = yD[y];
            for (unsigned x = 0; x < width; ++x) {
                const unsigned xl = xL[x];
                const unsigned xr = xR[x];

                const float c = nutrient[idx(x, y)];
                const float avg = (c + nutrient[idx(xl, y)] + nutrient[idx(xr, y)] + nutrient[idx(x, yu)] + nutrient[idx(x, yd)] +
                                   nutrient[idx(xl, yu)] + nutrient[idx(xr, yu)] + nutrient[idx(xl, yd)] + nutrient[idx(xr, yd)]) /
                                  9.f;

                float n = c + kDiffusion * (avg - c);
                n += kRegrowRate * (kMaxNutrient - n);
                nextNutrient[idx(x, y)] = std::clamp(n, 0.f, kMaxNutrient);
            }
        }

        std::fill(nextAlive.begin(), nextAlive.end(), 0);
        std::fill(nextSpecies.begin(), nextSpecies.end(), 0);
        std::fill(nextEnergy.begin(), nextEnergy.end(), 0.f);
        std::fill(nextAge.begin(), nextAge.end(), 0);

        aliveBySpecies = {0, 0};

        // Survivors + energy-driven reproduction
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                const unsigned i = idx(x, y);
                if (!alive[i]) continue;

                const std::uint8_t s = species[i] ? 1u : 0u;
                const auto& sp = kSpecies[s];

                const int neighbors = countNeighbors(x, y);

                float localN = nextNutrient[i];
                const float harvested = sp.harvestFrac * localN;
                localN -= harvested;
                nextNutrient[i] = localN;

                const float e = energy[i] + harvested - sp.upkeep;
                const std::uint16_t a = static_cast<std::uint16_t>(age[i] + 1);

                const bool crowdedOut = (neighbors < 2) || (neighbors > 4);
                const bool died = (e <= 0.f) || (a > sp.maxAge) || crowdedOut;

                if (died) {
                    nextNutrient[i] = std::clamp(nextNutrient[i] + sp.corpseValue, 0.f, kMaxNutrient);
                    continue;
                }

                nextAlive[i] = 1;
                nextSpecies[i] = s;
                nextEnergy[i] = e;
                nextAge[i] = a;
                ++aliveBySpecies[s];

                if (e < sp.birthThreshold) continue;
                if (neighbors < 2 || neighbors > 3) continue;

                // Pick an empty neighbor, preferring higher nutrient.
                const std::array<std::pair<int, int>, 8> dirs{{
                    {-1, -1}, {0, -1}, {1, -1},
                    {-1, 0},           {1, 0},
                    {-1, 1},  {0, 1},  {1, 1},
                }};

                const int start = static_cast<int>(rng() % 8u);

                int bestDir = -1;
                float bestFood = -1.f;

                for (int k = 0; k < 8; ++k) {
                    const auto [dx, dy] = dirs[static_cast<std::size_t>((start + k) & 7)];
                    const unsigned xx = wrap(static_cast<int>(x) + dx, width);
                    const unsigned yy = wrap(static_cast<int>(y) + dy, height);
                    const unsigned ni = idx(xx, yy);

                    if (alive[ni]) continue;
                    if (nextAlive[ni]) continue;

                    const float food = nextNutrient[ni];
                    if (food > bestFood) {
                        bestFood = food;
                        bestDir = (start + k) & 7;
                    }
                }

                if (bestDir == -1) continue;

                const auto [dx, dy] = dirs[static_cast<std::size_t>(bestDir)];
                const unsigned xx = wrap(static_cast<int>(x) + dx, width);
                const unsigned yy = wrap(static_cast<int>(y) + dy, height);
                const unsigned ni = idx(xx, yy);

                const float childEnergy = nextEnergy[i] * 0.5f;
                nextEnergy[i] -= childEnergy;

                nextAlive[ni] = 1;
                nextSpecies[ni] = mutateIfNeeded(s);
                nextEnergy[ni] = childEnergy;
                nextAge[ni] = 0;
                ++aliveBySpecies[nextSpecies[ni] ? 1 : 0];
            }
        }

        // Structure-preserving births into still-empty cells
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                const unsigned i = idx(x, y);
                if (alive[i]) continue;
                if (nextAlive[i]) continue;

                const int neighbors = countNeighbors(x, y);
                if (neighbors != 3) continue;

                float localN = nextNutrient[i];
                if (localN < kBirthNutrientMin) continue;

                const float spent = std::min(localN, 0.40f);
                localN -= spent;
                nextNutrient[i] = localN;

                const auto counts = neighborSpeciesCounts(x, y);
                std::uint8_t s = 0;
                if (counts[1] > counts[0]) s = 1;
                else if (counts[1] == counts[0] && (rng() & 1u)) s = 1;
                s = mutateIfNeeded(s);

                nextAlive[i] = 1;
                nextSpecies[i] = s;
                nextEnergy[i] = kInitialEnergy + 0.40f * spent;
                nextAge[i] = 0;
                ++aliveBySpecies[s];
            }
        }

        alive.swap(nextAlive);
        species.swap(nextSpecies);
        energy.swap(nextEnergy);
        age.swap(nextAge);
        nutrient.swap(nextNutrient);
    }
};

} // namespace

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke") smokeTest = true;
    }

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{kWindowWidth, kWindowHeight}), "Game of Life");
#else
    sf::RenderWindow window(sf::VideoMode(kWindowWidth, kWindowHeight), "Game of Life");
#endif
    window.setFramerateLimit(60);

    sf::Font font;
    const bool hasFont = loadFont(font);

    enum class State { Menu, Simulation };
    State state = smokeTest ? State::Simulation : State::Menu;

    Simulation sim{kGridWidth, kGridHeight};
    sim.reset();

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
    sf::Texture gridTexture(sf::Vector2u{kGridWidth, kGridHeight});
#else
    sf::Texture gridTexture;
    gridTexture.create(kGridWidth, kGridHeight);
#endif
    sf::Sprite gridSprite(gridTexture);
    gridSprite.setScale(sf::Vector2f{static_cast<float>(kCellSize), static_cast<float>(kCellSize)});

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kGridWidth) * kGridHeight * 4, 0);
    std::vector<float> trail(static_cast<std::size_t>(kGridWidth) * kGridHeight, 0.f);

    // Menu button
    sf::RectangleShape button(sf::Vector2f{420.f, 140.f});
    button.setOrigin(sf::Vector2f{210.f, 70.f});
    button.setPosition(sf::Vector2f{kWindowWidth * 0.5f, kWindowHeight * 0.55f});
    button.setFillColor(sf::Color::Black);

    const sf::Color neonIdle{0u, 255u, 190u, 255u};
    const sf::Color neonHover{130u, 255u, 255u, 255u};

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
    sf::Text startLabel(font);
    sf::Text hud(font);
#else
    sf::Text startLabel;
    sf::Text hud;
    if (hasFont) {
        startLabel.setFont(font);
        hud.setFont(font);
    }
#endif

    startLabel.setString("START");
    startLabel.setCharacterSize(58);
    startLabel.setFillColor(neonIdle);

    auto centerLabel = [&]() {
#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        const auto b = startLabel.getLocalBounds();
        startLabel.setOrigin(sf::Vector2f{b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
#else
        const auto b = startLabel.getLocalBounds();
        startLabel.setOrigin(sf::Vector2f{b.left + b.width * 0.5f, b.top + b.height * 0.5f});
#endif
        startLabel.setPosition(button.getPosition());
    };
    centerLabel();

    bool paused = false;
    bool showNutrients = true;
    bool showTrails = true;

    float accumulator = 0.f;
    sf::Clock clock;

    const int maxStepsPerFrame = 4;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        accumulator += dt;

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                continue;
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    if (state == State::Simulation) {
                        state = State::Menu;
                    } else {
                        window.close();
                    }
                }

                if (state == State::Simulation) {
                    if (key->code == sf::Keyboard::Key::Space) paused = !paused;
                    if (key->code == sf::Keyboard::Key::R) {
                        sim.reset();
                        std::fill(trail.begin(), trail.end(), 0.f);
                    }
                    if (key->code == sf::Keyboard::Key::C) {
                        sim.clear();
                        std::fill(trail.begin(), trail.end(), 0.f);
                    }
                    if (key->code == sf::Keyboard::Key::N) showNutrients = !showNutrients;
                    if (key->code == sf::Keyboard::Key::T) showTrails = !showTrails;
                }
            }

            if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>()) {
                const sf::Vector2f mouseF{static_cast<float>(mousePress->position.x), static_cast<float>(mousePress->position.y)};

                if (state == State::Menu) {
                    if (button.getGlobalBounds().contains(mouseF)) {
                        state = State::Simulation;
                    }
                } else {
                    const int cellX = mousePress->position.x / static_cast<int>(kCellSize);
                    const int cellY = mousePress->position.y / static_cast<int>(kCellSize);
                    if (cellX >= 0 && cellY >= 0 && cellX < static_cast<int>(kGridWidth) && cellY < static_cast<int>(kGridHeight)) {
                        if (mousePress->button == sf::Mouse::Button::Left) {
                            const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                               sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                            sim.seedCross(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), shift ? 1u : 0u);
                        } else if (mousePress->button == sf::Mouse::Button::Right) {
                            sim.addNutrientPatch(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), 1.8f, 12);
                        }
                    }
                }
            }
        }
#else
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                continue;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    if (state == State::Simulation) {
                        state = State::Menu;
                    } else {
                        window.close();
                    }
                }

                if (state == State::Simulation) {
                    if (event.key.code == sf::Keyboard::Space) paused = !paused;
                    if (event.key.code == sf::Keyboard::R) {
                        sim.reset();
                        std::fill(trail.begin(), trail.end(), 0.f);
                    }
                    if (event.key.code == sf::Keyboard::C) {
                        sim.clear();
                        std::fill(trail.begin(), trail.end(), 0.f);
                    }
                    if (event.key.code == sf::Keyboard::N) showNutrients = !showNutrients;
                    if (event.key.code == sf::Keyboard::T) showTrails = !showTrails;
                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                const float mx = static_cast<float>(event.mouseButton.x);
                const float my = static_cast<float>(event.mouseButton.y);

                if (state == State::Menu) {
                    if (button.getGlobalBounds().contains(mx, my)) {
                        state = State::Simulation;
                    }
                } else {
                    const int cellX = event.mouseButton.x / static_cast<int>(kCellSize);
                    const int cellY = event.mouseButton.y / static_cast<int>(kCellSize);
                    if (cellX >= 0 && cellY >= 0 && cellX < static_cast<int>(kGridWidth) && cellY < static_cast<int>(kGridHeight)) {
                        if (event.mouseButton.button == sf::Mouse::Left) {
                            const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                                               sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
                            sim.seedCross(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), shift ? 1u : 0u);
                        } else if (event.mouseButton.button == sf::Mouse::Right) {
                            sim.addNutrientPatch(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), 1.8f, 12);
                        }
                    }
                }
            }
        }
#endif

        // Update
        if (state == State::Simulation && !paused) {
            int steps = 0;
            while (accumulator >= kTickDt && steps < maxStepsPerFrame) {
                sim.step();
                accumulator -= kTickDt;
                ++steps;
            }
            accumulator = std::min(accumulator, kTickDt);
        } else {
            accumulator = 0.f;
        }

        // Render
        window.clear(sf::Color::Black);

        if (state == State::Menu) {
            const auto mouse = sf::Mouse::getPosition(window);
            const sf::Vector2f mouseF{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
            const bool hovered = button.getGlobalBounds().contains(mouseF);

            const sf::Color neon = hovered ? neonHover : neonIdle;

            for (int i = 0; i < 3; ++i) {
                const float grow = 7.f + static_cast<float>(i) * 7.f;
                sf::RectangleShape glow(button);
                glow.setSize(button.getSize() + sf::Vector2f{grow * 2.f, grow * 2.f});
                glow.setOrigin(button.getOrigin() + sf::Vector2f{grow, grow});
                glow.setFillColor(sf::Color::Transparent);
                glow.setOutlineThickness(2.f + static_cast<float>(i) * 2.f);
                glow.setOutlineColor(sf::Color{neon.r, neon.g, neon.b, static_cast<std::uint8_t>(70u / (i + 1))});
                window.draw(glow);
            }

            button.setOutlineThickness(3.f);
            button.setOutlineColor(neon);
            window.draw(button);

            startLabel.setFillColor(lerp(neon, sf::Color::White, hovered ? 0.15f : 0.f));
            centerLabel();
            if (hasFont) window.draw(startLabel);
        } else {
            // Build pixels (nutrient background + energy/age colored cells + trails)
            const sf::Color s0Young{0u, 255u, 190u, 255u};
            const sf::Color s0Old{255u, 90u, 230u, 255u};
            const sf::Color s1Young{255u, 220u, 80u, 255u};
            const sf::Color s1Old{80u, 160u, 255u, 255u};

            for (unsigned y = 0; y < kGridHeight; ++y) {
                for (unsigned x = 0; x < kGridWidth; ++x) {
                    const unsigned i = x + y * kGridWidth;

                    const float n = sim.nutrient[i] / kMaxNutrient;
                    const float bg = showNutrients ? clamp01(n) : 0.f;

                    const float baseR = 0.02f + 0.08f * bg;
                    const float baseG = 0.02f + 0.10f * bg;
                    const float baseB = 0.04f + 0.20f * bg;

                    float r = baseR;
                    float g = baseG;
                    float b = baseB;

                    if (showTrails) {
                        trail[i] *= kTrailDecay;
                    } else {
                        trail[i] = 0.f;
                    }

                    if (sim.alive[i]) {
                        const std::uint8_t s = sim.species[i] ? 1u : 0u;
                        const float e = clamp01(sim.energy[i] / 2.4f);
                        const float a = clamp01(static_cast<float>(sim.age[i]) / static_cast<float>(kSpecies[s].maxAge));

                        const sf::Color young = (s == 0) ? s0Young : s1Young;
                        const sf::Color old = (s == 0) ? s0Old : s1Old;
                        const sf::Color c = lerp(young, old, a);

                        const float bright = 0.30f + 0.70f * e;

                        float t = bright;
                        if (showTrails) {
                            trail[i] = std::max(trail[i], bright);
                            t = trail[i];
                        }

                        r = std::min(1.f, r + (c.r / 255.f) * t);
                        g = std::min(1.f, g + (c.g / 255.f) * t);
                        b = std::min(1.f, b + (c.b / 255.f) * t);
                    }

                    const std::size_t p = static_cast<std::size_t>(i) * 4;
                    pixels[p + 0] = static_cast<std::uint8_t>(std::lround(r * 255.f));
                    pixels[p + 1] = static_cast<std::uint8_t>(std::lround(g * 255.f));
                    pixels[p + 2] = static_cast<std::uint8_t>(std::lround(b * 255.f));
                    pixels[p + 3] = 255;
                }
            }

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
            gridTexture.update(pixels.data());
#else
            gridTexture.update(reinterpret_cast<const sf::Uint8*>(pixels.data()));
#endif
            window.draw(gridSprite);

            if (hasFont) {
                std::ostringstream oss;
                oss << "S0: " << sim.aliveBySpecies[0] << "   S1: " << sim.aliveBySpecies[1] << "    "
                    << "Speed: " << static_cast<int>(kTickHz) << " tps    "
                    << "[Space] pause  [R] reset  [C] clear  LMB seed  Shift+LMB seed S1  RMB nutrients  "
                    << "[N] nutrients:" << (showNutrients ? "on" : "off") << "  "
                    << "[T] trails:" << (showTrails ? "on" : "off") << "  "
                    << "[Esc] menu";
                if (paused) oss << "    (PAUSED)";

                hud.setString(oss.str());
                hud.setCharacterSize(22);
                hud.setFillColor(sf::Color{200u, 255u, 230u, 255u});
                hud.setPosition(sf::Vector2f{22.f, 18.f});
                window.draw(hud);
            }
        }

        window.display();

        if (smokeTest) {
            static int frames = 0;
            if (++frames > 180) window.close();
        }
    }

    return 0;
}