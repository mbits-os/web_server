/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>

namespace web {
	void server::set_routes(router& router)
	{
		m_routes = router.compile();

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

	void server::on_connection(request& req, response& resp)
	{
		std::vector<web::param> params;
		auto handler = req.method() == method::other
			? m_routes.find(req.smethod(), req.resource(), params)
			: m_routes.find(req.method(), req.resource(), params);

		if (!handler) {
			resp = response::stock_response(status::not_found);
			return;
		}

		auto& mask = handler->mask();
		auto mask_has_slash = !mask.empty() && mask.back() == '/';
		auto test_has_slash = req.resource()[req.resource().length() - 1] == '/';

		if (mask_has_slash != test_has_slash) {
			if (mask_has_slash) {
				resp.status(status::moved_permanently);
				resp.add(header::Location, req.resource() + "/");
				std::string content {
					"<html><head><title>Moved Permanently</title></head>"
					"<body><h1>Moved Permanently</h1><p>Seee <a href='"
				};
				content.append(req.resource());
				content.append("/'>");
				content.append(req.resource());
				content.append("/</a></p></body></html>");
				resp.contents(std::move(content));
			} else
				resp = response::stock_response(status::not_found);
			return;
		}

#if 0
		std::string response = "<html><head><title>Simple test</title></head><body><ul>";
		for (auto& param : params) {
			if (param.sname.empty())
				response += "<li><b>#" + std::to_string(param.nname);
			else
				response += "<li><b>" + param.sname;
			response += "</b>: " + param.value + "</li>";
		}
		response += "</li></body></html>";
		resp.contents(std::move(response));
#else
		handler->call(req, resp);
#endif
	}

}
