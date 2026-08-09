#ifndef CONFX_PARSER_HPP
#define CONFX_PARSER_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct ConfxData
{
  std::vector<std::string> raw_lines;
  std::unordered_map<std::string, std::string> key_values;
};

class ConfxParser
{
public:
  static ConfxData parse(const std::string &filepath)
  {
    ConfxData data;
    std::ifstream file(filepath);

    if (!file.is_open())
    {
      std::cerr << "[hypecc] Error: Could not open " << filepath << "\n";
      return data;
    }

    std::string line;
    while (std::getline(file, line))
    {
      if (line.empty() || line[0] == '#')
        continue;

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
};

#endif

