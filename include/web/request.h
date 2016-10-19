/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/attribute.h>
#include <web/headers.h>
#include <web/uri.h>

namespace web {
	enum class method {
		other,
		connect,
		del, // delete is a keyword, DELETE is defined in winnt.h...
		get,
		head,
		options,
		post,
		put,
		trace,
	};

	method make_method(std::string& textual);

	class request_parser;
	class request {
		friend class request_parser;
	protected:
		std::string m_smethod;
		web::method m_method;
		web::uri m_uri;
		http_version_t m_version;
		web::headers m_headers;
	public:
		const std::string& smethod() const { return m_smethod; }
		web::method method() const { return m_method; }
		const web::uri& uri() const { return m_uri; }
		http_version_t version() const { return m_version; }
		const web::headers& headers() const { return m_headers; }

		const std::string* find_front(const header_key& key) const
		{
			return m_headers.find_front(key);
		}
		const std::string* host() const { return find_front(header::Host); }
	};
}


namespace std {
	template <>
	struct hash<web::method> : public unary_function<web::method, size_t> {
		using int_t = typename std::underlying_type<web::method>::type;
		size_t operator()(web::method key) const noexcept(noexcept(hash<int_t>{}(0)))
		{
			return hash<int_t>{}((int_t)key);
		}
	};
};
