/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/path_compiler.h>
#include <iostream>

namespace web {
	inline static bool isString(char c)
	{
		// ".+*?=^!:${}()[]|/\"
		switch (c) {
		case '.': case '+': case '*': case '?': case '=': case '^': case '!':
		case '$': case '{': case '}': case '(': case ')': case '[': case ']':
		case ':': case '|': case '/': case '\\':
			return true;
		default:
			break;
		};
		return false;
	}

	template <bool(*checker)(char)>
	inline static std::string escape(const std::string& s)
	{
		size_t length = s.length();
		for (auto& c : s) {
			if (checker(c))
				++length;
		}

		std::string out;
		out.reserve(length + 1);
		for (auto& c : s) {
			if (checker(c))
				out.push_back('\\');
			out.push_back(c);
		}

		return out;
	}

	inline static std::string escapeString(const std::string& s)
	{
		return escape<isString>(s);
	}

	inline static bool isGroup(char c)
	{
		// "=!:$/()"
		switch (c) {
		case '=': case '!': case ':': case '$':
		case '/': case '(': case ')':
			return true;
		default:
			break;
		};
		return false;
	}

	inline static std::string escapeGroup(const std::string& s)
	{
		return escape<isGroup>(s);
	}

	inline static std::vector<key_type> parse_matcher(const std::string& mask)
	{
#if 1
		static const std::regex path_regex {
			// Match escaped characters that would otherwise appear in future matches.
			// This allows the user to escape special characters that won't transform.
			"(\\\\.)"
			"|"
			// Match Express-style parameters and un-named parameters with a prefix
			// and optional suffixes. Matches appear as:
			//
			// "/:test(\\d+)?" => ["/", "test", "\d+", undefined, "?", undefined]
			// "/route(\\d+)"  => [undefined, undefined, undefined, "\d+", undefined, undefined]
			// "/*"            => ["/", undefined, undefined, undefined, undefined, "*"]
			"([\\/.])?(?:(?:\\:(\\w+)(?:\\(((?:\\\\.|[^\\\\()])+)\\))?|\\(((?:\\\\.|[^\\\\()])+)\\))([+*?])?|(\\*))"
		};
#else
		static const std::regex path_regex {
			"((\\\\.)|(([\\/.])?(?:(?:\\:(\\w+)(?:\\(((?:\\\\.|[^\\\\()])+)\\))?|\\(((?:\\\\.|[^\\\\()])+)\\))([+*?])?|(\\*))))"
		};
#endif

		std::vector<key_type> out;

		std::string path;
		size_t key = 0;
		size_t index = 0;

		auto it = std::sregex_iterator { mask.begin(), mask.end(), path_regex };
		auto end = std::sregex_iterator { };
		for (; it != end; ++it) {
			auto& res = *it;
			std::string m = res[0];
			std::string escaped = res[1];
			int offset = res.position();
			path += mask.substr(index, offset - index);
			index = offset + res[0].length();

			// Ignore already escaped sequences.
			if (!escaped.empty()) {
				path += escaped[1];
				continue;
			}

			auto next = index < mask.length() ? std::string { 1, mask[index] } : std::string { };
			auto prefix = std::string { res[2] };
			auto name = std::string { res[3] };
			auto capture = std::string { res[4] };
			auto group = std::string { res[5] };
			auto modifier = std::string { res[6] };
			auto asterisk = std::string { res[7] };
			auto b_asterisk = asterisk == "*";

			// Push the current path onto the tokens.
			if (!path.empty())
				out.push_back(key_type::as_string(std::move(path)));

			auto partial = !prefix.empty() && !next.empty() && next != prefix;
			auto repeat = modifier == "+" || modifier == "*";
			auto optional = modifier == "?" || modifier == "*";
			auto delimiter = prefix.empty() ? "/" : prefix;
			auto pattern = !capture.empty() ? capture : group; // JS: capture || group
			if (!pattern.empty()) {
				pattern = escapeGroup(pattern);
			} else if (b_asterisk) {
				pattern = ".*";
			} else {
				pattern = "[^" + escapeString(delimiter) + "]+?";
			}

			if (name.empty()) {
				out.push_back(std::move(
					key_type::make(key++, std::move(prefix), std::move(delimiter), std::move(pattern))
					.set_flags(b_asterisk, optional, repeat, partial)
				));
			} else {
				out.push_back(std::move(
					key_type::make(std::move(name), std::move(prefix), std::move(delimiter), std::move(pattern))
					.set_flags(b_asterisk, optional, repeat, partial)
				));
			}
		}

		// Match any characters still remaining.
		if (index < mask.length())
			path += mask.substr(index);

		if (!path.empty())
			out.push_back(key_type::as_string(std::move(path)));

		return out;
	}

	description description::make(std::vector<key_type>&& tokens, int options)
	{
		auto strict = (options & COMPILE_STRICT) == COMPILE_STRICT;
		auto end = (options & COMPILE_END) == COMPILE_END;

		std::string route;
		std::vector<key_type> keys;

		auto endsWithSlash =
			!tokens.empty() &&
			(tokens.back().flags & KEY_IS_STRING) &&
			!tokens.back().svalue.empty() &&
			(tokens.back().svalue.back() == '/');

		for (auto& token : tokens) {
			if (token.flags & KEY_IS_STRING) {
				route += escapeString(token.svalue);
				continue;
			}

			auto prefix = escapeString(token.prefix);
			auto capture = "(?:" + token.pattern + ")";

			if (token.flags & KEY_REPEAT)
				capture += "(?:" + prefix + capture + ")*";

			if (token.flags & KEY_OPTIONAL) {
				if (token.flags & KEY_PARTIAL)
					capture = prefix + '(' + capture + ")?";
				else
					capture = "(?:" + prefix + '(' + capture + "))?";
			} else {
				capture = prefix + '(' + capture + ')';
			}

			route += capture;
			keys.push_back(token);
		}

		// In non-strict mode we allow a slash at the end of match. If the path to
		// match already ends with a slash, we remove it for consistency. The slash
		// is valid at the end of a path match, not in the middle. This is important
		// in non-ending mode, where "/test/" shouldn't match "/test//route".
		if (!strict) {
			if (endsWithSlash) {
				route.pop_back(); // '\'
				route.pop_back(); // '/'
			}
			route += "(?:\\/(?=$))?";
		}

		if (end) {
			route.push_back('$');
		} else if (!strict || !endsWithSlash) {
			// In non-ending mode, we need the capturing groups to match as much as
			// possible by using a positive lookahead to the end or next path segment.
			route += "(?=\\/|$)";
		}

		route = "^" + route;
		return { std::move(route), std::move(keys) };
	};

	description description::make(const std::string& mask, int options)
	{
		return make(parse_matcher(mask), options);
	}

	matcher matcher::make(description&& tokens, int options)
	{
		auto flags = std::regex_constants::ECMAScript;
		if ((options & COMPILE_SENSITIVE) != COMPILE_SENSITIVE)
			flags |= std::regex_constants::icase;
		if (options & COMPILE_OPTIMIZE)
			flags |= std::regex_constants::optimize;

		return { std::regex { tokens.route, flags }, std::move(tokens.keys) };
	}

	matcher matcher::make(const std::string& mask, int options)
	{
		return make(description::make(mask, options), options);
	}

	bool matcher::matches(const std::string& route, std::vector<param>& params) const
	{
		std::smatch match;
		auto matched = std::regex_match(route, match, regex);
		if (!matched)
			return false;

		params.clear();
		params.reserve(keys.size());

		size_t id = 0;
		for (auto& key : keys) {
			params.push_back({ key.svalue, key.nvalue, match[++id] });
		}

		return true;
	}
}