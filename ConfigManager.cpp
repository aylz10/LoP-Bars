#include "ConfigManager.hpp"

ConfigManager::ConfigManager() : configMutex(), configData()
{
	WCHAR tmp[MAX_PATH] = { 0 };
	if (GetModuleFileNameW(0, tmp, MAX_PATH) == 0)
	{
		wsFilePath = CONFIG_FILE_NAME;
	}
	else
	{
		wsFilePath = std::wstring(tmp);
		auto pos = wsFilePath.find_last_of('\\');
		wsFilePath = wsFilePath.substr(0, pos + 1) + CONFIG_FILE_NAME;
	}
}


ConfigManager::~ConfigManager()
{
	if (configFile.is_open())
		configFile.close();
}

void ConfigManager::LoadData()
{
	std::scoped_lock _{ configMutex };

	//
	keyOrder.clear();

	configFile.open(wsFilePath, std::ios::in);

	if (!configFile)
		return;

	std::string line, key, value;

	while (std::getline(configFile, line))
	{
		if (line.empty())
			continue;

		auto pos = line.find('=');
		if (pos == std::string::npos || pos == 0 || pos == line.size() - 1)
			continue;

		key = line.substr(0, pos);
		key.erase(std::remove(key.begin(), key.end(), ' '), key.end());

		if (key.empty())
			continue;

		value = line.substr(pos + 1, line.size() - pos - 1);

		//
		keyOrder.push_back(key);  
		configData[key] = value;
	}

	configFile.close();
}

void ConfigManager::SaveData()
{
	std::scoped_lock _{ configMutex };

	configFile.open(wsFilePath, std::ios::out | std::ios::trunc);

	if (!configFile)
		return;

	//
	for (const auto& key : keyOrder) 
	{
		auto it = configData.find(key);
		if (it != configData.end()) {
			configFile << it->first << '=' << it->second << std::endl;
		}
	}

	configFile.close();
}

std::optional<std::string> ConfigManager::Get(const std::string& key, bool loadAndGet)
{
	std::scoped_lock _{ configMutex };

	if (loadAndGet)
		LoadData();

	auto i = configData.find(key);

	if (i == configData.end())
		return {};

	return i->second;
}

void ConfigManager::Set(const std::string& key, const std::string& value, bool setAndSave)
{
	std::scoped_lock _{ configMutex };

	//
	if (configData.find(key) == configData.end()) {
		keyOrder.push_back(key);
		configData[key] = value;
	}

	if (setAndSave)
		SaveData();
}

void ConfigManager::Del(const std::string& key, bool setAndSave)
{
	std::scoped_lock _{ configMutex };

	//
	auto it = configData.find(key);
	if (it != configData.end()) {
		configData.erase(it);
	}

	//
	auto vecIt = std::find(keyOrder.begin(), keyOrder.end(), key);
	if (vecIt != keyOrder.end()) {
		keyOrder.erase(vecIt);
	}

	if (setAndSave)
		SaveData();
}
