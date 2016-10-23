/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/request.h>
#include <algorithm>
#include <cctype>

namespace web {
	method make_method(std::string& textual)
	{
		static constexpr const char* convertable[] = {
			"CONNECT",
			"DELETE",
			"GET",
			"HEAD",
			"OPTIONS",
			"POST",
			"PUT",
			"TRACE",
		};

		for (auto& c : textual)
			c = std::toupper((uint8_t)c);

		auto it = std::lower_bound(std::begin(convertable), std::end(convertable), textual);

		if (it != std::end(convertable)) {
			if (*it == textual) {
				return (method)((int)method::connect + std::distance(std::begin(convertable), it));
			}
		}

		return method::other;
	}

	const std::string* request::find_param(const std::string& key) const
	{
		for (auto& p : m_params) {
			if (!p.sname.empty() && p.sname == key)
				return &p.value;
		}
		return nullptr;
	}

	const std::string* request::find_param(size_t key) const
	{
		for (auto& p : m_params) {
			if (p.sname.empty() && p.nname == key)
				return &p.value;
		}
		return nullptr;
	}
}
