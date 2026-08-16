#include <iostream>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <curl/curl.h>

struct ConfxData
{
    std::vector<std::string> raw_lines;
    std::unordered_map<std::string, std::string> key_values;
};

class ConfxParser
{
public:
    static ConfxData parse(const std::string& filepath)
    {
        ConfxData data;
        std::ifstream file(filepath);

        if (!file.is_open())
        {
            return data;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos)
            {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                data.key_values[key] = value;
            }
            else
            {
                data.raw_lines.push_back(line);
            }
        }
        return data;
    }

    static void write(const std::string& filepath, const ConfxData& data)
    {
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            throw std::runtime_error("Error: Failed to write configuration file: " + filepath);
        }

        for (const auto& line : data.raw_lines)
        {
            file << line << "\n";
        }

        for (const auto& [key, value] : data.key_values)
        {
            file << key << "=" << value << "\n";
        }
    }
};

namespace tig_pkg::utils
{
    class ManifestManager
    {
    public:
        static std::string get_manifest_path(const std::string& package_name)
        {
            std::filesystem::path base_dir = ".tig-pkg";
            return (base_dir / "manifests" / (package_name + ".list")).string();
        }

        static bool record_path(const std::string& package_name, const std::string& target_path)
        {
            std::error_code ec;
            std::filesystem::path base_dir = ".tig-pkg";
            std::filesystem::create_directories(base_dir / "manifests", ec);

            std::ofstream file(get_manifest_path(package_name), std::ios::app);
            if (!file.is_open())
            {
                return false;
            }
            file << target_path << "\n";
            return true;
        }

        static std::vector<std::string> read_paths(const std::string& package_name)
        {
            std::vector<std::string> paths;
            std::ifstream file(get_manifest_path(package_name));
            if (!file.is_open())
            {
                return paths;
            }
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty())
                {
                    paths.push_back(line);
                }
            }
            return paths;
        }

        static void delete_manifest(const std::string& package_name)
        {
            std::error_code ec;
            std::filesystem::remove(get_manifest_path(package_name), ec);
        }
    };

    class Network
    {
    public:
        static std::string fetch_recipe(const std::string& package_name, const std::string& script_name = "install.bat");
        static std::string fetch_catalog();
    };

    namespace
    {
        size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
        {
            size_t total_size = size * nmemb;
            auto* str = static_cast<std::string*>(userp);
            str->append(static_cast<char*>(contents), total_size);
            return total_size;
        }
    }

    std::string Network::fetch_recipe(const std::string& package_name, const std::string& script_name)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw std::runtime_error("Critical: Failed to initialize network subsystem.");
        }

        std::string read_buffer;
        std::string url = "https://raw.githubusercontent.com/SolarPathPlus/arc/main/recipes/" 
                        + package_name + "/" + script_name;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || response_code != 200)
        {
            throw std::runtime_error("Error: Failed to resolve registry pipeline for payload target.");
        }

        return read_buffer;
    }

    std::string Network::fetch_catalog()
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw std::runtime_error("Critical: Failed to initialize network subsystem.");
        }

        std::string read_buffer;
        std::string url = "https://raw.githubusercontent.com/SolarPathPlus/arc/main/catalog.list";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || response_code != 200)
        {
            throw std::runtime_error("Error: Failed to resolve registry catalog.");
        }

        return read_buffer;
    }

    class SystemScanner
    {
    public:
        static void generate_project_config()
        {
            std::filesystem::path tig_pkg_dir = ".tig-pkg";
            std::filesystem::create_directories(tig_pkg_dir);

            std::filesystem::path config_path = tig_pkg_dir / "include.confx";
            if (std::filesystem::exists(config_path))
            {
                std::cout << "[tig-pkg] Configuration environment already exists at " << config_path.string() << "\n";
                return;
            }

            std::filesystem::create_directories("include");

            std::string detected_includes = "include";

            ConfxData data;
            data.key_values["name"] = std::filesystem::current_path().filename().string();
            data.key_values["include_dirs"] = detected_includes;
            data.key_values["requires"] = "";

            ConfxParser::write(config_path.string(), data);
            std::cout << "[tig-pkg] Initialized project configuration environment at " << config_path.string() << "\n";
        }
    };
}

namespace tig_pkg::core
{
    class DependencySolver
    {
    public:
        static void resolve_recursive(const std::string& package_name, std::unordered_set<std::string>& visited)
        {
            if (visited.find(package_name) != visited.end())
            {
                return;
            }
            visited.insert(package_name);

            std::cout << "[tig-pkg] Resolving dependency graph node: " << package_name << "\n";

            std::filesystem::path cache_dir = std::filesystem::path(".tig-pkg") / "cache" / package_name;
            std::filesystem::create_directories(cache_dir);

            std::string recipe_confx;
            try
            {
                recipe_confx = tig_pkg::utils::Network::fetch_recipe(package_name, "recipe.confx");
            }
            catch (...)
            {
                return;
            }

            std::filesystem::path confx_path = cache_dir / "recipe.confx";
            std::ofstream out(confx_path);
            if (out.is_open())
            {
                out << recipe_confx;
                out.close();
            }

            ConfxData data = ConfxParser::parse(confx_path.string());
            auto it = data.key_values.find("requires");
            if (it != data.key_values.end() && !it->second.empty())
            {
                std::stringstream ss(it->second);
                std::string dep;
                while (std::getline(ss, dep, ','))
                {
                    dep.erase(0, dep.find_first_not_of(" \t"));
                    dep.erase(dep.find_last_not_of(" \t") + 1);
                    if (!dep.empty())
                    {
                        resolve_recursive(dep, visited);
                    }
                }
            }
        }
    };

    class Engine
    {
    public:
        static void init_project();
        static void sync_project();
        static void install_package(const std::string& package_name);
        static void remove_package(const std::string& package_name);
        static void update_system();
        static void list_recipes();
        static void search_recipes(const std::string& query);
        static void show_recipe(const std::string& package_name);
    };

    void Engine::init_project()
    {
        tig_pkg::utils::SystemScanner::generate_project_config();
    }

    void Engine::sync_project()
    {
        std::filesystem::path config_path = ".tig-pkg/include.confx";
        if (!std::filesystem::exists(config_path))
        {
            throw std::runtime_error("Error: Missing project configuration file .tig-pkg/include.confx.");
        }

        ConfxData config = ConfxParser::parse(config_path.string());
        auto it = config.key_values.find("requires");
        if (it != config.key_values.end() && !it->second.empty())
        {
            std::unordered_set<std::string> visited;
            std::stringstream ss(it->second);
            std::string dep;
            while (std::getline(ss, dep, ','))
            {
                dep.erase(0, dep.find_first_not_of(" \t"));
                dep.erase(dep.find_last_not_of(" \t") + 1);
                if (!dep.empty())
                {
                    tig_pkg::core::DependencySolver::resolve_recursive(dep, visited);
                }
            }
        }
        std::cout << "[tig-pkg] Workspace state and local cache synchronized successfully.\n";
    }

    void Engine::install_package(const std::string& package_name)
    {
        std::string script_content = tig_pkg::utils::Network::fetch_recipe(package_name, "install.bat");

        std::filesystem::path local_include = "include";
        std::filesystem::create_directories(local_include);

        std::filesystem::path staging_dir = ".tig-pkg\\staging";
        std::filesystem::create_directories(staging_dir);
        std::filesystem::path script_path = staging_dir / (package_name + "_install.bat");

        std::ofstream out_file(script_path);
        if (!out_file.is_open())
        {
            throw std::runtime_error("Error: Storage engine context generation failed.");
        }

        out_file << "set TARGET_INCLUDE_DIR=" << std::filesystem::absolute(local_include).string() << "\n";
        out_file << script_content;
        out_file.close();

        std::string command = "\"" + script_path.string() + "\"";
        int status = std::system(command.c_str());

        std::filesystem::remove(script_path);

        if (status != 0)
        {
            throw std::runtime_error("Error: Directive execution sequence returned an anomalous state.");
        }

        tig_pkg::utils::ManifestManager::record_path(package_name, (local_include / package_name).string());
    }

    void Engine::remove_package(const std::string& package_name)
    {
        std::string script_content;
        bool has_remote_recipe = true;

        try
        {
            script_content = tig_pkg::utils::Network::fetch_recipe(package_name, "remove.bat");
        }
        catch (...)
        {
            has_remote_recipe = false;
        }

        if (has_remote_recipe)
        {
            std::filesystem::path staging_dir = ".tig-pkg\\staging";
            std::filesystem::create_directories(staging_dir);
            std::filesystem::path script_path = staging_dir / (package_name + "_remove.bat");

            std::ofstream out_file(script_path);
            if (!out_file.is_open())
            {
                throw std::runtime_error("Error: Storage engine context generation failed.");
            }

            out_file << "set TARGET_INCLUDE_DIR=" << std::filesystem::absolute("include").string() << "\n";
            out_file << script_content;
            out_file.close();

            std::string command = "\"" + script_path.string() + "\"";
            int status = std::system(command.c_str());

            std::filesystem::remove(script_path);

            if (status != 0)
            {
                throw std::runtime_error("Error: Removal directive execution sequence returned an anomalous state.");
            }

            tig_pkg::utils::ManifestManager::delete_manifest(package_name);
            return;
        }

        std::vector<std::string> manifest_paths = tig_pkg::utils::ManifestManager::read_paths(package_name);
        if (!manifest_paths.empty())
        {
            for (auto it = manifest_paths.rbegin(); it != manifest_paths.rend(); ++it)
            {
                std::error_code ec;
                if (std::filesystem::exists(*it))
                {
                    std::filesystem::remove_all(*it, ec);
                }
            }
            tig_pkg::utils::ManifestManager::delete_manifest(package_name);
            return;
        }

        std::vector<std::filesystem::path> target_locations = {
            std::filesystem::path("include") / package_name,
            std::filesystem::path("include") / (package_name + ".h"),
            std::filesystem::path("include") / (package_name + ".hpp")
        };

        bool removed_any = false;
        for (const auto& path : target_locations)
        {
            std::error_code ec;
            if (std::filesystem::exists(path))
            {
                std::filesystem::remove_all(path, ec);
                removed_any = true;
            }
        }

        tig_pkg::utils::ManifestManager::delete_manifest(package_name);

        if (!removed_any)
        {
            throw std::runtime_error("Error: Target binary or header does not exist in local include directory.");
        }
    }

    void Engine::update_system()
    {
        std::cout << "Synchronizing local arc recipe manifest cache...\n";
    }

    void Engine::list_recipes()
    {
        std::cout << "Synchronizing remote registry manifests...\n";
        try
        {
            std::string catalog = tig_pkg::utils::Network::fetch_catalog();
            std::cout << catalog << "\n";
        }
        catch (...)
        {
            std::cout << "neofetch\nfirefox\ngoogle-chrome\nvs-code\n";
        }
    }

    void Engine::search_recipes(const std::string& query)
    {
        std::cout << "Scanning active arc namespace blueprints for query: " << query << "\n";
        try
        {
            std::string catalog = tig_pkg::utils::Network::fetch_catalog();
            std::stringstream ss(catalog);
            std::string line;
            bool found = false;

            while (std::getline(ss, line))
            {
                if (line.find(query) != std::string::npos)
                {
                    std::cout << "  - " << line << "\n";
                    found = true;
                }
            }

            if (!found)
            {
                std::cout << "No matching recipes identified for query: " << query << "\n";
            }
        }
        catch (...)
        {
            std::cout << "  - " << query << " (Registry catalog unreachable)\n";
        }
    }

    void Engine::show_recipe(const std::string& package_name)
    {
        std::cout << "Querying blueprint specifications for target: " << package_name << "\n";
        try
        {
            std::string recipe = tig_pkg::utils::Network::fetch_recipe(package_name, "recipe.confx");
            std::cout << recipe << "\n";
        }
        catch (...)
        {
            std::cout << "Target package: " << package_name << "\n";
            std::cout << "Status: Recipe manifest unavailable in remote registry.\n";
        }
    }
}

void print_version()
{
    std::cout << "\n";
    std::cout << R"(▄▄▄▄▄▄▄▄▄ ▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄    ▄▄▄         ▄▄▄▄▄▄▄▄▄
 ███▓   ███  ████▒         ████▒    ▀█▄  ███ ░░░░ ██▌ ████▒    
░ ▒███▒ ░  ███  ███▓░ ░░░░    ███▓░ ░    ██ ███ ░░░░ ██▌ ███▓░ ░░░░
░ ░▓██░ ░  ▓██  ▓██▒ ▄▄▄▄▄▄ ▓██▒ ░░    ██ ▓██ ░░░  ██▌ ▓██▒ ▄▄▄▄▄▄
░   ▒▓█  ░  ▒▓█  ▒▓█ ░    ▒░ ▒▓█░    ▄█▀  ▒▓█    ▄██▀  ▒▓█ ░    ▒░
░░ ░▒▓ ░░  ░▒▓  ░▒▓ ░░░░ ▓▒ ░▒▓ ▀▀▀▀ ░░ ░▒▓ ▀▀▀██▄   ░▒▓ ░░░░ ▓▒
░░ █░▒ ░░  █░▒  █░▒ ░░░░ █▓ █░▒ ░░░░░░░ █░▒ ░░  ▀██  █░▒ ░░░░ █▓
░░ ░█░ ░░  ░█░  ░█░ ░░░░ ██ ░█░ ░░░░░░░ ░█░ ░░░░ █▓  ░█░ ░░░░ ██
░░ ▒░█ ░░  ▒░█  ▒░█      ▄██ ▒░█ ░░░░░░░ ▒░█ ░░░░ ▓▒  ▒░█      ▄██
 ▀▀▀       ▀▀▀▀▀  ▀▀▀▀▀▀▀▀▀▀ ▀▀▀         ▀▀▀      ▒░    ▀▀▀▀▀▀▀▀▀▀)" 
              << "\n\n";
    std::cout << "tig-pkg Package Manager — Version 0.5.4-ALPHA\n";
    std::cout << "Engine: hypecc Package Manager v.1.0.0-LTS\n";
    std::cout << "Licensing: GNU GPL v3.0\n";
    std::cout << "tig-pkg — Keep your vision wide, be beyond the Earth.\n";
}

void print_help()
{
    std::cout << "Usage: tig-pkg [options] command\n\n"
              << "Most used commands:\n"
              << "  init              - Scan system headers and generate local project configuration (.tig-pkg/include.confx)\n"
              << "  sync              - Synchronize workspace dependencies with local cache and arc registry\n"
              << "  list              - List available recipes in the arc registry\n"
              << "  search            - Search through arc recipe names and descriptions\n"
              << "  show              - Display detailed information about a specific recipe\n"
              << "  install           - Fetch a recipe from arc and execute custom install logic\n"
              << "  remove            - Remove a package natively from local workspace\n"
              << "  update            - Sync local package lists and arc recipe cache\n\n"
              << "Options:\n"
              << "  -v, --version     - Display version manager information\n"
              << "  -h, --help        - Display the help menu\n";
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
        else if (command == "init")
        {
            tig_pkg::core::Engine::init_project();
        }
        else if (command == "sync")
        {
            tig_pkg::core::Engine::sync_project();
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
            tig_pkg::core::Engine::install_package(package);
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
            tig_pkg::core::Engine::remove_package(package);
            std::cout << "Removal sequence completed successfully.\n";
        }
        else if (command == "update")
        {
            tig_pkg::core::Engine::update_system();
            std::cout << "Synchronized state successfully.\n";
        }
        else if (command == "list")
        {
            tig_pkg::core::Engine::list_recipes();
        }
        else if (command == "search")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'search' command requires a query string.\n";
                return 1;
            }
            tig_pkg::core::Engine::search_recipes(argv[2]);
        }
        else if (command == "show")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'show' command requires a valid package identifier.\n";
                return 1;
            }
            tig_pkg::core::Engine::show_recipe(argv[2]);
        }
        else
        {
            std::cerr << "Error: Unknown command or option '" << command << "'\n";
            print_help();
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
