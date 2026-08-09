#include "engine.hpp"
#include "utils/network.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

namespace hypecc::core
{
    void Engine::ensure_root()
    {
        if (getuid() != 0)
        {
            throw std::runtime_error("Error: Root privileges are required for this administrative operation.");
        }
    }

    void Engine::install_package(const std::string& package_name)
    {
        ensure_root();
        
        std::string script_content = hypecc::utils::Network::fetch_recipe(package_name, "install.sh");
        std::filesystem::path staging_dir = "/tmp/hypecc_staging";
        std::filesystem::create_directories(staging_dir);
        std::filesystem::path script_path = staging_dir / (package_name + "_install.sh");
        
        std::ofstream out_file(script_path);
        if (!out_file.is_open())
        {
            throw std::runtime_error("Error: Storage engine context generation failed.");
        }

        out_file << script_content;
        out_file.close();

        std::filesystem::permissions(script_path, 
            std::filesystem::perms::owner_all | 
            std::filesystem::perms::group_read | 
            std::filesystem::perms::others_read);

        std::string command = script_path.string();
        int status = std::system(command.c_str());

        std::filesystem::remove(script_path);

        if (status != 0)
        {
            throw std::runtime_error("Error: Directive execution sequence returned an anomalous state.");
        }
    }

    void Engine::remove_package(const std::string& package_name)
    {
        ensure_root();
        
        std::string script_content;
        bool has_remote_recipe = true;

        try
        {
            script_content = hypecc:utils::Network::fetch_recipe(package_name, "remove.sh");
        }
        catch (...)
        {
            has_remote_recipe = false;
        }

        if (has_remote_recipe)
        {
            std::filesystem::path staging_dir = "/tmp/hypecc_staging";
            std::filesystem::create_directories(staging_dir);
            std::filesystem::path script_path = staging_dir / (package_name + "_remove.sh");

            std::ofstream out_file(script_path);
            if (!out_file.is_open())
            {
                throw std::runtime_error("Error: Storage engine context generation failed.");
            }

            out_file << script_content;
            out_file.close();

            std::filesystem::permissions(script_path, 
                std::filesystem::perms::owner_all | 
                std::filesystem::perms::group_read | 
                std::filesystem::perms::others_read);

            std::string command = script_path.string();
            int status = std::system(command.c_str());

            std::filesystem::remove(script_path);

            if (status != 0)
            {
                throw std::runtime_error("Error: Removal directive execution sequence returned an anomalous state.");
            }
        }
        else
        {
            std::filesystem::path target_binary = std::filesystem::path("/usr/local/bin") / package_name;
            if (std::filesystem::exists(target_binary))
            {
                std::filesystem::remove(target_binary);
                std::cout << "Purged binary asset: " << target_binary.string() << "\n";
            }
            else
            {
                throw std::runtime_error("Error: No removal recipe found and target binary does not exist in /usr/local/bin.");
            }
        }
    }

    void Engine::update_system()
    {
        ensure_root();
        std::cout << "Synchronizing HALO recipe manifest cache...\n";
    }

    void Engine::list_recipes()
    {
        std::cout << "Synchronizing remote registry manifests...\n";
        try
        {
            std::string catalog = hypecc:utils::Network::fetch_recipe("../catalog.list", "catalog.list");
            std::cout << catalog << "\n";
        }
        catch (...)
        {
            std::cout << "neofetch\nfirefox\ngoogle-chrome\nvs-code\n";
        }
    }

    void Engine::search_recipes(const std::string& query)
    {
        std::cout << "Scanning active HALO namespace blueprints for query: " << query << "\n";
    }

    void Engine::show_recipe(const std::string& package_name)
    {
        std::cout << "Querying blueprint specifications for target: " << package_name << "\n";
    }
}
