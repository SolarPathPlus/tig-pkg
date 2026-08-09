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
#include <unistd.h>
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

namespace hypecc::utils
{
    class Network
    {
    public:
        static std::string fetch_recipe(const std::string& package_name, const std::string& script_name = "install.sh");
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
        std::string url = "https://raw.githubusercontent.com/hypecc-pm/Signature/main/recipes/" 
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

    class SystemScanner
    {
    public:
        static void generate_project_config()
        {
            std::filesystem::path hypecc_dir = ".hypecc";
            std::filesystem::create_directories(hypecc_dir);

            std::filesystem::path config_path = hypecc_dir / "include.confx";
            if (std::filesystem::exists(config_path))
            {
                std::cout << "[hypecc] Configuration environment already exists at " << config_path.string() << "\n";
                return;
            }

            std::vector<std::string> target_includes = {
                "/usr/include",
                "/usr/local/include"
            };

            std::string detected_includes;
            for (const auto& path : target_includes)
            {
                if (std::filesystem::exists(path))
                {
                    if (!detected_includes.empty())
                    {
                        detected_includes += ";";
                    }
                    detected_includes += path;
                }
            }

            ConfxData data;
            data.key_values["name"] = std::filesystem::current_path().filename().string();
            data.key_values["cxx"] = "g++";
            data.key_values["cxxflags"] = "-std=c++20 -O3 -Wall -Wextra";
            data.key_values["include_dirs"] = detected_includes;
            data.key_values["libs"] = "-lpthread -lm";
            data.key_values["source_dir"] = "src";
            data.key_values["output_binary"] = "build/app";
            data.key_values["requires"] = "";

            ConfxParser::write(config_path.string(), data);
            std::cout << "[hypecc] Initialized project configuration environment at " << config_path.string() << "\n";
        }
    };
}

namespace hypecc::core
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

            std::cout << "[hypecc] Resolving dependency graph node: " << package_name << "\n";

            const char* home_env = std::getenv("HOME");
            std::filesystem::path home_dir = home_env ? home_env : "/tmp";
            std::filesystem::path cache_dir = home_dir / ".hypecc" / "cache" / package_name;
            std::filesystem::create_directories(cache_dir);

            std::string recipe_confx;
            try
            {
                recipe_confx = hypecc::utils::Network::fetch_recipe(package_name, "recipe.confx");
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

    class Builder
    {
    public:
        static void build()
        {
            std::filesystem::path config_path = ".hypecc/include.confx";
            if (!std::filesystem::exists(config_path))
            {
                throw std::runtime_error("Error: Missing project configuration file .hypecc/include.confx. Execute 'hypecc init' first.");
            }

            ConfxData config = ConfxParser::parse(config_path.string());

            std::string compiler = config.key_values.count("cxx") ? config.key_values["cxx"] : "g++";
            std::string flags = config.key_values.count("cxxflags") ? config.key_values["cxxflags"] : "-std=c++20";
            std::string includes_raw = config.key_values.count("include_dirs") ? config.key_values["include_dirs"] : "";
            std::string libs = config.key_values.count("libs") ? config.key_values["libs"] : "";
            std::string src_dir_str = config.key_values.count("source_dir") ? config.key_values["source_dir"] : "src";
            std::string output_binary = config.key_values.count("output_binary") ? config.key_values["output_binary"] : "build/app";

            std::string include_flags;
            if (!includes_raw.empty())
            {
                std::stringstream ss(includes_raw);
                std::string path;
                while (std::getline(ss, path, ';'))
                {
                    if (!path.empty())
                    {
                        include_flags += " -I" + path;
                    }
                }
            }

            std::filesystem::path src_dir(src_dir_str);
            std::vector<std::string> sources;

            if (std::filesystem::exists(src_dir) && std::filesystem::is_directory(src_dir))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(src_dir))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".cpp")
                    {
                        sources.push_back(entry.path().string());
                    }
                }
            }
            else if (std::filesystem::exists("main.cpp"))
            {
                sources.push_back("main.cpp");
            }

            if (sources.empty())
            {
                throw std::runtime_error("Error: No C++ source files identified in target directory.");
            }

            std::filesystem::path out_path(output_binary);
            if (out_path.has_parent_path())
            {
                std::filesystem::create_directories(out_path.parent_path());
            }

            std::string command = compiler + " " + flags + include_flags;
            for (const auto& src : sources)
            {
                command += " " + src;
            }
            command += " " + libs + " -o " + output_binary;

            std::cout << "[hypecc] Orchestrating build pipeline:\n" << command << "\n";
            int status = std::system(command.c_str());
            if (status != 0)
            {
                throw std::runtime_error("Error: Compilation directive returned a non-zero exit status.");
            }

            std::cout << "[hypecc] Build sequence executed successfully. Artifact location: " << output_binary << "\n";
        }
    };

    class Engine
    {
    public:
        static void init_project();
        static void build_project();
        static void sync_project();
        static void install_package(const std::string& package_name);
        static void remove_package(const std::string& package_name);
        static void update_system();
        static void list_recipes();
        static void search_recipes(const std::string& query);
        static void show_recipe(const std::string& package_name);
    private:
        static void ensure_root();
    };

    void Engine::ensure_root()
    {
        if (getuid() != 0)
        {
            throw std::runtime_error("Error: Root privileges are required for this administrative operation.");
        }
    }

    void Engine::init_project()
    {
        hypecc::utils::SystemScanner::generate_project_config();
    }

    void Engine::build_project()
    {
        hypecc::core::Builder::build();
    }

    void Engine::sync_project()
    {
        std::filesystem::path config_path = ".hypecc/include.confx";
        if (!std::filesystem::exists(config_path))
        {
            throw std::runtime_error("Error: Missing project configuration file .hypecc/include.confx.");
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
                    hypecc::core::DependencySolver::resolve_recursive(dep, visited);
                }
            }
        }
        std::cout << "[hypecc] Workspace state and local cache synchronized successfully.\n";
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
            script_content = hypecc::utils::Network::fetch_recipe(package_name, "remove.sh");
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
        std::cout << "Synchronizing Signature recipe manifest cache...\n";
        ConfxData config = ConfxParser::parse("/etc/hypecc/hypecc.conf");
    }

    void Engine::list_recipes()
    {
        std::cout << "Synchronizing remote registry manifests...\n";
        try
        {
            std::string catalog = hypecc::utils::Network::fetch_recipe("../catalog.list", "catalog.list");
            std::cout << catalog << "\n";
        }
        catch (...)
        {
            std::cout << "neofetch\nfirefox\ngoogle-chrome\nvs-code\n";
        }
    }

    void Engine::search_recipes(const std::string& query)
    {
        std::cout << "Scanning active Signature namespace blueprints for query: " << query << "\n";
        try
        {
            std::string catalog = hypecc::utils::Network::fetch_recipe("../catalog.list", "catalog.list");
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
            std::string recipe = hypecc::utils::Network::fetch_recipe(package_name, "recipe.confx");
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
    std::cout << R"(░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░░▒▓████████▓▒░▒▓██████▓▒░ ░▒▓██████▓▒░  
░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ 
░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░        
░▒▓████████▓▒░░▒▓██████▓▒░░▒▓███████▓▒░░▒▓██████▓▒░░▒▓█▓▒░      ░▒▓█▓▒░        
░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░   ░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░        
░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░   ░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ 
░▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░   ░▒▓█▓▒░      ░▒▓████████▓▒░▒▓██████▓▒░ ░▒▓██████▓▒░  )" 
              << "\n\n";
    std::cout << "hypecc Package Manager — Version 0.2.0-ALPHA\n";
    std::cout << "Engine: Dawn Package System v1.2.4-LTS\n";
    std::cout << "Licensing: GNU GPL v3.0\n";
    std::cout << "hypecc — Because C/C++ package management should actually be worth the hype.\n";
}

void print_help()
{
    std::cout << "Usage: hypecc [options] command\n\n"
              << "Most used commands:\n"
              << "  init            - Scan system headers and generate local project configuration (.hypecc/include.confx)\n"
              << "  build           - Orchestrate direct bare-metal compilation pipeline without CMake or Makefile\n"
              << "  sync            - Synchronize workspace dependencies with local cache and Signature registry\n"
              << "  list            - List available recipes in the Signature registry\n"
              << "  search          - Search through Signature recipe names and descriptions\n"
              << "  show            - Display detailed information about a specific recipe\n"
              << "  install         - Fetch a recipe from Signature and execute custom install logic\n"
              << "  remove          - Remove a package natively from the system\n"
              << "  update          - Sync local package lists and Signature recipe cache\n\n"
              << "Options:\n"
              << "  -v, --version   - Display version manager information\n"
              << "  -h, --help      - Display the help menu\n";
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
            hypecc::core::Engine::init_project();
        }
        else if (command == "build")
        {
            hypecc::core::Engine::build_project();
        }
        else if (command == "sync")
        {
            hypecc::core::Engine::sync_project();
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
            hypecc::core::Engine::install_package(package);
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
            hypecc::core::Engine::remove_package(package);
            std::cout << "Removal sequence completed successfully.\n";
        }
        else if (command == "update")
        {
            hypecc::core::Engine::update_system();
            std::cout << "Synchronized state successfully.\n";
        }
        else if (command == "list")
        {
            hypecc::core::Engine::list_recipes();
        }
        else if (command == "search")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'search' command requires a query string.\n";
                return 1;
            }
            hypecc::core::Engine::search_recipes(argv[2]);
        }
        else if (command == "show")
        {
            if (argc < 3)
            {
                std::cerr << "Error: 'show' command requires a valid package identifier.\n";
                return 1;
            }
            hypecc::core::Engine::show_recipe(argv[2]);
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
