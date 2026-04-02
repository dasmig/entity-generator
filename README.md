# Entity Generator for C++

[![Entity Generator for C++](https://raw.githubusercontent.com/dasmig/entity-generator/master/docs/entity-generator.png)](https://github.com/dasmig/entity-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/entity-generator/main/LICENSE)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/releases)
[![GitHub Downloads](https://img.shields.io/github/downloads/dasmig/entity-generator/total)](https://github.com/dasmig/entity-generator/releases)
[![GitHub Issues](https://img.shields.io/github/issues/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/issues)

## Features

The library currently supports the following:

- **Entity Generation**. The library generates entities composed of user-defined components, each producing a random value.

- **Extensible Component Interface**. Define custom components by implementing the `component` interface. Components can produce any type (`std::wstring`, `int`, `double`, structs, etc.).

- **Typed Retrieval**. Access generated component values with full type safety via `entity.get<T>(key)`.

- **Selective Generation**. Generate entities with all registered components, or only a specific subset.

- **Component Dependencies**. Components receive a `generation_context` providing access to previously generated values, enabling dependent generation logic.

- **Reproducible Randomness**. Seed the generator for deterministic output. Supports per-call seeds and generator-level seeding for reproducible sequences.

- **Seed Signatures**. Every generated entity and component stores the random seed used to produce it, enabling replay and debugging.

- **Batch Generation**. Generate multiple entities in a single call with `generate_batch(count)`.

- **Component Groups**. Define named groups of components for convenient selective generation.

- **Component Weights**. Assign inclusion probabilities to components. A weight of `1.0` (default) means always included; `0.5` means included roughly half the time. Weights can be defined in the component interface or overridden per-component at registration time.

- **Thread Safety**. Create independent `eg` instances for lock-free concurrent generation.

- **Fluent Registration**. Chain `add()` and `remove()` calls to configure the generator.

- **Composable**. Components can internally use [name-generator](https://github.com/dasmig/name-generator) and [nickname-generator](https://github.com/dasmig/nickname-generator) or any other logic.

## Integration

[`entitygen.hpp`](https://github.com/dasmig/entity-generator/blob/main/dasmig/entitygen.hpp) is the single required file [released here](https://github.com/dasmig/entity-generator/releases). You need to add

```cpp
#include <dasmig/entitygen.hpp>

// For convenience.
using eg = dasmig::eg;
```

to the files you want to generate entities and set the necessary switches to enable C++23 (e.g., `-std=c++23` for GCC and Clang).

The library makes use of a [`random generation library`](https://github.com/effolkronium/random) by [effolkronium](https://github.com/effolkronium). For the convenience of the user, the header-only file containing the implementation was added to this repository.

## Usage

You define what an entity contains by implementing the `component` interface and registering instances with the generator.

### Defining Components

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
        return ctx.random().get(classes);
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
        return ctx.random().get(18, 65);
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};
```

### Registering and Generating

```cpp
using eg = dasmig::eg;

// Register components with fluent chaining.
eg::instance()
    .add(std::make_unique<character_class>())
    .add(std::make_unique<age>());

// Generate an entity with all registered components.
auto entity = eg::instance().generate();

// Generate an entity with only specific components.
auto partial = eg::instance().generate({L"class"});

// Output to stream.
std::wcout << eg::instance().generate() << std::endl;

// Generate with a seed for reproducible results.
auto seeded = eg::instance().generate(42);

// Seed the generator for a deterministic sequence.
eg::instance().seed(123);
auto first  = eg::instance().generate();
auto second = eg::instance().generate();
eg::instance().unseed();
```

### Typed Retrieval

```cpp
auto entity = eg::instance().generate();

// Access values with their original types.
std::wstring char_class = entity.get<std::wstring>(L"class");
int char_age = entity.get<int>(L"age");

// Check if a component exists.
if (entity.has(L"name")) { /* ... */ }
```

### Component Dependencies

Components receive a `generation_context` that provides access to values generated by earlier components. Registration order determines generation order.

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

The context also exposes `ctx.has(key)` to check whether a dependency was generated (useful when generating with a subset of components).

### Composing with Other Generators

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

### Custom Types

Components can produce any type. Override `to_string()` on your component to enable stream output:

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
            ctx.random().get(-100.f, 100.f),
            ctx.random().get(-100.f, 100.f)};
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        auto pos = std::any_cast<vec2>(value);
        return L"(" + std::to_wstring(pos.x) + L", " + std::to_wstring(pos.y) + L")";
    }
};
```

### Seed Signatures

Every generated entity and component stores the random seed that produced it. This enables replay, debugging, and logging.

```cpp
auto entity = eg::instance().generate();

// Retrieve the entity-level seed. This single value can reproduce
// all component seeds and thus the entire entity.
std::uint64_t entity_seed = entity.seed();

// Retrieve the seed used for a specific component.
std::uint64_t age_seed = entity.seed(L"age");
```

Component seeds can be used to replay individual component generation:

```cpp
// Replay a component's random values using its captured seed.
effolkronium::random_local rng;
rng.seed(static_cast<std::mt19937::result_type>(entity.seed(L"age")));
int replayed_age = rng.get(18, 65); // Same value as entity.get<int>(L"age")
```

The seed hierarchy is fully deterministic:

```
generation seed (per-call or generator-level)
  └─ entity seed          ← entity.seed()
       └─ local engine
            ├─ component A seed   ← entity.seed(L"A")
            ├─ component B seed   ← entity.seed(L"B")
            └─ ...
```

### Batch Generation

Generate multiple entities in a single call:

```cpp
// Generate 10 entities with all registered components.
auto batch = eg::instance().generate_batch(10);

for (const auto& e : batch)
{
    std::wcout << e << L'\n';
}

// Seeded batch for deterministic results.
auto seeded_batch = eg::instance().generate_batch(10, 42);
```

### Component Groups

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
auto fighter = eg::instance().generate_group(L"combat");

// Seeded group generation.
auto seeded = eg::instance().generate_group(L"combat", 42);

// Check and remove groups.
if (eg::instance().has_group(L"identity")) { /* ... */ }
eg::instance().remove_group(L"identity");
```

### Thread Safety

The `eg` class supports independent instances. Create one per thread for lock-free concurrent generation:

```cpp
// Each thread gets its own generator — no locks needed.
std::vector<std::jthread> threads;
for (int i = 0; i < 4; ++i)
{
    threads.emplace_back([&](int id) {
        dasmig::eg gen;
        gen.add(std::make_unique<name>(/* … */));
        gen.seed(id);
        auto e = gen.generate();
    }, i);
}
```

`eg` is move-constructible and move-assignable, so instances can be transferred between scopes. The `instance()` singleton is still available for single-threaded convenience.

### Component Weights

Components can declare an inclusion weight via the `weight()` virtual method (default `1.0`). Values range from `0.0` (never included) to `1.0` (always included). The generator rolls against the weight during generation; components that fail the roll are skipped.

```cpp
class rare_trait : public component
{
  public:
    std::wstring key() const override { return L"rare_trait"; }
    double weight() const override { return 0.2; } // 20% chance

    std::any generate(const generation_context& ctx) const override
    {
        return ctx.random().get<std::wstring>({L"scar", L"tattoo", L"birthmark"});
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