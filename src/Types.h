#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <random>

#include "Constants.h"

class Simulation {
public:
    Simulation(unsigned width, unsigned height);
    ~Simulation() = default;

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    unsigned getWidth() const { return width_; }
    unsigned getHeight() const { return height_; }
    unsigned getGridSize() const { return width_ * height_; }

    const std::vector<std::uint8_t>& getAlive() const { return alive_; }
    const std::vector<std::uint8_t>& getSpecies() const { return species_; }
    const std::vector<float>& getEnergy() const { return energy_; }
    const std::vector<std::uint16_t>& getAge() const { return age_; }
    const std::vector<float>& getNutrient() const { return nutrient_; }

    const std::vector<float>& getHarvestMultiplier() const { return harvestMultiplier_; }
    const std::vector<float>& getUpkeepMultiplier() const { return upkeepMultiplier_; }
    const std::vector<float>& getBirthMultiplier() const { return birthMultiplier_; }
    const std::vector<float>& getAgeMultiplier() const { return ageMultiplier_; }

    const std::array<std::size_t, 2>& getAliveCountBySpecies() const { return aliveCountBySpecies_; }
    const std::array<float, 2>& getHarvestMultiplierSum() const { return harvestMultiplierSum_; }
    const std::array<float, 2>& getUpkeepMultiplierSum() const { return upkeepMultiplierSum_; }
    const std::array<float, 2>& getBirthMultiplierSum() const { return birthMultiplierSum_; }
    const std::array<float, 2>& getAgeMultiplierSum() const { return ageMultiplierSum_; }

    void clear();
    void reset();
    void step(bool enableMovement);

    void spawnCellCluster(unsigned x, unsigned y, std::uint8_t species);
    void addNutrientPatch(unsigned x, unsigned y, float amount, int radius);

private:
    unsigned width_;
    unsigned height_;

    std::vector<std::uint8_t> alive_;
    std::vector<std::uint8_t> species_;

    std::vector<float> harvestMultiplier_;
    std::vector<float> upkeepMultiplier_;
    std::vector<float> birthMultiplier_;
    std::vector<float> corpseMultiplier_;
    std::vector<float> ageMultiplier_;

    std::vector<std::uint8_t> surviveMinGene_;
    std::vector<std::uint8_t> surviveMaxGene_;
    std::vector<std::uint8_t> reproduceMinGene_;
    std::vector<std::uint8_t> reproduceMaxGene_;

    std::vector<float> energy_;
    std::vector<std::uint16_t> age_;
    std::vector<float> nutrient_;

    std::vector<unsigned> leftNeighbor_;
    std::vector<unsigned> rightNeighbor_;
    std::vector<unsigned> upNeighbor_;
    std::vector<unsigned> downNeighbor_;

    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist01_;
    std::normal_distribution<float> distNormal_;

    std::array<std::size_t, 2> aliveCountBySpecies_;
    std::array<float, 2> harvestMultiplierSum_;
    std::array<float, 2> upkeepMultiplierSum_;
    std::array<float, 2> birthMultiplierSum_;
    std::array<float, 2> ageMultiplierSum_;

    std::vector<std::uint8_t> nextAlive_;
    std::vector<std::uint8_t> nextSpecies_;
    std::vector<float> nextHarvestMultiplier_;
    std::vector<float> nextUpkeepMultiplier_;
    std::vector<float> nextBirthMultiplier_;
    std::vector<float> nextCorpseMultiplier_;
    std::vector<float> nextAgeMultiplier_;
    std::vector<std::uint8_t> nextSurviveMinGene_;
    std::vector<std::uint8_t> nextSurviveMaxGene_;
    std::vector<std::uint8_t> nextReproduceMinGene_;
    std::vector<std::uint8_t> nextReproduceMaxGene_;
    std::vector<float> nextEnergy_;
    std::vector<std::uint16_t> nextAge_;
    std::vector<float> nextNutrient_;

    unsigned index(unsigned x, unsigned y) const { return x + y * width_; }

    int countNeighbors(unsigned x, unsigned y) const;
    std::array<int, 2> neighborSpeciesCounts(unsigned x, unsigned y) const;
    unsigned pickAliveNeighborForSpecies(unsigned x, unsigned y, std::uint8_t desiredSpecies);

    float mutateMultiplier(float value, float sigma, float minVal, float maxVal);
    std::uint8_t mutateRule(std::uint8_t value, int minVal, int maxVal);
    std::uint8_t maybeFlipSpecies(std::uint8_t species);

    void initializeGenetics(unsigned index);
    void copyGeneticsFrom(unsigned source, unsigned target, bool applyMutation);

    void diffuseNutrients();
    void processSurvivorsAndReproduction(bool enableMovement);
    void processSpontaneousBirths();
    void swapBuffers();
};
