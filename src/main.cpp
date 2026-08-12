#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

class ManifestManager
{
public:
    static std::string getManifestPath(const std::string& packageName)
    {
        return "/var/lib/signature/manifests/" + packageName + ".list";
    }

    static bool recordPath(const std::string& packageName, const std::string& targetPath)
    {
        std::error_code ec;
        fs::create_directories("/var/lib/signature/manifests", ec);
        std::ofstream file(getManifestPath(packageName), std::ios::app);
        if (!file.is_open())
        {
            return false;
        }
        file << targetPath << "\n";
        return true;
    }

    static std::vector<std::string> readPaths(const std::string& packageName)
    {
        std::vector<std::string> paths;
        std::ifstream file(getManifestPath(packageName));
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

    static void deleteManifest(const std::string& packageName)
    {
        std::error_code ec;
        fs::remove(getManifestPath(packageName), ec);
    }
};

class PackageUninstaller
{
public:
    static bool removePackage(const std::string& packageName)
    {
        std::string removeScript = "./remove.sh";
        if (fs::exists(removeScript))
        {
            int res = std::system(("bash " + removeScript).c_str());
            return (res == 0);
        }

        std::vector<std::string> manifestPaths = ManifestManager::readPaths(packageName);
        if (!manifestPaths.empty())
        {
            for (auto it = manifestPaths.rbegin(); it != manifestPaths.rend(); ++it)
            {
                std::error_code ec;
                if (fs::exists(*it))
                {
                    fs::remove_all(*it, ec);
                }
            }
            ManifestManager::deleteManifest(packageName);
            return true;
        }

        return executeFallback(packageName);
    }

private:
    static bool executeFallback(const std::string& packageName)
    {
        std::vector<fs::path> targetLocations = {
            "/usr/local/bin/" + packageName,
            "/usr/local/include/" + packageName,
            "/usr/local/include/" + packageName + ".h",
            "/usr/local/lib/lib" + packageName + ".a",
            "/usr/local/lib/lib" + packageName + ".so"
        };

        bool removedAny = false;
        for (const auto& path : targetLocations)
        {
            std::error_code ec;
            if (fs::exists(path))
            {
                fs::remove_all(path, ec);
                removedAny = true;
            }
        }
        return removedAny;
    }
};
