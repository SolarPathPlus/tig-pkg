#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>

namespace hypecc::core
{
    class Engine
    {
    public:
        static void install_package(const std::string& package_name);
        static void remove_package(const std::string& package_name);
        static void update_system();
        static void list_recipes();
        static void search_recipes(const std::string& query);
        static void show_recipe(const std::string& package_name);
    private:
        static void ensure_root();
    };
}

#endif
