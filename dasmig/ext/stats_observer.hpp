#ifndef DASMIG_EXT_STATS_OBSERVER_HPP
#define DASMIG_EXT_STATS_OBSERVER_HPP

#include "../entitygen.hpp"
#include <map>
#include <string>

namespace dasmig::ext
{

// Observer that collects generation statistics. All fields are public
// for direct access. Use reset() to zero everything.
class stats_observer : public generation_observer
{
  public:
    // Entity counters.
    std::size_t entities_generated{0};
    std::size_t entity_retries{0};
    std::size_t entity_failures{0};

    // Component counters.
    std::size_t components_generated{0};
    std::size_t components_skipped{0};
    std::size_t component_failures{0};

    // Per-component retry counts (key -> total retries).
    std::map<std::wstring, std::size_t> component_retries;

    // Reset all counters to zero.
    void reset()
    {
        entities_generated = 0;
        entity_retries = 0;
        entity_failures = 0;
        components_generated = 0;
        components_skipped = 0;
        component_failures = 0;
        component_retries.clear();
    }

    // Entity lifecycle.
    void on_after_generate(const entity& /*e*/) override
    {
        ++entities_generated;
    }

    // Component generation.
    void on_after_component(const std::wstring& /*key*/,
                            const std::any& /*value*/) override
    {
        ++components_generated;
    }

    // Component skip (weight roll or conditional exclusion).
    void on_skip(const std::wstring& /*key*/) override
    {
        ++components_skipped;
    }

    // Component validation retry.
    void on_after_retry(const std::wstring& key,
                        std::size_t /*attempt*/,
                        const std::any& /*value*/) override
    {
        ++component_retries[key];
    }

    // Component validation failure (terminal).
    void on_component_fail(const std::wstring& /*key*/) override
    {
        ++component_failures;
    }

    // Entity validation retry.
    void on_after_entity_retry(std::size_t /*attempt*/) override
    {
        ++entity_retries;
    }

    // Entity validation failure (terminal).
    void on_entity_fail() override
    {
        ++entity_failures;
    }
};

} // namespace dasmig::ext

#endif // DASMIG_EXT_STATS_OBSERVER_HPP
