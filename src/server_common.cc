/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>
#include <web/log.h>
#include <mutex>
#include <atomic>

namespace web {
	static std::mutex io_mtx;
	static std::atomic<size_t> conn_count{}, closed_conn_count{};

	static void print_conn() {
		std::lock_guard<std::mutex> lck{ io_mtx };
		fprintf(stderr, "\r%zu reqs/%zu resps", conn_count.load(), closed_conn_count.load());
	}

	void server::set_routes(router& router)
	{
		m_routes = router.compile();
	}

	void server::print() const
	{
		for (auto& pair : m_routes.filters()) {
			LOG_NFO() << "[FILTER] " << pair.first;
		}

		std::vector<std::pair<std::string, std::string>> list;
		auto add_route = [&](std::string const& path, auto const& method) {
			auto it = std::find_if(list.begin(), list.end(),
				[&](auto const& pair) { return pair.first == path; }
			);
			if (it == list.end())
				list.emplace_back(path, method);
			else {
				it->second.push_back('|');
				it->second.append(method);
			}
		};

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
				add_route(handler->mask(), method);
		}
		for (auto& pair : m_routes.sroutes()) {
			for (auto& handler : pair.second)
				add_route(handler->mask(), pair.first);
		}

		for (auto const&[path, methods] : list)
			LOG_NFO() << "[ROUTE] " << methods << ' ' << path;

		print_conn();
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

	class reporter {
		std::string remote;
		std::string remotest;
		request& req;
		response& resp;
	public:
		reporter(std::string remote, request& req, response& resp)
			: remote(std::move(remote)), req(req), resp(resp)
		{
			++conn_count;
			print_conn();
		}
		void forwarded_for(const std::string& v) { remotest = v; }
		~reporter()
		{
			++closed_conn_count;
			print_conn();
			LOG_NFO() << *this;
		}

		friend std::ostream& operator<<(std::ostream& o, reporter const& rr) {
			o << "REQ [" << rr.remote;
			if (!rr.remotest.empty())
				o << '|' << rr.remotest;
			o << "] ";

			switch (rr.req.method()) {
			case web::method::connect: o << "CONNECT"; break;
			case web::method::del: o << "DELETE"; break;
			case web::method::get: o << "GET"; break;
			case web::method::head: o << "HEAD"; break;
			case web::method::options: o << "OPTIONS"; break;
			case web::method::post: o << "POST"; break;
			case web::method::put: o << "PUT"; break;
			case web::method::trace: o << "TRACE"; break;
			default:
				o << rr.req.smethod();
			}

			auto const uri = web::uri::normal(rr.req.uri(), web::uri::ui_safe);
			auto const ver = rr.req.version();
			return o
				<< " \"" << uri.path() << uri.query()
				<< "\" HTTP/" << ver.M_ver() << '.' << ver.m_ver()
				<< " -- " << static_cast<unsigned>(rr.resp.status());
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

		unsigned conn_no{};
		while (io.is_open()) {
			io.conn_no(++conn_no);

			request_parser parser;
			auto ret = parser.decode(src);
			if (ret != parsing::separator) {
				io.shutdown();
				LOG_DBG2() << "[CONN " << conn_no << "] ERROR";
				break;
			}

			request req{ this };
			response resp { &io, &req };
			auto local = io.local_endpoint();
			auto remote = io.remote_endpoint();
			reporter rep(remote.host + ":" + std::to_string(remote.port), req, resp);
			if (!parser.extract(secure, req, local.port, local.host)) {
				LOG_DBG2() << "[CONN " << conn_no << "] REQ " << remote.host << ":" << remote.port << " ERROR";
				try {
					resp.version(http_version::http_1_1);
					resp.stock_response(status::bad_request);
				} catch (response::write_exception&) {
					// ignore, we are breaking anyway
				}
				io.shutdown();
				break;
			}

			{
				diag dg{ req.version(), web::uri::normal(req.uri(), web::uri::ui_safe).string(), req.method() };
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

				auto const path_view = dg.uri.path();
				auto const query_view = dg.uri.query();

				LOG_DBG2() << "[CONN " << conn_no << "] REQ  | " << remote.host << ":" << remote.port << " | "
				          << method << " " << path_view << query_view
				          << " HTTP/" << dg.ver.M_ver() << "." << dg.ver.m_ver();
				for (auto const& [header, values] : req.headers()) {
					auto name = header.name();
					if (!name) name = "(null)";
					for (auto const& value : values)
						LOG_DBG2() << "[CONN " << conn_no << "]      | " << name << ": " << value;
				}

				auto fwdd = req.find_front(
					web::header_key::make("x-forwarded-for")
				);

				if (fwdd)
					rep.forwarded_for(*fwdd);
			}

			try {
				load_content(io, req);
				resp.version(req.version());
				handle_connection(req, resp);
				resp.finish();
				LOG_DBG2() << "[CONN " << conn_no << "] RESP | " << remote.host << ":" << remote.port
				          << " | HTTP/" << resp.version().M_ver() << "." << resp.version().m_ver()
				          << " " << (unsigned)resp.status() << " " << status_name(resp.status());
				for (auto const&[header, values] : resp.headers()) {
					auto name = header.name();
					if (!name) name = "(null)";
					for (auto const& value : values)
						LOG_DBG2() << "[CONN " << conn_no << "]      | " << name << ": " << value;
				}
			} catch (response::write_exception&) {
				io.shutdown();
				break;
			}

			if (!should_keep_alive(req)) {
				LOG_DBG2() << conn_no << ". shutdown : don't keep alive";
				io.shutdown();
				break;
			}
			LOG_DBG2() << "NEXT ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
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
