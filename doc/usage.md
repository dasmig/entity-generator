# Usage Guide

This guide covers every feature of the entity-generator library in detail. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#defining-components">Defining Components</a></li>
<li><a href="#registering-and-generating">Registering and Generating</a></li>
<li><a href="#typed-retrieval">Typed Retrieval</a></li>
<li><a href="#component-dependencies">Component Dependencies</a></li>
<li><a href="#per-call-inputs">Per-Call Inputs</a></li>
<li><a href="#composing-with-other-generators">Composing with Other Generators</a></li>
<li><a href="#custom-types">Custom Types</a></li>
<li><a href="#seed-signatures">Seed Signatures</a></li>
<li><a href="#batch-generation">Batch Generation</a></li>
<li><a href="#component-groups">Component Groups</a></li>
<li><a href="#thread-safety">Thread Safety</a></li>
<li><a href="#component-weights">Component Weights</a></li>
<li><a href="#entity-serialization">Entity Serialization</a></li>
<li><a href="#structured-serialization">Structured Serialization</a></li>
<li><a href="#concurrent-batch-generation">Concurrent Batch Generation</a></li>
<li><a href="#validation-hooks">Validation Hooks</a></li>
<li><a href="#event-hooks">Event Hooks</a></li>
<li><a href="#conditional-components">Conditional Components</a></li>
<li><a href="#generic-components">Generic Components</a></li>
<li><a href="#entity-introspection">Entity Introspection</a></li>
<li><a href="#generator-introspection">Generator Introspection</a></li>
<li><a href="#extensions">Extensions</a></li>
<li><a href="#ecs-integration-entt">ECS Integration: EnTT</a></li>
<li><a href="#ecs-integration-flecs">ECS Integration: Flecs</a></li>
<li><a href="#random-numbers-in-components">Random Numbers in Components</a></li>
<li><a href="#migrating-to-the-request-first-api">Migrating to the Request-First API</a></li>
<li><a href="#migrating-from-v3">Migrating from v3</a></li>
<li><a href="#migrating-from-v1">Migrating from v1</a></li>
<li><a href="#error-reference">Error Reference</a></li>
</ul>

## Defining Components

```cpp
#include <dasmig/entitygen.hpp>

// A component that generates a random character class.
class character_class : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"class"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> classes{
            L"Warrior", L"Mage", L"Rogue", L"Healer"};
        return dasmig::core::pick(ctx.random(), classes);
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// A component that generates a random age.
class age : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"age"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return dasmig::core::uniform_int(ctx.random(), 18, 65);
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

`default_to_string()` handles `std::wstring`, `std::string` (read as UTF-8), `bool`, `float`, `double`, and the integer types `int`, `long`, `long long` and their unsigned forms. All other types return `[?]` — override `to_string()` for custom types. The default JSON conversion handles the same types; an unsigned value above the largest `std::int64_t` is written as a string so it stays exact.

## Registering and Generating

```cpp
using eg = dasmig::eg;

// Register components with fluent chaining.
eg::instance()
    .add(std::make_unique<character_class>())
    .add(std::make_unique<age>());

// Generate an entity with all registered components.
auto entity = eg::instance().generate({});

// Generate an entity with only specific components.
auto partial = eg::instance().generate({.component_keys = std::vector<std::wstring>{L"class"}});

// Output to stream.
std::wcout << eg::instance().generate({}) << std::endl;

// Generate with a seed for reproducible results.
auto seeded = eg::instance().generate({}, 42);

// Keep deterministic sequence state in the caller, not the generator.
dasmig::core::rng seeds{123};
auto first  = eg::instance().generate({}, seeds());
auto second = eg::instance().generate({}, seeds());
```

Every generation method accepts a `const dasmig::entity_request&` first, with an optional explicit `std::uint64_t` seed second. Pass `{}` for default generation. Seeded overloads are `const` and do not advance the instance's internal entropy-seeded engine. Reuse or copy a request to apply the same choices to single, group, synchronous-batch, and concurrent-batch generation.

| Request field | Default | Behavior |
|---|---|---|
| `component_keys` | `std::nullopt` | All registered components; an explicitly supplied empty `std::vector<std::wstring>{}` selects none. Unknown keys are skipped, duplicates do not duplicate components, and registration order is preserved. |
| `group` | `std::nullopt` | Select a registered group instead of `component_keys`; supported by all generation methods and required by `generate_group`. Setting both selectors throws `std::invalid_argument`; an unknown group throws `std::out_of_range`. |
| `count` | `1` | Number of entities for batch methods; `0` returns an empty vector. Single-entity methods ignore it. |
| `validator` | Empty callable | Accept all entities after component validation. Supply a callback to validate each entity and retry on rejection. |

The request owns its keys and group name. Registration, named-group definitions, component weights, observers, and retry policy remain instance-level setup. The entity generator loads no resources of its own; any resource loading inside user components remains their setup, not a request field. A request never changes the instance's selection or validator for subsequent calls.

**Behavior notes:**
- `add()` with a key that already exists **replaces** the component in place, preserving registration order. Any existing weight override for that key persists.
- `remove()` on a non-existent key is a **no-op** (does not throw).
- `remove_group()` on a non-existent group is a **no-op**.

## Typed Retrieval

```cpp
auto entity = eg::instance().generate({});

// Access values with their original types.
std::wstring char_class = entity.get<std::wstring>(L"class");
int char_age = entity.get<int>(L"age");

// Type-erased access (returns const std::any&).
const auto& raw = entity.get_any(L"class");

// Check if a component exists.
if (entity.has(L"name")) { /* ... */ }

// Iterate over all keys in generation order.
for (const auto& key : entity.keys()) { /* ... */ }
```

## Component Dependencies

Components receive a `generation_context` that provides access to values generated by earlier components. Registration order determines generation order, unless a component declares what it reads with `depends_on()` (below).

```cpp
// A greeting component that depends on a previously generated name.
class greeting : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"greeting"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        // Access a value generated by an earlier component.
        auto name = ctx.get<std::wstring>(L"name");
        return L"Hello, " + name + L"!";
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Register name before greeting so the dependency is available.
eg::instance()
    .add(std::make_unique<character_name>())
    .add(std::make_unique<greeting>());
```

The context also exposes `ctx.has(key)` to check whether a dependency was generated (useful when generating with a subset of components). `ctx.try_get<T>(key)` returns an empty optional and `ctx.get_or<T>(key, fallback)` the fallback when it was not; `ctx.get<T>(key)` throws `std::out_of_range` with a message that names the key.

### Declaring dependencies

Override `depends_on()` (or call `.depends_on({...})` on a field) to say which components a component reads:

```cpp
gen.add(field(L"job", [](const dasmig::generation_context& ctx) {
            return pick_job(ctx.get<int>(L"age"));
        })
        .depends_on({L"age"}));
gen.add(field(L"age", [](const dasmig::generation_context& ctx) {
    return dasmig::core::uniform_int(ctx.random(), 18, 90);
}));
```

- A request that selects `job` (by `component_keys` or by group) also generates `age`, transitively, and the entity contains both.
- Dependencies are ordered before their dependents, whatever the registration order. Components with no dependency relation keep registration order.
- If a dependency is skipped (a weight roll or `should_generate()`), the dependent is skipped too, and observers get `on_skip` for it.
- An unregistered dependency throws `std::invalid_argument` and a cycle `std::logic_error` when generating; both messages name the keys.
- Reordering does not change any value: each component draws from a stream split from the entity seed by its key.

## Per-Call Inputs

A request can carry values the components need for this call only (a country, a reference date, a forced sex), so no global is needed and `generate(request, seed)` stays a function of its arguments:

```cpp
gen.add(field(L"country", [](const dasmig::generation_context& ctx) {
    return ctx.input_or<std::string>("country", "BR");
}));
gen.add(field(L"birth_year", [](const dasmig::generation_context& ctx) {
    return ctx.input<std::int64_t>("reference_year") - 30;
}));

const dasmig::entity_request request{
    .inputs = {{"country", std::string{"PT"}}, {"reference_year", std::int64_t{2026}}}};
auto e = gen.generate(request, 42);
```

- An input is a `bool`, `std::int64_t`, `double` or `std::string`, read with the type it was given with. `ctx.input<T>(name)` throws `std::invalid_argument` naming the input when it is missing or has another type; `ctx.input_or<T>(name, fallback)` and `ctx.has_input(name)` tolerate a missing one.
- Give strings as `std::string{"PT"}`. A bare literal also works with C++23 compilers, but it is easy to get a `bool`.
- Inputs are part of `entity.signature().data`, so two entities from the same seed but different inputs have different signatures. They never change the draws.

## Composing with Other Generators

Components can wrap [name-generator](https://github.com/dasmig/name-generator) and [nickname-generator](https://github.com/dasmig/nickname-generator) to bring in name and nickname generation:

```cpp
#include <dasmig/entitygen.hpp>
#include <dasmig/namegen.hpp>
#include <dasmig/nicknamegen.hpp>

class full_name : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"name"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return static_cast<std::wstring>(
            dasmig::ng::instance().get_name().append_surname());
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

class player_nickname : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"nickname"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return static_cast<std::wstring>(
            dasmig::nng::instance().get_nickname());
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

## Custom Types

Components can produce any type. Override `to_string()` on your component to enable stream output, and `to_json()` to write something richer than that string in entity::to_json() (see Entity Serialization):

```cpp
struct vec2 { float x, y; };

class position : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"position"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return vec2{
            static_cast<float>(dasmig::core::uniform_real(ctx.random(), -100.0, 100.0)),
            static_cast<float>(dasmig::core::uniform_real(ctx.random(), -100.0, 100.0))};
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        auto pos = std::any_cast<vec2>(value);
        return L"(" + std::to_wstring(pos.x) + L", " + std::to_wstring(pos.y) + L")";
    }
};
```

## Seed Signatures

Every generated entity and component stores the random seed that produced it. This enables replay, debugging, and logging. Results are identical on every compiler, standard library and CPU (dasmig-core), so a seed logged on one machine replays on another.

```cpp
auto entity = eg::instance().generate({});

// The entity-level seed: generate(request, seed) with the same selection
// reproduces the entity.
std::uint64_t entity_seed = entity.seed();
auto again = eg::instance().generate({}, entity_seed);

// Retrieve the seed used for a specific component.
std::uint64_t age_seed = entity.seed(L"age");
```

Component seeds can be used to replay individual component generation:

```cpp
// Replay a component's random values using its captured seed.
dasmig::core::rng rng{entity.seed(L"age")};
int replayed_age = dasmig::core::uniform_int(rng, 18, 65); // Same value as entity.get<int>(L"age")
```

The seed hierarchy is fully deterministic:

```
call seed (per call, or drawn from the generator's engine)
  └─ entity seed = the call seed  ← entity.seed()
     (attempt k of entity validation: k-th value of rng{call seed}.split("attempt"))
       └─ rng{entity seed}.split(key), one stream per component key
            ├─ .split("weight")   the inclusion roll
            └─ .split("seed")     the component seed (then one per retry)
                                  ← entity.seed(key); ctx.random() = rng{component seed}
```

Because each component's stream is split from the entity seed by its key, adding, removing or reordering components does not change the values of the others.

### What the signature's data covers

`entity.signature().data` is 0 for an entity of plain components. It becomes a hash of the request's inputs and of `component::data_hash()` of every component that ran (a component that calls a generator with data returns that generator's `data_hash()`; `field().data_hash(h)` sets it). A component that was skipped does not count. Two runs with the same seed and request give the same `data` only if the inputs and the data behind the components are the same, which is what the seed-replay rule of every dasmig library asks for.

## Batch Generation

Generate multiple entities in a single call:

```cpp
// Generate 10 entities with all registered components.
auto batch = eg::instance().generate_batch({.count = 10});

for (const auto& e : batch)
{
    std::wcout << e << L'\n';
}

// Seeded batch for deterministic results.
auto seeded_batch = eg::instance().generate_batch({.count = 10}, 42);
```

## Component Groups

Define named groups of component keys for convenient selective generation:

```cpp
// Register components.
eg::instance()
    .add(std::make_unique<character_name>())
    .add(std::make_unique<character_class>())
    .add(std::make_unique<age>())
    .add(std::make_unique<level>())
    .add(std::make_unique<character_stats>());

// Define groups.
eg::instance()
    .add_group(L"identity", {L"name", L"class"})
    .add_group(L"combat", {L"class", L"level", L"stats"});

// Generate using a group.
auto fighter = eg::instance().generate_group({.group = L"combat"});

// Seeded group generation.
auto seeded = eg::instance().generate_group({.group = L"combat"}, 42);

// Check and remove groups.
if (eg::instance().has_group(L"identity")) { /* ... */ }
eg::instance().remove_group(L"identity");
```

## Thread Safety

The `eg` class supports independent instances. Create one per thread for lock-free concurrent generation:

```cpp
// Each thread gets its own generator — no locks needed.
std::vector<std::jthread> threads;
for (int i = 0; i < 4; ++i)
{
    threads.emplace_back([&](int id) {
        dasmig::eg gen;
        gen.add(std::make_unique<name>(/* … */));
        auto e = gen.generate({}, static_cast<std::uint64_t>(id));
    }, i);
}
```

`eg` is move-constructible and move-assignable, so instances can be transferred between scopes. The `instance()` singleton is still available for single-threaded convenience.

## Component Weights

Components can declare an inclusion weight via the `weight()` virtual method (default `1.0`). Values range from `0.0` (never included) to `1.0` (always included). The generator rolls against the weight during generation; components that fail the roll are skipped.

```cpp
class rare_trait : public dasmig::component
{
  public:
    std::wstring key() const override { return L"rare_trait"; }
    double weight() const override { return 0.2; } // 20% chance

    std::any generate(const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> traits{L"scar", L"tattoo", L"birthmark"};
        return dasmig::core::pick(ctx.random(), traits);
    }

    std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

Weights can also be overridden at registration time or updated later, taking precedence over the component's own `weight()` method:

```cpp
// Override weight at registration.
eg::instance().add(std::make_unique<rare_trait>(), 0.5);

// Update weight after registration.
eg::instance().weight(L"rare_trait", 0.8);
```

**Note:** When a weighted component is skipped, dependent components that call `ctx.get<T>()` for it will throw. Use `ctx.has()` to guard dependency access in weight-sensitive components.

## Entity Serialization

Convert an entity to a formatted string with `to_string()`. Each component is rendered as `"label: display"` separated by two spaces; the label is `component::label()`, which defaults to the key. The output matches `operator<<`.

```cpp
auto entity = eg::instance().generate({});

// Get the formatted string.
std::wstring text = entity.to_string();
// e.g. "name: Alice  class: Mage  age: 34"

// Equivalent to streaming:
std::wcout << entity << L'\n';
```

### JSON

`to_json()` writes the entity as JSON (UTF-8), the form the WebAssembly layer and the game consume. Values keep their types: integers and numbers stay numbers, `bool` a boolean, strings strings; other types use the component's `to_json()` override, or its `to_string()`.

```cpp
auto e = eg::instance().generate({}, 42);
std::string json = e.to_json();
// {"components":{"name":"Helena","class":"Rogue","age":53,"level":94,...},
//  "signature":{"seed":"000000000000002a","data":"0000000000000000"}}
```

Components appear in generation order. The signature is the replay seed in the format every dasmig library uses; `data` is 0 unless the request has inputs or a component reports a `data_hash()` (see Seed Signatures). Seeds are strings because JavaScript numbers cannot hold every 64-bit value. `to_json(writer)` writes into an existing `dasmig::core::json_writer`, for composing larger documents.

#### Nested documents

A key with dots is a path. `identity.name` and `identity.age` are written as members of one `identity` object, placed where its first member comes, and `entity.get<T>(L"identity.age")` still uses the full key:

```cpp
gen.add(field(L"identity.name", name_fn));
gen.add(field(L"identity.age", age_fn));
gen.add(field(L"place", place_fn));
// {"components":{"identity":{"name":"Helena","age":53},"place":"Lisbon"},"signature":{...}}
```

`add()` throws `std::invalid_argument` for a key with an empty segment (`a..b`, `.a`, `a.`), or one that would be both a value and the parent of another (`a` with `a.b`).

#### Lists and objects

`dasmig::json_value` holds null, a boolean, an integer, a number, a string, an array or an object. A component returns one from `to_json()` (or `field().json(fn)`) to write a richer value than its type's default:

```cpp
.json([](const std::vector<std::string>& languages) {
    dasmig::json_value::array list;
    for (const auto& l : languages) list.emplace_back(l);
    return list;                       // "languages":["pt","en"]
})
```

Objects keep their members in the order given: `dasmig::json_value::object{{"city", "Lisbon"}, {"lat", 38.7}}`. A non-finite number is written as null.

## Structured Serialization

`to_map()` returns component values as a `std::map<std::wstring, std::wstring>` mapping keys to their display strings. This is convenient for integration with JSON, XML, or any key–value serialization format.

```cpp
auto entity = eg::instance().generate({});
auto m = entity.to_map();
// m["name"] = "Alice", m["class"] = "Mage", m["age"] = "34"

// Iterate all entries.
for (const auto& [key, value] : m)
{
    std::wcout << key << L" = " << value << L'\n';
}
```

Custom types work seamlessly — the map uses each component's `to_string()` output:

```cpp
auto m = entity.to_map();
// m["pos"] = "(3,7)" — uses custom_type_component::to_string()
```

## Concurrent Batch Generation

`generate_batch_async(request)` generates entities in parallel using `std::async`. Entity `i` uses the `i`-th value of `rng{seed}.split("batch")` as its call seed, exactly as `generate_batch(request)` does, so the two give the same entities for the same request and seed. Each entity is generated in its own async task; results are returned in order.

```cpp
// Generate 100 entities concurrently.
auto batch = eg::instance().generate_batch_async({.count = 100});

// Seeded: deterministic regardless of thread scheduling.
auto batch1 = eg::instance().generate_batch_async({.count = 50}, 42);
auto batch2 = eg::instance().generate_batch_async({.count = 50}, 42);
// batch1[i] == batch2[i] for all i
```

Entity-level validation applies to each concurrent task independently. Observers, components, the validator callable and any captured state must be safe for concurrent invocation. Do not mutate instance setup or the request while generation is running. On WebAssembly without threads, tasks run sequentially with identical results.

## Validation Hooks

Validation operates at two levels, both opt-in with zero-cost defaults.

**Component-level:** Override `validate()` on your component. If it returns `false`, the generator re-rolls with a new seed up to `max_retries` times, then throws `std::runtime_error`.

```cpp
class even_age : public dasmig::component
{
  public:
    std::wstring key() const override { return L"age"; }
    std::any generate(const dasmig::generation_context& ctx) const override
    {
        return dasmig::core::uniform_int(ctx.random(), 18, 65);
    }
    bool validate(const std::any& value) const override
    {
        return std::any_cast<int>(value) % 2 == 0;
    }
    std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

**Entity-level:** Supply a callback in the request. If it returns `false`, the entire entity is re-generated up to `max_retries` additional times. It applies only to calls using that request.

```cpp
const dasmig::entity_request adult{
    .validator = [](const dasmig::entity& e) {
        return e.get<int>(L"age") > 30;
    }};

auto e = eg::instance().generate(adult); // age is guaranteed > 30
auto any_age = eg::instance().generate({}); // no entity-level validator
```

Configure the retry limit (default 10) for both levels:

```cpp
eg::instance().max_retries(20);
```

## Event Hooks

Attach one or more `generation_observer` instances to any generator to receive lifecycle callbacks. Override only the hooks you need; all default to no-ops.

```cpp
class log_observer : public dasmig::generation_observer
{
  public:
    void on_before_generate() override
    {
        std::wcout << L"generating entity...\n";
    }
    void on_after_component(const std::wstring& key,
                            const std::any& /*value*/) override
    {
        std::wcout << L"produced " << key << L'\n';
    }
};

auto obs = std::make_shared<log_observer>();
eg::instance().add_observer(obs);
eg::instance().generate({}); // prints hook messages
eg::instance().remove_observer(obs);

// Or remove all observers at once.
eg::instance().clear_observers();
```

Multiple observers fire in registration order. The same observer can be added more than once.

**Thread safety:** Observers are **not** thread-safe by default. When using `generate_batch_async`, the caller is responsible for ensuring observer implementations are safe for concurrent invocation (e.g., using atomics or mutexes).

The full set of hooks (6 before/after pairs + 3 single-fire hooks, 15 methods):

| Event | Hook(s) |
|---|---|
| Entity generation | `on_before_generate()` / `on_after_generate(entity)` |
| Component generation | `on_before_component(key)` / `on_after_component(key, value)` |
| Component skip | `on_skip(key)` |
| Component retry | `on_before_retry(key, attempt)` / `on_after_retry(key, attempt, value)` |
| Component fail | `on_component_fail(key)` |
| Entity retry | `on_before_entity_retry(attempt)` / `on_after_entity_retry(attempt)` |
| Entity fail | `on_entity_fail()` |
| Registration | `on_before_add(key)` / `on_after_add(key)` |
| Removal | `on_before_remove(key)` / `on_after_remove(key)` |

For per-component filtering, check the `key` parameter inside your hook override.

## Conditional Components

Components can override `should_generate(ctx)` to conditionally skip generation based on the current context. Unlike weights (probabilistic), this is logic-driven. The default returns `true`.

```cpp
class warrior_title : public dasmig::component
{
  public:
    std::wstring key() const override { return L"title"; }
    std::any generate(const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> titles{
            L"Warlord", L"Champion", L"Berserker"};
        return dasmig::core::pick(ctx.random(), titles);
    }
    bool should_generate(const dasmig::generation_context& ctx) const override
    {
        return ctx.has(L"class")
            && ctx.get<std::wstring>(L"class") == L"Warrior";
    }
    std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

When `should_generate` returns `false`, the component is skipped entirely — no value is produced and `entity.has(key)` returns `false`. The skip fires the same observer hooks as a weight skip (`on_before_skip` / `on_after_skip`).

**Note:** `should_generate` is checked after the weight roll. If a component is excluded by weight, `should_generate` is never called.

## Generic Components

Five reusable class templates for the most common component patterns. All live in the `dasmig::` namespace and accept an optional custom formatter as a trailing template parameter.

### constant_component&lt;T&gt;

Always returns the same value.

```cpp
gen.add(std::make_unique<dasmig::constant_component<int>>(L"version", 1));
gen.add(std::make_unique<dasmig::constant_component<std::wstring>>(
    L"label", L"npc"));
```

### choice_component&lt;T&gt;

Picks uniformly at random from a list.

```cpp
gen.add(std::make_unique<dasmig::choice_component<std::wstring>>(
    L"class", std::vector<std::wstring>{L"Warrior", L"Mage", L"Rogue"}));
```

### range_component&lt;T&gt;

Uniform random in a numeric range: `[lo, hi]` for integer types, `[lo, hi)` for floating-point types (`bool` is not allowed).

```cpp
gen.add(std::make_unique<dasmig::range_component<int>>(L"age", 18, 65));
gen.add(std::make_unique<dasmig::range_component<double>>(L"luck", 0.0, 1.0));
```

### callback_component

Wraps a callable that takes `generation_context&` and returns a value. Avoids subclassing for one-off or context-dependent components. Use `std::function` to erase the callable type for `make_unique`.

```cpp
using cb_wstr = dasmig::callback_component<std::wstring,
    std::function<std::wstring(const dasmig::generation_context&)>>;
gen.add(std::make_unique<cb_wstr>(
    L"greeting", [](const dasmig::generation_context& ctx) -> std::wstring {
        return L"Hello, " + ctx.get<std::wstring>(L"name") + L"!";
    }));
```

A CTAD deduction guide is provided for stack-allocated usage where `make_unique` is not needed.

### field(key, fn)

`field` builds a component from a callable and sets the optional hooks with chained calls, so a field is one expression and needs no class. `T` is what the callable returns:

```cpp
gen.add(dasmig::field(L"identity.age", [](const dasmig::generation_context& ctx) {
            return ctx.get<birth>(L"birth").age;
        })
        .label(L"Age")
        .depends_on({L"birth"})
        .format([](const int& years) { return std::to_wstring(years) + L" years"; })
        .json([](const int& years) { return dasmig::json_value{years}; })
        .validate([](const int& years) { return years >= 0; })
        .when([](const dasmig::generation_context& ctx) { return ctx.has(L"birth"); })
        .weight(1.0)
        .data_hash(birth_generator.data_hash()));
```

| Call | Sets | Default |
|---|---|---|
| `.label(text)` | `component::label()` | the key |
| `.depends_on(keys)` | `component::depends_on()` | none |
| `.format(fn)` | `to_string()` | the standard conversion |
| `.json(fn)` | `to_json()` | the standard conversion |
| `.validate(fn)` | `validate()` | accept |
| `.when(fn)` | `should_generate()` | always |
| `.weight(w)` | `weight()` | 1.0 |
| `.data_hash(h)` | `data_hash()` | 0 |

The setters work on a temporary, so chain them in one expression, and `eg::add(field...)` takes the result directly.

### weighted_choice_component&lt;T&gt;

Picks from a list of values using per-option weights. Weights are relative (they don't need to sum to 1.0). A weight of 0 means the option is never selected; negative or non-finite weights are rejected. The weighted choice is dasmig-core's, built once when the component is constructed.

```cpp
using rarity_t = dasmig::weighted_choice_component<std::wstring>;
gen.add(std::make_unique<rarity_t>(
    L"rarity", std::vector<rarity_t::option>{
        {L"Common", 60.0}, {L"Uncommon", 25.0},
        {L"Rare", 10.0}, {L"Legendary", 5.0}}));
```

### Custom formatters

All generic components use `default_to_string` by default. Pass a custom formatter as an extra template parameter and constructor argument for types it doesn't handle:

```cpp
auto fmt = [](int v) { return L"#" + std::to_wstring(v); };
gen.add(std::make_unique<dasmig::constant_component<int, decltype(fmt)>>(
    L"id", 5, fmt));
// to_map()["id"] == "#5"
```

## Entity Introspection

```cpp
auto entity = eg::instance().generate({});

// Number of component values.
std::size_t n = entity.size();

// Check if empty.
if (entity.empty()) { /* ... */ }

// All keys in generation order.
auto keys = entity.keys();

// Type-erased access.
const std::any& val = entity.get_any(L"class");

// Seed of the entity and individual components.
auto entity_seed = entity.seed();
auto comp_seed = entity.seed(L"class");

// Display string map.
auto m = entity.to_map(); // map<wstring, wstring>
```

## Generator Introspection

```cpp
dasmig::eg gen;
gen.add(std::make_unique<age>())
   .add(std::make_unique<character_class>());

// Number of registered components.
std::size_t n = gen.size(); // 2

// All registered keys in registration order.
auto keys = gen.component_keys(); // {"age", "class"}

// Check if a specific component is registered.
bool has_age = gen.has(L"age"); // true

// Remove everything (components, weight overrides, groups).
gen.clear();
```

## Extensions

Optional headers in `dasmig/ext/` provide ready-made functionality built on the observer interface.

### stats_observer

`#include <dasmig/ext/stats_observer.hpp>`

Tracks comprehensive generation statistics: counts, timing, per-component breakdowns, and value distributions. Attach it like any other observer; read the fields after generation.

```cpp
auto stats = std::make_shared<dasmig::ext::stats_observer>();
gen.add_observer(stats);

gen.generate_batch({.count = 100});

std::wcout << L"Entities:   " << stats->entities_generated  << L'\n'
           << L"Components: " << stats->components_generated << L'\n'
           << L"Skipped:    " << stats->components_skipped   << L'\n'
           << L"Failures:   " << stats->component_failures   << L'\n';

// Per-key counts.
for (const auto& [key, n] : stats->component_counts)
    std::wcout << key << L": " << n << L" generated\n";

// Per-key retry counts.
for (const auto& [key, n] : stats->component_retries)
    std::wcout << key << L": " << n << L" retries\n";

// Timing.
using ms = std::chrono::milliseconds;
std::wcout << L"Avg entity: "
           << std::chrono::duration_cast<ms>(stats->avg_entity_time()).count()
           << L" ms\n";
for (const auto& [key, _] : stats->component_counts)
    std::wcout << key << L" avg: "
               << std::chrono::duration_cast<ms>(
                      stats->avg_component_time(key)).count()
               << L" ms\n";

// Value distribution.
for (const auto& [key, dist] : stats->value_distribution)
    for (const auto& [val, count] : dist)
        std::wcout << key << L"=" << val << L": " << count << L'\n';

stats->reset(); // zeroes all counters

// Generate a full formatted report.
std::wcout << stats->report();

// Or use the stream operator (equivalent).
std::wcout << *stats;
```

**Entity counters:**

| Field | Description |
|---|---|
| `entities_generated` | Successful entities produced |
| `entity_retries` | Entity-level validation retries |
| `entity_failures` | Entity validation exhausted (threw) |

**Component counters:**

| Field | Description |
|---|---|
| `components_generated` | Total component values produced |
| `components_skipped` | Weight or conditional skips |
| `component_failures` | Component validation exhausted (threw) |

**Per-component-key maps (all `std::map<std::wstring, …>`):**

| Field | Value type | Description |
|---|---|---|
| `component_counts` | `size_t` | Times each key was generated |
| `component_skip_counts` | `size_t` | Times each key was skipped |
| `component_failure_counts` | `size_t` | Times each key failed |
| `component_retries` | `size_t` | Total retries per key |
| `component_times` | `duration` | Cumulative wall-clock per key |
| `component_min_times` | `duration` | Fastest generation per key |
| `component_max_times` | `duration` | Slowest generation per key |

**Entity timing:**

| Field | Description |
|---|---|
| `total_generation_time` | Sum of wall-clock across all entities |
| `min_entity_time` | Fastest entity generation |
| `max_entity_time` | Slowest entity generation |

**Components-per-entity:**

| Field | Description |
|---|---|
| `min_components_per_entity` | Fewest components in a single entity |
| `max_components_per_entity` | Most components in a single entity |
| `total_components_in_entities` | Sum for computing averages |

**Value distribution:**

`value_distribution` is a `map<wstring, map<wstring, size_t>>` — for each component key, counts how many times each display value appeared. Common `std::any` types (`wstring`, `int`, `double`, `float`, `long`, `bool`) are stringified automatically; others record as `<other>`.

**Computed helpers:**

| Method | Returns |
|---|---|
| `avg_entity_time()` | `total_generation_time / entities_generated` |
| `avg_component_time(key)` | per-key average generation time |
| `avg_components_per_entity()` | `double` average |
| `component_retry_rate()` | total retries / components generated |
| `entity_retry_rate()` | entity retries / entities generated |

## ECS Integration: EnTT

`dasmig/ext/entt_adapter.hpp` bridges entity-generator with [EnTT](https://github.com/skypjack/entt). It maps generator component keys to typed EnTT components via user-defined callables.

**Requires:** EnTT header (`<entt/entt.hpp>`) available on the include path.

```cpp
#include <dasmig/entitygen.hpp>
#include <dasmig/ext/entt_adapter.hpp>

// ECS component types
struct Name { std::wstring value; };
struct Position { float x, y; };

// Set up generator
dasmig::eg gen;
gen.add(std::make_unique<dasmig::constant_component<std::wstring>>(L"name", L"Alice"))
   .add(std::make_unique<dasmig::range_component<float>>(L"x", 0.f, 100.f))
   .add(std::make_unique<dasmig::range_component<float>>(L"y", 0.f, 100.f));

// Set up adapter
entt::registry registry;
dasmig::ext::entt_adapter adapter(registry);

adapter.map<Name>(L"name", [](const dasmig::entity& e) {
    return Name{e.get<std::wstring>(L"name")};
});

// Multi-key → single component: triggered by key "x", reads both "x" and "y"
adapter.map<Position>(L"x", [](const dasmig::entity& e) {
    return Position{e.get<float>(L"x"), e.get<float>(L"y")};
});

// Spawn
auto entt_entity = adapter.spawn(gen.generate({}));
registry.get<Name>(entt_entity);     // Name{"Alice"}
registry.get<Position>(entt_entity); // Position{...}

// Batch spawn
auto batch = gen.generate_batch({.count = 100});
auto entities = adapter.spawn_batch(batch); // 100 EnTT entities
```

**Direct mapping** (no transform, when generator value type == ECS type):

```cpp
gen.add(std::make_unique<dasmig::range_component<int>>(L"hp", 50, 200));
adapter.map<int>(L"hp"); // any_cast<int> directly emplaced
```

**Spawn into existing entity:**

```cpp
auto prefab = registry.create();
registry.emplace<some_tag>(prefab);
adapter.spawn_into(prefab, gen.generate({})); // adds mapped components
```

**Skipped components** (weight = 0, conditional = false) are silently ignored — no EnTT component is emplaced for that key.

## ECS Integration: Flecs

`dasmig/ext/flecs_adapter.hpp` bridges entity-generator with [Flecs](https://github.com/SanderMertens/flecs). Same mapping pattern as the EnTT adapter.

**Requires:** Flecs header (`<flecs.h>`) and compiled `flecs.c` linked.

```cpp
#include <dasmig/entitygen.hpp>
#include <dasmig/ext/flecs_adapter.hpp>

struct Name { std::wstring value; };
struct Position { float x, y; };

dasmig::eg gen;
gen.add(std::make_unique<dasmig::constant_component<std::wstring>>(L"name", L"Alice"))
   .add(std::make_unique<dasmig::range_component<float>>(L"x", 0.f, 100.f))
   .add(std::make_unique<dasmig::range_component<float>>(L"y", 0.f, 100.f));

flecs::world world;
dasmig::ext::flecs_adapter adapter(world);

adapter.map<Name>(L"name", [](const dasmig::entity& e) {
    return Name{e.get<std::wstring>(L"name")};
});

adapter.map<Position>(L"x", [](const dasmig::entity& e) {
    return Position{e.get<float>(L"x"), e.get<float>(L"y")};
});

auto flecs_entity = adapter.spawn(gen.generate({}));
flecs_entity.get<Name>();     // const Name&
flecs_entity.get<Position>(); // const Position&

// Batch spawn
auto entities = adapter.spawn_batch(gen.generate_batch({.count = 100}));

// Spawn into existing entity
auto prefab = world.entity();
adapter.spawn_into(prefab, gen.generate({}));
```

**API summary** (identical for both adapters):

| Method | Description |
|---|---|
| `map<T>(key, fn)` | Register a key→component mapping with transform |
| `map<T>(key)` | Direct mapping (value type must match T) |
| `spawn(entity)` | Create ECS entity with mapped components |
| `spawn_into(target, entity)` | Apply mapped components to existing entity |
| `spawn_batch(vector)` | Spawn multiple entities at once |
| `clear_mappings()` | Remove all registered mappings |

## Random Numbers in Components

`ctx.random()` is a `dasmig::core::rng` seeded for this component. Draw from it with the dasmig-core functions, which give the same results on every platform (the `std::` distributions do not):

| Need | Call |
|------|------|
| Integer in [lo, hi] | `dasmig::core::uniform_int(ctx.random(), lo, hi)` |
| Real in [lo, hi) | `dasmig::core::uniform_real(ctx.random(), lo, hi)` |
| Element of a container | `dasmig::core::pick(ctx.random(), values)` (a reference into it) |
| True with probability p | `dasmig::core::bernoulli(ctx.random(), p)` |
| Index by weights | `dasmig::core::discrete{weights}(ctx.random())` (build once, reuse) |
| Normal, log-normal | `dasmig::core::normal(ctx.random(), mean, sd)`, `lognormal(...)` |
| Shuffle, sample | `dasmig::core::shuffle(ctx.random(), range)`, `sample_indices(...)` |

The algorithms are specified in dasmig-core's usage guide.

## Migrating to the Request-First API

Generation now has two forms for each established method: `(request)` and `(request, seed)`. The old positional overloads are removed, not retained as compatibility wrappers.

| Previous call | Request-first call |
|---|---|
| `gen.generate()` | `gen.generate({})` |
| `gen.generate(seed)` | `gen.generate({}, seed)` |
| `gen.generate({L"a", L"b"}, seed)` | `gen.generate({.component_keys = std::vector<std::wstring>{L"a", L"b"}}, seed)` |
| `gen.generate_batch(n, seed)` | `gen.generate_batch({.count = n}, seed)` |
| `gen.generate_batch_async(n, seed)` | `gen.generate_batch_async({.count = n}, seed)` |
| `gen.generate_group(L"identity", seed)` | `gen.generate_group({.group = L"identity"}, seed)` |
| `gen.set_validator(fn)` followed by generation | `gen.generate({.validator = fn})` |

`seed`, `unseed`, `set_validator`, and `clear_validator` no longer exist on `eg`. Use a caller-owned `dasmig::core::rng` for a deterministic sequence of call seeds, as shown under Registering and Generating, or a seeded batch. Keep `max_retries`, observers, registration, group definitions, and weights on the instance.

Seeded outputs, component behavior, and batch seed derivation are unchanged from the previous dasmig-core-based API when the request expresses the same choices. Replay an entity with `gen.generate(request, entity.seed())`, preserving the selection, validator, and instance setup. A seed signature records the seed, not the request or the registered components.

## Migrating from v3

Version 4 makes the library usable as the base of a composition such as a persona generator. Draws are unchanged: a v3 component gives the same value for the same seed, and entities built from v3 components give the same JSON, so existing known answers still hold.

- **`json_value` is a class.** It was a `std::variant` of scalars; it now also holds arrays and objects, with implicit conversions from the usual types. Code that returned `std::int64_t{n}`, `double`, `bool`, `nullptr` or a string from `to_json()` compiles as it was. Code that used the variant directly (`std::get`, `std::visit`) must use `json_value::write()`, `get_array()` and `get_object()` instead.
- **Stricter `add()`.** A key with an empty path segment, or one that is also the parent of another key, throws `std::invalid_argument`. Keys with dots now mean a path in the JSON.
- **`entity::to_string()` shows labels.** A component's `label()` defaults to its key, so nothing changes until you override it.
- **`generation_context::get` message.** A missing key still throws `std::out_of_range`; the message now names it.
- **New:** `entity_request::inputs` with `generation_context::input()`, `input_or()` and `has_input()`; `component::depends_on()`; `component::label()`; `component::data_hash()`; `field()` and `field_component<T>`; `generation_context::try_get()` and `get_or()`; nested JSON from dotted keys; `entity::signature().data`.

## Migrating from v1

Version 2 moves the library onto [dasmig-core](https://github.com/dasmig/dasmig-core).

- **Every generated entity changes.** The engine (xoshiro256\*\*) and the distributions are the core's, so a seed gives the same entity on every compiler, standard library and CPU. Seeds saved with v1 do not reproduce their v1 results. The distributions of the generic components are unchanged (checked against v1 over 20,000 draws).
- **`ctx.random()` returns `dasmig::core::rng&`** instead of effolkronium's `random_local&`. Replace `ctx.random().get(lo, hi)` with `dasmig::core::uniform_int(ctx.random(), lo, hi)` (or `uniform_real` for floating point), and `*ctx.random().get(values)` with `dasmig::core::pick(ctx.random(), values)` (see Random Numbers in Components).
- **Full 64-bit seeds.** v1 used a 32-bit `std::mt19937` and cut seeds to 32 bits, so seeds differing only in their high bits gave the same entity.
- **Components are independent.** Each component draws from a stream split from the entity seed by its key; in v1 adding or removing a component shifted the values of every later one.
- **`entity::seed()` is the call seed** (or the accepted attempt's), so `generate(request, e.seed())` replays the entity; in v1 it was a value drawn from the call seed, which could not be passed back.
- **Batches.** `generate_batch(request, seed)` and `generate_batch_async(request, seed)` give the same entities, and each entity replays alone from its seed. Entity-validation retries no longer consume the batch's seeds.
- **`range_component<T>`** requires an integer (not `bool`) or floating-point `T`; a float range stays below `hi`. **`weighted_choice_component`** builds its weighted choice once and rejects negative or non-finite weights.
- **Files:** replace `random.hpp` with `core.hpp` and the `core/` directory, kept next to `entitygen.hpp`.
- **New:** `entity::to_json()`, `entity::signature()`, `component::to_json()` (and `dasmig::json_value`).

## Error Reference

The library throws standard exceptions. No custom exception types are used.

| Exception | Thrown by | Condition |
|---|---|---|
| `std::out_of_range` | `entity::get<T>(key)`, `entity::get_any(key)` | Key not present in the entity |
| `std::out_of_range` | `entity::seed(key)` | Component key not found |
| `std::out_of_range` | `eg::weight(key, value)` | Component not registered |
| `std::out_of_range` | Any generation method with `request.group` | Group not found |
| `std::invalid_argument` | Any generation method | Both `request.group` and `request.component_keys` are set |
| `std::invalid_argument` | `eg::generate_group(request)` | No group specified |
| `std::runtime_error` | Any generation method | Component validation exhausted after `max_retries` |
| `std::runtime_error` | Any generation method | Entity validation exhausted after `max_retries` |
| `std::invalid_argument` | `choice_component` constructor | Empty choices vector |
| `std::invalid_argument` | `weighted_choice_component` constructor | Empty options vector or all weights are zero |
| `std::bad_any_cast` | `entity::get<T>(key)`, `generation_context::get<T>(key)` | Type mismatch on the stored value |
| `std::out_of_range` | `generation_context::get<T>(key)` | No value was generated for the key (the message names it) |
| `std::invalid_argument` | `generation_context::input<T>(name)` | The request has no such input, or it has another type |
| `std::invalid_argument` | `eg::add` | A key with an empty path segment, or that collides with another key as a parent |
| `std::invalid_argument` | Any generation method | A selected component depends on a key that is not registered |
| `std::logic_error` | Any generation method | The selected components depend on each other in a cycle |
