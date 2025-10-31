#include "Settings.h"

#include "Configuration.h"

const std::vector<std::pair<std::string, bool>> Settings::boolyesnoany = {
		{  "yes",  true},
        {   "no", false},
        { "true",  true},
        {"false", false},
		{    "1",  true},
        {    "0", false},
        {   "on",  true},
        {  "off", false}
};
const std::vector<std::pair<std::string, bool>> Settings::booltruefalseany = {
		{ "true",  true},
        {"false", false},
        {  "yes",  true},
        {   "no", false},
		{    "1",  true},
        {    "0", false},
        {   "on",  true},
        {  "off", false}
};
const std::vector<std::pair<std::string, bool>> Settings::boolonoffany = {
		{   "on",  true},
        {  "off", false},
        {  "yes",  true},
        {   "no", false},
		{ "true",  true},
        {"false", false},
        {    "1",  true},
        {    "0", false},
};
Settings Settings::instance;

bool Settings::property::config_write(const char* s, bool writeback) {
	if (!instance.configuration) {
		return false;
	}
	instance.configuration->set(config_key, s, writeback);
	return true;
}

bool Settings::property::config_read(
		std::string& s, const std::string& defaultvalue) {
	if (!instance.configuration) {
		s = defaultvalue;
		return false;
	}
	return instance.configuration->value(config_key, s, defaultvalue);
}

bool Settings::property::config_write(const std::string& s, bool writeback) {
	if (!instance.configuration) {
		return false;
	}
	instance.configuration->set(config_key, s, writeback);
	return true;
}

void Settings::save_all(bool write_out) {
	for (auto* set : property_sets) {
		set->save_all(false);
	}
	if (write_out && configuration) {
		instance.configuration->write_back();
	}
}

inline void Settings::save_dirty(bool write_out) {
	for (auto* set : property_sets) {
		set->save_dirty(false);
	}
	if (write_out && configuration) {
		configuration->write_back();
	}
}

void Settings::load_all(Configuration* config) {
	this->configuration = config;
	if (configuration) {
		bool write_out = false;
		for (auto* set : property_sets) {
			write_out |= set->load_all();
		}

		if (write_out) {
			instance.configuration->write_back();
		}
	}
}

void Settings::PropertySet::save_all(bool write_out) {
	for (auto* prop : *this) {
		prop->save(false);
	}
	if (write_out) {
		instance.configuration->write_back();
	}
}

bool Settings::PropertySet::load_all() {
	bool write_out = false;
	for (auto* prop : *this) {
		if (!prop->get_synced_to_config()) {
			write_out |= prop->load();
		}
	}
	return write_out;
}

void Settings::PropertySet::save_dirty(bool write_out) {
	for (auto* prop : *this) {
		if (prop->dirty()) {
			prop->save(false);
		}
	}
	if (write_out) {
		instance.configuration->write_back();
	}
}
