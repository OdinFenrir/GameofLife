#pragma once

#include <cstdint>

#include <SFML/Graphics/Color.hpp>

namespace Config {

namespace Window {
    constexpr unsigned Width = 2560;
    constexpr unsigned Height = 1440;
    constexpr unsigned CellSize = 2;
    constexpr unsigned GridWidth = Width / CellSize;
    constexpr unsigned GridHeight = Height / CellSize;
}

namespace Timing {
    constexpr float TicksPerSecond = 20.0f;
    constexpr float TickDeltaTime = 1.0f / TicksPerSecond;
    constexpr int MaxStepsPerFrame = 4;
}

namespace Nutrients {
    constexpr float Maximum = 3.0f;
    constexpr float DiffusionRate = 0.22f;
    constexpr float RegrowthRate = 0.0025f;
    constexpr float InitialBase = Maximum * 0.55f;
    constexpr float ReproductionMinimum = 1.10f;
    constexpr float ReproductionNutrientCost = 0.40f;
}

namespace Cell {
    constexpr float InitialEnergy = 0.55f;
    constexpr float EnergyForNewCell = 0.40f;
    constexpr float TrailDecay = 0.92f;
    constexpr float EnergyDisplayDivisor = 2.4f;
}

namespace SpeciesBase {
    struct Parameters {
        float harvestFraction;
        float upkeepCost;
        float birthEnergyThreshold;
        uint16_t maximumAge;
        float corpseNutrientValue;
    };

    constexpr Parameters Data[2] = {
        {0.095f, 0.033f, 1.70f, 1100, 0.70f},
        {0.070f, 0.026f, 2.05f, 1700, 0.90f},
    };
}

namespace Movement {
    constexpr float EnergyThresholdToMove = 0.28f;
    constexpr float NutrientThresholdToMove = 0.85f;
    constexpr float NutrientImprovementRequired = 0.18f;
    constexpr float MovementEnergyCost = 0.02f;
}

namespace Evolution {
    constexpr float TraitMutationChance = 0.06f;
    constexpr float MutationSigmaHarvest = 0.06f;
    constexpr float MutationSigmaUpkeep = 0.05f;
    constexpr float MutationSigmaBirth = 0.06f;
    constexpr float MutationSigmaCorpse = 0.06f;
    constexpr float MutationSigmaAge = 0.05f;

    constexpr float MinimumMultiplier = 0.60f;
    constexpr float MaximumHarvestMultiplier = 1.60f;
    constexpr float MaximumUpkeepMultiplier = 1.60f;
    constexpr float MaximumBirthMultiplier = 1.80f;
    constexpr float MaximumCorpseMultiplier = 1.80f;
    constexpr float MaximumAgeMultiplier = 1.70f;

    constexpr float RuleMutationChance = 0.015f;
    constexpr float SpeciationChance = 0.0015f;
}

namespace Genetics {
    constexpr uint8_t DefaultSurviveMin = 2;
    constexpr uint8_t DefaultSurviveMax = 4;
    constexpr uint8_t DefaultReproduceMin = 2;
    constexpr uint8_t DefaultReproduceMax = 3;

    constexpr int SurviveMinRangeLow = 1;
    constexpr int SurviveMinRangeHigh = 3;
    constexpr int SurviveMaxRangeLow = 3;
    constexpr int SurviveMaxRangeHigh = 5;
    constexpr int ReproduceMinRangeLow = 2;
    constexpr int ReproduceMinRangeHigh = 3;
    constexpr int ReproduceMaxRangeLow = 3;
    constexpr int ReproduceMaxRangeHigh = 4;

    constexpr int MinimumEffectiveAge = 650;
    constexpr int MaximumEffectiveAge = 3200;
}

namespace Metabolism {
    constexpr float HarvestMetabolicCost = 0.32f;
}

namespace Rendering {
    constexpr float CellColorBrightnessMin = 0.30f;
    constexpr float CellColorBrightnessMax = 1.0f;

    constexpr uint8_t BackgroundBaseR = 5;
    constexpr uint8_t BackgroundBaseG = 5;
    constexpr uint8_t BackgroundBaseB = 10;
    constexpr uint8_t NutrientTintR = 20;
    constexpr uint8_t NutrientTintG = 40;
    constexpr uint8_t NutrientTintB = 55;
}

namespace Colors {
    const sf::Color NeonIdle{0u, 255u, 190u, 255u};
    const sf::Color NeonHover{130u, 255u, 255u, 255u};

    const sf::Color Species0Young{0u, 255u, 255u, 255u};
    const sf::Color Species0Old{0u, 100u, 255u, 255u};
    const sf::Color Species1Young{255u, 255u, 0u, 255u};
    const sf::Color Species1Old{255u, 0u, 0u, 255u};

    const sf::Color HudText{200u, 255u, 230u, 255u};
}

}
