/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/response.h>

namespace web {
	const char* status_name(status st)
	{
#define SWITCH_VALUE(val, msg, name) case status::name: return msg;
		switch(st) {
			HTTP_RESPONSE(SWITCH_VALUE)
		default: break;
		};
#undef SWITCH_VALUE
		return nullptr;
	}

	response response::stock_response(web::status st)
	{
		auto text = status_name(st);
		if (!text) {
			st = web::status::internal_server_error;
			text = "Internal Server Error";
		}

		response out;
		out.status(st);

		std::string content { "<html><head><title>" };
		content.append(text);
		content.append("</title></head><body><h1>");
		content.append(text);
		content.append("</h1></body></html>");
		out.contents(std::move(content));
		return out;
	}
}
