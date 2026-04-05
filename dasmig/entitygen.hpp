#ifndef DASMIG_ENTITYGEN_HPP
#define DASMIG_ENTITYGEN_HPP

#include "random.hpp"
#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/// @file entitygen.hpp
/// @brief Entity generator library — composable, deterministic entity generation for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.
namespace dasmig
{

// Forward declaration.
class eg;

/// @brief Context passed to components during generation.
///
/// Provides access to previously generated component values and a seeded
/// random engine. Created and populated by the entity generator; not
/// user-constructible.
/// @see component::generate()
class generation_context
{
  public:
    /// @brief Check if a component value has already been generated.
    /// @param component_key The component key to look up.
    /// @return `true` if a value for the key exists in the context.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return _values.contains(component_key);
    }

    /// @brief Typed retrieval of an already-generated component value.
    /// @tparam T The expected type of the stored value.
    /// @param component_key The component key to retrieve.
    /// @return The value cast to @p T.
    /// @throws std::bad_any_cast If @p T does not match the stored type.
    template <typename T>
    [[nodiscard]] T get(const std::wstring& component_key) const
    {
        return std::any_cast<T>(_values.at(component_key));
    }

    /// @brief Access the random engine for this generation.
    /// @return A reference to the per-component seeded random engine.
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

/// @brief Abstract base for entity components.
///
/// Implement this interface to define custom components that the entity
/// generator can produce. Each component has a unique key, generates typed
/// random values, and converts them to display strings.
/// @see generation_context, eg::add()
class component
{
  public:
    /// @brief Virtual destructor for proper cleanup of derived classes.
    virtual ~component() = default;

    /// @brief Unique key identifying this component (e.g. `"name"`, `"age"`).
    /// @return The component key as a wide string.
    [[nodiscard]] virtual std::wstring key() const = 0;

    /// @brief Generate a random value for this component.
    /// @param ctx Context providing access to previously generated values
    ///            and a seeded random engine.
    /// @return The generated value wrapped in `std::any`.
    [[nodiscard]] virtual std::any generate(
        const generation_context& ctx) const = 0;

    /// @brief Convert a generated value to a displayable string.
    /// @param value The value previously returned by generate().
    /// @return A human-readable wide-string representation.
    /// @see default_to_string()
    [[nodiscard]] virtual std::wstring to_string(const std::any& value) const = 0;

    /// @brief Inclusion weight for this component (0.0 to 1.0).
    ///
    /// A value of 1.0 means always included; 0.5 means included roughly
    /// half the time. The generator may override this per-component at
    /// registration time via eg::add(comp, weight) or eg::weight().
    /// @return The inclusion probability.
    /// @see eg::weight()
    [[nodiscard]] virtual double weight() const { return 1.0; }

    /// @brief Validate a generated value.
    /// @param value The value to validate.
    /// @return `true` to accept, `false` to retry generation.
    /// @see eg::max_retries()
    [[nodiscard]] virtual bool validate([[maybe_unused]] const std::any& value) const
    {
        return true;
    }

    /// @brief Decide whether this component should be generated.
    ///
    /// Unlike weight() (probabilistic), this is logic-driven. Return
    /// `false` to skip generation entirely based on the current context.
    /// @param ctx Context with previously generated component values.
    /// @return `true` to generate, `false` to skip.
    [[nodiscard]] virtual bool should_generate(
        [[maybe_unused]] const generation_context& ctx) const
    {
        return true;
    }

  protected:
    /// @brief Default conversion covering common standard types.
    ///
    /// Handles `std::wstring`, `int`, `double`, `float`, `long`, and `bool`.
    /// Returns `"[?]"` for unrecognized types.
    /// @param value The value to convert.
    /// @return A wide-string representation.
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
        if (value.type() == typeid(long))
        {
            return std::to_wstring(std::any_cast<long>(value));
        }
        if (value.type() == typeid(bool))
        {
            return std::any_cast<bool>(value) ? L"true" : L"false";
        }

        return L"[?]";
    }
};

/// @name Generic Components
/// @brief Reusable component templates for common patterns.
/// @{

/// @brief Tag type indicating that generic components should use default_to_string().
struct use_default_formatter {};

/// @brief Component that always returns a fixed value.
/// @tparam T The value type.
/// @tparam Formatter Optional callable for custom string conversion
///                    (defaults to use_default_formatter).
template <typename T, typename Formatter = use_default_formatter>
class constant_component : public component
{
  public:
    /// @brief Construct a constant component.
    /// @param key Unique component key.
    /// @param value The fixed value to always return.
    /// @param fmt Optional custom formatter.
    explicit constant_component(std::wstring key, T value,
                                Formatter fmt = {})
        : _key{std::move(key)}, _value{std::move(value)},
          _fmt{std::move(fmt)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& /*ctx*/) const override
    {
        return _value;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    std::wstring _key;
    T _value;
    Formatter _fmt;
};

/// @brief Component that picks uniformly at random from a list of values.
/// @tparam T The value type.
/// @tparam Formatter Optional callable for custom string conversion.
template <typename T, typename Formatter = use_default_formatter>
class choice_component : public component
{
  public:
    /// @brief Construct a choice component.
    /// @param key Unique component key.
    /// @param choices Non-empty list of values to choose from.
    /// @param fmt Optional custom formatter.
    /// @throws std::invalid_argument If @p choices is empty.
    explicit choice_component(std::wstring key, std::vector<T> choices,
                              Formatter fmt = {})
        : _key{std::move(key)}, _choices{std::move(choices)},
          _fmt{std::move(fmt)}
    {
        if (_choices.empty())
            throw std::invalid_argument("choices must not be empty");
    }

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        return *ctx.random().get(_choices);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    std::wstring _key;
    std::vector<T> _choices;
    Formatter _fmt;
};

/// @brief Component that generates a uniform random value in [lo, hi].
/// @tparam T An arithmetic type (`int`, `double`, `float`, etc.).
/// @tparam Formatter Optional callable for custom string conversion.
template <typename T, typename Formatter = use_default_formatter>
    requires std::is_arithmetic_v<T>
class range_component : public component
{
  public:
    /// @brief Construct a range component.
    /// @param key Unique component key.
    /// @param lo Lower bound (inclusive).
    /// @param hi Upper bound (inclusive).
    /// @param fmt Optional custom formatter.
    explicit range_component(std::wstring key, T lo, T hi,
                             Formatter fmt = {})
        : _key{std::move(key)}, _lo{lo}, _hi{hi}, _fmt{std::move(fmt)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        return ctx.random().get(_lo, _hi);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    std::wstring _key;
    T _lo;
    T _hi;
    Formatter _fmt;
};

/// @brief Component that wraps a callable for one-off or computed values.
///
/// Avoids subclassing component entirely. The callable receives a
/// generation_context and returns a value of type @p T.
/// @tparam T The return type of the callable.
/// @tparam GenFn Callable type taking `const generation_context&`.
/// @tparam Formatter Optional callable for custom string conversion.
template <typename T, typename GenFn, typename Formatter = use_default_formatter>
class callback_component : public component
{
  public:
    /// @brief Construct a callback component.
    /// @param key Unique component key.
    /// @param fn Callable invoked during generation.
    /// @param fmt Optional custom formatter.
    explicit callback_component(std::wstring key, GenFn fn,
                                Formatter fmt = {})
        : _key{std::move(key)}, _fn{std::move(fn)}, _fmt{std::move(fmt)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        return _fn(ctx);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    std::wstring _key;
    GenFn _fn;
    Formatter _fmt;
};

/// @brief Deduction guide for callback_component.
///
/// Deduces @p T from the return type of @p GenFn so users don't need
/// to spell out the lambda type explicitly.
template <typename GenFn, typename Formatter = use_default_formatter>
callback_component(std::wstring, GenFn, Formatter = {})
    -> callback_component<
           std::invoke_result_t<GenFn, const generation_context&>,
           GenFn, Formatter>;

/// @brief Component that picks from a list of values using per-option weights.
///
/// Weights are relative (they don't need to sum to 1.0). A weight of 0
/// means the option is never selected.
/// @tparam T The value type.
/// @tparam Formatter Optional callable for custom string conversion.
template <typename T, typename Formatter = use_default_formatter>
class weighted_choice_component : public component
{
  public:
    /// @brief A value–weight pair for weighted selection.
    struct option
    {
        T value;         ///< The selectable value.
        double weight;   ///< Relative selection weight (must be >= 0).
    };

    /// @brief Construct a weighted choice component.
    /// @param key Unique component key.
    /// @param options Non-empty list of value–weight pairs. At least one
    ///                must have a positive weight.
    /// @param fmt Optional custom formatter.
    /// @throws std::invalid_argument If @p options is empty or all weights are zero.
    explicit weighted_choice_component(std::wstring key,
                                       std::vector<option> options,
                                       Formatter fmt = {})
        : _key{std::move(key)}, _options{std::move(options)},
          _fmt{std::move(fmt)}
    {
        if (_options.empty())
            throw std::invalid_argument("options must not be empty");
        if (!std::ranges::any_of(_options,
                [](const auto& o) { return o.weight > 0.0; }))
            throw std::invalid_argument(
                "at least one option must have a positive weight");
    }

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        std::vector<double> weights;
        weights.reserve(_options.size());
        std::ranges::transform(_options, std::back_inserter(weights),
            &option::weight);

        std::discrete_distribution<std::size_t> dist(
            weights.begin(), weights.end());
        return _options[dist(ctx.random().engine())].value;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    std::wstring _key;
    std::vector<option> _options;
    Formatter _fmt;
};

/// @}

/// @brief A generated entity holding component values in registration order.
///
/// Entities are produced by eg::generate() and provide typed access to
/// component values, seed signatures for replay, and serialization.
/// @see eg::generate()
class entity
{
  public:
    /// @brief Typed retrieval of a component value by key.
    /// @tparam T The expected type of the stored value.
    /// @param component_key The component key to retrieve.
    /// @return The value cast to @p T.
    /// @throws std::out_of_range If the key is not present.
    /// @throws std::bad_any_cast If @p T does not match the stored type.
    template <typename T>
    [[nodiscard]] T get(const std::wstring& component_key) const
    {
        return std::any_cast<T>(find_value(component_key));
    }

    /// @brief Type-erased retrieval of a component value by key.
    /// @param component_key The component key to retrieve.
    /// @return A const reference to the stored `std::any`.
    /// @throws std::out_of_range If the key is not present.
    [[nodiscard]] const std::any& get_any(const std::wstring& component_key) const
    {
        return find_value(component_key);
    }

    /// @brief Check if a component value exists by key.
    /// @param component_key The component key to look up.
    /// @return `true` if the entity contains a value for the key.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return std::ranges::any_of(_entries, [&component_key](const auto& e) {
            return e.key == component_key;
        });
    }

    /// @brief Retrieve the random seed used to generate a specific component.
    /// @param component_key The component key.
    /// @return The per-component seed for replay.
    /// @throws std::out_of_range If the key is not present.
    [[nodiscard]] std::uint64_t seed(const std::wstring& component_key) const
    {
        auto it = std::ranges::find(_entries, component_key, &entry::key);

        if (it == _entries.end())
        {
            throw std::out_of_range("component not found");
        }

        return it->seed;
    }

    /// @brief Retrieve the random seed used to generate this entity.
    ///
    /// This seed can reproduce all component seeds and thus the entire entity.
    /// @return The entity-level seed.
    [[nodiscard]] std::uint64_t seed() const
    {
        return _seed;
    }

    /// @brief Get all component keys present in this entity, in generation order.
    /// @return A vector of component keys.
    [[nodiscard]] std::vector<std::wstring> keys() const
    {
        std::vector<std::wstring> result;
        result.reserve(_entries.size());
        std::ranges::transform(_entries, std::back_inserter(result), &entry::key);
        return result;
    }

    /// @brief Return component values as a map of key to display string.
    /// @return An ordered map of key → formatted value.
    [[nodiscard]] std::map<std::wstring, std::wstring> to_map() const
    {
        std::map<std::wstring, std::wstring> result;
        for (const auto& e : _entries)
        {
            result.emplace(e.key, e.display);
        }
        return result;
    }

    /// @brief Number of component values in this entity.
    [[nodiscard]] std::size_t size() const { return _entries.size(); }

    /// @brief Check if the entity has no component values.
    [[nodiscard]] bool empty() const { return _entries.empty(); }

    /// @brief Convert all component values to a single formatted string.
    ///
    /// Each entry is rendered as `"key: display"` separated by two spaces.
    /// @return The formatted string, or empty if the entity is empty.
    [[nodiscard]] std::wstring to_string() const
    {
        auto parts = _entries
            | std::views::transform([](const auto& e) {
                return e.key + L": " + e.display;
            });

        return std::ranges::fold_left_first(parts,
            [](std::wstring acc, const std::wstring& part) {
                return std::move(acc) + L"  " + part;
            }).value_or(std::wstring{});
    }

    /// @brief Stream all component values in generation order.
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

/// @brief Observer interface for hooking into generation lifecycle events.
///
/// Override only the methods you care about; all default to no-ops.
/// Multiple observers can be attached to a generator via eg::add_observer().
/// @see eg::add_observer(), dasmig::ext::stats_observer
class generation_observer
{
  public:
    /// @brief Virtual destructor.
    virtual ~generation_observer() = default;

    /// @name Entity Lifecycle
    /// @{

    /// @brief Called before an entity is generated.
    virtual void on_before_generate() {}
    /// @brief Called after an entity is successfully generated.
    /// @param e The generated entity.
    virtual void on_after_generate([[maybe_unused]] const entity& e) {}

    /// @}
    /// @name Component Generation
    /// @{

    /// @brief Called before a component is generated.
    /// @param key The component key about to be generated.
    virtual void on_before_component([[maybe_unused]] const std::wstring& key) {}
    /// @brief Called after a component is successfully generated.
    /// @param key The component key.
    /// @param value The generated value.
    virtual void on_after_component([[maybe_unused]] const std::wstring& key,
                                    [[maybe_unused]] const std::any& value) {}

    /// @}
    /// @name Component Skip
    /// @{

    /// @brief Called when a component is skipped (weight roll or conditional exclusion).
    /// @param key The skipped component key.
    virtual void on_skip([[maybe_unused]] const std::wstring& key) {}

    /// @}
    /// @name Component Validation
    /// @{

    /// @brief Called before a component validation retry.
    /// @param key The component key being retried.
    /// @param attempt The retry attempt number (1-based).
    virtual void on_before_retry([[maybe_unused]] const std::wstring& key,
                                 [[maybe_unused]] std::size_t attempt) {}
    /// @brief Called after a component validation retry.
    /// @param key The component key.
    /// @param attempt The retry attempt number.
    /// @param value The newly generated value.
    virtual void on_after_retry([[maybe_unused]] const std::wstring& key,
                                [[maybe_unused]] std::size_t attempt,
                                [[maybe_unused]] const std::any& value) {}

    /// @brief Called when component validation is exhausted (precedes exception).
    /// @param key The failed component key.
    virtual void on_component_fail([[maybe_unused]] const std::wstring& key) {}

    /// @}
    /// @name Entity Validation
    /// @{

    /// @brief Called before an entity validation retry.
    /// @param attempt The entity retry attempt number (1-based).
    virtual void on_before_entity_retry([[maybe_unused]] std::size_t attempt) {}
    /// @brief Called after an entity validation retry.
    /// @param attempt The entity retry attempt number.
    virtual void on_after_entity_retry([[maybe_unused]] std::size_t attempt) {}

    /// @brief Called when entity validation is exhausted (precedes exception).
    virtual void on_entity_fail() {}

    /// @}
    /// @name Component Registration
    /// @{

    /// @brief Called before a component is registered.
    /// @param key The component key being added.
    virtual void on_before_add([[maybe_unused]] const std::wstring& key) {}
    /// @brief Called after a component is registered.
    /// @param key The component key that was added.
    virtual void on_after_add([[maybe_unused]] const std::wstring& key) {}

    /// @brief Called before a component is removed.
    /// @param key The component key being removed.
    virtual void on_before_remove([[maybe_unused]] const std::wstring& key) {}
    /// @brief Called after a component is removed.
    /// @param key The component key that was removed.
    virtual void on_after_remove([[maybe_unused]] const std::wstring& key) {}

    /// @}
};

/// @brief The entity generator — produces entities with configurable components.
///
/// Components are registered by implementing the component interface and
/// adding them to the generator. Components are generated in registration
/// order, allowing later components to access earlier ones via context.
/// @see component, entity, generation_context
class eg
{
  public:
    /// @brief Default constructor creates an empty generator.
    eg() = default;

    eg(const eg&) = delete;            ///< Not copyable.
    eg& operator=(const eg&) = delete; ///< Not copyable.
    eg(eg&&) = default;                ///< Move constructor.
    eg& operator=(eg&&) = default;     ///< Move assignment.
    ~eg() = default;                   ///< Default destructor.

    /// @brief Access the global singleton instance.
    ///
    /// For multi-threaded use, prefer creating independent eg instances.
    /// @return A reference to the singleton.
    static eg& instance()
    {
        static eg instance;
        return instance;
    }

    /// @name Registration
    /// @{

    /// @brief Register a component.
    ///
    /// Takes ownership of the component. If a component with the same key
    /// already exists, it will be replaced. Components are generated in the
    /// order they are registered.
    /// @param comp The component to register (moved).
    /// @return `*this` for chaining.
    eg& add(std::unique_ptr<component> comp)
    {
        const auto key = comp->key();
        notify(&generation_observer::on_before_add, key);

        auto it = std::ranges::find(_components, key, &component_entry::key);

        if (it != _components.end())
        {
            it->comp = std::move(comp);
        }
        else
        {
            _components.push_back({key, std::move(comp)});
        }

        notify(&generation_observer::on_after_add, key);
        return *this;
    }

    /// @brief Register a component with a weight override.
    ///
    /// The override takes precedence over the component's own weight() method.
    /// @param comp The component to register (moved).
    /// @param weight_override Inclusion probability override (0.0–1.0).
    /// @return `*this` for chaining.
    eg& add(std::unique_ptr<component> comp, double weight_override)
    {
        const auto key = comp->key();
        add(std::move(comp));
        _weight_overrides[key] = weight_override;
        return *this;
    }

    /// @brief Set or update the weight override for a registered component.
    /// @param component_key The component key.
    /// @param weight_value New inclusion probability (0.0–1.0).
    /// @return `*this` for chaining.
    /// @throws std::out_of_range If the component is not registered.
    eg& weight(const std::wstring& component_key, double weight_value)
    {
        if (!has(component_key))
        {
            throw std::out_of_range("component not found");
        }

        _weight_overrides[component_key] = weight_value;
        return *this;
    }

    /// @brief Remove a registered component by key.
    /// @param component_key The component key to remove.
    /// @return `*this` for chaining.
    eg& remove(const std::wstring& component_key)
    {
        notify(&generation_observer::on_before_remove, component_key);

        std::erase_if(_components, [&component_key](const auto& entry) {
            return entry.key == component_key;
        });
        _weight_overrides.erase(component_key);

        notify(&generation_observer::on_after_remove, component_key);
        return *this;
    }

    /// @brief Check if a component is registered by key.
    /// @param component_key The component key to look up.
    /// @return `true` if a component with the key exists.
    [[nodiscard]] bool has(const std::wstring& component_key) const
    {
        return std::ranges::any_of(_components, [&component_key](const auto& entry) {
            return entry.key == component_key;
        });
    }

    /// @brief Remove all registered components, weight overrides, and groups.
    /// @return `*this` for chaining.
    eg& clear()
    {
        _components.clear();
        _weight_overrides.clear();
        _groups.clear();
        return *this;
    }

    /// @brief Return the number of registered components.
    [[nodiscard]] std::size_t size() const { return _components.size(); }

    /// @brief Return all registered component keys in registration order.
    /// @return A vector of component keys.
    [[nodiscard]] std::vector<std::wstring> component_keys() const
    {
        std::vector<std::wstring> keys;
        keys.reserve(_components.size());
        std::ranges::transform(_components, std::back_inserter(keys),
            &component_entry::key);
        return keys;
    }

    /// @}
    /// @name Seeding
    /// @{

    /// @brief Seed the internal random engine.
    ///
    /// Subsequent generate() calls without an explicit seed will draw from
    /// this seeded sequence.
    /// @param seed_value The seed value.
    /// @return `*this` for chaining.
    eg& seed(std::uint64_t seed_value)
    {
        _engine.seed(static_cast<std::mt19937::result_type>(seed_value));

        return *this;
    }

    /// @brief Reseed the engine with a non-deterministic source.
    ///
    /// Subsequent generate() calls will produce non-reproducible results.
    /// @return `*this` for chaining.
    eg& unseed()
    {
        _engine.seed(std::random_device{}());

        return *this;
    }

    /// @}
    /// @name Validation
    /// @{

    /// @brief Set an entity-level validation function.
    ///
    /// After each entity is fully generated, the validator is called. If it
    /// returns `false`, the entity is re-generated up to max_retries times.
    /// @param fn Validator callable returning `true` to accept.
    /// @return `*this` for chaining.
    eg& set_validator(std::function<bool(const entity&)> fn)
    {
        _validator = std::move(fn);
        return *this;
    }

    /// @brief Remove the entity-level validation function.
    /// @return `*this` for chaining.
    eg& clear_validator()
    {
        _validator = nullptr;
        return *this;
    }

    /// @brief Set the maximum number of retries for validation (default 10).
    ///
    /// Applies to both component-level and entity-level validation.
    /// @param retries The maximum retry count.
    /// @return `*this` for chaining.
    eg& max_retries(std::size_t retries)
    {
        _max_retries = retries;
        return *this;
    }

    /// @}
    /// @name Observers
    /// @{

    /// @brief Add an observer to receive generation lifecycle callbacks.
    /// @param obs The observer (shared ownership).
    /// @return `*this` for chaining.
    /// @see generation_observer
    eg& add_observer(std::shared_ptr<generation_observer> obs)
    {
        _observers.push_back(std::move(obs));
        return *this;
    }

    /// @brief Remove a specific observer by identity.
    /// @param obs The observer to remove.
    /// @return `*this` for chaining.
    eg& remove_observer(const std::shared_ptr<generation_observer>& obs)
    {
        std::erase(_observers, obs);
        return *this;
    }

    /// @brief Remove all observers.
    /// @return `*this` for chaining.
    eg& clear_observers()
    {
        _observers.clear();
        return *this;
    }

    /// @}
    /// @name Generation
    /// @{

    /// @brief Generate an entity with all registered components.
    ///
    /// Uses the generator's internal engine.
    /// @return The generated entity.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] entity generate()
    {
        auto refs = all_component_refs();
        return generate_with_hooks([&] {
            return generate_impl(refs, _engine, _max_retries, _observers);
        });
    }

    /// @brief Generate an entity with all registered components using a seed.
    /// @param call_seed Seed for reproducible results.
    /// @return The generated entity.
    /// @throws std::runtime_error If validation is exhausted.
    [[nodiscard]] entity generate(std::uint64_t call_seed) const
    {
        auto refs = all_component_refs();
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_with_hooks([&] {
            return generate_impl(refs, engine, _max_retries, _observers);
        });
    }

    /// @brief Generate an entity with only the specified components.
    ///
    /// Uses the generator's internal engine. Unknown keys are silently skipped.
    /// @param component_keys The keys to include.
    /// @return The generated entity.
    /// @throws std::runtime_error If validation is exhausted.
    [[nodiscard]] entity generate(std::span<const std::wstring> component_keys)
    {
        auto filtered = filter_components(component_keys);
        return generate_with_hooks([&] {
            return generate_impl(filtered, _engine, _max_retries, _observers);
        });
    }

    /// @brief Generate an entity with only the specified components using a seed.
    /// @param component_keys The keys to include.
    /// @param call_seed Seed for reproducible results.
    /// @return The generated entity.
    /// @throws std::runtime_error If validation is exhausted.
    [[nodiscard]] entity generate(std::span<const std::wstring> component_keys,
                                  std::uint64_t call_seed) const
    {
        auto filtered = filter_components(component_keys);
        std::mt19937 engine{static_cast<std::mt19937::result_type>(call_seed)};
        return generate_with_hooks([&] {
            return generate_impl(filtered, engine, _max_retries, _observers);
        });
    }

    /// @brief Generate with an initializer list of keys (convenience overload).
    [[nodiscard]] entity generate(
        std::initializer_list<std::wstring> component_keys)
    {
        return generate(std::span<const std::wstring>{
            component_keys.begin(), component_keys.size()});
    }

    /// @brief Generate with an initializer list of keys and a seed.
    [[nodiscard]] entity generate(
        std::initializer_list<std::wstring> component_keys,
        std::uint64_t call_seed) const
    {
        return generate(std::span<const std::wstring>{
            component_keys.begin(), component_keys.size()}, call_seed);
    }

    /// @}
    /// @name Batch Generation
    /// @{

    /// @brief Generate multiple entities with all registered components.
    /// @param count Number of entities to generate.
    /// @return A vector of generated entities.
    [[nodiscard]] std::vector<entity> generate_batch(std::size_t count)
    {
        std::vector<entity> entities;
        entities.reserve(count);

        auto refs = all_component_refs();
        std::generate_n(std::back_inserter(entities), count,
            [&] {
                return generate_with_hooks([&] {
                    return generate_impl(refs, _engine, _max_retries, _observers);
                });
            });

        return entities;
    }

    /// @brief Generate multiple entities using a seed.
    ///
    /// The seed initializes the engine once; successive entities draw from
    /// the same deterministic sequence.
    /// @param count Number of entities to generate.
    /// @param call_seed Seed for reproducible results.
    /// @return A vector of generated entities.
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
                    return generate_impl(refs, engine, _max_retries, _observers);
                });
            });

        return entities;
    }

    /// @}
    /// @name Concurrent Batch Generation
    /// @{

    /// @brief Generate multiple entities concurrently.
    ///
    /// Entity seeds are pre-derived sequentially from the engine, then each
    /// entity is generated in its own async task. Results are returned in
    /// seed order. If observers are set, the caller is responsible for their
    /// thread safety.
    /// @param count Number of entities to generate.
    /// @return A vector of generated entities.
    [[nodiscard]] std::vector<entity> generate_batch_async(std::size_t count)
    {
        auto refs = all_component_refs();

        std::vector<std::uint64_t> seeds(count);
        std::ranges::generate(seeds,
            [&] { return static_cast<std::uint64_t>(_engine()); });

        std::vector<std::future<entity>> futures;
        futures.reserve(count);
        std::ranges::transform(seeds, std::back_inserter(futures),
            [this, &refs](auto task_seed) {
                return std::async(std::launch::async,
                    [this, &refs, task_seed] {
                        std::mt19937 engine{
                            static_cast<std::mt19937::result_type>(task_seed)};
                        return generate_with_hooks([&] {
                            return generate_impl(
                                refs, engine, _max_retries, _observers);
                        });
                    });
            });

        std::vector<entity> entities;
        entities.reserve(count);
        std::ranges::transform(futures, std::back_inserter(entities),
            [](auto& f) { return f.get(); });

        return entities;
    }

    /// @brief Generate multiple entities concurrently using a seed.
    ///
    /// The seed initializes a dedicated engine for pre-deriving per-entity
    /// seeds; all subsequent generation is parallel.
    /// @param count Number of entities to generate.
    /// @param call_seed Seed for reproducible results.
    /// @return A vector of generated entities.
    [[nodiscard]] std::vector<entity> generate_batch_async(
        std::size_t count, std::uint64_t call_seed) const
    {
        auto refs = all_component_refs();

        std::mt19937 seed_engine{
            static_cast<std::mt19937::result_type>(call_seed)};
        std::vector<std::uint64_t> seeds(count);
        std::ranges::generate(seeds,
            [&] { return static_cast<std::uint64_t>(seed_engine()); });

        std::vector<std::future<entity>> futures;
        futures.reserve(count);
        std::ranges::transform(seeds, std::back_inserter(futures),
            [this, &refs](auto task_seed) {
                return std::async(std::launch::async,
                    [this, &refs, task_seed] {
                        std::mt19937 engine{
                            static_cast<std::mt19937::result_type>(task_seed)};
                        return generate_with_hooks([&] {
                            return generate_impl(
                                refs, engine, _max_retries, _observers);
                        });
                    });
            });

        std::vector<entity> entities;
        entities.reserve(count);
        std::ranges::transform(futures, std::back_inserter(entities),
            [](auto& f) { return f.get(); });

        return entities;
    }

    /// @}
    /// @name Component Groups
    /// @{

    /// @brief Register a named group of component keys.
    /// @param group_name Name for the group.
    /// @param component_keys Keys in the group.
    /// @return `*this` for chaining.
    eg& add_group(const std::wstring& group_name,
                  std::vector<std::wstring> component_keys)
    {
        _groups[group_name] = std::move(component_keys);
        return *this;
    }

    /// @brief Remove a named group.
    /// @param group_name The group to remove.
    /// @return `*this` for chaining.
    eg& remove_group(const std::wstring& group_name)
    {
        _groups.erase(group_name);
        return *this;
    }

    /// @brief Check if a named group exists.
    /// @param group_name The group name to look up.
    /// @return `true` if the group exists.
    [[nodiscard]] bool has_group(const std::wstring& group_name) const
    {
        return _groups.contains(group_name);
    }

    /// @brief Generate an entity using a named group.
    /// @param group_name The group to generate.
    /// @return The generated entity.
    /// @throws std::out_of_range If the group does not exist.
    [[nodiscard]] entity generate_group(const std::wstring& group_name)
    {
        auto it = _groups.find(group_name);
        if (it == _groups.end())
        {
            throw std::out_of_range("group not found");
        }

        auto filtered = filter_components(it->second);
        return generate_with_hooks([&] {
            return generate_impl(filtered, _engine, _max_retries, _observers);
        });
    }

    /// @brief Generate an entity using a named group with a seed.
    /// @param group_name The group to generate.
    /// @param call_seed Seed for reproducible results.
    /// @return The generated entity.
    /// @throws std::out_of_range If the group does not exist.
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
            return generate_impl(filtered, engine, _max_retries, _observers);
        });
    }

    /// @}

  private:
    // Component entry: key + owned component pointer.
    struct component_entry
    {
        std::wstring key;
        std::unique_ptr<component> comp;
    };

    // Reference to a component entry for filtered generation.
    struct component_ref
    {
        std::wstring key;
        std::reference_wrapper<const component> comp;
        double effective_weight;
    };

    // Type alias for the observer list.
    using observer_list =
        std::vector<std::shared_ptr<generation_observer>>;

    // Notify all observers in a list by calling a member function.
    template <typename Hook, typename... Args>
    static void notify_all(const observer_list& observers,
                           Hook hook, Args&&... args)
    {
        for (const auto& obs : observers)
        {
            ((*obs).*hook)(std::forward<Args>(args)...);
        }
    }

    // Convenience: notify using the instance's observer list.
    template <typename Hook, typename... Args>
    void notify(Hook hook, Args&&... args) const
    {
        notify_all(_observers, hook, std::forward<Args>(args)...);
    }

    // Core generation logic shared by all overloads.
    [[nodiscard]] static entity generate_impl(
        std::span<const component_ref> components, std::mt19937& engine,
        std::size_t max_retries, const observer_list& observers)
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
                    notify_all(observers,
                        &generation_observer::on_skip, ref.key);
                    continue;
                }
            }

            // Conditional check: skip if the component opts out based on context.
            if (!ref.comp.get().should_generate(ctx))
            {
                notify_all(observers,
                    &generation_observer::on_skip, ref.key);
                continue;
            }

            notify_all(observers,
                &generation_observer::on_before_component, ref.key);

            ctx._random.seed(static_cast<std::mt19937::result_type>(component_seed));
            auto value = ref.comp.get().generate(ctx);

            // Component-level validation with retries.
            for (std::size_t retry = 0;
                 retry < max_retries && !ref.comp.get().validate(value);
                 ++retry)
            {
                notify_all(observers,
                    &generation_observer::on_before_retry, ref.key, retry + 1);
                component_seed = static_cast<std::uint64_t>(local_engine());
                ctx._random.seed(
                    static_cast<std::mt19937::result_type>(component_seed));
                value = ref.comp.get().generate(ctx);
                notify_all(observers,
                    &generation_observer::on_after_retry, ref.key,
                    retry + 1, value);
            }

            if (!ref.comp.get().validate(value))
            {
                notify_all(observers,
                    &generation_observer::on_component_fail, ref.key);
                throw std::runtime_error(
                    "component validation failed after max retries");
            }

            auto display = ref.comp.get().to_string(value);

            notify_all(observers,
                &generation_observer::on_after_component, ref.key, value);

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
        for (std::size_t attempt = 0; attempt <= _max_retries; ++attempt)
        {
            if (attempt > 0)
                notify_all(_observers,
                    &generation_observer::on_before_entity_retry, attempt);
            auto e = fn();
            if (attempt > 0)
                notify_all(_observers,
                    &generation_observer::on_after_entity_retry, attempt);
            if (!_validator || _validator(e)) return e;
        }

        notify_all(_observers, &generation_observer::on_entity_fail);
        throw std::runtime_error(
            "entity validation failed after max retries");
    }

    // Wrap a generation function with before/after generate hooks.
    template <typename GenFn>
    [[nodiscard]] entity generate_with_hooks(GenFn&& fn) const
    {
        notify_all(_observers, &generation_observer::on_before_generate);
        auto e = with_entity_validation(std::forward<GenFn>(fn));
        notify_all(_observers,
            &generation_observer::on_after_generate, e);
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
                return {entry.key, std::cref(*entry.comp),
                        effective_weight(entry.key, *entry.comp)};
            });
        return refs;
    }

    // Filter registered components by the requested keys, preserving
    // registration order.
    [[nodiscard]] std::vector<component_ref> filter_components(
        std::span<const std::wstring> component_keys) const
    {
        std::vector<component_ref> filtered;

        auto matching = _components
            | std::views::filter([&](const auto& entry) {
                return std::ranges::contains(component_keys, entry.key);
            })
            | std::views::transform([this](const auto& entry) -> component_ref {
                return {entry.key, std::cref(*entry.comp),
                        effective_weight(entry.key, *entry.comp)};
            });
        std::ranges::copy(matching, std::back_inserter(filtered));

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

    // Generation lifecycle observers (optional).
    observer_list _observers;

    // Maximum retries for component and entity validation.
    std::size_t _max_retries{10};

    // Internal random engine for the generator.
    std::mt19937 _engine{std::random_device{}()};
};
} // namespace dasmig

#endif // DASMIG_ENTITYGEN_HPP
