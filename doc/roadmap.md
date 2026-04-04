# Roadmap

Planned features for the entity-generator library. Items are not in priority order.

## ~~EnTT Integration~~

Adapter layer (`dasmig/ext/entt_adapter.hpp`) bridges entity-generator with [EnTT](https://github.com/skypjack/entt). Users register key→component mappings, then `spawn()` creates typed EnTT entities from generated output.

## ~~Flecs Integration~~

Adapter layer (`dasmig/ext/flecs_adapter.hpp`) bridges entity-generator with [Flecs](https://github.com/SanderMertens/flecs). Same mapping pattern as the EnTT adapter, using Flecs' `set<T>()` API.

## Python Wrapper

A Python binding (via pybind11 or nanobind) exposing the entity-generator API to Python. Components are defined as Python classes inheriting from a base, registered with the generator, and entities are returned as typed Python objects. Enables rapid prototyping and scripting of entity generation logic.

## Node.js Wrapper

A Node.js addon (via node-addon-api / N-API) providing the entity-generator API to JavaScript/TypeScript. Components are defined as JS classes, entities are returned as plain objects. Supports both synchronous and async generation for server-side and tooling use cases.

## .NET Wrapper

A .NET binding (via C++/CLI or P/Invoke) exposing the entity-generator API to C# and other .NET languages. Components are defined as C# classes implementing an interface, entities are returned as strongly-typed objects. Targets integration with Unity and other .NET game frameworks.
