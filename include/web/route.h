#pragma once
#include <web/delegate.h>
#include <web/path_compiler.h>
#include <web/request.h>
#include <web/response.h>
#include <memory>
#include <regex>

namespace web {
	using endpoint_type = delegate<void(const request&, response&)>;
	class route {
		std::string m_mask;
		matcher m_matcher;
		endpoint_type m_endpoint;

		friend class router;
		void mask(const std::string& value) { m_mask = value; }
	public:
		route(const std::string& mask, const endpoint_type& et, int options)
			: m_mask(mask), m_matcher(web::matcher::make(mask, options)), m_endpoint(et)
		{
		}

		const std::string& mask() const { return m_mask; }
		const web::matcher& matcher() const { return m_matcher; }

		void call(request& req, response& resp)
		{
			if (m_endpoint)
				m_endpoint(req, resp);
			else
				resp = response::stock_response(status::internal_server_error);
		}
	};
}