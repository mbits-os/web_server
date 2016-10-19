/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>

namespace web {
	void server::set_routes(router& router)
	{
		m_routes = router.compile();

		for (auto& pair : m_routes.filters()) {
			printf("[FILTER] %s\n", pair.first.c_str());
		}

		for (auto& pair : m_routes.routes()) {
			const char* method = nullptr;
			switch (pair.first) {
			case web::method::connect: method = "CONNECT"; break;
			case web::method::del: method = "DELETE"; break;
			case web::method::get: method = "GET"; break;
			case web::method::head: method = "HEAD"; break;
			case web::method::options: method = "OPTIONS"; break;
			case web::method::post: method = "POST"; break;
			case web::method::put: method = "PUT"; break;
			case web::method::trace: method = "TRACE"; break;
			default:
				assert(false && "unexpected method (other)");
			}
			for (auto& handler : pair.second)
				printf("%s %s\n", method, handler->mask().c_str());
		}
		for (auto& pair : m_routes.sroutes()) {
			for (auto& handler : pair.second)
				printf("%s %s\n", pair.first.c_str(), handler->mask().c_str());
		}

	}

	inline bool starts_with(const std::string& value, const std::string& prefix)
	{
		if (value.length() < prefix.length())
			return false;

		if (value.compare(0, prefix.length(), prefix) != 0)
			return false;

		if (value.length() > prefix.length()) {
			if (!prefix.empty() && prefix[prefix.length() - 1] == '/')
				return true;
			return value[prefix.length()] == '/';
		}
		return true;
	}

	void server::on_connection(request& req, response& resp)
	{
		auto resource = req.uri().path().str();

		for (auto& pair : m_routes.filters()) {
			if (starts_with(resource, pair.first)) {
				auto res = pair.second->handle(req, resp);
				if (res == middleware::finished)
					return;
			}
		}

		std::vector<web::param> params;
		auto handler = req.method() == method::other
			? m_routes.find(req.smethod(), resource, params)
			: m_routes.find(req.method(), resource, params);

		if (!handler) {
			resp = response::stock_response(status::not_found);
			return;
		}

		auto& mask = handler->mask();
		auto mask_has_slash = !mask.empty() && mask.back() == '/';
		auto test_has_slash = resource[resource.length() - 1] == '/';

		if (mask_has_slash != test_has_slash) {
			if (mask_has_slash) {
				auto uri = req.uri();
				uri.path(resource + "/");
				resp.status(status::moved_permanently);
				resp.add(header::Location, uri.string() + "/");
				std::string content {
					"<html><head><title>Moved Permanently</title></head>"
					"<body><h1>Moved Permanently</h1><p>Seee <a href='"
				};
				content.append(uri.string());
				content.append("/'>");
				content.append(uri.string(web::uri::ui_safe));
				content.append("/</a></p></body></html>");
				resp.contents(std::move(content));
			} else
				resp = response::stock_response(status::not_found);
			return;
		}

		handler->call(req, resp);
	}

}
