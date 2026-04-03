#ifndef DASMIG_ENTITYGEN_HPP
#define DASMIG_ENTITYGEN_HPP

#include "random.hpp"
#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Written by Diego Dasso Migotto - diegomigotto at hotmail dot com
namespace dasmig
{

// Forward declaration.
class eg;

// Context passed to components during generation, providing access to
// previously generated component values and a seeded random engine.
class generation_context
{
  public:
    // Check if a component value has already been generated.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return _values.contains(component_key);
    }

    // Typed retrieval of an already-generated component value by key.
    template <typename T>
    [[nodiscard]] T get(const std::wstring& component_key) const
    {
        return std::any_cast<T>(_values.at(component_key));
    }

    // Access the random engine for this generation.
    [[nodiscard]] effolkronium::random_local& random() const
    {
        return _random;
    }

  private:
    // Private constructor, this is managed by the entity generator.
    generation_context() = default;

    // Already-generated component values indexed by key.
    std::map<std::wstring, std::any> _values;

    // Instance-based random engine for reproducible generation.
    mutable effolkronium::random_local _random;

    // Allows entity generator to construct and populate contexts.
    friend class eg;
};

// Abstract base for entity components. Implement this interface to define
// custom components that the entity generator can produce.
class component
{
  public:
    // Virtual destructor to ensure proper cleanup of derived classes.
    virtual ~component() = default;

    // Unique key identifying this component (e.g. "name", "age", "class").
    [[nodiscard]] virtual std::wstring key() const = 0;

    // Generate a random value for this component. The context provides access
    // to previously generated component values and a seeded random engine.
    [[nodiscard]] virtual std::any generate(
        const generation_context& ctx) const = 0;

    // Convert a generated value to a displayable string. Derived classes must
    // implement this. Use default_to_string() for standard type handling.
    [[nodiscard]] virtual std::wstring to_string(const std::any& value) const = 0;

    // Inclusion weight for this component (0.0 to 1.0). A value of 1.0 means
    // always included; 0.5 means included roughly half the time. The generator
    // may override this value per-component at registration time.
    [[nodiscard]] virtual double weight() const { return 1.0; }

    // Validate a generated value. Return true to accept, false to retry.
    // Called automatically during generation. Default always accepts.
    [[nodiscard]] virtual bool validate(const std::any& /*value*/) const
    {
        return true;
    }

  protected:
    // Default conversion covering common standard types. Derived classes can
    // call this from their to_string() implementation.
    [[nodiscard]] static std::wstring default_to_string(const std::any& value)
    {
        if (value.type() == typeid(std::wstring))
        {
            return std::any_cast<std::wstring>(value);
        }
        if (value.type() == typeid(int))
        {
            return std::to_wstring(std::any_cast<int>(value));
        }
        if (value.type() == typeid(double))
        {
            return std::to_wstring(std::any_cast<double>(value));
        }
        if (value.type() == typeid(float))
        {
            return std::to_wstring(std::any_cast<float>(value));
        }
        if (value.type() == typeid(bool))
        {
            return std::any_cast<bool>(value) ? L"true" : L"false";
        }

        return L"[?]";
    }
};

// Internal class representing a generated entity. Holds component values
// produced by the entity generator in registration order.
class entity
{
  public:
    // Typed retrieval of a component value by key. The caller must know the
    // type that the component produces.
    template <typename T>
    [[nodiscard]] T get(const std::wstring& component_key) const
    {
        return std::any_cast<T>(find_value(component_key));
    }

    // Type-erased retrieval of a component value by key.
    [[nodiscard]] const std::any& get_any(const std::wstring& component_key) const
    {
        return find_value(component_key);
    }

    // Check if a component value exists by key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return std::ranges::any_of(_entries, [&component_key](const auto& e) {
            return e.key == component_key;
        });
    }

    // Retrieve the random seed used to generate a specific component.
    // This seed can be used to reproduce the component's random values.
    [[nodiscard]] std::uint64_t seed(const std::wstring& component_key) const
    {
        auto it = std::ranges::find(_entries, component_key, &entry::key);

        if (it == _entries.end())
        {
            throw std::out_of_range("component not found");
        }

        return it->seed;
    }

    // Retrieve the random seed used to generate this entity. This seed
    // can reproduce all component seeds and thus the entire entity.
    [[nodiscard]] std::uint64_t seed() const
    {
        return _seed;
    }

    // Get all component keys present in this entity, in generation order.
    [[nodiscard]] std::vector<std::wstring> keys() const
    {
        std::vector<std::wstring> result;
        result.reserve(_entries.size());
        std::ranges::transform(_entries, std::back_inserter(result), &entry::key);
        return result;
    }

    // Convert all component values to a single formatted string.
    // Each entry is rendered as "key: display" separated by "  ".
    [[nodiscard]] std::wstring to_string() const
    {
        std::wstring result;

        for (const auto& e : _entries)
        {
            if (!result.empty())
            {
                result += L"  ";
            }

            result += e.key + L": " + e.display;
        }

        return result;
    }

    // Operator ostream streaming all component values in generation order.
    friend std::wostream& operator<<(std::wostream& wos, const entity& e)
    {
        return wos << e.to_string();
    }

  private:
    // A single component entry in generation order.
    struct entry
    {
        std::wstring key;
        std::any value;
        std::wstring display;
        std::uint64_t seed;
    };

    // Private constructor, this is mostly a helper class to the entity
    // generator, not the intended API.
    entity() = default;

    // Find a component value by key.
    [[nodiscard]] const std::any& find_value(const std::wstring& component_key) const
    {
        auto it = std::ranges::find(_entries, component_key, &entry::key);

        if (it == _entries.end())
        {
            throw std::out_of_range("component not found");
        }

        return it->value;
    }

    // Component entries in generation order.
    std::vector<entry> _entries;

    // Seed used to generate this entity.
    std::uint64_t _seed{0};

    // Allows entity generator to construct entities.
    friend class eg;
};

// Observer interface for hooking into generation lifecycle events.
// Override only the methods you care about; all default to no-ops.
class generation_observer
{
  public:
    virtual ~generation_observer() = default;

    // Entity lifecycle.
    virtual void on_before_generate() {}
    virtual void on_after_generate(const entity& /*e*/) {}

    // Component generation.
    virtual void on_before_component(const std::wstring& /*key*/) {}
    virtual void on_after_component(const std::wstring& /*key*/,
                                    const std::any& /*value*/) {}

    // Component skip (weight roll excluded the component).
    virtual void on_before_skip(const std::wstring& /*key*/) {}
    virtual void on_after_skip(const std::wstring& /*key*/) {}

    // Component validation retry (attempt is 1-based).
    virtual void on_before_retry(const std::wstring& /*key*/,
                                 std::size_t /*attempt*/) {}
    virtual void on_after_retry(const std::wstring& /*key*/,
                                std::size_t /*attempt*/,
                                const std::any& /*value*/) {}

    // Component validation failure (terminal, precedes exception).
    virtual void on_before_fail(const std::wstring& /*key*/) {}
    virtual void on_after_fail(const std::wstring& /*key*/) {}

    // Entity validation retry (attempt is 1-based).
    virtual void on_before_entity_retry(std::size_t /*attempt*/) {}
    virtual void on_after_entity_retry(std::size_t /*attempt*/) {}

    // Entity validation failure (terminal, precedes exception).
    virtual void on_before_entity_fail() {}
    virtual void on_after_entity_fail() {}

    // Component registration.
    virtual void on_before_add(const std::wstring& /*key*/) {}
    virtual void on_after_add(const std::wstring& /*key*/) {}

    // Component removal.
    virtual void on_before_remove(const std::wstring& /*key*/) {}
    virtual void on_after_remove(const std::wstring& /*key*/) {}
};

// The entity generator generates entities with configurable components.
// Components are registered by implementing the component interface and
// adding them to the generator. Components are generated in registration
// order, allowing later components to access earlier ones via context.
class eg
{
  public:
    // Default constructor creates an empty generator.
    eg() = default;

    // Move-only: components are held via unique_ptr.
    eg(const eg&) = delete;
    eg& operator=(const eg&) = delete;
    eg(eg&&) = default;
    eg& operator=(eg&&) = default;
    ~eg() = default;

    // Access the global singleton instance. For multi-threaded use,
    // prefer creating independent eg instances instead.
    static eg& instance()
    {
        static eg instance;
        return instance;
    }

    // --- Registration ----------------------------------------------------

    // Register a component. Takes ownership of the component. If a component
    // with the same key already exists, it will be replaced. Components are
    // generated in the order they are registered.
    eg& add(std::unique_ptr<component> comp)
    {
        const auto key = comp->key();
        if (_observer) _observer->on_before_add(key);

        auto it = std::ranges::find(_components, key, &component_entry::first);

        if (it != _components.end())
        {
            it->second = std::move(comp);
        }
        else
        {
            _components.emplace_back(key, std::move(comp));
        }

        if (_observer) _observer->on_after_add(key);
        return *this;
    }

    // Register a component with a weight override. The override takes
    // precedence over the component's own weight() method.
    eg& add(std::unique_ptr<component> comp, double weight_override)
    {
        const auto key = comp->key();
        add(std::move(comp));
        _weight_overrides[key] = weight_override;
        return *this;
    }

    // Set or update the weight override for an already-registered component.
    // Throws std::out_of_range if the component is not registered.
    eg& weight(const std::wstring& component_key, double weight_value)
    {
        if (!has(component_key))
        {
            throw std::out_of_range("component not found");
        }

        _weight_overrides[component_key] = weight_value;
        return *this;
    }

    // Remove a registered component by key.
    eg& remove(const std::wstring& component_key)
    {
        if (_observer) _observer->on_before_remove(component_key);

        std::erase_if(_components, [&component_key](const auto& pair) {
            return pair.first == component_key;
        });
        _weight_overrides.erase(component_key);

        if (_observer) _observer->on_after_remove(component_key);
        return *this;
    }

    // Check if a component is registered by key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return std::ranges::any_of(_components, [&component_key](const auto& pair) {
            return pair.first == component_key;
        });
    }

    // --- Seeding ---------------------------------------------------------

    // Seed the internal random engine. Subsequent generate() calls without
    // an explicit seed will draw from this seeded sequence.
    eg& seed(std::uint64_t seed_value)
    {
        _engine.seed(static_cast<std::mt19937::result_type>(seed_value));

        return *this;
    }

    // Reseed the internal engine with a non-deterministic source.
    // Subsequent generate() calls will produce non-reproducible results.
    eg& unseed()
    {
        _engine.seed(std::random_device{}());

        return *this;
    }

    // --- Validation ------------------------------------------------------

    // Set an entity-level validation function. After each entity is fully
    // generated, the validator is called. If it returns false, the entity
    // is re-generated up to max_retries additional times.
    eg& set_validator(std::function<bool(const entity&)> fn)
    {
        _validator = std::move(fn);
        return *this;
    }

    // Remove the entity-level validation function.
    eg& clear_validator()
    {
        _validator = nullptr;
        return *this;
    }

    // Set the maximum number of retries for both component-level and
    // entity-level validation (default 10).
    eg& max_retries(std::size_t retries)
    {
        _max_retries = retries;
        return *this;
    }

    // --- Observer ---------------------------------------------------------

    // Set an observer to receive generation lifecycle callbacks.
    eg& set_observer(std::shared_ptr<generation_observer> obs)
    {
        _observer = std::move(obs);
        return *this;
    }

    // Remove the observer.
    eg& clear_observer()
    {
        _observer.reset();
        return *this;
    }

    // --- Generation ------------------------------------------------------

    // Generate an entity with all registered components using the
    // generator's internal engine.
    [[nodiscard]] entity generate()
    {
        auto refs = all_component_refs();
        return generate_with_hooks([&] {
            return generate_impl(refs, _engine, _max_retries, _observer.get());
        });
    }

    // Generate an entity with all registered components using a specific
    // seed for reproducible results.
    [[nodiscard]] entity generate(std::uint64_t call_seed) const
    {
        auto refs = all_component_refs();
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_with_hooks([&] {
            return generate_impl(refs, engine, _max_retries, _observer.get());
        });
    }

    // Generate an entity with only the specified components using the
    // generator's internal engine. Components with keys not found in the
    // registry are silently skipped. Note: components that depend on other
    // components via context will only see values from components that are
    // both registered before them and included in the requested keys.
    [[nodiscard]] entity generate(std::span<const std::wstring> component_keys)
    {
        auto filtered = filter_components(component_keys);
        return generate_with_hooks([&] {
            return generate_impl(filtered, _engine, _max_retries, _observer.get());
        });
    }

    // Generate an entity with only the specified components using a specific
    // seed. Components with keys not found in the registry are silently
    // skipped. Note: components that depend on other components via context
    // will only see values from components that are both registered before
    // them and included in the requested keys.
    [[nodiscard]] entity generate(std::span<const std::wstring> component_keys,
                                  std::uint64_t call_seed) const
    {
        auto filtered = filter_components(component_keys);
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_with_hooks([&] {
            return generate_impl(filtered, engine, _max_retries, _observer.get());
        });
    }

    // Overload accepting an initializer list for convenience.
    [[nodiscard]] entity generate(
        std::initializer_list<std::wstring> component_keys)
    {
        return generate(std::span<const std::wstring>{
            component_keys.begin(), component_keys.size()});
    }

    // Overload accepting an initializer list with seed for convenience.
    [[nodiscard]] entity generate(
        std::initializer_list<std::wstring> component_keys,
        std::uint64_t call_seed) const
    {
        return generate(std::span<const std::wstring>{
            component_keys.begin(), component_keys.size()}, call_seed);
    }

    // --- Batch generation ------------------------------------------------

    // Generate multiple entities with all registered components.
    [[nodiscard]] std::vector<entity> generate_batch(std::size_t count)
    {
        std::vector<entity> entities;
        entities.reserve(count);

        auto refs = all_component_refs();
        std::generate_n(std::back_inserter(entities), count,
            [&] {
                return generate_with_hooks([&] {
                    return generate_impl(refs, _engine, _max_retries, _observer.get());
                });
            });

        return entities;
    }

    // Generate multiple entities with all registered components using a
    // specific seed. The seed initializes the engine once; successive
    // entities draw from the same deterministic sequence.
    [[nodiscard]] std::vector<entity> generate_batch(
        std::size_t count, std::uint64_t call_seed) const
    {
        std::vector<entity> entities;
        entities.reserve(count);

        auto refs = all_component_refs();
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        std::generate_n(std::back_inserter(entities), count,
            [&] {
                return generate_with_hooks([&] {
                    return generate_impl(refs, engine, _max_retries, _observer.get());
                });
            });

        return entities;
    }

    // --- Component groups ------------------------------------------------

    // Register a named group of component keys for convenient generation.
    eg& add_group(const std::wstring& group_name,
                  std::vector<std::wstring> component_keys)
    {
        _groups[group_name] = std::move(component_keys);
        return *this;
    }

    // Remove a named group.
    eg& remove_group(const std::wstring& group_name)
    {
        _groups.erase(group_name);
        return *this;
    }

    // Check if a named group exists.
    [[nodiscard]] bool has_group(const std::wstring& group_name) const
    {
        return _groups.contains(group_name);
    }

    // Generate an entity using a named group of components.
    [[nodiscard]] entity generate_group(const std::wstring& group_name)
    {
        auto it = _groups.find(group_name);
        if (it == _groups.end())
        {
            throw std::out_of_range("group not found");
        }

        auto filtered = filter_components(it->second);
        return generate_with_hooks([&] {
            return generate_impl(filtered, _engine, _max_retries, _observer.get());
        });
    }

    // Generate an entity using a named group with a specific seed.
    [[nodiscard]] entity generate_group(const std::wstring& group_name,
                                        std::uint64_t call_seed) const
    {
        auto it = _groups.find(group_name);
        if (it == _groups.end())
        {
            throw std::out_of_range("group not found");
        }

        auto filtered = filter_components(it->second);
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_with_hooks([&] {
            return generate_impl(filtered, engine, _max_retries, _observer.get());
        });
    }

  private:
    // Component entry: key + owned component pointer.
    using component_entry =
        std::pair<std::wstring, std::unique_ptr<component>>;

    // Reference to a component entry for filtered generation.
    struct component_ref
    {
        std::wstring key;
        std::reference_wrapper<const component> comp;
        double effective_weight;
    };

    // Core generation logic shared by all overloads.
    [[nodiscard]] static entity generate_impl(
        std::span<const component_ref> components, std::mt19937& engine,
        std::size_t max_retries, generation_observer* obs)
    {
        entity generated_entity;
        generation_context ctx;

        // Derive an entity-level seed and use it to create a local engine
        // for per-component seed derivation.
        auto entity_seed = static_cast<std::uint64_t>(engine());
        generated_entity._seed = entity_seed;
        std::mt19937 local_engine{static_cast<std::mt19937::result_type>(entity_seed)};

        for (const auto& ref : components)
        {
            auto component_seed = static_cast<std::uint64_t>(local_engine());

            // Roll against the effective weight. A seed is always consumed
            // to keep the deterministic sequence stable regardless of which
            // components are included.
            if (ref.effective_weight < 1.0)
            {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(local_engine) >= ref.effective_weight)
                {
                    if (obs)
                    {
                        obs->on_before_skip(ref.key);
                        obs->on_after_skip(ref.key);
                    }
                    continue;
                }
            }

            if (obs) obs->on_before_component(ref.key);

            ctx._random.seed(static_cast<std::mt19937::result_type>(component_seed));
            auto value = ref.comp.get().generate(ctx);

            // Component-level validation with retries.
            for (std::size_t retry = 0;
                 retry < max_retries && !ref.comp.get().validate(value);
                 ++retry)
            {
                if (obs) obs->on_before_retry(ref.key, retry + 1);
                component_seed = static_cast<std::uint64_t>(local_engine());
                ctx._random.seed(
                    static_cast<std::mt19937::result_type>(component_seed));
                value = ref.comp.get().generate(ctx);
                if (obs) obs->on_after_retry(ref.key, retry + 1, value);
            }

            if (!ref.comp.get().validate(value))
            {
                if (obs)
                {
                    obs->on_before_fail(ref.key);
                    obs->on_after_fail(ref.key);
                }
                throw std::runtime_error(
                    "component validation failed after max retries");
            }

            auto display = ref.comp.get().to_string(value);

            if (obs) obs->on_after_component(ref.key, value);

            ctx._values[ref.key] = value;
            generated_entity._entries.push_back(
                {.key = ref.key, .value = std::move(value),
                 .display = std::move(display),
                 .seed = component_seed});
        }

        return generated_entity;
    }

    // --- Generation helpers -----------------------------------------------

    // Apply entity-level validation with retries.
    template <typename GenFn>
    [[nodiscard]] entity with_entity_validation(GenFn&& fn) const
    {
        auto* obs = _observer.get();

        for (std::size_t attempt = 0; attempt <= _max_retries; ++attempt)
        {
            if (attempt > 0 && obs) obs->on_before_entity_retry(attempt);
            auto e = fn();
            if (attempt > 0 && obs) obs->on_after_entity_retry(attempt);
            if (!_validator || _validator(e)) return e;
        }

        if (obs)
        {
            obs->on_before_entity_fail();
            obs->on_after_entity_fail();
        }
        throw std::runtime_error(
            "entity validation failed after max retries");
    }

    // Wrap a generation function with before/after generate hooks.
    template <typename GenFn>
    [[nodiscard]] entity generate_with_hooks(GenFn&& fn) const
    {
        auto* obs = _observer.get();
        if (obs) obs->on_before_generate();
        auto e = with_entity_validation(std::forward<GenFn>(fn));
        if (obs) obs->on_after_generate(e);
        return e;
    }

    // --- Component resolution ---------------------------------------------

    // Build a component_ref span from the owned component entries.
    [[nodiscard]] std::vector<component_ref> all_component_refs() const
    {
        std::vector<component_ref> refs;
        refs.reserve(_components.size());
        std::ranges::transform(_components, std::back_inserter(refs),
            [this](const auto& entry) -> component_ref {
                return {entry.first, std::cref(*entry.second),
                        effective_weight(entry.first, *entry.second)};
            });
        return refs;
    }

    // Filter registered components by the requested keys, preserving
    // registration order.
    [[nodiscard]] std::vector<component_ref> filter_components(
        std::span<const std::wstring> component_keys) const
    {
        std::vector<component_ref> filtered;

        for (const auto& [key, comp] : _components)
        {
            if (std::ranges::contains(component_keys, key))
            {
                filtered.push_back({key, std::cref(*comp),
                    effective_weight(key, *comp)});
            }
        }

        return filtered;
    }

    // Resolve the effective weight for a component: override wins, then
    // the component's own weight() method.
    [[nodiscard]] double effective_weight(
        const std::wstring& key, const component& comp) const
    {
        auto it = _weight_overrides.find(key);
        return it != _weight_overrides.end() ? it->second : comp.weight();
    }

    // Registered components in insertion order.
    std::vector<component_entry> _components;

    // Named groups of component keys.
    std::map<std::wstring, std::vector<std::wstring>> _groups;

    // Per-component weight overrides (key -> weight).
    std::map<std::wstring, double> _weight_overrides;

    // Entity-level validation function (optional).
    std::function<bool(const entity&)> _validator;

    // Generation lifecycle observer (optional).
    std::shared_ptr<generation_observer> _observer;

    // Maximum retries for component and entity validation.
    std::size_t _max_retries{10};

    // Internal random engine for the generator.
    std::mt19937 _engine{std::random_device{}()};
};
} // namespace dasmig

#endif // DASMIG_ENTITYGEN_HPP
