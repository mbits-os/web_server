/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/response.h>
#include <web/uri.h>

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

	void response::send_headers()
	{
		if (!has(header::Content_Type))
			set(header::Content_Type, "text/html; charset=UTF-8");

		m_headers_sent = true;

		char status_line[1024];
		auto status_s = status_name(status());
		if (!status_s)
			status_s = "Unknown";
		sprintf(status_line, "HTTP/%u.%u %u %s\r\n",
			version().M_ver(), version().m_ver(),
			(unsigned)status(), status_s);
		ll_print(status_line);

		for (auto& header : m_headers) {
			auto name = header.first.name();
			if (!name)
				continue;

			for (auto& val : header.second) {
				std::string header = name;
				header.append(": ");
				header.append(val);
				header.append("\r\n");
				ll_print(header);
			}
		}
		ll_print("\r\n");
	}

	void response::write(const void* data, size_t length)
	{
		auto ptr = (const char*)data;
		if (m_cache_content) {
			m_contents.insert(m_contents.end(), ptr, ptr + length);
			return;
		}

		if (!m_headers_sent) {
			if (!has(header::Transfer_Encoding))
				set(header::Transfer_Encoding, "chunked");
			send_headers();
		}

		// todo: flush/gather chunks
	}

	void response::stock_response(web::status st)
	{
		auto text = status_name(st);
		if (!text) {
			st = web::status::internal_server_error;
			text = "Internal Server Error";
		}

		status(st);

		auto status = std::to_string((unsigned)st);

		print("<html><head><title>");
		print(status);
		print(" ");
		print(text);
		print("</title></head><body><h1>");
		print(status);
		print(" ");
		print(text);
		print("</h1>");

		if (auto loc = location()) {
			print("<p>See <a href='");
			print(*loc);
			print("'>");
			print(uri { *loc }.string(uri::ui_safe));
			print("</a></p>");
		}

		print("</body></html>");
	}

	void response::finish()
	{
		if (!m_cache_content) {
			// output last chunk
			return;
		}

		if (!has(header::Content_Length))
			set(header::Content_Length, std::to_string(m_contents.size()));
		send_headers();
		ll_write(m_contents.data(), m_contents.size());
		m_contents.clear();
		if (!m_os->overflow())
			throw write_exception();
	}

}
