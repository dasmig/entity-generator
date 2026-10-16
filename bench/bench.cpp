// Benchmarks for entity-generator; see bench.hpp for the options.
//
// The library ships no data, so --tier changes nothing; the benchmarks run
// a small set of components like the example's.

#include "../dasmig/entitygen.hpp"
#include "bench.hpp"

#include <memory>

namespace
{

class role : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"role"; }

    [[nodiscard]] std::any generate(const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> roles{L"Warrior", L"Mage", L"Rogue",
                                                     L"Healer",  L"Ranger", L"Paladin"};
        return dasmig::core::pick(ctx.random(), roles);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

class level : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"level"; }

    [[nodiscard]] std::any generate(const dasmig::generation_context& ctx) const override
    {
        return dasmig::core::uniform_int(ctx.random(), 1, 100);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Depends on level, so the generator orders the components.
class health : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"health"; }

    [[nodiscard]] std::vector<std::wstring> depends_on() const override { return {L"level"}; }

    [[nodiscard]] std::any generate(const dasmig::generation_context& ctx) const override
    {
        return ctx.get<int>(L"level") * 10 + dasmig::core::uniform_int(ctx.random(), 0, 50);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

} // namespace

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"entity-generator", argc, argv};
    if (run.tier() == dasmig::dataset::full) return run.missing_tier();

    dasmig::eg gen;
    gen.add(std::make_unique<role>()).add(std::make_unique<level>()).add(std::make_unique<health>());

    run.calls("generate", [&](std::uint64_t seed) { return gen.generate({}, seed); });
    run.calls("generate_batch x100", [&](std::uint64_t seed) {
        return gen.generate_batch({.count = 100}, seed);
    });
    run.calls("generate to_json", [&](std::uint64_t seed) {
        return gen.generate({}, seed).to_json();
    });

    return run.finish();
}
