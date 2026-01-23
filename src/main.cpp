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

namespace {
constexpr unsigned kWindowWidth = 2560;
constexpr unsigned kWindowHeight = 1440;

// Smaller cell size => finer grid (more cells, smaller squares).
// 2 => 1280x720 cells at 2560x1440.
constexpr unsigned kCellSize = 2;
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

// “Food seeking” movement (chemotaxis-like)
constexpr float kMoveEnergyThreshold = 0.28f;
constexpr float kMoveNutrientThreshold = 0.85f;
constexpr float kMoveRequireDelta = 0.18f;
constexpr float kMoveCost = 0.02f;

// Evolution (heritable traits + selection)
constexpr float kTraitMutationChance = 0.06f;       // per-trait chance
constexpr float kMutationSigmaHarvest = 0.06f;      // multiplier noise
constexpr float kMutationSigmaUpkeep = 0.05f;       // multiplier noise
constexpr float kMutationSigmaBirth = 0.06f;        // multiplier noise
constexpr float kMutationSigmaCorpse = 0.06f;       // multiplier noise
constexpr float kMutationSigmaAge = 0.05f;          // multiplier noise
constexpr float kRuleMutationChance = 0.015f;       // tweak neighbor-rule genes
constexpr float kSpeciesFlipChance = 0.0015f;        // rare “speciation” flip on birth

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

static constexpr std::array<std::pair<int, int>, 8> kDirs{{
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1},
}};

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
    if (font.openFromFile("assets/DejaVuSans.ttf")) return true;
    if (font.openFromFile("assets/arial.ttf")) return true;
    return false;
}

struct Simulation {
    unsigned width{};
    unsigned height{};

    std::vector<std::uint8_t> alive;
    std::vector<std::uint8_t> nextAlive;

    std::vector<std::uint8_t> species;
    std::vector<std::uint8_t> nextSpecies;

    // Heritable “genes” (multipliers over base species params)
    std::vector<float> harvestMul;
    std::vector<float> nextHarvestMul;

    std::vector<float> upkeepMul;
    std::vector<float> nextUpkeepMul;

    std::vector<float> birthMul;
    std::vector<float> nextBirthMul;

    std::vector<float> corpseMul;
    std::vector<float> nextCorpseMul;

    std::vector<float> ageMul;
    std::vector<float> nextAgeMul;

    // Heritable neighborhood-rule “genes”
    std::vector<std::uint8_t> surviveMinGene;
    std::vector<std::uint8_t> nextSurviveMinGene;

    std::vector<std::uint8_t> surviveMaxGene;
    std::vector<std::uint8_t> nextSurviveMaxGene;

    std::vector<std::uint8_t> reproduceMinGene;
    std::vector<std::uint8_t> nextReproduceMinGene;

    std::vector<std::uint8_t> reproduceMaxGene;
    std::vector<std::uint8_t> nextReproduceMaxGene;

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
    std::normal_distribution<float> distN{0.f, 1.f};

    std::array<std::size_t, 2> aliveBySpecies{};
    std::array<float, 2> harvestMulSum{};
    std::array<float, 2> upkeepMulSum{};
    std::array<float, 2> birthMulSum{};
    std::array<float, 2> ageMulSum{};

    explicit Simulation(unsigned w, unsigned h)
        : width(w)
        , height(h)
        , alive(w * h, 0)
        , nextAlive(w * h, 0)
        , species(w * h, 0)
        , nextSpecies(w * h, 0)
        , harvestMul(w * h, 1.f)
        , nextHarvestMul(w * h, 1.f)
        , upkeepMul(w * h, 1.f)
        , nextUpkeepMul(w * h, 1.f)
        , birthMul(w * h, 1.f)
        , nextBirthMul(w * h, 1.f)
        , corpseMul(w * h, 1.f)
        , nextCorpseMul(w * h, 1.f)
        , ageMul(w * h, 1.f)
        , nextAgeMul(w * h, 1.f)
        , surviveMinGene(w * h, 2)
        , nextSurviveMinGene(w * h, 2)
        , surviveMaxGene(w * h, 4)
        , nextSurviveMaxGene(w * h, 4)
        , reproduceMinGene(w * h, 2)
        , nextReproduceMinGene(w * h, 2)
        , reproduceMaxGene(w * h, 3)
        , nextReproduceMaxGene(w * h, 3)
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

    unsigned pickAliveNeighborForSpecies(unsigned x, unsigned y, std::uint8_t desiredSpecies) {
        const unsigned xl = xL[x];
        const unsigned xr = xR[x];
        const unsigned yu = yU[y];
        const unsigned yd = yD[y];

        std::array<unsigned, 8> ns{
            idx(xl, yu), idx(x, yu), idx(xr, yu), idx(xl, y), idx(xr, y), idx(xl, yd), idx(x, yd), idx(xr, yd),
        };

        std::array<unsigned, 8> preferred{};
        int preferredCount = 0;
        std::array<unsigned, 8> any{};
        int anyCount = 0;

        desiredSpecies = static_cast<std::uint8_t>(desiredSpecies ? 1u : 0u);

        for (unsigned ni : ns) {
            if (!alive[ni]) continue;
            any[static_cast<std::size_t>(anyCount++)] = ni;
            if ((species[ni] ? 1u : 0u) == desiredSpecies) {
                preferred[static_cast<std::size_t>(preferredCount++)] = ni;
            }
        }

        if (preferredCount > 0) {
            return preferred[static_cast<std::size_t>(rng() % static_cast<unsigned>(preferredCount))];
        }
        return any[static_cast<std::size_t>(rng() % static_cast<unsigned>(anyCount))];
    }

    float mutateMul(float v, float sigma, float lo, float hi) {
        if (dist01(rng) < kTraitMutationChance) {
            v *= std::max(0.2f, 1.f + distN(rng) * sigma);
        }
        return std::clamp(v, lo, hi);
    }

    std::uint8_t mutateRule(std::uint8_t v, int lo, int hi) {
        if (dist01(rng) < kRuleMutationChance) {
            const int sign = (rng() & 1u) ? 1 : -1;
            v = static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(v) + sign, lo, hi));
        }
        return v;
    }

    std::uint8_t maybeFlipSpecies(std::uint8_t s) {
        if (dist01(rng) < kSpeciesFlipChance) return static_cast<std::uint8_t>(s ^ 1u);
        return s;
    }

    void initGenes(unsigned i) {
        harvestMul[i] = 1.f;
        upkeepMul[i] = 1.f;
        birthMul[i] = 1.f;
        corpseMul[i] = 1.f;
        ageMul[i] = 1.f;
        surviveMinGene[i] = 2;
        surviveMaxGene[i] = 4;
        reproduceMinGene[i] = 2;
        reproduceMaxGene[i] = 3;
    }

    void writeNextGenesFrom(unsigned from, unsigned to, bool mutate) {
        float hm = harvestMul[from];
        float um = upkeepMul[from];
        float bm = birthMul[from];
        float cm = corpseMul[from];
        float am = ageMul[from];

        std::uint8_t sMin = surviveMinGene[from];
        std::uint8_t sMax = surviveMaxGene[from];
        std::uint8_t rMin = reproduceMinGene[from];
        std::uint8_t rMax = reproduceMaxGene[from];

        if (mutate) {
            hm = mutateMul(hm, kMutationSigmaHarvest, 0.60f, 1.60f);
            um = mutateMul(um, kMutationSigmaUpkeep, 0.60f, 1.60f);
            bm = mutateMul(bm, kMutationSigmaBirth, 0.60f, 1.80f);
            cm = mutateMul(cm, kMutationSigmaCorpse, 0.60f, 1.80f);
            am = mutateMul(am, kMutationSigmaAge, 0.70f, 1.70f);

            sMin = mutateRule(sMin, 1, 3);
            sMax = mutateRule(sMax, 3, 5);
            if (sMin >= sMax) sMin = static_cast<std::uint8_t>(sMax - 1);

            rMin = mutateRule(rMin, 2, 3);
            rMax = mutateRule(rMax, 3, 4);
            if (rMin > rMax) rMin = rMax;
        }

        nextHarvestMul[to] = hm;
        nextUpkeepMul[to] = um;
        nextBirthMul[to] = bm;
        nextCorpseMul[to] = cm;
        nextAgeMul[to] = am;
        nextSurviveMinGene[to] = sMin;
        nextSurviveMaxGene[to] = sMax;
        nextReproduceMinGene[to] = rMin;
        nextReproduceMaxGene[to] = rMax;

        const std::uint8_t s = nextSpecies[to] ? 1u : 0u;
        harvestMulSum[s] += hm;
        upkeepMulSum[s] += um;
        birthMulSum[s] += bm;
        ageMulSum[s] += am;
    }

    void clear() {
        std::fill(alive.begin(), alive.end(), 0);
        std::fill(species.begin(), species.end(), 0);
        std::fill(harvestMul.begin(), harvestMul.end(), 1.f);
        std::fill(upkeepMul.begin(), upkeepMul.end(), 1.f);
        std::fill(birthMul.begin(), birthMul.end(), 1.f);
        std::fill(corpseMul.begin(), corpseMul.end(), 1.f);
        std::fill(ageMul.begin(), ageMul.end(), 1.f);
        std::fill(surviveMinGene.begin(), surviveMinGene.end(), 2);
        std::fill(surviveMaxGene.begin(), surviveMaxGene.end(), 4);
        std::fill(reproduceMinGene.begin(), reproduceMinGene.end(), 2);
        std::fill(reproduceMaxGene.begin(), reproduceMaxGene.end(), 3);
        std::fill(energy.begin(), energy.end(), 0.f);
        std::fill(age.begin(), age.end(), 0);
        aliveBySpecies = {0, 0};
        harvestMulSum = {0.f, 0.f};
        upkeepMulSum = {0.f, 0.f};
        birthMulSum = {0.f, 0.f};
        ageMulSum = {0.f, 0.f};
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
            initGenes(i);
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

    void step(bool enableMovement) {
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

        std::fill(nextHarvestMul.begin(), nextHarvestMul.end(), 1.f);
        std::fill(nextUpkeepMul.begin(), nextUpkeepMul.end(), 1.f);
        std::fill(nextBirthMul.begin(), nextBirthMul.end(), 1.f);
        std::fill(nextCorpseMul.begin(), nextCorpseMul.end(), 1.f);
        std::fill(nextAgeMul.begin(), nextAgeMul.end(), 1.f);
        std::fill(nextSurviveMinGene.begin(), nextSurviveMinGene.end(), 2);
        std::fill(nextSurviveMaxGene.begin(), nextSurviveMaxGene.end(), 4);
        std::fill(nextReproduceMinGene.begin(), nextReproduceMinGene.end(), 2);
        std::fill(nextReproduceMaxGene.begin(), nextReproduceMaxGene.end(), 3);

        aliveBySpecies = {0, 0};
        harvestMulSum = {0.f, 0.f};
        upkeepMulSum = {0.f, 0.f};
        birthMulSum = {0.f, 0.f};
        ageMulSum = {0.f, 0.f};

        // Survivors + energy-driven reproduction
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                const unsigned i = idx(x, y);
                if (!alive[i]) continue;

                const std::uint8_t s = species[i] ? 1u : 0u;
                const auto& sp = kSpecies[s];

                const int neighbors = countNeighbors(x, y);

                const float effHarvest = sp.harvestFrac * harvestMul[i];
                const float effUpkeep = sp.upkeep * upkeepMul[i];
                const float effBirthThreshold = sp.birthThreshold * birthMul[i];
                const float effCorpseValue = sp.corpseValue * corpseMul[i];
                const auto effMaxAge = static_cast<std::uint16_t>(
                    std::clamp(static_cast<int>(std::lround(static_cast<float>(sp.maxAge) * ageMul[i])), 650, 3200));

                float localN = nextNutrient[i];
                const float harvested = effHarvest * localN;
                localN -= harvested;
                nextNutrient[i] = localN;

                // Tradeoff: higher harvest has an implicit metabolic cost.
                const float metabolicCost = effUpkeep + 0.32f * effHarvest;
                float e = energy[i] + harvested - metabolicCost;
                const std::uint16_t a = static_cast<std::uint16_t>(age[i] + 1);

                const bool crowdedOut = (neighbors < static_cast<int>(surviveMinGene[i])) || (neighbors > static_cast<int>(surviveMaxGene[i]));
                const bool died = (e <= 0.f) || (a > effMaxAge) || crowdedOut;

                if (died) {
                    nextNutrient[i] = std::clamp(nextNutrient[i] + effCorpseValue, 0.f, kMaxNutrient);
                    continue;
                }

                unsigned target = i;

                if (enableMovement && e < effBirthThreshold && e < kMoveEnergyThreshold && nextNutrient[i] < kMoveNutrientThreshold) {
                    const int startMove = static_cast<int>(rng() % 8u);
                    unsigned bestSpot = 0xFFFFFFFFu;
                    float bestFood = -1.f;
                    for (int k = 0; k < 8; ++k) {
                        const auto [dx, dy] = kDirs[static_cast<std::size_t>((startMove + k) & 7)];
                        const unsigned xx = wrap(static_cast<int>(x) + dx, width);
                        const unsigned yy = wrap(static_cast<int>(y) + dy, height);
                        const unsigned ni = idx(xx, yy);
                        if (alive[ni]) continue;
                        if (nextAlive[ni]) continue;
                        const float food = nextNutrient[ni];
                        if (food > bestFood) {
                            bestFood = food;
                            bestSpot = ni;
                        }
                    }
                    if (bestSpot != 0xFFFFFFFFu && bestFood > nextNutrient[i] + kMoveRequireDelta) {
                        e = std::max(0.f, e - kMoveCost);
                        target = bestSpot;
                    }
                }

                if (nextAlive[target]) target = i;

                nextAlive[target] = 1;
                nextSpecies[target] = s;
                nextEnergy[target] = e;
                nextAge[target] = a;
                ++aliveBySpecies[s];
                writeNextGenesFrom(i, target, false);

                if (e < effBirthThreshold) continue;
                if (neighbors < static_cast<int>(reproduceMinGene[i]) || neighbors > static_cast<int>(reproduceMaxGene[i])) continue;

                // Pick an empty neighbor, preferring higher nutrient.
                const int start = static_cast<int>(rng() % 8u);

                int bestDir = -1;
                float bestFood = -1.f;

                for (int k = 0; k < 8; ++k) {
                    const auto [dx, dy] = kDirs[static_cast<std::size_t>((start + k) & 7)];
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

                const auto [dx, dy] = kDirs[static_cast<std::size_t>(bestDir)];
                const unsigned xx = wrap(static_cast<int>(x) + dx, width);
                const unsigned yy = wrap(static_cast<int>(y) + dy, height);
                const unsigned ni = idx(xx, yy);

                const float childEnergy = nextEnergy[target] * 0.5f;
                nextEnergy[target] -= childEnergy;

                nextAlive[ni] = 1;
                nextSpecies[ni] = maybeFlipSpecies(s);
                nextEnergy[ni] = childEnergy;
                nextAge[ni] = 0;
                ++aliveBySpecies[nextSpecies[ni] ? 1 : 0];
                writeNextGenesFrom(i, ni, true);
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

                nextAlive[i] = 1;
                nextSpecies[i] = maybeFlipSpecies(s);
                nextEnergy[i] = kInitialEnergy + 0.40f * spent;
                nextAge[i] = 0;
                ++aliveBySpecies[nextSpecies[i] ? 1 : 0];
                const unsigned parent = pickAliveNeighborForSpecies(x, y, s);
                writeNextGenesFrom(parent, i, true);
            }
        }

        alive.swap(nextAlive);
        species.swap(nextSpecies);
        energy.swap(nextEnergy);
        age.swap(nextAge);
        nutrient.swap(nextNutrient);

        harvestMul.swap(nextHarvestMul);
        upkeepMul.swap(nextUpkeepMul);
        birthMul.swap(nextBirthMul);
        corpseMul.swap(nextCorpseMul);
        ageMul.swap(nextAgeMul);
        surviveMinGene.swap(nextSurviveMinGene);
        surviveMaxGene.swap(nextSurviveMaxGene);
        reproduceMinGene.swap(nextReproduceMinGene);
        reproduceMaxGene.swap(nextReproduceMaxGene);
    }
};

} // namespace

int main(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke") smokeTest = true;
    }

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{kWindowWidth, kWindowHeight}), "Game of Life");
    window.setFramerateLimit(60);

    sf::Font font;
    const bool hasFont = loadFont(font);

    enum class State { Menu, Simulation };
    State state = smokeTest ? State::Simulation : State::Menu;

    Simulation sim{kGridWidth, kGridHeight};
    sim.reset();

    sf::Texture gridTexture(sf::Vector2u{kGridWidth, kGridHeight});
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

    sf::Text startLabel(font);
    sf::Text hud(font);

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
    bool enableMovement = true;

    float accumulator = 0.f;
    sf::Clock clock;

    const int maxStepsPerFrame = 4;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        accumulator += dt;

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
                    if (key->code == sf::Keyboard::Key::M) enableMovement = !enableMovement;
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
                        const bool shiftDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                               sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                        if (mousePress->button == sf::Mouse::Button::Left) {
                            sim.seedCross(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), shiftDown ? 1u : 0u);
                        } else if (mousePress->button == sf::Mouse::Button::Right) {
                            sim.addNutrientPatch(static_cast<unsigned>(cellX), static_cast<unsigned>(cellY), 1.8f, 12);
                        }
                    }
                }
            }
        }

        // Update
        if (state == State::Simulation && !paused) {
            int steps = 0;
            while (accumulator >= kTickDt && steps < maxStepsPerFrame) {
                sim.step(enableMovement);
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
                        const float maxAgeEff = std::max(1.f, static_cast<float>(kSpecies[s].maxAge) * sim.ageMul[i]);
                        const float a = clamp01(static_cast<float>(sim.age[i]) / maxAgeEff);

                        const float effHarvest = kSpecies[s].harvestFrac * sim.harvestMul[i];
                        const float effUpkeep = kSpecies[s].upkeep * sim.upkeepMul[i];
                        const float ratio = effHarvest / std::max(0.001f, effUpkeep);
                        const float tGene = clamp01((ratio - 1.6f) / 2.0f);

                        const sf::Color young = lerp(s0Young, s1Young, tGene);
                        const sf::Color old = lerp(s0Old, s1Old, tGene);
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

            gridTexture.update(pixels.data());
            window.draw(gridSprite);

            if (hasFont) {
                const float inv0 = (sim.aliveBySpecies[0] > 0) ? (1.f / static_cast<float>(sim.aliveBySpecies[0])) : 0.f;
                const float inv1 = (sim.aliveBySpecies[1] > 0) ? (1.f / static_cast<float>(sim.aliveBySpecies[1])) : 0.f;

                std::ostringstream line1;
                line1.setf(std::ios::fixed);
                line1 << std::setprecision(2);
                line1 << "Cells " << (sim.aliveBySpecies[0] + sim.aliveBySpecies[1])
                      << " | S0 " << sim.aliveBySpecies[0]
                      << " | S1 " << sim.aliveBySpecies[1]
                      << " | Hmul " << (sim.harvestMulSum[0] * inv0) << "/" << (sim.harvestMulSum[1] * inv1)
                      << " | Umul " << (sim.upkeepMulSum[0] * inv0) << "/" << (sim.upkeepMulSum[1] * inv1)
                      << " | Speed " << static_cast<int>(kTickHz) << " tps";

                std::ostringstream line2;
                line2 << "[Space] pause  [R] reset  [C] clear  LMB seed  Shift+LMB seed S1  RMB nutrients  "
                      << "[M] move:" << (enableMovement ? "on" : "off") << "  "
                      << "[N] nutrients:" << (showNutrients ? "on" : "off") << "  "
                      << "[T] trails:" << (showTrails ? "on" : "off") << "  "
                      << "[Esc] menu";
                if (paused) line2 << "   (PAUSED)";

                hud.setString(line1.str() + "\n" + line2.str());
                hud.setCharacterSize(20);
                hud.setLineSpacing(1.0f);
                hud.setFillColor(sf::Color{200u, 255u, 230u, 255u});
                hud.setPosition(sf::Vector2f{18.f, 14.f});
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
