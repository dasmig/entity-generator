#ifndef DASMIG_ENTITYGEN_HPP
#define DASMIG_ENTITYGEN_HPP

#include <any>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>

// Written by Diego Dasso Migotto - diegomigotto at hotmail dot com
namespace dasmig
{

// Abstract base for entity components. Implement this interface to define
// custom components that the entity generator can produce.
class component
{
  public:
    // Virtual destructor to ensure proper cleanup of derived classes.
    virtual ~component() = default;

    // Unique key identifying this component (e.g. "name", "age", "class").
    [[nodiscard]] virtual std::wstring key() const = 0;

    // Generate a random value for this component.
    [[nodiscard]] virtual std::any generate() const = 0;

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
// produced by the entity generator.
class entity
{
  public:
    // Typed retrieval of a component value by key. The caller must know the
    // type that the component produces.
    template <typename T>
    [[nodiscard]] T get(const std::wstring& component_key) const
    {
        return std::any_cast<T>(_components.at(component_key));
    }

    // Type-erased retrieval of a component value by key.
    [[nodiscard]] const std::any& get_any(const std::wstring& component_key) const
    {
        return _components.at(component_key);
    }

    // Check if a component value exists by key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return _components.contains(component_key);
    }

    // Get all component keys present in this entity.
    [[nodiscard]] std::vector<std::wstring> keys() const
    {
        return _components | std::views::keys
                           | std::ranges::to<std::vector<std::wstring>>();
    }

    // Operator ostream streaming all component values.
    friend std::wostream& operator<<(std::wostream& wos, const entity& entity)
    {
        for (const auto& [key, display] : entity._display_strings)
        {
            wos << key << L": " << display << L"  ";
        }

        return wos;
    }

  private:
    // Private constructor, this is mostly a helper class to the entity
    // generator, not the intended API.
    entity() = default;

    // Component values indexed by key.
    std::map<std::wstring, std::any> _components;

    // Pre-computed display strings for streaming, indexed by key.
    std::map<std::wstring, std::wstring> _display_strings;

    // Allows entity generator to construct entities.
    friend class eg;
};

// The entity generator generates entities with configurable components.
// Components are registered by implementing the component interface and
// adding them to the generator.
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
    // with the same key already exists, it will be replaced.
    eg& add(std::unique_ptr<component> comp)
    {
        const auto key = comp->key();
        _components[key] = std::move(comp);

        return *this;
    }

    // Remove a registered component by key.
    eg& remove(const std::wstring& component_key)
    {
        _components.erase(component_key);

        return *this;
    }

    // Check if a component is registered by key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return _components.contains(component_key);
    }

    // Generate an entity with all registered components.
    [[nodiscard]] entity generate() const
    {
        entity generated_entity;

        for (const auto& [key, comp] : _components)
        {
            auto value = comp->generate();

            generated_entity._display_strings[key] = comp->to_string(value);
            generated_entity._components[key] = std::move(value);
        }

        return generated_entity;
    }

    // Generate an entity with only the specified components. Components with
    // keys not found in the registry are silently skipped.
    [[nodiscard]] entity generate(
        std::span<const std::wstring> component_keys) const
    {
        entity generated_entity;

        for (const auto& key : component_keys)
        {
            const auto it = _components.find(key);

            if (it != _components.end())
            {
                auto value = it->second->generate();

                generated_entity._display_strings[key] =
                    it->second->to_string(value);
                generated_entity._components[key] = std::move(value);
            }
        }

        return generated_entity;
    }

    // Overload accepting an initializer list for convenience.
    [[nodiscard]] entity generate(
        std::initializer_list<std::wstring> component_keys) const
    {
        return generate(std::span<const std::wstring>{
            component_keys.begin(), component_keys.size()});
    }

  private:
    // Initialize entity generator with no components.
    eg() = default;

    // We don't manage any resource, all should gracefully deallocate by itself.
    ~eg() = default;

    // Registered components indexed by key.
    std::map<std::wstring, std::unique_ptr<component>> _components;
};
} // namespace dasmig

#endif // DASMIG_ENTITYGEN_HPP
