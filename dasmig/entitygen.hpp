#ifndef DASMIG_ENTITYGEN_HPP
#define DASMIG_ENTITYGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <any>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

/// @file entitygen.hpp
/// @brief Entity generator library — composable, deterministic entity generation for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 0);

namespace dasmig
{

/// @brief A component value as JSON: null, a boolean, an integer, a number
///        or a (UTF-8) string.
/// @see component::to_json()
using json_value = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string>;

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

    /// @brief Access the random engine for this component.
    ///
    /// Seeded with the component's seed (entity::seed(key)), so the same
    /// draws reproduce the value. Use it with the dasmig-core functions,
    /// which give the same results on every platform:
    /// @code
    /// int age = dasmig::core::uniform_int(ctx.random(), 18, 65);
    /// const auto& title = dasmig::core::pick(ctx.random(), titles);
    /// bool lucky = dasmig::core::bernoulli(ctx.random(), 0.1);
    /// @endcode
    /// @return A reference to the per-component seeded engine.
    [[nodiscard]] core::rng& random() const
    {
        return _random;
    }

  private:
    // Private constructor, this is managed by the entity generator.
    generation_context() = default;

    // Already-generated component values indexed by key.
    std::map<std::wstring, std::any> _values;

    // Per-component engine, reseeded before each component generates.
    mutable core::rng _random{0};

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
    component() = default;                                ///< Default constructor.
    component(const component&) = default;                ///< Copyable.
    component& operator=(const component&) = default;     ///< Copyable.
    component(component&&) noexcept = default;            ///< Movable.
    component& operator=(component&&) noexcept = default; ///< Movable.

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

    /// @brief Convert a generated value to JSON, for entity::to_json().
    ///
    /// The default writes `bool` as a boolean, integers as integers,
    /// `double` and `float` as numbers (non-finite ones as null),
    /// `std::wstring` and `std::string` as strings, and anything else as the
    /// string from to_string(). Override it to write a richer value.
    /// @param value The value previously returned by generate().
    /// @return The JSON value.
    [[nodiscard]] virtual json_value to_json(const std::any& value) const
    {
        return default_to_json(value, to_string(value));
    }

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

    /// @brief Default JSON conversion used by to_json().
    /// @param value The value to convert.
    /// @param display Its to_string() form, used for unrecognized types.
    /// @return The JSON value.
    [[nodiscard]] static json_value default_to_json(const std::any& value,
                                                    const std::wstring& display)
    {
        const auto& type = value.type();
        if (type == typeid(bool)) return std::any_cast<bool>(value);
        if (type == typeid(int)) return std::int64_t{std::any_cast<int>(value)};
        if (type == typeid(long)) return std::int64_t{std::any_cast<long>(value)};
        if (type == typeid(long long)) return std::int64_t{std::any_cast<long long>(value)};
        if (type == typeid(unsigned)) return std::int64_t{std::any_cast<unsigned>(value)};
        if (type == typeid(double) || type == typeid(float))
        {
            const double d = type == typeid(double) ? std::any_cast<double>(value)
                                                    : double{std::any_cast<float>(value)};
            if (!std::isfinite(d)) return nullptr;
            return d;
        }
        if (type == typeid(std::wstring)) return core::to_utf8(std::any_cast<std::wstring>(value));
        if (type == typeid(std::string)) return std::any_cast<std::string>(value);
        return core::to_utf8(display);
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
        return core::pick(ctx.random(), _choices);
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

/// @brief Component that generates a uniform random value: integers in
///        [lo, hi], floating-point values in [lo, hi).
/// @tparam T An integer type (not `bool`) or a floating-point type.
/// @tparam Formatter Optional callable for custom string conversion.
template <typename T, typename Formatter = use_default_formatter>
    requires(std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>
class range_component : public component
{
  public:
    /// @brief Construct a range component.
    /// @param key Unique component key.
    /// @param lo Lower bound (inclusive).
    /// @param hi Upper bound (inclusive).
    /// @param fmt Optional custom formatter.
    explicit range_component(std::wstring key, T lo, T hi, // NOLINT(bugprone-easily-swappable-parameters)
                             Formatter fmt = {})
        : _key{std::move(key)}, _lo{lo}, _hi{hi}, _fmt{std::move(fmt)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        if constexpr (std::floating_point<T>)
        {
            auto v = static_cast<T>(core::uniform_real(ctx.random(), static_cast<double>(_lo),
                                                       static_cast<double>(_hi)));
            // Narrowing to float can round up to hi; keep the range half-open.
            if (v >= _hi && _hi > _lo) v = std::nextafter(_hi, _lo);
            return v;
        }
        else
        {
            return core::uniform_int(ctx.random(), _lo, _hi);
        }
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
/// means the option is never selected. The choice is dasmig-core's weighted
/// choice (the same algorithm as core::weighted_table), built once.
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
    /// @throws std::invalid_argument If @p options is empty, a weight is
    ///         negative or not finite, or all weights are zero.
    explicit weighted_choice_component(std::wstring key,
                                       std::vector<option> options,
                                       Formatter fmt = {})
        : _key{std::move(key)}, _options{std::move(options)},
          _fmt{std::move(fmt)}, _dist{weights_of(_options)}
    {
    }

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const generation_context& ctx) const override
    {
        return _options[_dist(ctx.random())].value;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        if constexpr (std::is_same_v<Formatter, use_default_formatter>)
            return default_to_string(value);
        else
            return _fmt(std::any_cast<T>(value));
    }

  private:
    // Checked here so the messages name the problem before core::discrete
    // sees the weights.
    static std::vector<double> weights_of(const std::vector<option>& options)
    {
        if (options.empty())
            throw std::invalid_argument("options must not be empty");
        if (!std::ranges::all_of(options, [](const auto& o) {
                return std::isfinite(o.weight) && o.weight >= 0.0;
            }))
            throw std::invalid_argument("option weights must be finite and non-negative");
        if (!std::ranges::any_of(options, [](const auto& o) { return o.weight > 0.0; }))
            throw std::invalid_argument(
                "at least one option must have a positive weight");
        std::vector<double> weights;
        weights.reserve(options.size());
        std::ranges::transform(options, std::back_inserter(weights), &option::weight);
        return weights;
    }

    std::wstring _key;
    std::vector<option> _options;
    Formatter _fmt;
    core::discrete _dist;
};

/// @}

/// @brief A generated entity holding component values in registration order.
///
/// Entities are produced by eg::generate and provide typed access to
/// component values, seed signatures for replay, and serialization.
/// @see eg::generate
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
    /// eg::generate(request, seed()) with the same request and setup reproduces
    /// the entity:
    /// it is the call seed, or the seed of the attempt the entity validator
    /// accepted. Component @c k drew from
    /// `core::rng{seed()}.split(core::to_utf8(k))`.
    /// @return The entity-level seed.
    [[nodiscard]] std::uint64_t seed() const
    {
        return _seed;
    }

    /// @brief The seed this entity was generated from, in the format every
    ///        dasmig result uses (the entity generator has no data, so
    ///        `data` is 0).
    /// @return The replay signature.
    [[nodiscard]] core::seed_signature signature() const
    {
        return {.seed = _seed, .data = 0};
    }

    /// @brief Write this entity as a JSON object
    ///        `{"components":{"key":value,…},"signature":{…}}`, components
    ///        in generation order, values from component::to_json().
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object().key("components").begin_object();
        for (const auto& e : _entries)
        {
            w.key(core::to_utf8(e.key));
            std::visit([&w](const auto& v) { w.value(v); }, e.json);
        }
        w.end_object().key("signature");
        signature().to_json(w);
        w.end_object();
    }

    /// @brief This entity as a JSON document.
    /// @return Compact JSON.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
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
        // A plain loop: std::ranges::fold_left_first is missing from libc++
        // 18, Apple's libc++ and Emscripten.
        std::wstring result;
        for (const auto& e : _entries)
        {
            if (!result.empty()) result += L"  ";
            result += e.key + L": " + e.display;
        }
        return result;
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
        json_value json;
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
    generation_observer() = default;                                          ///< Default constructor.
    generation_observer(const generation_observer&) = default;                ///< Copyable.
    generation_observer& operator=(const generation_observer&) = default;     ///< Copyable.
    generation_observer(generation_observer&&) noexcept = default;            ///< Movable.
    generation_observer& operator=(generation_observer&&) noexcept = default; ///< Movable.

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

/// @brief Per-call entity generation and selection options.
///
/// Defaults select all registered components, generate one entity in a batch,
/// and accept entities without an entity-level validator. Component validation,
/// weights, conditions, registration order and the generator's retry policy
/// still apply. Replay requires the same request and instance setup.
struct entity_request
{
    /// @brief Optional key subset; absent selects all, an empty vector selects none.
    /// Unknown keys are skipped and registration order is preserved.
    std::optional<std::vector<std::wstring>> component_keys{};

    /// @brief Optional registered group name, mutually exclusive with component_keys.
    /// Required by eg::generate_group; also supported by the other generation methods.
    std::optional<std::wstring> group{};

    /// @brief Number of entities for batch methods (ignored by single-entity methods).
    std::size_t count{1};

    /// @brief Optional entity validator; false retries the whole entity.
    /// Concurrent batches require a thread-safe callable and captured state.
    std::function<bool(const entity&)> validator{};
};

/// @brief The entity generator — produces entities with configurable components.
///
/// Components are registered by implementing the component interface and
/// adding them to the generator. Components are generated in registration
/// order, allowing later components to access earlier ones via context.
/// @see component, entity, entity_request, generation_context
class eg
{
  public:
    /// @brief Default constructor creates an empty generator.
    eg() = default;

    eg(const eg&) = delete;            ///< Not copyable.
    eg& operator=(const eg&) = delete; ///< Not copyable.
    eg(eg&&) noexcept = default;            ///< Move constructor.
    eg& operator=(eg&&) noexcept = default; ///< Move assignment.
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
            _components.push_back({.key = key, .comp = std::move(comp)});
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
    /// @name Validation
    /// @{

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

    /// @brief Generate an entity using per-call options.
    ///
    /// Draws a call seed from the generator's engine.
    /// @param request Selection and entity validation options.
    /// @return The generated entity.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    [[nodiscard]] entity generate(const entity_request& request)
    {
        const auto refs = resolve_components(request);
        return generate_seeded(refs, _engine(), request.validator);
    }

    /// @brief Generate an entity using a request and a seed.
    ///
    /// The same seed and components give the same entity on every compiler
    /// and platform. Each component draws from a stream split from the seed
    /// by its key, so adding or removing a component does not change the
    /// others.
    /// @param request Selection and entity validation options.
    /// @param call_seed Seed for reproducible results; does not advance the internal engine.
    /// @return The generated entity.
    /// @throws std::runtime_error If validation is exhausted.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    [[nodiscard]] entity generate(const entity_request& request,
                                  std::uint64_t call_seed) const
    {
        return generate_seeded(resolve_components(request), call_seed,
                               request.validator);
    }

    /// @}
    /// @name Batch Generation
    /// @{

    /// @brief Generate request.count entities using per-call options.
    ///
    /// Draws one call seed from the generator's engine for the batch.
    /// @param request Selection, batch count and entity validation options.
    /// @return A vector of generated entities.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] std::vector<entity> generate_batch(const entity_request& request)
    {
        const auto refs = resolve_components(request);
        return generate_batch_seeded(refs, request, _engine());
    }

    /// @brief Generate multiple entities using a seed.
    ///
    /// Entity @c i uses the @c i-th value of `core::rng{call_seed}.split("batch")`
    /// as its call seed, so the result equals generate_batch_async(request,
    /// call_seed) and each entity replays with generate(request, e.seed()).
    /// @param request Selection, batch count and entity validation options.
    /// @param call_seed Seed for reproducible results.
    /// @return A vector of generated entities.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] std::vector<entity> generate_batch(
        const entity_request& request, std::uint64_t call_seed) const
    {
        return generate_batch_seeded(resolve_components(request), request, call_seed);
    }

    /// @}
    /// @name Concurrent Batch Generation
    /// @{

    /// @brief Generate multiple entities concurrently.
    ///
    /// Same entities as generate_batch(request) would give from the same engine
    /// state; each entity is generated in its own async task. Results are
    /// returned in order. Components, observers and the request's validator
    /// (including captured state) must be thread-safe. In a WebAssembly build without threads
    /// (Emscripten without `-pthread`) the tasks run one after another, with
    /// the same results.
    /// @param request Selection, batch count and entity validation options.
    /// @return A vector of generated entities.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] std::vector<entity> generate_batch_async(const entity_request& request)
    {
        const auto refs = resolve_components(request);
        return generate_batch_async_seeded(refs, request, _engine());
    }

    /// @brief Generate multiple entities concurrently using a seed.
    ///
    /// Gives the same entities as generate_batch(request, call_seed).
    /// @param request Selection, batch count and entity validation options.
    /// @param call_seed Seed for reproducible results.
    /// @return A vector of generated entities.
    /// @throws std::invalid_argument If both selection fields are set.
    /// @throws std::out_of_range If the requested group does not exist.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] std::vector<entity> generate_batch_async(
        const entity_request& request, std::uint64_t call_seed) const
    {
        return generate_batch_async_seeded(resolve_components(request), request, call_seed);
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
    /// @param request Options with a registered group name.
    /// @return The generated entity.
    /// @throws std::out_of_range If the group does not exist.
    /// @throws std::invalid_argument If the group is absent or component_keys is set.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] entity generate_group(const entity_request& request)
    {
        require_group(request);
        return generate(request);
    }

    /// @brief Generate an entity using a named group with a seed.
    /// @param request Options with a registered group name.
    /// @param call_seed Seed for reproducible results.
    /// @return The generated entity.
    /// @throws std::out_of_range If the group does not exist.
    /// @throws std::invalid_argument If the group is absent or component_keys is set.
    /// @throws std::runtime_error If component or entity validation is exhausted.
    [[nodiscard]] entity generate_group(const entity_request& request,
                                        std::uint64_t call_seed) const
    {
        require_group(request);
        return generate(request, call_seed);
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

    // Notify all observers in a list by calling a member function. The
    // arguments go to every observer, so they are passed as lvalues (never
    // forwarded: the first observer could move from them).
    template <typename Hook, typename... Args>
    static void notify_all(const observer_list& observers,
                           Hook hook, const Args&... args)
    {
        for (const auto& obs : observers)
        {
            ((*obs).*hook)(args...);
        }
    }

    // Convenience: notify using the instance's observer list.
    template <typename Hook, typename... Args>
    void notify(Hook hook, const Args&... args) const
    {
        notify_all(_observers, hook, args...);
    }

    // Emscripten without pthreads cannot start threads; deferred tasks give
    // the same entities, generated on the calling thread.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    static constexpr std::launch async_policy = std::launch::deferred;
#else
    static constexpr std::launch async_policy = std::launch::async;
#endif

    // The call seeds of a batch.
    [[nodiscard]] static std::vector<std::uint64_t> batch_seeds(
        std::size_t count, std::uint64_t call_seed) // NOLINT(bugprone-easily-swappable-parameters)
    {
        auto r = core::rng{call_seed}.split("batch");
        std::vector<std::uint64_t> seeds(count);
        std::ranges::generate(seeds, [&r] { return r(); });
        return seeds;
    }

    // One entity from a call seed: hooks, entity validation and retries.
    // Attempt 0 uses the call seed itself; attempt k uses the k-th value of
    // core::rng{call_seed}.split("attempt").
    [[nodiscard]] entity generate_seeded(std::span<const component_ref> refs,
                                         std::uint64_t call_seed,
                                         const std::function<bool(const entity&)>& validator) const
    {
        notify_all(_observers, &generation_observer::on_before_generate);
        auto attempts = core::rng{call_seed}.split("attempt");
        std::optional<entity> accepted;
        for (std::size_t attempt = 0; attempt <= _max_retries; ++attempt)
        {
            if (attempt > 0)
                notify_all(_observers,
                    &generation_observer::on_before_entity_retry, attempt);
            const std::uint64_t entity_seed = attempt == 0 ? call_seed : attempts();
            auto e = generate_impl(refs, entity_seed, _max_retries, _observers);
            if (attempt > 0)
                notify_all(_observers,
                    &generation_observer::on_after_entity_retry, attempt);
            if (!validator || validator(e))
            {
                accepted = std::move(e);
                break;
            }
        }

        if (!accepted)
        {
            notify_all(_observers, &generation_observer::on_entity_fail);
            throw std::runtime_error(
                "entity validation failed after max retries");
        }
        notify_all(_observers, &generation_observer::on_after_generate, *accepted);
        return std::move(*accepted);
    }

    // One generation attempt. Component k draws from
    // core::rng{entity_seed}.split(to_utf8(k)): its weight roll from
    // .split("weight"), its seeds (first try, then retries) from
    // .split("seed"); the component's own engine is core::rng{its seed}.
    [[nodiscard]] static entity generate_impl(
        std::span<const component_ref> components, std::uint64_t entity_seed, // NOLINT(bugprone-easily-swappable-parameters)
        std::size_t max_retries, const observer_list& observers)
    {
        entity generated_entity;
        generation_context ctx;
        generated_entity._seed = entity_seed;
        const core::rng root{entity_seed};

        for (const auto& ref : components)
        {
            const auto stream = root.split(core::to_utf8(ref.key));

            // Roll against the effective weight.
            if (ref.effective_weight < 1.0)
            {
                auto roll = stream.split("weight");
                if (core::uniform01(roll) >= ref.effective_weight)
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

            auto seeds = stream.split("seed");
            auto component_seed = seeds();
            ctx._random = core::rng{component_seed};
            auto value = ref.comp.get().generate(ctx);

            // Component-level validation with retries.
            for (std::size_t retry = 0;
                 retry < max_retries && !ref.comp.get().validate(value);
                 ++retry)
            {
                notify_all(observers,
                    &generation_observer::on_before_retry, ref.key, retry + 1);
                component_seed = seeds();
                ctx._random = core::rng{component_seed};
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
            auto json = ref.comp.get().to_json(value);

            notify_all(observers,
                &generation_observer::on_after_component, ref.key, value);

            ctx._values[ref.key] = value;
            generated_entity._entries.push_back(
                {.key = ref.key, .value = std::move(value),
                 .display = std::move(display),
                 .seed = component_seed, .json = std::move(json)});
        }

        return generated_entity;
    }

    // --- Component resolution ---------------------------------------------

    static void require_group(const entity_request& request)
    {
        if (!request.group)
            throw std::invalid_argument("generate_group requires a group");
    }

    [[nodiscard]] std::vector<component_ref> resolve_components(
        const entity_request& request) const
    {
        if (request.group && request.component_keys)
            throw std::invalid_argument("group and component_keys are mutually exclusive");
        if (request.group)
        {
            const auto it = _groups.find(*request.group);
            if (it == _groups.end())
                throw std::out_of_range("group not found");
            return filter_components(it->second);
        }
        return request.component_keys ? filter_components(*request.component_keys)
                                      : all_component_refs();
    }

    [[nodiscard]] std::vector<entity> generate_batch_seeded(
        std::span<const component_ref> refs, const entity_request& request,
        std::uint64_t call_seed) const
    {
        std::vector<entity> entities;
        entities.reserve(request.count);
        for (const auto entity_seed : batch_seeds(request.count, call_seed))
            entities.push_back(generate_seeded(refs, entity_seed, request.validator));
        return entities;
    }

    [[nodiscard]] std::vector<entity> generate_batch_async_seeded(
        std::span<const component_ref> refs, const entity_request& request,
        std::uint64_t call_seed) const
    {
        std::vector<std::future<entity>> futures;
        futures.reserve(request.count);
        for (const auto entity_seed : batch_seeds(request.count, call_seed))
        {
            futures.push_back(std::async(async_policy, [this, refs, &request, entity_seed] {
                return generate_seeded(refs, entity_seed, request.validator);
            }));
        }

        std::vector<entity> entities;
        entities.reserve(request.count);
        std::ranges::transform(futures, std::back_inserter(entities),
            [](auto& f) { return f.get(); });
        return entities;
    }

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

    // Generation lifecycle observers (optional).
    observer_list _observers;

    // Maximum retries for component and entity validation.
    std::size_t _max_retries{10};

    // Internal random engine for the generator.
    core::rng _engine{core::rng::from_entropy()};
};
} // namespace dasmig

#endif // DASMIG_ENTITYGEN_HPP
