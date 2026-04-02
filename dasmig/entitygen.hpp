#ifndef DASMIG_ENTITYGEN_HPP
#define DASMIG_ENTITYGEN_HPP

#include "random.hpp"
#include <algorithm>
#include <any>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <ranges>
#include <span>
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

    // Get all component keys present in this entity, in generation order.
    [[nodiscard]] std::vector<std::wstring> keys() const
    {
        return _entries | std::views::transform(&entry::key)
                        | std::ranges::to<std::vector<std::wstring>>();
    }

    // Operator ostream streaming all component values in generation order.
    friend std::wostream& operator<<(std::wostream& wos, const entity& entity)
    {
        for (const auto& e : entity._entries)
        {
            wos << e.key << L": " << e.display << L"  ";
        }

        return wos;
    }

  private:
    // A single component entry in generation order.
    struct entry
    {
        std::wstring key;
        std::any value;
        std::wstring display;
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

    // Allows entity generator to construct entities.
    friend class eg;
};

// The entity generator generates entities with configurable components.
// Components are registered by implementing the component interface and
// adding them to the generator. Components are generated in registration
// order, allowing later components to access earlier ones via context.
class eg
{
  public:
    // Copy/move constructors can be deleted since they are not going to be
    // used due to singleton pattern.
    eg(const eg&) = delete;
    eg(eg&&) = delete;
    eg& operator=(const eg&) = delete;
    eg& operator=(eg&&) = delete;

    // Thread safe access to entity generator singleton.
    static eg& instance()
    {
        static eg instance;
        return instance;
    }

    // Register a component. Takes ownership of the component. If a component
    // with the same key already exists, it will be replaced. Components are
    // generated in the order they are registered.
    eg& add(std::unique_ptr<component> comp)
    {
        const auto key = comp->key();

        // Replace if already registered, preserving position.
        for (auto& [existing_key, existing_comp] : _components)
        {
            if (existing_key == key)
            {
                existing_comp = std::move(comp);
                return *this;
            }
        }

        _components.emplace_back(key, std::move(comp));

        return *this;
    }

    // Remove a registered component by key.
    eg& remove(const std::wstring& component_key)
    {
        std::erase_if(_components, [&component_key](const auto& pair) {
            return pair.first == component_key;
        });

        return *this;
    }

    // Check if a component is registered by key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return std::ranges::any_of(_components, [&component_key](const auto& pair) {
            return pair.first == component_key;
        });
    }

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

    // Generate an entity with all registered components using the
    // generator's internal engine.
    [[nodiscard]] entity generate()
    {
        auto refs = all_component_refs();
        return generate_impl(refs, _engine);
    }

    // Generate an entity with all registered components using a specific
    // seed for reproducible results.
    [[nodiscard]] entity generate(std::uint64_t call_seed) const
    {
        auto refs = all_component_refs();
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_impl(refs, engine);
    }

    // Generate an entity with only the specified components using the
    // generator's internal engine. Components with keys not found in the
    // registry are silently skipped. Note: components that depend on other
    // components via context will only see values from components that are
    // both registered before them and included in the requested keys.
    [[nodiscard]] entity generate(std::span<const std::wstring> component_keys)
    {
        auto filtered = filter_components(component_keys);
        return generate_impl(filtered, _engine);
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
        return generate_impl(filtered, engine);
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

  private:
    // Component entry: key + owned component pointer.
    using component_entry =
        std::pair<std::wstring, std::unique_ptr<component>>;

    // Reference to a component entry for filtered generation.
    using component_ref =
        std::pair<std::wstring, const component*>;

    // Initialize entity generator with no components.
    eg() = default;

    // We don't manage any resource, all should gracefully deallocate by itself.
    ~eg() = default;

    // Core generation logic shared by all overloads.
    [[nodiscard]] static entity generate_impl(
        std::span<const component_ref> components, std::mt19937& engine)
    {
        entity generated_entity;
        generation_context ctx;
        ctx._random.seed(engine());

        for (const auto& [key, comp] : components)
        {
            auto value = comp->generate(ctx);
            auto display = comp->to_string(value);

            ctx._values[key] = value;
            generated_entity._entries.push_back(
                {.key = key, .value = std::move(value),
                 .display = std::move(display)});
        }

        return generated_entity;
    }

    // Build a component_ref span from the owned component entries.
    [[nodiscard]] std::vector<component_ref> all_component_refs() const
    {
        std::vector<component_ref> refs;
        refs.reserve(_components.size());

        for (const auto& [key, comp] : _components)
        {
            refs.emplace_back(key, comp.get());
        }

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
            for (const auto& requested_key : component_keys)
            {
                if (key == requested_key)
                {
                    filtered.emplace_back(key, comp.get());
                    break;
                }
            }
        }

        return filtered;
    }

    // Registered components in insertion order.
    std::vector<component_entry> _components;

    // Internal random engine for the generator.
    std::mt19937 _engine{std::random_device{}()};
};
} // namespace dasmig

#endif // DASMIG_ENTITYGEN_HPP
