#pragma once
// Fake climate component for local testing.
// Only provides what samsung_ac_device_custClim.h needs to be declared.

namespace esphome
{
    namespace climate
    {
        enum ClimateMode
        {
            CLIMATE_MODE_OFF = 0,
            CLIMATE_MODE_HEAT_COOL = 1,
            CLIMATE_MODE_COOL = 2,
            CLIMATE_MODE_HEAT = 3,
            CLIMATE_MODE_FAN_ONLY = 4,
            CLIMATE_MODE_DRY = 5,
            CLIMATE_MODE_AUTO = 6,
        };

        enum ClimatePreset
        {
            CLIMATE_PRESET_NONE = 0,
            CLIMATE_PRESET_HOME = 1,
            CLIMATE_PRESET_AWAY = 2,
            CLIMATE_PRESET_BOOST = 3,
            CLIMATE_PRESET_COMFORT = 4,
            CLIMATE_PRESET_ECO = 5,
            CLIMATE_PRESET_SLEEP = 6,
            CLIMATE_PRESET_ACTIVITY = 7,
        };

        class ClimateTraits
        {
        };

        class ClimateCall
        {
        };

        class Climate
        {
        public:
            ClimateMode mode{CLIMATE_MODE_OFF};
            ClimatePreset preset{CLIMATE_PRESET_NONE};
            void publish_state() {}
        };
    } // namespace climate
} // namespace esphome
