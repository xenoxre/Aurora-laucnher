#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

namespace aurora {

struct Instance {
    std::wstring id;
    std::wstring name;
    std::wstring mcVersion;
    std::wstring loader;       // none | fabric | quilt
    std::wstring loaderVersion;
    std::wstring versionId;    // version profile to launch
    std::filesystem::path dir;
};

bool loadInstance(const Paths& paths, const std::wstring& id, Instance& out);
bool scanInstances(const Paths& paths, std::vector<Instance>& out);
bool createInstance(const Paths& paths, const Instance& inst, std::wstring& error);
bool deleteInstance(const Paths& paths, const std::wstring& id, std::wstring& error);
Instance* findInstance(std::vector<Instance>& list, const std::wstring& id);
const Instance* findInstance(const std::vector<Instance>& list, const std::wstring& id);

} // namespace aurora
