# Entity Generator for C++

```
                 __  _ __                                       __
  ___  ____  / /_(_) /___  __   ____ ____  ____  ___  _________ _/ /_____  _____
 / _ \/ __ \/ __/ / __/ / / /  / __ `/ _ \/ __ \/ _ \/ ___/ __ `/ __/ __ \/ ___/
/  __/ / / / /_/ / /_/ /_/ /  / /_/ /  __/ / / /  __/ /  / /_/ / /_/ /_/ / /
\___/_/ /_/\__/_/\__/\__, /   \__, /\___/_/ /_/\___/_/   \__,_/\__/\____/_/
                    /____/   /____/
```

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
    [[nodiscard]] std::any generate() const override
    {
        static const std::vector<std::wstring> classes{
            L"Warrior", L"Mage", L"Rogue", L"Healer"};
        return *effolkronium::random_thread_local::get(classes);
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
    [[nodiscard]] std::any generate() const override
    {
        return effolkronium::random_thread_local::get(18, 65);
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
    [[nodiscard]] std::any generate() const override
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
    [[nodiscard]] std::any generate() const override
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
    [[nodiscard]] std::any generate() const override
    {
        return vec2{
            effolkronium::random_thread_local::get(-100.f, 100.f),
            effolkronium::random_thread_local::get(-100.f, 100.f)};
    }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        auto pos = std::any_cast<vec2>(value);
        return L"(" + std::to_wstring(pos.x) + L", " + std::to_wstring(pos.y) + L")";
    }
};
```