#pragma once

#include <string>

namespace curl
{

bool urlValidate(const std::string &url, bool allowMissingScheme = false);

bool urlReplacePath(std::string &url, const std::string &path, bool overwriteExisting = true);

}
