#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>

namespace hypecc::utils
{
    class Network
    {
    public:
        static std::string fetch_recipe(const std::string& package_name, const std::string& script_name = "install.sh");
    };
}

#endif
