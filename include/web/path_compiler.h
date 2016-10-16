#pragma once

// https://github.com/pillarjs/path-to-regexp/blob/master/index.js

#include <string>
#include <regex>

namespace web {
	enum {
		KEY_IS_STRING = 0x0001,
		KEY_ASTERISK = 0x0004,
		KEY_OPTIONAL = 0x0008,
		KEY_REPEAT = 0x0010,
		KEY_PARTIAL = 0x0020,
	};

	enum {
		COMPILE_STRICT = 0x0001,
		COMPILE_END = 0x0002,
		COMPILE_SENSITIVE = 0x0004,
		COMPILE_OPTIMIZE = 0x0008,
		COMPILE_DEFAULT = /*COMPILE_STRICT | */COMPILE_END | COMPILE_SENSITIVE | COMPILE_OPTIMIZE
	};

	struct key_type {
		int         flags;
		size_t      nvalue;
		std::string svalue;
		std::string prefix;
		std::string delimiter;
		std::string pattern;

		static key_type as_string(std::string&& value)
		{
			return { KEY_IS_STRING, 0, std::move(value) };
		}

		key_type set_flags(bool asterisk, bool optional, bool repeat, bool partial)
		{
			if (asterisk)
				flags |= KEY_ASTERISK;
			if (optional)
				flags |= KEY_OPTIONAL;
			if (repeat)
				flags |= KEY_REPEAT;
			if (partial)
				flags |= KEY_PARTIAL;
			return *this;
		}
		static key_type make(std::string&& value, std::string&& prefix, std::string&& delimiter, std::string&& pattern)
		{
			return { 0, 0, value, prefix, delimiter, pattern };
		}
		static key_type make(size_t value, std::string&& prefix, std::string&& delimiter, std::string&& pattern)
		{
			return { 0, value, { }, prefix, delimiter, pattern };
		}
	};

	struct description {
		std::string route;
		std::vector<key_type> keys;

		static description make(std::vector<key_type>&& tokens, int options = COMPILE_DEFAULT);
		static description make(const std::string& mask, int options = COMPILE_DEFAULT);
	};

	struct param {
		std::string sname;
		size_t nname;
		std::string value;
	};

	struct matcher {
		std::regex regex;
		const std::vector<key_type> keys;

		static matcher make(description&& tokens, int options = COMPILE_DEFAULT);
		static matcher make(const std::string& mask, int options = COMPILE_DEFAULT);

		bool matches(const std::string& route, std::vector<param>& params) const;
	};
}