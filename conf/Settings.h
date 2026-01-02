#pragma once

#include "istring.h"

#include <algorithm>
#include <charconv>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class Configuration;

class Settings {
public:
	class property {
	protected:
		const char* const                           config_key;
		std::vector<std::function<void(property*)>> on_change_callbacks;
		bool                                        synced_to_config = false;

		bool config_write(const char* s, bool writeback);
		bool config_read(std::string& s, const std::string& defaultvalue);
		bool config_write(const std::string& s, bool writeback);

	private:
		property(const property&)            = delete;
		property& operator=(const property&) = delete;

	public:
		property(const char* key, std::set<property*>* set) : config_key(key) {
			{
				if (set) {
					set->insert(this);
				}
			}
		}

		virtual ~property() = default;

		virtual void save(bool write_out = false) = 0;
		virtual bool load()                       = 0;
		virtual bool dirty() const                = 0;

		bool get_synced_to_config() const {
			return synced_to_config;
		}

		static bool callback_equal(
				const std::function<void(property*)>& a,
				const std::function<void(property*)>& b) {
			return a.target_type() == b.target_type()
				   && a.target<void(property*)>()
							  == b.target<void(property*)>();
		}

		void add_on_change_callback(std::function<void(property*)> cb) {
			if (!cb) {
				return;
			}
			auto it = std::find_if(
					on_change_callbacks.begin(), on_change_callbacks.end(),
					[cb](auto& check) {
						return callback_equal(cb, check);
					});
			if (it == on_change_callbacks.end()) {
				on_change_callbacks.push_back(cb);
			}
		}

		void remove_on_change_callback(std::function<void(property*)> cb) {
			if (!cb) {
				return;
			}
			auto it = std::find_if(
					on_change_callbacks.begin(), on_change_callbacks.end(),
					[cb](auto& check) {
						return callback_equal(cb, check);
					});
			if (it != on_change_callbacks.end()) {
				on_change_callbacks.erase(it);
			}
		}

		virtual void on_change() {}

		void changed() {
			on_change();
			for (const auto& cb : on_change_callbacks) {
				cb(this);
			}
		}
	};

	template <typename T>
	class Tproperty : public property {
	protected:
		T value;
		T default_value;
		T saved_value;

	public:
		using valuetype = T;

		Tproperty(
				const char* key, const T& def_value,
				std::set<property*>* set = nullptr)
				: property(key, set), value(def_value),
				  default_value(def_value), saved_value(def_value) {}

		bool dirty() const final {
			return value != saved_value;
		}

		void set(const T& v) {
			if (value != v) {
				value = v;
				changed();
				synced_to_config = saved_value == value;
			}
		}

		const T& get() {
			if (!synced_to_config) {
				load();
			}
			return value;
		}

		void revert() {
			value            = saved_value;
			synced_to_config = true;
		}

		operator const T&() {
			return get();
		}

		T operator=(const T& v) {
			set(v);
			return v;
		}
	};

	template <typename T>
	class enum_like_property : public Tproperty<T> {
	protected:
		std::vector<std::pair<std::string, T>> values;
		bool                                   normalize;

	public:
		enum_like_property(
				const char* key, const T& def_value,
				const std::vector<std::pair<std::string, T>>& values,
				bool                 normalize_values = true,
				std::set<property*>* set              = nullptr)
				: Tproperty<T>(key, def_value, set), values(values),
				  normalize(normalize_values) {}

		enum_like_property(
				const char* key, const T& def_value,
				std::vector<std::pair<std::string, T>>&& values,
				bool                 normalize_values = true,
				std::set<property*>* set              = nullptr)
				: Tproperty<T>(key, def_value, set), values(std::move(values)),
				  normalize(normalize_values) {}

		virtual void save(bool write_out = false) override {
			// Find first matching string for current enum value
			for (const auto& [str, enum_val] : values) {
				if (enum_val == this->value) {
					if (this->config_write(std::string(str), write_out)) {
						this->saved_value      = this->value;
						this->synced_to_config = true;
					}
					return;
				}
			}
		}

		bool load() override {
			auto& value = this->value;

			std::string str_value;
			bool        need_save = !this->config_read(str_value, {});

			auto find_it = values.begin();
			for (; find_it != values.end(); ++find_it) {
				if (!Pentagram::strcasecmp(
							str_value.c_str(), find_it->first.c_str())) {
					break;
				}
			}

			if (find_it != values.end()) {
				value = find_it->second;
			} else {
				value     = this->default_value;
				need_save = true;
			}

			this->saved_value      = value;
			this->synced_to_config = true;
			if (normalize || need_save) {
				for (const auto& [str, enum_val] : values) {
					if (enum_val == value) {
						if (need_save || str != str_value) {
							if (this->config_write(std::string(str), false)) {
								this->saved_value      = value;
								this->synced_to_config = true;
							}

							return true;
						}
						// If we get here the value read from config was
						// normalized so we do nothing
						else if (!need_save) {
							break;
						}
					}
				}
			}
			return false;
		}

		using Tproperty<T>::operator=;
	};

	template <typename T = int>
	class int_property : public Tproperty<T> {
		T min_value;
		T max_value;

		bool config_read(T& i, T defaultvalue) {
			std::string str_value;
			if (!property::config_read(str_value, {})) {
				i = defaultvalue;
				return false;
			}
			if (std::from_chars(
						str_value.data(), str_value.data() + str_value.size(),
						i)
						.ec
				!= std::errc()) {
				i = defaultvalue;
				return false;
			}
			return true;
		}

		bool config_write(T i, bool writeback) {
			return property::config_write(std::to_string(i), writeback);
		}

	public:
		int_property(
				const char* key, T def_value, T min_value, T max_value,
				std::set<property*>* set = nullptr)
				: Tproperty<int>(key, def_value, set), min_value(min_value),
				  max_value(max_value) {}

		virtual void save(bool write_out = false) override {
			if (config_write(this->value, write_out)) {
				this->saved_value      = this->value;
				this->synced_to_config = true;
			}
		}

		bool load() override {
			auto& value     = this->value;
			bool  need_save = !config_read(value, this->default_value);
			if (value < min_value) {
				value     = min_value;
				need_save = true;
			} else if (value > max_value) {
				value     = max_value;
				need_save = true;
			}
			this->saved_value      = value;
			this->synced_to_config = true;
			if (need_save) {
				if (!config_write(value, false)) {
					this->synced_to_config = false;
				}
			}
			return need_save;
		}

		T get_min() const {
			return min_value;
		}

		T get_max() const {
			return max_value;
		}

		using Tproperty<T>::operator=;
	};

	class string_property : public Tproperty<std::string> {
	public:
		struct Insensitive_less {
			bool operator()(
					std::string_view left, std::string_view right) const {
				int result = Pentagram::strncasecmp(
						left.data(), right.data(),
						std::min(left.size(), right.size()));

				// same till the size of the shorter string
				if (result == 0) {
					// If left is smaller it is less
					if (left.size() < right.size()) {
						return true;
					}
					// Left is same length so equal or longer
					return false;
				}
				// Just use the string comparison result
				return result < 0;
			}

			using is_transparent = void;
		};

	protected:
		std::set<std::string, Insensitive_less> values;

	public:
		string_property(
				const char* key, const std::string& def_value = {},
				const std::set<std::string, Insensitive_less>& values = {},
				std::set<property*>*                           set    = nullptr)
				: Tproperty<std::string>(key, def_value, set), values(values) {}

		string_property(
				const char* key, const std::string& def_value = {},
				std::set<std::string, Insensitive_less>&& values = {},
				std::set<property*>*                      set    = nullptr)
				: Tproperty<std::string>(key, def_value, set),
				  values(std::move(values)) {}

		virtual void save(bool write_out = false) override {
			config_write(value, write_out);
			saved_value      = value;
			synced_to_config = true;
		}

		bool load() override {
			bool need_save = !config_read(value, default_value);
			if (values.size()) {
				auto it = values.find(value);

				if (it != values.end()) {
					// Found but case changed so use the proper form
					if (*it != value) {
						value     = *it;
						need_save = true;
					}
				}
				// Not in the set so set to default
				else {
					value     = default_value;
					need_save = true;
				}
			}
			if (need_save) {
				config_write(value, false);
			}
			saved_value      = value;
			synced_to_config = true;
			return need_save;
		}

		using Tproperty<std::string>::operator=;
	};

	// ReadOnly wrapper for properties that only allows the friend class to
	// modify them
	template <typename Base, typename Tfriend = void>
	class ReadOnly : private Base {
		friend Tfriend;

	public:
		using Base::Base;
		using Base::get;
		using typename Base::valuetype;
		using Base::operator const typename Base::valuetype &;

		// save does nothing
		virtual void save(bool) override {
		}

	private:
		void forceSave(bool write_out = false) {
			Base::save(write_out);
		}
		using Base::set;
		using Base::operator=;
	};

	static const std::vector<std::pair<std::string, bool>> boolyesnoany;
	static const std::vector<std::pair<std::string, bool>> booltruefalseany;
	static const std::vector<std::pair<std::string, bool>> boolonoffany;
	static Settings                                        instance;
	struct PropertySet;

private:
	std::set<PropertySet*> property_sets;

	Configuration* configuration;

public:
	struct PropertySet : protected std::set<property*> {
		PropertySet(Settings* settings) {
			settings->property_sets.insert(this);
		}

		using const_iterator = std::set<property*>::const_iterator;
		using const_reverse_iterator
				= std::set<property*>::const_reverse_iterator;

		const_iterator begin() const {
			return std::set<property*>::begin();
		}

		const_iterator end() const {
			return std::set<property*>::end();
		}

		const_reverse_iterator rbegin() const {
			return std::set<property*>::rbegin();
		}

		const_reverse_iterator rend() const {
			return std::set<property*>::rend();
		}

		void add(property* prop) {
			insert(prop);
		}

		void remove(property* prop) {
			erase(prop);
		}

		void save_all(bool write_out = false);

		bool load_all();

		void save_dirty(bool write_out = false);
	};

	// public setting property sets follow

	// Disk settings
	struct Disk : public PropertySet {
		Disk(Settings* settings) : PropertySet(settings) {}

		int_property<> save_compression_level
				= {"config/disk/save_compression_level", 1, 0, 2, this};

		int_property<> autosave_count
				= {"config/disk/autosave_count", 5, 0,
				   std::numeric_limits<int>::max(), this};

		int_property<> flagautosave_count
				= {"config/disk/globalflag_autosave_count", 5, 0,
				   std::numeric_limits<int>::max(), this};

		int_property<> quicksave_count
				= {"config/disk/quicksave_count", 5, 1,
				   std::numeric_limits<int>::max(), this};

		ReadOnly<enum_like_property<bool>> use_old_style_save_load
				= {"config/disk/use_old_style_save_load", false,
				   Settings::boolyesnoany, true, this};

		ReadOnly<int_property<>> autosave_limit_time
				= {"config/disk/autosave_limit_time", 60, 0,
				   std::numeric_limits<int>::max(), this};

		enum SaveGame_sort {
			SORTBY_REALTIME = 0,
			SORTBY_NAME     = 1,
			SORTBY_GAMETIME = 2
		};

		enum_like_property<int> savegame_sort_by = {
				"config/disk/savegame_sort_by",
				SORTBY_REALTIME,
				{{"realtime", SORTBY_REALTIME},
				  {"time", SORTBY_REALTIME},
				  {"savename", SORTBY_NAME},
				  {"name", SORTBY_NAME},
				  {"gametime", SORTBY_GAMETIME},
				  {"ingametime", SORTBY_GAMETIME}},
				true,
				this
        };

		enum_like_property<bool> savegame_group_by_type
				= {"config/disk/savegame_group_by_type", false,
				   Settings::boolyesnoany, true, this};

		enum_like_property<bool> autosaves_write_to_gamedat
				= {"config/disk/autosaves_write_to_gamedat", false,
				   Settings::boolyesnoany, true, this};
	} disk{this};

	static Settings& get() {
		return instance;
	}

	void load_all(Configuration*);

	void save_all(bool write_out = false);

	void save_dirty(bool write_out = false);
};