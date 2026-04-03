# Roadmap

Planned features for the entity-generator library. Items are not in priority order.

## ~~Concurrent Batch Generation~~ ✓

Done — `generate_batch_async(count)` and `generate_batch_async(count, seed)` generate entities in parallel with deterministic seeding.

## ~~Structured Serialization~~ ✓

Done — `entity::to_map()` returns `std::map<std::wstring, std::wstring>` for easy integration with JSON/XML serializers.

## Generation Statistics

A built-in observer (or standalone stats struct) that tracks: entities generated, components skipped (weight), retries per component, validation failures. Accessible via `eg::stats()` or as a reusable observer. Useful for tuning weights and retry limits.

## ~~Generic Components~~ ✓

Done — Five reusable class templates: `constant_component<T>`, `choice_component<T>`, `range_component<T>`, `callback_component`, and `weighted_choice_component<T>`. All support optional custom formatters.

## EnTT Integration

An adapter layer that bridges entity-generator with the [EnTT](https://github.com/skypjack/entt) ECS library. Generated component values are automatically assigned as EnTT components to an entity in a registry, enabling direct use in ECS-based game loops without manual mapping.

## Python Wrapper

A Python binding (via pybind11 or nanobind) exposing the entity-generator API to Python. Components are defined as Python classes inheriting from a base, registered with the generator, and entities are returned as typed Python objects. Enables rapid prototyping and scripting of entity generation logic.

## Node.js Wrapper

A Node.js addon (via node-addon-api / N-API) providing the entity-generator API to JavaScript/TypeScript. Components are defined as JS classes, entities are returned as plain objects. Supports both synchronous and async generation for server-side and tooling use cases.

## .NET Wrapper

A .NET binding (via C++/CLI or P/Invoke) exposing the entity-generator API to C# and other .NET languages. Components are defined as C# classes implementing an interface, entities are returned as strongly-typed objects. Targets integration with Unity and other .NET game frameworks.
