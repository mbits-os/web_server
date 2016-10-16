#pragma once

#include <web/headers.h>

namespace web {
#define HTTP_RESPONSE(X) \
	X(100, "Continue", cont) \
	X(101, "Switching Protocols", switching_protocols) \
	X(200, "OK", ok) \
	X(201, "Created", created) \
	X(202, "Accepted", accepted) \
	X(203, "Non-Authoritative Information", non_auth_information) \
	X(204, "No Content", no_content) \
	X(205, "Reset Content", reset_content) \
	X(206, "Partial Content", partial_content) \
	X(300, "Multiple Choices", multiple_choises) \
	X(301, "Moved Permanently", moved_permanently) \
	X(302, "Found", found) \
	X(303, "See Other", see_other) \
	X(304, "Not Modified", not_modified) \
	X(305, "Use Proxy", use_proxy) \
	X(307, "Temporary Redirect", temporary_redirect) \
	X(400, "Bad Request", bad_request) \
	X(401, "Unauthorized", unauthorised) \
	X(402, "Payment Required", payment_required) \
	X(403, "Forbidden", forbidden) \
	X(404, "Not Found", not_found) \
	X(405, "Method Not Allowed", method_not_allowed) \
	X(406, "Not Acceptable", not_acceptable) \
	X(407, "Proxy Authentication Required", proxy_auth_required) \
	X(408, "Request Timeout", request_timeout) \
	X(409, "Conflict", conflict) \
	X(410, "Gone", gone) \
	X(411, "Length Required", length_required) \
	X(412, "Precondition Failed", precondition_failed) \
	X(413, "Payload Too Large", payload_too_large) \
	X(414, "URI Too Long", uri_too_long) \
	X(415, "Unsupported Media Type", unsupported_media_type) \
	X(416, "Range Not Satisfiable", range_not_satisfiable) \
	X(417, "Expectation Failed", expectation_failed) \
	X(418, "I'm a teapot", im_a_teapot) \
	X(500, "Internal Server Error", internal_server_error) \
	X(501, "Not Implemented", not_implemented) \
	X(502, "Bad Gateway", bad_gateway) \
	X(503, "Service Unavailable", service_unavailable) \
	X(504, "Gateway Timeout", gateway_timeout) \
	X(505, "HTTP Version Not Supported", http_version_not_supported)

#define ENUM_VALUE(val, msg, name) name = val,
	enum class status {
		HTTP_RESPONSE(ENUM_VALUE)
	};
#undef ENUM_VALUE
	const char* status_name(status st);

	class response {
		web::headers m_headers;
		web::status m_status = web::status::ok;
		http_version_t m_version = http_version::http_none;
		std::string m_contents;
	public:
		void clear()
		{
			m_headers.clear();
			m_status = web::status::ok;
			m_version = http_version::http_none;
			m_contents.clear();
		}

		bool has(const header_key& key) const {
			return m_headers.has(key);
		}
		void add(const header_key& key, const std::string& value)
		{
			m_headers.add(key, value);
		}
		void add(const header_key& key, std::string&& value)
		{
			m_headers.add(key, std::move(value));
		}
		void set(const header_key& key, const std::string& value)
		{
			m_headers.erase(key);
			m_headers.add(key, value);
		}
		void set(const header_key& key, std::string&& value)
		{
			m_headers.erase(key);
			m_headers.add(key, std::move(value));
		}
		void erase(const header_key& key)
		{
			m_headers.erase(key);
		}
		const web::headers& headers() const { return m_headers; }

		void status(web::status value) { m_status = value; }
		web::status status() const { return m_status; }
		void version(http_version_t value) { m_version = value; }
		http_version_t version() const { return m_version; }
		void contents(std::string&& value) { m_contents = std::move(value); }
		const std::string& contents() const { return m_contents; }

		static response stock_response(web::status st);
	};
}
