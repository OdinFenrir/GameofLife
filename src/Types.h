#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <random>

#include "Constants.h"

struct CellData {
    std::uint8_t alive = 0;
    std::uint8_t species = 0;
    std::uint8_t corpseSpecies = 0;
    
    float harvestMultiplier = 1.0f;
    float upkeepMultiplier = 1.0f;
    float birthMultiplier = 1.0f;
    float corpseMultiplier = 1.0f;
    float ageMultiplier = 1.0f;
    
    std::uint8_t surviveMinGene = Config::Genetics::DefaultSurviveMin;
    std::uint8_t surviveMaxGene = Config::Genetics::DefaultSurviveMax;
    std::uint8_t reproduceMinGene = Config::Genetics::DefaultReproduceMin;
    std::uint8_t reproduceMaxGene = Config::Genetics::DefaultReproduceMax;
    
    float energy = 0.0f;
    std::uint16_t age = 0;
};

class Simulation {
public:
    Simulation(unsigned width, unsigned height);
    ~Simulation() = default;

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    unsigned getWidth() const { return width_; }
    unsigned getHeight() const { return height_; }
    unsigned getGridSize() const { return width_ * height_; }

    const std::vector<std::uint8_t>& getAlive() const { 
        static std::vector<std::uint8_t> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].alive;
        return result;
    }
    const std::vector<std::uint8_t>& getSpecies() const {
        static std::vector<std::uint8_t> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].species;
        return result;
    }
    const std::vector<std::uint8_t>& getCorpseSpecies() const {
        static std::vector<std::uint8_t> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].corpseSpecies;
        return result;
    }
    const std::vector<float>& getEnergy() const {
        static std::vector<float> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].energy;
        return result;
    }
    const std::vector<std::uint16_t>& getAge() const {
        static std::vector<std::uint16_t> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].age;
        return result;
    }
    const std::vector<float>& getNutrient() const { return nutrient_; }
    const std::vector<CellData>& getCells() const { return cells_; }

    const std::vector<float>& getHarvestMultiplier() const {
        static std::vector<float> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].harvestMultiplier;
        return result;
    }
    const std::vector<float>& getUpkeepMultiplier() const {
        static std::vector<float> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].upkeepMultiplier;
        return result;
    }
    const std::vector<float>& getBirthMultiplier() const {
        static std::vector<float> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].birthMultiplier;
        return result;
    }
    const std::vector<float>& getAgeMultiplier() const {
        static std::vector<float> result;
        result.resize(gridSize_);
        for (size_t i = 0; i < gridSize_; ++i) result[i] = cells_[i].ageMultiplier;
        return result;
    }

    const std::array<std::size_t, 2>& getAliveCountBySpecies() const { return aliveCountBySpecies_; }
    const std::array<float, 2>& getHarvestMultiplierSum() const { return harvestMultiplierSum_; }
    const std::array<float, 2>& getUpkeepMultiplierSum() const { return upkeepMultiplierSum_; }
    const std::array<float, 2>& getBirthMultiplierSum() const { return birthMultiplierSum_; }
    const std::array<float, 2>& getAgeMultiplierSum() const { return ageMultiplierSum_; }

    bool hasChanged() const { return hasChanged_; }
    void clearChangedFlag() { hasChanged_ = false; }

    void clear();
    void reset();
    void step(bool enableMovement);

    void spawnCellCluster(unsigned x, unsigned y, std::uint8_t species);
    void addNutrientPatch(unsigned x, unsigned y, float amount, int radius);

private:
    unsigned width_;
    unsigned height_;
    unsigned gridSize_;

    std::vector<CellData> cells_;
    std::vector<CellData> nextCells_;
    std::vector<float> nutrient_;
    std::vector<float> nextNutrient_;

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

    bool hasChanged_ = true;

    unsigned index(unsigned x, unsigned y) const { return x + y * width_; }

    int countNeighbors(unsigned x, unsigned y) const;
    std::array<int, 2> neighborSpeciesCounts(unsigned x, unsigned y) const;
    unsigned pickAliveNeighborForSpecies(unsigned x, unsigned y, std::uint8_t desiredSpecies);

    float mutateMultiplier(float value, float sigma, float minVal, float maxVal);
    std::uint8_t mutateRule(std::uint8_t value, int minVal, int maxVal);
    std::uint8_t maybeFlipSpecies(std::uint8_t species);

    void initializeGenetics(unsigned idx);
    void copyGeneticsFrom(unsigned source, unsigned target, bool applyMutation);

    void diffuseNutrients();
    void processSurvivorsAndReproduction(bool enableMovement);
    void processSpontaneousBirths();
    void swapBuffers();
};
