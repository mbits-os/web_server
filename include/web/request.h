/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/attribute.h>
#include <web/headers.h>

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
		method m_method;
		std::string m_resource;
		http_version_t m_version;
		web::headers m_headers;
	public:
		const std::string& smethod() const { return m_smethod; }
		method method() const { return m_method; }
		const std::string& resource() const { return m_resource; }
		http_version_t version() const { return m_version; }
		const web::headers& headers() const { return m_headers; }
	};
}
