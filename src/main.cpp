#include <iostream>
#include <string_view>
#include "core/engine.hpp"

void print_version()
{
    std::cout << R"(________                        
\______ \ _____ __  _  ______  
 |    |  \\__  \\ \/ \/ /    \ 
 |    `   \/ __ \\     /   |  \
/_______  (____  /\/\_/|___|  /
        \/     \/           \/ )" 
              << "\n\n";
    std::cout << "Dawn Package System — Version 1.2.2-LTS\n";
    std::cout << "Engine: Modern C++ 20 (Bare-Metal Execution Layer)\n";
    std::cout << "Licensing: GNU GPL v3.0\n";
    std::cout << "The DAWN of Linux is not Flatpak.\n";
}

void print_help()
{
    std::cout << "Usage: dawn [options] command\n\n"
              << "Most used commands:\n"
              << "  list         - List available recipes in the HALO registry\n"
              << "  search       - Search through HALO recipe names and descriptions\n"
              << "  show         - Display detailed information about a specific recipe\n"
              << "  install      - Fetch a recipe from HALO and execute custom install logic\n"
              << "  remove       - Remove a package natively\n"
              << "  update       - Sync local package lists and HALO recipe cache\n\n"
              << "Options:\n"
              << "  -v, --version   - Display version manager information\n"
              << "  -h, --help      - Display this help menu\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_help();
        return 0;
    }

    std::string_view command(argv[1]);

    try
    {
        if (command == "--version" || command == "-v")
        {
            print_version();
        } 
        else if (command == "--help" || command == "-h")
        {
            print_help();
        }
        else if (command == "install")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'install' command requires a valid package identifier.\n";
                return 1;
            }
            std::string package(argv[2]);
            std::cout << "Resolving package pipeline for '" << package << "'...\n";
            dawn::core::Engine::install_package(package);
            std::cout << "Deployment sequence completed successfully.\n";
        }
        else if (command == "remove")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'remove' command requires a valid package identifier.\n";
                return 1;
            }
            std::string package(argv[2]);
            dawn::core::Engine::remove_package(package);
            std::cout << "Removal sequence completed successfully.\n";
        }
        else if (command == "update")
        {
            dawn::core::Engine::update_system();
            std::cout << "Synchronized state successfully.\n";
        }
        else if (command == "list")
        {
            dawn::core::Engine::list_recipes();
        }
        else if (command == "search")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'search' command requires a query string.\n";
                return 1;
            }
            dawn::core::Engine::search_recipes(argv[2]);
        }
        else if (command == "show")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'show' command requires a valid package identifier.\n";
                return 1;
            }
            dawn::core::Engine::show_recipe(argv[2]);
        }
        else
        {
            std::cerr << "Error: Unknown command or option '" << command << "'\n";
            return 1;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
