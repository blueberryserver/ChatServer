#include "ConfigManager.h"
#include <fstream>
#include <iostream>
#include <sstream>

//using json = nlohmann::json;

namespace config {

// 👇 명시적 인스턴스화
template int ConfigManager::get<int>(const std::string&, const int&) const;
template std::string ConfigManager::get<std::string>(const std::string&, const std::string&) const;
template bool ConfigManager::get<bool>(const std::string&, const bool&) const;

} // namespace config