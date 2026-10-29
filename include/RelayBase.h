#pragma once

#include "EntityBase.h"
#include <atomic>

constexpr const int DEFAULT_FAILSAFE_TIMEOUT_SEC = 300;
constexpr const int MIN_FAILSAFE_SEC=30;
constexpr const char CONFIG_KEY_FAILSAFE_TIMEOUT[] = "failsafe";
static_assert(sizeof(CONFIG_KEY_FAILSAFE_TIMEOUT) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");
constexpr const char CONFIG_KEY_OUTPUT[] = "output";
static_assert(sizeof(CONFIG_KEY_OUTPUT) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");
constexpr const char CONFIG_KEY_RELAY_MODE[] = "mode";
static_assert(sizeof(CONFIG_KEY_RELAY_MODE) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");


/**
 * @brief who decides the relay state: the switching rule (AUTO) or the user (FORCED_*)
 * @note the values are stored in the NVS - never renumber or reuse one.
 */
enum class RelayMode
{
    AUTO=0,
    FORCED_ON=1,
    FORCED_OFF=2
};

// Base for every switchable entity (relay output), regardless of manufacturer.
// Adds the Controller's decision surface (desiredOn, mode) and the confirmed device
// state (actualOn) on top of the generic EntityBase, so the Controller can reach any
// relay type through a RelayBase* without knowing the concrete manufacturer class.
// desiredOn/actualOn live only in RAM and reset to false on every boot. The mode is
// stored in the NVS, so a FORCED_ON set by the user survives a restart.
class RelayBase : public EntityBase
{
public:
    virtual ~RelayBase() = default;

    virtual bool buildSwitchRequest(bool on, char *buffer, size_t len) = 0;

    bool getActualOn() const;
    bool getDesiredOn() const;
    void setDesiredOn(bool on);

    RelayMode getMode() const;
    bool setMode(RelayMode mode);

    int getFailsafeSeconds() const;
    bool setFailsafeSeconds(int seconds);

    uint8_t getOutputIdx() const;

protected:
    RelayBase(const char *nvsNamespace, uint8_t idx, EntityType type);
    bool load() override;
    bool write(JsonObjectConst data) override;
    std::atomic<bool> actualOn{false}; // set by the concrete class's applyResponse()

private:
    void loadFailsafe();
    void loadMode();
    std::atomic<bool> _desiredOn{false};
    std::atomic<RelayMode> _mode{RelayMode::AUTO};
    std::atomic<int> _failsafeSeconds{DEFAULT_FAILSAFE_TIMEOUT_SEC};
    uint8_t _outputIdx=0;
};
