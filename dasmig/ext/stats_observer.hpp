#ifndef DASMIG_EXT_STATS_OBSERVER_HPP
#define DASMIG_EXT_STATS_OBSERVER_HPP

#include "../entitygen.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>

namespace dasmig::ext
{

// Observer that collects comprehensive generation statistics. All fields
// are public for direct access. Use reset() to zero everything.
class stats_observer : public generation_observer
{
  public:
    using clock = std::chrono::steady_clock;
    using duration = std::chrono::steady_clock::duration;

    // --- Entity counters --------------------------------------------------

    std::size_t entities_generated{0};
    std::size_t entity_retries{0};
    std::size_t entity_failures{0};

    // --- Entity timing ----------------------------------------------------

    // Wall-clock time summed across all entities generated.
    duration total_generation_time{duration::zero()};
    duration min_entity_time{duration::max()};
    duration max_entity_time{duration::zero()};

    // --- Component counters -----------------------------------------------

    std::size_t components_generated{0};
    std::size_t components_skipped{0};
    std::size_t component_failures{0};

    // --- Per-component-key counters ---------------------------------------

    // Total retries per component key.
    std::map<std::wstring, std::size_t> component_retries;

    // Times each key was generated successfully.
    std::map<std::wstring, std::size_t> component_counts;

    // Times each key was skipped (weight or conditional).
    std::map<std::wstring, std::size_t> component_skip_counts;

    // Times each key's validation was exhausted.
    std::map<std::wstring, std::size_t> component_failure_counts;

    // --- Per-component-key timing -----------------------------------------

    // Total generation time per component key (including retries).
    std::map<std::wstring, duration> component_times;

    // Fastest / slowest per component key.
    std::map<std::wstring, duration> component_min_times;
    std::map<std::wstring, duration> component_max_times;

    // --- Components-per-entity tracking -----------------------------------

    std::size_t min_components_per_entity{
        std::numeric_limits<std::size_t>::max()};
    std::size_t max_components_per_entity{0};
    // total_components_in_entities / entities_generated = average
    std::size_t total_components_in_entities{0};

    // --- Value distribution -----------------------------------------------

    // Per-component key, counts how many times each display string appeared.
    // Useful for analysing uniformity of random components.
    std::map<std::wstring,
             std::map<std::wstring, std::size_t>> value_distribution;

    // --- Computed helpers --------------------------------------------------

    // Average generation time per entity (zero if none generated).
    [[nodiscard]] duration avg_entity_time() const
    {
        if (entities_generated == 0) return duration::zero();
        return total_generation_time / entities_generated;
    }

    // Average generation time per component key.
    [[nodiscard]] duration avg_component_time(const std::wstring& key) const
    {
        auto it = component_times.find(key);
        auto ct = component_counts.find(key);
        if (it == component_times.end() || ct == component_counts.end()
            || ct->second == 0)
            return duration::zero();
        return it->second / ct->second;
    }

    // Average components produced per entity (0.0 if none generated).
    [[nodiscard]] double avg_components_per_entity() const
    {
        if (entities_generated == 0) return 0.0;
        return static_cast<double>(total_components_in_entities)
             / static_cast<double>(entities_generated);
    }

    // Retry rate: total retries / total component generations (0.0 if none).
    [[nodiscard]] double component_retry_rate() const
    {
        if (components_generated == 0) return 0.0;
        std::size_t total{0};
        for (const auto& [_, n] : component_retries) total += n;
        return static_cast<double>(total)
             / static_cast<double>(components_generated);
    }

    // Entity retry rate: entity retries / entities generated (0.0 if none).
    [[nodiscard]] double entity_retry_rate() const
    {
        if (entities_generated == 0) return 0.0;
        return static_cast<double>(entity_retries)
             / static_cast<double>(entities_generated);
    }

    // --- Report -----------------------------------------------------------

    // Produce a human-readable wide-string report of all collected stats.
    [[nodiscard]] std::wstring report() const
    {
        std::wostringstream o;
        o << std::fixed << std::setprecision(2);

        auto us = [](duration d) {
            return std::chrono::duration_cast<
                std::chrono::microseconds>(d).count();
        };

        // -- Entity summary ------------------------------------------------
        o << L"=== Entity Summary ===\n"
          << L"  Generated : " << entities_generated << L'\n'
          << L"  Retries   : " << entity_retries     << L'\n'
          << L"  Failures  : " << entity_failures     << L'\n';

        if (entities_generated > 0)
        {
            o << L"  Retry rate: " << (entity_retry_rate() * 100.0) << L" %\n";
        }

        // -- Entity timing -------------------------------------------------
        o << L"\n=== Entity Timing ===\n"
          << L"  Total : " << us(total_generation_time) << L" us\n";

        if (entities_generated > 0)
        {
            o << L"  Avg   : " << us(avg_entity_time()) << L" us\n"
              << L"  Min   : " << us(min_entity_time)   << L" us\n"
              << L"  Max   : " << us(max_entity_time)   << L" us\n";
        }

        // -- Components per entity -----------------------------------------
        o << L"\n=== Components per Entity ===\n";

        if (entities_generated > 0)
        {
            o << L"  Avg : " << avg_components_per_entity() << L'\n'
              << L"  Min : " << min_components_per_entity  << L'\n'
              << L"  Max : " << max_components_per_entity  << L'\n';
        }
        else
        {
            o << L"  (no entities generated)\n";
        }

        // -- Component summary ---------------------------------------------
        o << L"\n=== Component Summary ===\n"
          << L"  Generated : " << components_generated << L'\n'
          << L"  Skipped   : " << components_skipped   << L'\n'
          << L"  Failures  : " << component_failures   << L'\n';

        if (components_generated > 0)
        {
            o << L"  Retry rate: " << (component_retry_rate() * 100.0)
              << L" %\n";
        }

        // -- Per-key breakdown ---------------------------------------------
        // Collect all keys that appear in any per-key map.
        std::map<std::wstring, int> all_keys;
        for (const auto& [k, _] : component_counts)        all_keys[k];
        for (const auto& [k, _] : component_skip_counts)   all_keys[k];
        for (const auto& [k, _] : component_failure_counts) all_keys[k];
        for (const auto& [k, _] : component_retries)       all_keys[k];

        if (!all_keys.empty())
        {
            o << L"\n=== Per-Component Breakdown ===\n";

            for (const auto& [key, _] : all_keys)
            {
                o << L"\n  [" << key << L"]\n";

                auto count_or = [](const auto& m, const std::wstring& k)
                    -> std::size_t {
                    auto it = m.find(k);
                    return it != m.end() ? it->second : 0;
                };

                auto gen_n  = count_or(component_counts, key);
                auto skip_n = count_or(component_skip_counts, key);
                auto fail_n = count_or(component_failure_counts, key);
                auto ret_n  = count_or(component_retries, key);

                o << L"    Generated : " << gen_n  << L'\n'
                  << L"    Skipped   : " << skip_n << L'\n'
                  << L"    Retries   : " << ret_n  << L'\n'
                  << L"    Failures  : " << fail_n << L'\n';

                if (auto it = component_times.find(key);
                    it != component_times.end())
                {
                    o << L"    Time total: " << us(it->second) << L" us\n"
                      << L"    Time avg  : "
                      << us(avg_component_time(key)) << L" us\n";
                }

                if (auto it = component_min_times.find(key);
                    it != component_min_times.end())
                {
                    o << L"    Time min  : " << us(it->second) << L" us\n";
                }

                if (auto it = component_max_times.find(key);
                    it != component_max_times.end())
                {
                    o << L"    Time max  : " << us(it->second) << L" us\n";
                }
            }
        }

        // -- Value distribution --------------------------------------------
        if (!value_distribution.empty())
        {
            o << L"\n=== Value Distribution ===\n";

            for (const auto& [key, dist] : value_distribution)
            {
                o << L"\n  [" << key << L"]  ("
                  << dist.size() << L" distinct values)\n";

                // Sort by count descending for readability.
                std::vector<std::pair<std::wstring, std::size_t>> sorted(
                    dist.begin(), dist.end());
                std::ranges::sort(sorted, [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });

                auto total_n = count_for(key);
                for (const auto& [val, n] : sorted)
                {
                    double pct = total_n > 0
                        ? static_cast<double>(n)
                          / static_cast<double>(total_n) * 100.0
                        : 0.0;
                    o << L"    " << val << L" : " << n
                      << L"  (" << pct << L" %)\n";
                }
            }
        }

        return o.str();
    }

    // --- Reset ------------------------------------------------------------

    void reset()
    {
        entities_generated = 0;
        entity_retries = 0;
        entity_failures = 0;

        total_generation_time = duration::zero();
        min_entity_time = duration::max();
        max_entity_time = duration::zero();

        components_generated = 0;
        components_skipped = 0;
        component_failures = 0;

        component_retries.clear();
        component_counts.clear();
        component_skip_counts.clear();
        component_failure_counts.clear();

        component_times.clear();
        component_min_times.clear();
        component_max_times.clear();

        min_components_per_entity = std::numeric_limits<std::size_t>::max();
        max_components_per_entity = 0;
        total_components_in_entities = 0;

        value_distribution.clear();
    }

    // --- Hook overrides ---------------------------------------------------

    void on_before_generate() override
    {
        _entity_start = clock::now();
        _current_entity_components = 0;
    }

    void on_after_generate(const entity& /*e*/) override
    {
        ++entities_generated;

        auto elapsed = clock::now() - _entity_start;
        total_generation_time += elapsed;
        if (elapsed < min_entity_time) min_entity_time = elapsed;
        if (elapsed > max_entity_time) max_entity_time = elapsed;

        total_components_in_entities += _current_entity_components;
        if (_current_entity_components < min_components_per_entity)
            min_components_per_entity = _current_entity_components;
        if (_current_entity_components > max_components_per_entity)
            max_components_per_entity = _current_entity_components;
    }

    void on_before_component(const std::wstring& key) override
    {
        _component_start[key] = clock::now();
    }

    void on_after_component(const std::wstring& key,
                            const std::any& value) override
    {
        ++components_generated;
        ++component_counts[key];
        ++_current_entity_components;

        auto it = _component_start.find(key);
        if (it != _component_start.end())
        {
            auto elapsed = clock::now() - it->second;
            component_times[key] += elapsed;

            auto& mn = component_min_times[key];
            auto& mx = component_max_times[key];
            if (component_counts[key] == 1)
            {
                mn = elapsed;
                mx = elapsed;
            }
            else
            {
                if (elapsed < mn) mn = elapsed;
                if (elapsed > mx) mx = elapsed;
            }
        }

        // Record value distribution via the component's display string.
        // on_after_component only receives (key, value). The entity hasn't
        // stored the display string yet, so we use any_cast on common
        // types to produce a representative key. A catch-all records
        // "<other>" for types we can't stringify cheaply.
        value_distribution[key][display_for(value)]++;
    }

    void on_skip(const std::wstring& key) override
    {
        ++components_skipped;
        ++component_skip_counts[key];
    }

    void on_after_retry(const std::wstring& key,
                        std::size_t /*attempt*/,
                        const std::any& /*value*/) override
    {
        ++component_retries[key];
    }

    void on_component_fail(const std::wstring& key) override
    {
        ++component_failures;
        ++component_failure_counts[key];
    }

    void on_after_entity_retry(std::size_t /*attempt*/) override
    {
        ++entity_retries;
    }

    void on_entity_fail() override
    {
        ++entity_failures;
    }

  private:
    // Timing state (not part of the public stats).
    clock::time_point _entity_start{};
    std::map<std::wstring, clock::time_point> _component_start;
    std::size_t _current_entity_components{0};

    // Best-effort display string for value distribution tracking.
    static std::wstring display_for(const std::any& value)
    {
        if (auto* ws = std::any_cast<std::wstring>(&value))
            return *ws;
        if (auto* i = std::any_cast<int>(&value))
            return std::to_wstring(*i);
        if (auto* d = std::any_cast<double>(&value))
            return std::to_wstring(*d);
        if (auto* f = std::any_cast<float>(&value))
            return std::to_wstring(*f);
        if (auto* l = std::any_cast<long>(&value))
            return std::to_wstring(*l);
        if (auto* b = std::any_cast<bool>(&value))
            return *b ? L"true" : L"false";
        return L"<other>";
    }

    // Total generation count for a key (used by report).
    [[nodiscard]] std::size_t count_for(const std::wstring& key) const
    {
        auto it = component_counts.find(key);
        return it != component_counts.end() ? it->second : 0;
    }
};

} // namespace dasmig::ext

#endif // DASMIG_EXT_STATS_OBSERVER_HPP
