# Entity Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![Entity Generator for C++](https://raw.githubusercontent.com/dasmig/entity-generator/master/doc/entity-generator.png)](https://github.com/dasmig/entity-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/entity-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/entity-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/entity-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/releases)
[![GitHub Issues](https://img.shields.io/github/issues/dasmig/entity-generator.svg)](https://github.com/dasmig/entity-generator/issues)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/entity-generator#integration)
[![Platform](https://img.shields.io/badge/platform-linux%20|%20windows%20|%20macos-lightgrey.svg)](https://github.com/dasmig/entity-generator)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/entity-generator/)

**[API Reference](https://dasmig.github.io/entity-generator/)** · **[Usage Guide](doc/usage.md)** · **[Releases](https://github.com/dasmig/entity-generator/releases)**

## Features

- **Core** — Generate entities composed of user-defined components. Each component produces a typed random value accessible via `entity.get<T>(key)`. Register, remove, and generate with fluent chaining.

- **Generic Components** — Ready-made templates for common patterns: `constant_component<T>`, `choice_component<T>`, `range_component<T>`, `callback_component`, and `weighted_choice_component<T>`. All accept an optional custom formatter.

- **Randomness & Replay** — Deterministic seeding at per-call and generator level. Every entity and component stores its seed for full replay via seed signatures; `generate(e.seed())` reproduces an entity.

- **Identical Everywhere** — A seed gives the same entity on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests. Each component draws from its own stream, so adding a component does not change the others.

- **Advanced Generation** — Batch generation (synchronous and concurrent), named component groups, selective generation by key subset, component weights for probabilistic inclusion, and conditional components for logic-driven inclusion via `should_generate(ctx)`.

- **Validation & Hooks** — Per-component `validate()` with automatic retries, entity-level validator callbacks, and a `generation_observer` interface with 15 lifecycle hooks. Multiple observers can be attached simultaneously via `add_observer` / `remove_observer`.

- **Extensions** — `dasmig::ext::stats_observer` tracks generation counts, skips, retries, and failures out of the box.

- **ECS Integration** — Optional adapter headers for [EnTT](https://github.com/skypjack/entt) and [Flecs](https://github.com/SanderMertens/flecs). Register key→component mappings, then `spawn()` converts generated entities into typed ECS entities.

- **Serialization** — `entity.to_string()`, `operator<<`, `entity.to_map()` for structured key–value export, and `entity.to_json()` with typed values and the replay seed.

- **Composable & Thread-Safe** — Components can wrap [name-generator](https://github.com/dasmig/name-generator) and [nickname-generator](https://github.com/dasmig/nickname-generator). Independent `eg` instances enable lock-free concurrent generation.

## Integration

[`entitygen.hpp`](https://github.com/dasmig/entity-generator/blob/main/dasmig/entitygen.hpp) is the library [released here](https://github.com/dasmig/entity-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/entity-generator/blob/main/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/entity-generator/tree/main/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. You need to add

```cpp
#include <dasmig/entitygen.hpp>

// For convenience.
using eg = dasmig::eg;
```

to the files you want to generate entities and set the necessary switches to enable C++23 (e.g., `-std=c++23` for GCC and Clang).

Components draw random numbers from `ctx.random()` with the [dasmig-core](https://github.com/dasmig/dasmig-core) functions (`dasmig::core::uniform_int`, `pick`, `bernoulli`, ...), which give the same results on every platform. Version 2 changes every seeded result and the component random API; see [Migrating from v1](doc/usage.md#migrating-from-v1).

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
        return dasmig::core::pick(ctx.random(), classes);
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
        return dasmig::core::uniform_int(ctx.random(), 18, 65);
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

For planned features — Python/Node.js/.NET wrappers and more — see the **[Roadmap](https://github.com/dasmig/entity-generator/blob/main/doc/roadmap.md)**.

## Building

```bash
# Example
make

# Tests
make test

# Code coverage
make coverage

# API docs
make docs
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 0.3 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |
| [EnTT](https://github.com/skypjack/entt), [Flecs](https://github.com/SanderMertens/flecs) | - | No | Only for the optional adapter headers in `dasmig/ext/` |

## Related Libraries

| Library | Description |
|---------|-------------|
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [city-generator](https://github.com/dasmig/city-generator) | Weighted city selection by population |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
