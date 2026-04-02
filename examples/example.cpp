#include "../dasmig/entitygen.hpp"
#include "../dasmig/random.hpp"
#include <iostream>

// Example component: generates a random character class.
class character_class : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"class";
    }

    [[nodiscard]] std::any generate() const override
    {
        static const std::vector<std::wstring> classes{
            L"Warrior", L"Mage", L"Rogue", L"Healer", L"Ranger", L"Paladin"};

        return *effolkronium::random_thread_local::get(classes);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random age.
class age : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"age";
    }

    [[nodiscard]] std::any generate() const override
    {
        return effolkronium::random_thread_local::get(18, 65);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random level.
class level : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"level";
    }

    [[nodiscard]] std::any generate() const override
    {
        return effolkronium::random_thread_local::get(1, 100);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random character name.
class character_name : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"name";
    }

    [[nodiscard]] std::any generate() const override
    {
        static const std::vector<std::wstring> names{
            L"Aldric",  L"Branwen", L"Cedric",  L"Dahlia",  L"Eldrin",
            L"Freya",   L"Gareth",  L"Helena",  L"Isolde",  L"Jareth",
            L"Kaelen",  L"Lyria",   L"Magnus",  L"Nadia",   L"Orion",
            L"Petra",   L"Quinlan", L"Rosalind"};

        return *effolkronium::random_thread_local::get(names);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

int main()
{
    // Register components.
    dasmig::eg::instance()
        .add(std::make_unique<character_name>())
        .add(std::make_unique<character_class>())
        .add(std::make_unique<age>())
        .add(std::make_unique<level>());

    // Generate entities with all components.
    std::wcout << L"--- Full entities ---" << std::endl;
    for (std::size_t i = 0; i < 5; i++)
    {
        std::wcout << dasmig::eg::instance().generate() << std::endl;
    }

    // Generate entities with only specific components.
    std::wcout << std::endl << L"--- Name and class only ---" << std::endl;
    for (std::size_t i = 0; i < 5; i++)
    {
        std::wcout << dasmig::eg::instance().generate({L"name", L"class"})
                   << std::endl;
    }

    // Typed retrieval.
    std::wcout << std::endl << L"--- Typed access ---" << std::endl;
    auto entity = dasmig::eg::instance().generate();
    std::wcout << L"Name:  " << entity.get<std::wstring>(L"name") << std::endl;
    std::wcout << L"Class: " << entity.get<std::wstring>(L"class") << std::endl;
    std::wcout << L"Age:   " << entity.get<int>(L"age") << std::endl;
    std::wcout << L"Level: " << entity.get<int>(L"level") << std::endl;

    return 0;
}
