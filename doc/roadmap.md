# Roadmap

Planned features for the entity-generator library. Items are not in priority order.

## Concurrent Batch Generation

`generate_batch_async(count)` generates entities in parallel while preserving deterministic seeding. The approach: pre-derive N entity seeds from the engine sequentially (cheap), then dispatch N independent generation calls concurrently — each with its own pre-computed seed. Same seeds produce the same entities regardless of thread scheduling. Returns `std::vector<entity>` in seed order.

## Structured Serialization

`entity::to_map()` returning `std::map<std::wstring, std::wstring>` for easy integration with JSON/XML serializers. Optionally `entity::from_map()` with a component registry for deserialization/replay.

## Generation Statistics

A built-in observer (or standalone stats struct) that tracks: entities generated, components skipped (weight), retries per component, validation failures. Accessible via `eg::stats()` or as a reusable observer. Useful for tuning weights and retry limits.

## Generic Components

A library of ready-made, reusable component templates for common patterns — e.g., `choice_component<T>` (pick from a list), `range_component<T>` (uniform random in a range), `constant_component<T>` (fixed value). Reduces boilerplate for simple components that don't need custom logic.

## EnTT Integration

An adapter layer that bridges entity-generator with the [EnTT](https://github.com/skypjack/entt) ECS library. Generated component values are automatically assigned as EnTT components to an entity in a registry, enabling direct use in ECS-based game loops without manual mapping.

## Python Wrapper

A Python binding (via pybind11 or nanobind) exposing the entity-generator API to Python. Components are defined as Python classes inheriting from a base, registered with the generator, and entities are returned as typed Python objects. Enables rapid prototyping and scripting of entity generation logic.

## Node.js Wrapper

A Node.js addon (via node-addon-api / N-API) providing the entity-generator API to JavaScript/TypeScript. Components are defined as JS classes, entities are returned as plain objects. Supports both synchronous and async generation for server-side and tooling use cases.

## .NET Wrapper

A .NET binding (via C++/CLI or P/Invoke) exposing the entity-generator API to C# and other .NET languages. Components are defined as C# classes implementing an interface, entities are returned as strongly-typed objects. Targets integration with Unity and other .NET game frameworks.
