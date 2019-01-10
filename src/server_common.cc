/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>
#include <mutex>

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

	bool should_keep_alive(const request& req)
	{
		auto it = req.find_front(header::Connection);
		if (it) {
			constexpr char keep_alive[] = "keep-alive";
			auto& val = *it;
			auto pos = val.find(keep_alive);
			if (pos != std::string::npos) {
				auto right = pos + sizeof(keep_alive) - 1;
				if (!pos || val[pos - 1] == ',' || std::isspace((uint8_t)val[pos - 1])) {
					if ((right == val.length()) || val[right] == ',' || std::isspace((uint8_t)val[right])) {
						return true;
					}
				}
			}
		}
		return false;
	}

	struct diag {
		http_version_t ver;
		web::uri uri;
		method mth;
		std::string smth;
		status st;
	};

	std::mutex io_mtx;
	class reporter {
		std::string remote;
		std::string remotest;
		request& req;
		response& resp;
	public:
		reporter(std::string remote, request& req, response& resp)
			: remote(std::move(remote)), req(req), resp(resp)
		{
		}
		void forwarded_for(const std::string& v) { remotest = v; }
		~reporter()
		{
			diag dg { req.version(), web::uri::normal(req.uri(), web::uri::ui_safe).string(), req.method() };
			if (dg.mth == method::other)
				dg.smth = req.smethod();
			dg.st = resp.status();

			const char* method = nullptr;
			switch (dg.mth) {
			case web::method::connect: method = "CONNECT"; break;
			case web::method::del: method = "DELETE"; break;
			case web::method::get: method = "GET"; break;
			case web::method::head: method = "HEAD"; break;
			case web::method::options: method = "OPTIONS"; break;
			case web::method::post: method = "POST"; break;
			case web::method::put: method = "PUT"; break;
			case web::method::trace: method = "TRACE"; break;
			default:
				method = dg.smth.c_str();
			}

			auto local = std::move(web::uri::auth_builder::parse(dg.uri.authority()).host);

			std::lock_guard<std::mutex> lck { io_mtx };
			std::ostringstream os; os << std::this_thread::get_id();

			auto const path_view = dg.uri.path();
			auto const query_view = dg.uri.query();
#define SV_PRINT(sv) static_cast<int>((sv).length()), (sv).data()

			if (remotest.empty()) {
				fprintf(stderr, "(%s) [%s] %s \"%.*s%.*s\" HTTP/%u.%u -- %u\n",
					os.str().c_str(), remote.c_str(),
					method, SV_PRINT(path_view), SV_PRINT(query_view), dg.ver.M_ver(), dg.ver.m_ver(),
					(unsigned)dg.st);
			} else {
				fprintf(stderr, "(%s) [%s|%s] %s \"%.*s%.*s\" HTTP/%u.%u -- %u\n",
					os.str().c_str(), remote.c_str(), remotest.c_str(),
					method, SV_PRINT(path_view), SV_PRINT(query_view), dg.ver.M_ver(), dg.ver.m_ver(),
					(unsigned)dg.st);
			}
		}
	};

	struct stream_src : public data_src {
		stream* p;
		stream_src(stream* p) : p { p } { }
		size_t get(char* data, size_t s) override
		{
			return p->read(data, s);
		}
	};

	void server::on_connection(stream& io, bool secure)
	{
		stream_src src { &io };

		while (io.is_open()) {
			request_parser parser;
			auto ret = parser.decode(src);
			if (ret != parsing::separator) {
				io.shutdown();
				break;
			}

			request req;
			response resp { &io, &req };
			auto local = io.local_endpoint();
			auto remote = io.remote_endpoint();
			reporter rep(remote.host + ":" + std::to_string(remote.port), req, resp);
			if (!parser.extract(secure, req, local.port, local.host)) {
				try {
					resp.version(http_version::http_1_1);
					resp.stock_response(status::bad_request);
				} catch (response::write_exception&) {
					// ignore, we are breaking anyway
				}
				io.shutdown();
				break;
			}

			try {
				auto fwdd = req.find_front(
					web::header_key::make("x-forwarded-for")
					);
				if (fwdd)
					rep.forwarded_for(*fwdd);

				load_content(io, req);
				resp.version(req.version());
				handle_connection(req, resp);
				resp.finish();
			} catch (response::write_exception&) {
				io.shutdown();
				break;
			}

			if (!should_keep_alive(req)) {
				io.shutdown();
				break;
			}
		}
	}

	void server::load_content(stream& io, request& req)
	{
		auto slen = req.find_front(header::Content_Length);
		if (!slen)
			return;

		char* bad = nullptr;
		auto length = static_cast<size_t>(std::strtoull(slen->c_str(), &bad, 10));
		if (bad && *bad)
			return;

		req.m_payload.resize(length);
		io.read(&req.m_payload[0], length);
	}

	void server::handle_connection(request& req, response& resp)
	{
		auto const res_view = req.uri().path();
		auto resource = std::string{ res_view.data(), res_view.length() };

		for (auto& pair : m_routes.filters()) {
			if (starts_with(resource, pair.first)) {
				auto res = pair.second->handle(req, resp);
				if (res == middleware_base::finished)
					return;
			}
		}

		std::vector<web::param> params;
		auto handler = req.method() == method::other
			? m_routes.find(req.smethod(), resource, params)
			: m_routes.find(req.method(), resource, params);

		if (!handler) {
			resp.stock_response(status::not_found);
			return;
		}

		auto& mask = handler->mask();
		auto mask_has_slash = !mask.empty() && mask.back() == '/';
		auto test_has_slash = resource[resource.length() - 1] == '/';

		if (mask_has_slash != test_has_slash) {
			if (mask_has_slash) {
				auto uri = req.uri();
				uri.path(resource + "/");
				resp.add(header::Location, uri.string());
				resp.stock_response(status::moved_permanently);
			} else
				resp.stock_response(status::not_found);
			return;
		}

		std::swap(params, req.m_params);
		handler->call(req, resp);
	}

}
