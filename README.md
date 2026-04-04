# Entity Generator for C++

[![Entity Generator for C++](https://raw.githubusercontent.com/dasmig/entity-generator/master/doc/entity-generator.png)](https://github.com/dasmig/entity-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/entity-generator/main/LICENSE)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/releases)
[![GitHub Issues](https://img.shields.io/github/issues/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/issues)

## Features

- **Core** — Generate entities composed of user-defined components. Each component produces a typed random value accessible via `entity.get<T>(key)`. Register, remove, and generate with fluent chaining.

- **Generic Components** — Ready-made templates for common patterns: `constant_component<T>`, `choice_component<T>`, `range_component<T>`, `callback_component`, and `weighted_choice_component<T>`. All accept an optional custom formatter.

- **Randomness & Replay** — Deterministic seeding at per-call and generator level. Every entity and component stores its seed for full replay via seed signatures.

- **Advanced Generation** — Batch generation (synchronous and concurrent), named component groups, selective generation by key subset, component weights for probabilistic inclusion, and conditional components for logic-driven inclusion via `should_generate(ctx)`.

- **Validation & Hooks** — Per-component `validate()` with automatic retries, entity-level validator callbacks, and a `generation_observer` interface with 15 lifecycle hooks. Multiple observers can be attached simultaneously via `add_observer` / `remove_observer`.

- **Extensions** — `dasmig::ext::stats_observer` tracks generation counts, skips, retries, and failures out of the box.

- **Serialization** — `entity.to_string()`, `operator<<`, and `entity.to_map()` for structured key–value export.

- **Composable & Thread-Safe** — Components can wrap [name-generator](https://github.com/dasmig/name-generator) and [nickname-generator](https://github.com/dasmig/nickname-generator). Independent `eg` instances enable lock-free concurrent generation.

## Integration

[`entitygen.hpp`](https://github.com/dasmig/entity-generator/blob/main/dasmig/entitygen.hpp) is the single required file [released here](https://github.com/dasmig/entity-generator/releases). You need to add

```cpp
#include <dasmig/entitygen.hpp>

// For convenience.
using eg = dasmig::eg;
```

to the files you want to generate entities and set the necessary switches to enable C++23 (e.g., `-std=c++23` for GCC and Clang).

The library makes use of a [`random generation library`](https://github.com/effolkronium/random) by [effolkronium](https://github.com/effolkronium). For the convenience of the user, the header-only file containing the implementation was added to this repository.

## Quick Start

Define a component by implementing the `component` interface:

```cpp
#include <dasmig/entitygen.hpp>

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

Register components and generate entities:

```cpp
using eg = dasmig::eg;

eg::instance()
    .add(std::make_unique<character_class>())
    .add(std::make_unique<age>());

// Generate and stream.
std::wcout << eg::instance().generate() << std::endl;
// e.g. "class: Mage  age: 34"

// Typed access.
auto entity = eg::instance().generate();
std::wstring cls = entity.get<std::wstring>(L"class");
int char_age = entity.get<int>(L"age");

// Reproducible with a seed.
auto seeded = eg::instance().generate(42);
```

For the complete feature guide — component dependencies, custom types, seed signatures, batch generation, groups, weights, validation, event hooks, extensions, and more — see the **[Usage Guide](doc/usage.md)**.

For planned features — concurrent generation, conditional components, structured serialization, EnTT integration, and more — see the **[Roadmap](doc/roadmap.md)**.
