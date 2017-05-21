/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/response.h>
#include <web/request.h>
#include <web/mime_type.h>
#include <web/uri.h>
#include <sys/stat.h>
#include <ctime>

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

	struct fcloser {
		void operator()(FILE* f) const
		{
			fclose(f);
		}
	};

	void response::send_file(const std::string& path)
	{
		throw_if_sent("send_file");
		m_cache_content = true; // first, for stock_response()s, second, to navigate the finish();

		bool only_head = m_req_ref->method() == method::head;
		struct stat st;
		if (stat(path.c_str(), &st)) {
			stock_response(web::status::not_found);
			return;
		}

		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			stock_response(web::status::forbidden);
			return;
		}

		std::unique_ptr<FILE, fcloser> f { std::fopen(path.c_str(), "rb") };
		if (!f) {
			stock_response(web::status::not_found);
			return;
		}

		set(header::Content_Length, std::to_string(st.st_size));
		set(header::Content_Type, mime_type(path));
		{
			char buf[1000];
			auto tme = *gmtime(&st.st_mtime);
			strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tme);
			set(header::Last_Modified, buf);

			if (!only_head) {
				auto if_modified = m_req_ref->find_front(header::If_Modified_Since);
				if (if_modified && *if_modified == buf) {
					status(web::status::not_modified);
					only_head = true;
				}
			}
		}

		send_headers();

		if (only_head)
			return;

		char buffer[8192];

		auto read = std::fread(buffer, 1, sizeof(buffer), f.get());
		while (read) {
			ll_write(buffer, read);
			read = std::fread(buffer, 1, sizeof(buffer), f.get());
		}
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

	response& response::print_json(const char* s, size_t length)
	{
		return print('"').print_json_chunk(s, length).print('"');
	}

	response& response::print_json_chunk(const char* s, size_t length)
	{
		// TODO: UNICODE
		auto c = s;
		auto e = s + length;
		for (; c != e; ++c) {
			switch (*c) {
			case '"': print("\\\""); break;
			case '\\': print("\\\\"); break;
			case '/': print("\\/"); break;
			case '\b': print("\\b"); break;
			case '\f': print("\\f"); break;
			case '\n': print("\\n"); break;
			case '\r': print("\\r"); break;
			case '\t': print("\\t"); break;
			default:
				print(*c);
			}
		}
		return *this;
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

		// haders would be sent from APIs like send_file()
		if (!m_headers_sent) {
			bool only_head = m_req_ref->method() == method::head;
			if (!only_head) {
				auto last_modified = m_headers.find_front(header::Last_Modified);
				if (last_modified) {
					auto if_modified = m_req_ref->find_front(header::If_Modified_Since);
					if (if_modified && *if_modified == *last_modified) {
						status(web::status::not_modified);
						only_head = true;
					}
				}
			}

			if (!has(header::Content_Length))
				set(header::Content_Length, std::to_string(m_contents.size()));
			send_headers();

			if (!only_head)
				ll_write(m_contents.data(), m_contents.size());

			m_contents.clear();
		}

		if (!m_os->overflow())
			throw write_exception();
	}

}
