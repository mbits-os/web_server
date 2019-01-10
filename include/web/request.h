/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/attribute.h>
#include <web/headers.h>
#include <web/uri.h>
#include <web/path_compiler.h>

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

	class server;
	class request_parser;
	class request {
		friend class request_parser;
		friend class server;
	protected:
		std::string m_smethod;
		web::method m_method;
		web::uri m_uri;
		http_version_t m_version;
		std::vector<param> m_params;
		web::headers m_headers;
		std::vector<char> m_payload;
	public:
		const std::string& smethod() const { return m_smethod; }
		web::method method() const { return m_method; }
		const web::uri& uri() const { return m_uri; }
		http_version_t version() const { return m_version; }
		const std::vector<param>& params() const { return m_params; }
		const std::string* find_param(const std::string& key) const;
		const std::string* find_param(size_t key) const;
		const web::headers& headers() const { return m_headers; }

		const std::vector<char>& payload() const { return m_payload; }

		const std::string* find_front(const header_key& key) const
		{
			return m_headers.find_front(key);
		}
		const std::string* host() const { return find_front(header::Host); }
	};
}
