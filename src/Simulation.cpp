#include "Types.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr std::array<std::pair<int, int>, 8> kNeighborOffsets{{
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1},
}};

unsigned wrapCoordinate(int value, unsigned max) {
    const int m = static_cast<int>(max);
    int result = value % m;
    if (result < 0) result += m;
    return static_cast<unsigned>(result);
}

}

Simulation::Simulation(unsigned width, unsigned height)
    : width_(width)
    , height_(height)
    , alive_(width * height, 0)
    , species_(width * height, 0)
    , harvestMultiplier_(width * height, 1.0f)
    , upkeepMultiplier_(width * height, 1.0f)
    , birthMultiplier_(width * height, 1.0f)
    , corpseMultiplier_(width * height, 1.0f)
    , ageMultiplier_(width * height, 1.0f)
    , surviveMinGene_(width * height, Config::Genetics::DefaultSurviveMin)
    , surviveMaxGene_(width * height, Config::Genetics::DefaultSurviveMax)
    , reproduceMinGene_(width * height, Config::Genetics::DefaultReproduceMin)
    , reproduceMaxGene_(width * height, Config::Genetics::DefaultReproduceMax)
    , energy_(width * height, 0.0f)
    , age_(width * height, 0)
    , nutrient_(width * height, 0.0f)
    , leftNeighbor_(width)
    , rightNeighbor_(width)
    , upNeighbor_(height)
    , downNeighbor_(height)
    , rng_(std::random_device{}())
    , dist01_(0.0f, 1.0f)
    , distNormal_(0.0f, 1.0f)
    , nextAlive_(width * height, 0)
    , nextSpecies_(width * height, 0)
    , nextHarvestMultiplier_(width * height, 1.0f)
    , nextUpkeepMultiplier_(width * height, 1.0f)
    , nextBirthMultiplier_(width * height, 1.0f)
    , nextCorpseMultiplier_(width * height, 1.0f)
    , nextAgeMultiplier_(width * height, 1.0f)
    , nextSurviveMinGene_(width * height, Config::Genetics::DefaultSurviveMin)
    , nextSurviveMaxGene_(width * height, Config::Genetics::DefaultSurviveMax)
    , nextReproduceMinGene_(width * height, Config::Genetics::DefaultReproduceMin)
    , nextReproduceMaxGene_(width * height, Config::Genetics::DefaultReproduceMax)
    , nextEnergy_(width * height, 0.0f)
    , nextAge_(width * height, 0)
    , nextNutrient_(width * height, 0.0f) {
    for (unsigned x = 0; x < width_; ++x) {
        leftNeighbor_[x] = (x == 0) ? (width_ - 1) : (x - 1);
        rightNeighbor_[x] = (x + 1 == width_) ? 0 : (x + 1);
    }
    for (unsigned y = 0; y < height_; ++y) {
        upNeighbor_[y] = (y == 0) ? (height_ - 1) : (y - 1);
        downNeighbor_[y] = (y + 1 == height_) ? 0 : (y + 1);
    }
}

int Simulation::countNeighbors(unsigned x, unsigned y) const {
    const unsigned xl = leftNeighbor_[x];
    const unsigned xr = rightNeighbor_[x];
    const unsigned yu = upNeighbor_[y];
    const unsigned yd = downNeighbor_[y];

    return alive_[index(xl, yu)] + alive_[index(x, yu)] + alive_[index(xr, yu)] +
           alive_[index(xl, y)] + alive_[index(xr, y)] +
           alive_[index(xl, yd)] + alive_[index(x, yd)] + alive_[index(xr, yd)];
}

std::array<int, 2> Simulation::neighborSpeciesCounts(unsigned x, unsigned y) const {
    const unsigned xl = leftNeighbor_[x];
    const unsigned xr = rightNeighbor_[x];
    const unsigned yu = upNeighbor_[y];
    const unsigned yd = downNeighbor_[y];

    const std::array<unsigned, 8> neighborIndices{
        index(xl, yu), index(x, yu), index(xr, yu),
        index(xl, y), index(xr, y),
        index(xl, yd), index(x, yd), index(xr, yd),
    };

    std::array<int, 2> counts{0, 0};
    for (const unsigned ni : neighborIndices) {
        if (!alive_[ni]) continue;
        counts[species_[ni] ? 1 : 0] += 1;
    }
    return counts;
}

unsigned Simulation::pickAliveNeighborForSpecies(unsigned x, unsigned y, std::uint8_t desiredSpecies) {
    const unsigned xl = leftNeighbor_[x];
    const unsigned xr = rightNeighbor_[x];
    const unsigned yu = upNeighbor_[y];
    const unsigned yd = downNeighbor_[y];

    const std::array<unsigned, 8> neighborIndices{
        index(xl, yu), index(x, yu), index(xr, yu),
        index(xl, y), index(xr, y),
        index(xl, yd), index(x, yd), index(xr, yd),
    };

    std::array<unsigned, 8> preferred{};
    int preferredCount = 0;
    std::array<unsigned, 8> any{};
    int anyCount = 0;

    desiredSpecies = static_cast<std::uint8_t>(desiredSpecies ? 1u : 0u);

    for (const unsigned ni : neighborIndices) {
        if (!alive_[ni]) continue;
        any[static_cast<std::size_t>(anyCount++)] = ni;
        if ((species_[ni] ? 1u : 0u) == desiredSpecies) {
            preferred[static_cast<std::size_t>(preferredCount++)] = ni;
        }
    }

    if (preferredCount > 0) {
        return preferred[static_cast<std::size_t>(rng_() % static_cast<unsigned>(preferredCount))];
    }
    return any[static_cast<std::size_t>(rng_() % static_cast<unsigned>(anyCount))];
}

float Simulation::mutateMultiplier(float value, float sigma, float minVal, float maxVal) {
    if (dist01_(rng_) < Config::Evolution::TraitMutationChance) {
        value *= std::max(0.2f, 1.0f + distNormal_(rng_) * sigma);
    }
    return std::clamp(value, minVal, maxVal);
}

std::uint8_t Simulation::mutateRule(std::uint8_t value, int minVal, int maxVal) {
    if (dist01_(rng_) < Config::Evolution::RuleMutationChance) {
        const int sign = (rng_() & 1u) ? 1 : -1;
        value = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(value) + sign, minVal, maxVal));
    }
    return value;
}

std::uint8_t Simulation::maybeFlipSpecies(std::uint8_t species) {
    if (dist01_(rng_) < Config::Evolution::SpeciationChance) {
        return static_cast<std::uint8_t>(species ^ 1u);
    }
    return species;
}

void Simulation::initializeGenetics(unsigned idx) {
    harvestMultiplier_[idx] = 1.0f;
    upkeepMultiplier_[idx] = 1.0f;
    birthMultiplier_[idx] = 1.0f;
    corpseMultiplier_[idx] = 1.0f;
    ageMultiplier_[idx] = 1.0f;
    surviveMinGene_[idx] = Config::Genetics::DefaultSurviveMin;
    surviveMaxGene_[idx] = Config::Genetics::DefaultSurviveMax;
    reproduceMinGene_[idx] = Config::Genetics::DefaultReproduceMin;
    reproduceMaxGene_[idx] = Config::Genetics::DefaultReproduceMax;
}

void Simulation::copyGeneticsFrom(unsigned source, unsigned target, bool applyMutation) {
    float hm = harvestMultiplier_[source];
    float um = upkeepMultiplier_[source];
    float bm = birthMultiplier_[source];
    float cm = corpseMultiplier_[source];
    float am = ageMultiplier_[source];

    std::uint8_t sMin = surviveMinGene_[source];
    std::uint8_t sMax = surviveMaxGene_[source];
    std::uint8_t rMin = reproduceMinGene_[source];
    std::uint8_t rMax = reproduceMaxGene_[source];

    if (applyMutation) {
        hm = mutateMultiplier(hm, Config::Evolution::MutationSigmaHarvest,
                              Config::Evolution::MinimumMultiplier,
                              Config::Evolution::MaximumHarvestMultiplier);
        um = mutateMultiplier(um, Config::Evolution::MutationSigmaUpkeep,
                              Config::Evolution::MinimumMultiplier,
                              Config::Evolution::MaximumUpkeepMultiplier);
        bm = mutateMultiplier(bm, Config::Evolution::MutationSigmaBirth,
                              Config::Evolution::MinimumMultiplier,
                              Config::Evolution::MaximumBirthMultiplier);
        cm = mutateMultiplier(cm, Config::Evolution::MutationSigmaCorpse,
                              Config::Evolution::MinimumMultiplier,
                              Config::Evolution::MaximumCorpseMultiplier);
        am = mutateMultiplier(am, Config::Evolution::MutationSigmaAge,
                              Config::Evolution::MinimumMultiplier,
                              Config::Evolution::MaximumAgeMultiplier);

        sMin = mutateRule(sMin, Config::Genetics::SurviveMinRangeLow, Config::Genetics::SurviveMinRangeHigh);
        sMax = mutateRule(sMax, Config::Genetics::SurviveMaxRangeLow, Config::Genetics::SurviveMaxRangeHigh);
        if (sMin >= sMax) sMin = static_cast<std::uint8_t>(sMax - 1);

        rMin = mutateRule(rMin, Config::Genetics::ReproduceMinRangeLow, Config::Genetics::ReproduceMinRangeHigh);
        rMax = mutateRule(rMax, Config::Genetics::ReproduceMaxRangeLow, Config::Genetics::ReproduceMaxRangeHigh);
        if (rMin > rMax) rMin = rMax;
    }

    nextHarvestMultiplier_[target] = hm;
    nextUpkeepMultiplier_[target] = um;
    nextBirthMultiplier_[target] = bm;
    nextCorpseMultiplier_[target] = cm;
    nextAgeMultiplier_[target] = am;
    nextSurviveMinGene_[target] = sMin;
    nextSurviveMaxGene_[target] = sMax;
    nextReproduceMinGene_[target] = rMin;
    nextReproduceMaxGene_[target] = rMax;

    const std::uint8_t s = nextSpecies_[target] ? 1u : 0u;
    harvestMultiplierSum_[s] += hm;
    upkeepMultiplierSum_[s] += um;
    birthMultiplierSum_[s] += bm;
    ageMultiplierSum_[s] += am;
}

void Simulation::clear() {
    std::fill(alive_.begin(), alive_.end(), 0);
    std::fill(species_.begin(), species_.end(), 0);
    std::fill(harvestMultiplier_.begin(), harvestMultiplier_.end(), 1.0f);
    std::fill(upkeepMultiplier_.begin(), upkeepMultiplier_.end(), 1.0f);
    std::fill(birthMultiplier_.begin(), birthMultiplier_.end(), 1.0f);
    std::fill(corpseMultiplier_.begin(), corpseMultiplier_.end(), 1.0f);
    std::fill(ageMultiplier_.begin(), ageMultiplier_.end(), 1.0f);
    std::fill(surviveMinGene_.begin(), surviveMinGene_.end(), Config::Genetics::DefaultSurviveMin);
    std::fill(surviveMaxGene_.begin(), surviveMaxGene_.end(), Config::Genetics::DefaultSurviveMax);
    std::fill(reproduceMinGene_.begin(), reproduceMinGene_.end(), Config::Genetics::DefaultReproduceMin);
    std::fill(reproduceMaxGene_.begin(), reproduceMaxGene_.end(), Config::Genetics::DefaultReproduceMax);
    std::fill(energy_.begin(), energy_.end(), 0.0f);
    std::fill(age_.begin(), age_.end(), 0);
    aliveCountBySpecies_ = {0, 0};
    harvestMultiplierSum_ = {0.0f, 0.0f};
    upkeepMultiplierSum_ = {0.0f, 0.0f};
    birthMultiplierSum_ = {0.0f, 0.0f};
    ageMultiplierSum_ = {0.0f, 0.0f};
}

void Simulation::reset() {
    clear();

    const float base = Config::Nutrients::InitialBase;
    std::fill(nutrient_.begin(), nutrient_.end(), base);

    std::uniform_int_distribution<unsigned> distX(0, width_ - 1);
    std::uniform_int_distribution<unsigned> distY(0, height_ - 1);
    std::uniform_int_distribution<int> distRadius(40, 140);
    std::uniform_real_distribution<float> distAmplitude(0.6f, 1.6f);

    for (int blob = 0; blob < 10; ++blob) {
        const unsigned cx = distX(rng_);
        const unsigned cy = distY(rng_);
        const float amplitude = distAmplitude(rng_);
        const int radius = distRadius(rng_);
        const float invRadiusSquared = 1.0f / static_cast<float>(radius * radius);

        for (unsigned y = 0; y < height_; ++y) {
            const int dy = static_cast<int>(y) - static_cast<int>(cy);
            for (unsigned x = 0; x < width_; ++x) {
                const int dx = static_cast<int>(x) - static_cast<int>(cx);
                const float distanceSquared = static_cast<float>(dx * dx + dy * dy);
                const float falloff = 1.0f - distanceSquared * invRadiusSquared;
                if (falloff <= 0.0f) continue;
                nutrient_[index(x, y)] += amplitude * falloff * falloff;
            }
        }
    }

    for (float& n : nutrient_) {
        n = std::clamp(n, 0.0f, Config::Nutrients::Maximum);
    }

    spawnCellCluster(width_ / 2, height_ / 2, 0);
}

void Simulation::spawnCellCluster(unsigned x, unsigned y, std::uint8_t speciesId) {
    const std::array<std::pair<int, int>, 5> offsets{
        {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}},
    };

    for (auto [ox, oy] : offsets) {
        const unsigned xx = wrapCoordinate(static_cast<int>(x) + ox, width_);
        const unsigned yy = wrapCoordinate(static_cast<int>(y) + oy, height_);
        const unsigned idx = index(xx, yy);
        if (alive_[idx]) continue;
        alive_[idx] = 1;
        species_[idx] = speciesId;
        initializeGenetics(idx);
        energy_[idx] = Config::Cell::InitialEnergy;
        age_[idx] = 0;
        ++aliveCountBySpecies_[speciesId ? 1 : 0];
    }
}

void Simulation::addNutrientPatch(unsigned x, unsigned y, float amount, int radius) {
    const float invRadiusSquared = 1.0f / static_cast<float>(radius * radius);

    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            const float distanceSquared = static_cast<float>(ox * ox + oy * oy);
            const float falloff = 1.0f - distanceSquared * invRadiusSquared;
            if (falloff <= 0.0f) continue;

            const unsigned xx = wrapCoordinate(static_cast<int>(x) + ox, width_);
            const unsigned yy = wrapCoordinate(static_cast<int>(y) + oy, height_);
            const unsigned idx = index(xx, yy);
            nutrient_[idx] = std::clamp(nutrient_[idx] + amount * falloff, 0.0f, Config::Nutrients::Maximum);
        }
    }
}

void Simulation::diffuseNutrients() {
    for (unsigned y = 0; y < height_; ++y) {
        const unsigned yu = upNeighbor_[y];
        const unsigned yd = downNeighbor_[y];
        for (unsigned x = 0; x < width_; ++x) {
            const unsigned xl = leftNeighbor_[x];
            const unsigned xr = rightNeighbor_[x];

            const float center = nutrient_[index(x, y)];
            const float average = (center +
                                   nutrient_[index(xl, y)] + nutrient_[index(xr, y)] +
                                   nutrient_[index(x, yu)] + nutrient_[index(x, yd)] +
                                   nutrient_[index(xl, yu)] + nutrient_[index(xr, yu)] +
                                   nutrient_[index(xl, yd)] + nutrient_[index(xr, yd)]) /
                                  9.0f;

            float nutrients = center + Config::Nutrients::DiffusionRate * (average - center);
            nutrients += Config::Nutrients::RegrowthRate * (Config::Nutrients::Maximum - nutrients);
            nextNutrient_[index(x, y)] = std::clamp(nutrients, 0.0f, Config::Nutrients::Maximum);
        }
    }
}

void Simulation::processSurvivorsAndReproduction(bool enableMovement) {
    std::fill(nextAlive_.begin(), nextAlive_.end(), 0);
    std::fill(nextSpecies_.begin(), nextSpecies_.end(), 0);
    std::fill(nextEnergy_.begin(), nextEnergy_.end(), 0.0f);
    std::fill(nextAge_.begin(), nextAge_.end(), 0);

    std::fill(nextHarvestMultiplier_.begin(), nextHarvestMultiplier_.end(), 1.0f);
    std::fill(nextUpkeepMultiplier_.begin(), nextUpkeepMultiplier_.end(), 1.0f);
    std::fill(nextBirthMultiplier_.begin(), nextBirthMultiplier_.end(), 1.0f);
    std::fill(nextCorpseMultiplier_.begin(), nextCorpseMultiplier_.end(), 1.0f);
    std::fill(nextAgeMultiplier_.begin(), nextAgeMultiplier_.end(), 1.0f);
    std::fill(nextSurviveMinGene_.begin(), nextSurviveMinGene_.end(), Config::Genetics::DefaultSurviveMin);
    std::fill(nextSurviveMaxGene_.begin(), nextSurviveMaxGene_.end(), Config::Genetics::DefaultSurviveMax);
    std::fill(nextReproduceMinGene_.begin(), nextReproduceMinGene_.end(), Config::Genetics::DefaultReproduceMin);
    std::fill(nextReproduceMaxGene_.begin(), nextReproduceMaxGene_.end(), Config::Genetics::DefaultReproduceMax);

    aliveCountBySpecies_ = {0, 0};
    harvestMultiplierSum_ = {0.0f, 0.0f};
    upkeepMultiplierSum_ = {0.0f, 0.0f};
    birthMultiplierSum_ = {0.0f, 0.0f};
    ageMultiplierSum_ = {0.0f, 0.0f};

    for (unsigned y = 0; y < height_; ++y) {
        for (unsigned x = 0; x < width_; ++x) {
            const unsigned idx = index(x, y);
            if (!alive_[idx]) continue;

            const std::uint8_t speciesId = species_[idx] ? 1u : 0u;
            const auto& baseParams = Config::SpeciesBase::Data[speciesId];

            const int neighbors = countNeighbors(x, y);

            const float effectiveHarvest = baseParams.harvestFraction * harvestMultiplier_[idx];
            const float effectiveUpkeep = baseParams.upkeepCost * upkeepMultiplier_[idx];
            const float effectiveBirthThreshold = baseParams.birthEnergyThreshold * birthMultiplier_[idx];
            const float effectiveCorpseValue = baseParams.corpseNutrientValue * corpseMultiplier_[idx];
            const std::uint16_t effectiveMaxAge = static_cast<std::uint16_t>(
                std::clamp(static_cast<int>(std::lround(static_cast<float>(baseParams.maximumAge) * ageMultiplier_[idx])),
                           Config::Genetics::MinimumEffectiveAge, Config::Genetics::MaximumEffectiveAge));

            float localNutrients = nextNutrient_[idx];
            const float harvested = effectiveHarvest * localNutrients;
            localNutrients -= harvested;
            nextNutrient_[idx] = localNutrients;

            const float metabolicCost = effectiveUpkeep + Config::Metabolism::HarvestMetabolicCost * effectiveHarvest;
            float cellEnergy = energy_[idx] + harvested - metabolicCost;
            const std::uint16_t newAge = static_cast<std::uint16_t>(age_[idx] + 1);

            const bool overcrowded = (neighbors < static_cast<int>(surviveMinGene_[idx])) ||
                                     (neighbors > static_cast<int>(surviveMaxGene_[idx]));
            const bool died = (cellEnergy <= 0.0f) || (newAge > effectiveMaxAge) || overcrowded;

            if (died) {
                nextNutrient_[idx] = std::clamp(nextNutrient_[idx] + effectiveCorpseValue, 0.0f, Config::Nutrients::Maximum);
                continue;
            }

            unsigned targetCell = idx;

            if (enableMovement && cellEnergy < effectiveBirthThreshold &&
                cellEnergy < Config::Movement::EnergyThresholdToMove &&
                nextNutrient_[idx] < Config::Movement::NutrientThresholdToMove) {
                const int startDirection = static_cast<int>(rng_() % 8u);
                unsigned bestSpot = 0xFFFFFFFFu;
                float bestNutrients = -1.0f;

                for (int direction = 0; direction < 8; ++direction) {
                    const auto [dx, dy] = kNeighborOffsets[static_cast<std::size_t>((startDirection + direction) & 7)];
                    const unsigned xx = wrapCoordinate(static_cast<int>(x) + dx, width_);
                    const unsigned yy = wrapCoordinate(static_cast<int>(y) + dy, height_);
                    const unsigned neighborIdx = index(xx, yy);
                    if (alive_[neighborIdx]) continue;
                    if (nextAlive_[neighborIdx]) continue;
                    const float food = nextNutrient_[neighborIdx];
                    if (food > bestNutrients) {
                        bestNutrients = food;
                        bestSpot = neighborIdx;
                    }
                }

                if (bestSpot != 0xFFFFFFFFu &&
                    bestNutrients > nextNutrient_[idx] + Config::Movement::NutrientImprovementRequired) {
                    cellEnergy = std::max(0.0f, cellEnergy - Config::Movement::MovementEnergyCost);
                    targetCell = bestSpot;
                }
            }

            if (nextAlive_[targetCell]) targetCell = idx;

            nextAlive_[targetCell] = 1;
            nextSpecies_[targetCell] = speciesId;
            nextEnergy_[targetCell] = cellEnergy;
            nextAge_[targetCell] = newAge;
            ++aliveCountBySpecies_[speciesId];
            copyGeneticsFrom(idx, targetCell, false);

            if (cellEnergy < effectiveBirthThreshold) continue;
            if (neighbors < static_cast<int>(reproduceMinGene_[idx]) ||
                neighbors > static_cast<int>(reproduceMaxGene_[idx])) continue;

            const int reproductionStart = static_cast<int>(rng_() % 8u);
            int bestDirection = -1;
            float bestNutrients = -1.0f;

            for (int direction = 0; direction < 8; ++direction) {
                const auto [dx, dy] = kNeighborOffsets[static_cast<std::size_t>((reproductionStart + direction) & 7)];
                const unsigned xx = wrapCoordinate(static_cast<int>(x) + dx, width_);
                const unsigned yy = wrapCoordinate(static_cast<int>(y) + dy, height_);
                const unsigned neighborIdx = index(xx, yy);

                if (alive_[neighborIdx]) continue;
                if (nextAlive_[neighborIdx]) continue;

                const float food = nextNutrient_[neighborIdx];
                if (food > bestNutrients) {
                    bestNutrients = food;
                    bestDirection = (reproductionStart + direction) & 7;
                }
            }

            if (bestDirection == -1) continue;

            const auto [dx, dy] = kNeighborOffsets[static_cast<std::size_t>(bestDirection)];
            const unsigned xx = wrapCoordinate(static_cast<int>(x) + dx, width_);
            const unsigned yy = wrapCoordinate(static_cast<int>(y) + dy, height_);
            const unsigned childIdx = index(xx, yy);

            const float childEnergy = nextEnergy_[targetCell] * 0.5f;
            nextEnergy_[targetCell] -= childEnergy;

            nextAlive_[childIdx] = 1;
            nextSpecies_[childIdx] = maybeFlipSpecies(speciesId);
            nextEnergy_[childIdx] = childEnergy;
            nextAge_[childIdx] = 0;
            ++aliveCountBySpecies_[nextSpecies_[childIdx] ? 1 : 0];
            copyGeneticsFrom(idx, childIdx, true);
        }
    }
}

void Simulation::processSpontaneousBirths() {
    for (unsigned y = 0; y < height_; ++y) {
        for (unsigned x = 0; x < width_; ++x) {
            const unsigned idx = index(x, y);
            if (alive_[idx]) continue;
            if (nextAlive_[idx]) continue;

            const int neighbors = countNeighbors(x, y);
            if (neighbors != 3) continue;

            float localNutrients = nextNutrient_[idx];
            if (localNutrients < Config::Nutrients::ReproductionMinimum) continue;

            const float spent = std::min(localNutrients, Config::Nutrients::ReproductionNutrientCost);
            localNutrients -= spent;
            nextNutrient_[idx] = localNutrients;

            const auto counts = neighborSpeciesCounts(x, y);
            std::uint8_t speciesId = 0;
            if (counts[1] > counts[0]) speciesId = 1;
            else if (counts[1] == counts[0] && (rng_() & 1u)) speciesId = 1;

            nextAlive_[idx] = 1;
            nextSpecies_[idx] = maybeFlipSpecies(speciesId);
            nextEnergy_[idx] = Config::Cell::InitialEnergy + Config::Cell::EnergyForNewCell * spent;
            nextAge_[idx] = 0;
            ++aliveCountBySpecies_[nextSpecies_[idx] ? 1 : 0];
            const unsigned parent = pickAliveNeighborForSpecies(x, y, speciesId);
            copyGeneticsFrom(parent, idx, true);
        }
    }
}

void Simulation::swapBuffers() {
    alive_.swap(nextAlive_);
    species_.swap(nextSpecies_);
    energy_.swap(nextEnergy_);
    age_.swap(nextAge_);
    nutrient_.swap(nextNutrient_);

    harvestMultiplier_.swap(nextHarvestMultiplier_);
    upkeepMultiplier_.swap(nextUpkeepMultiplier_);
    birthMultiplier_.swap(nextBirthMultiplier_);
    corpseMultiplier_.swap(nextCorpseMultiplier_);
    ageMultiplier_.swap(nextAgeMultiplier_);
    surviveMinGene_.swap(nextSurviveMinGene_);
    surviveMaxGene_.swap(nextSurviveMaxGene_);
    reproduceMinGene_.swap(nextReproduceMinGene_);
    reproduceMaxGene_.swap(nextReproduceMaxGene_);
}

void Simulation::step(bool enableMovement) {
    diffuseNutrients();
    processSurvivorsAndReproduction(enableMovement);
    processSpontaneousBirths();
    swapBuffers();
}
