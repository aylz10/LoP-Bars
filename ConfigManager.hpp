#pragma once

#include <unordered_map>
#include <string>
#include <fstream>
#include <mutex>
#include <optional>
#include <windows.h>

#define CONFIG_FILE_NAME L"lop_bars.cfg"

class ConfigManager
{
public:
	static ConfigManager& GetInstance()
	{
		static ConfigManager instance;
		return instance;
	}
	virtual ~ConfigManager();

	ConfigManager(const ConfigManager& other) = delete;
	ConfigManager(ConfigManager&& other) = delete;
	ConfigManager& operator=(const ConfigManager& other) = delete;
	ConfigManager& operator=(const ConfigManager&& other) = delete;

	void LoadData();
	void SaveData();

	// Getters and Setters

	template <typename T>
	std::optional<T> Get(const std::string& key, bool loadAndGet = false)
	{
		auto value = Get(key, loadAndGet);
		if (!value)
			return {};

		if constexpr (std::is_integral_v<T>)
		{
			//if constexpr (std::is_unsigned_v<T>)
			//	return (T)std::stoul(*value);

			return (T)std::stoi(*value);
		}

		return (T)std::stof(*value);
	}

	template <typename T>
	void Set(const std::string& key, const T& value, bool setAndSave = false)
	{
		Set(key, std::to_string(value), setAndSave);
	}

	std::optional<std::string> Get(const std::string& key, bool loadAndGet = false);
	void Set(const std::string& key, const std::string& value, bool setAndSave = false);
	void Del(const std::string& key, bool setAndSave = false);
private:
	std::wstring wsFilePath;
	std::recursive_mutex configMutex;
	std::fstream configFile;
	std::unordered_map<std::string, std::string> configData;
	//
	std::vector<std::string> keyOrder;
	ConfigManager();
};
