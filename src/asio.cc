#include <web/server.h>

namespace web { namespace asio {

	connection::connection(io_service& io, connection_manager& manager)
		: m_socket { io }
		, m_connection_manager { manager }
	{
	}

	void connection::async_read()
	{
		auto self = shared_from_this();

		m_socket.async_read_some(buffer(m_buffer), [this, self](error_code ec, std::size_t bytes_transferred) {
			if (ec) {
				if (ec != boost::asio::error::operation_aborted)
					m_connection_manager.stop(shared_from_this());
				return;
			}

			on_data(bytes_transferred);
		});
	}

	bool should_keep_alive(const request& req)
	{
		auto it = req.headers().find_front(header::Connection);
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

	void connection::on_data(size_t read)
	{
		parsing result;
		std::tie(std::ignore, result) = m_parser.append(m_buffer.data(), read);
		if (result == parsing::error) {
			// todo: bad request
			m_connection_manager.stop(shared_from_this());
			return;
		}

		if (result == parsing::reading) {
			async_read();
			return;
		}

		request req;
		if (!m_parser.extract(req))
			m_response = response::stock_response(status::bad_request);
		else
			m_connection_manager.on_connection(req, m_response);

		write_response(req);
	}

	struct diag {
		http_version_t ver;
		std::string resource;
		method mth;
		std::string smth;
		status st;
		std::string host;
	};

	std::string host_from(request& req)
	{
		auto it = req.headers().find_front(header::Host);
		if (it) return *it;
		return { };
	}

	void connection::write_response(request& req)
	{
		diag dg { req.version(), req.resource(), req.method() };
		if (dg.mth == method::other)
			dg.smth = req.smethod();
		dg.st = m_response.status();
		dg.host = host_from(req);

		if (m_response.version().major() < 1)
			m_response.version(req.version());
		auto& content = m_response.contents();
		if (req.method() == method::get && content.empty() && m_response.status() == status::ok)
			m_response.status(status::no_content);

		if (!m_connection_manager.server().empty())
			m_response.set(header::Server, m_connection_manager.server());

		if (!m_response.has(header::Content_Type))
			m_response.set(header::Content_Type, "text/html; charset=UTF-8");

		m_response.set(header::Content_Length, std::to_string(content.length()));

		auto buffers = std::make_unique<std::vector<std::string>>();
		size_t headers = 2; // response status, empty line and content
		if (!content.empty())
			++headers;
		for (auto& header : m_response.headers()) {
			auto name = header.first.name();
			if (!name)
				continue;
			headers += header.second.size();
		}
		buffers->reserve(headers);

		char status_line[1024];
		auto status = status_name(m_response.status());
		if (!status)
			status = "Unknown";
		sprintf(status_line, "HTTP/%u.%u %u %s\r\n",
			m_response.version().major(), m_response.version().minor(),
			(unsigned)m_response.status(), status);
		buffers->emplace_back(status_line);

		for (auto& header : m_response.headers()) {
			auto name = header.first.name();
			if (!name)
				continue;

			for (auto& val : header.second) {
				std::string header = name;
				header.append(": ");
				header.append(val);
				header.append("\r\n");
				buffers->emplace_back(std::move(header));
			}
		}
		buffers->emplace_back("\r\n");
		if (!content.empty())
			buffers->emplace_back(content);

		auto self = shared_from_this();
		bool keep_alive = should_keep_alive(req);

		std::vector<const_buffer> asio_buffers;
		asio_buffers.reserve(buffers->size());
		for (auto& buff : *buffers) {
			asio_buffers.emplace_back(buff.c_str(), buff.length());
		}

		auto cb = [this,
			self = shared_from_this(),
			keep_buffers = buffers.get(),
			keep_alive](const boost::system::error_code& ec, std::size_t read)
		{
			std::unique_ptr<std::vector<std::string>> deleter { keep_buffers };
			write_done(ec, read, keep_alive);
		};

		async_write(m_socket, asio_buffers, cb);
		buffers.release();

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

		auto remote = m_socket.remote_endpoint();
		fprintf(stderr, "[%s:%u :: %s] %s \"%s\" HTTP/%u.%u -- %u\n",
			remote.address().to_string().c_str(), remote.port(),
			dg.host.c_str(),
			method, dg.resource.c_str(), dg.ver.major(), dg.ver.minor(),
			(unsigned)dg.st);
	}

	void connection::write_done(error_code ec, std::size_t, bool keep_alive)
	{
		if (!ec) {
			if (keep_alive) {
				m_response.clear();
				async_read();
				return;
			}

			// Initiate graceful connection closure.
			boost::system::error_code ignored_ec;
			m_socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
		}

		if (ec != boost::asio::error::operation_aborted) {
			m_connection_manager.stop(shared_from_this());
		}
	}

	void connection::start()
	{
		async_read();
	}

	void connection::stop()
	{
		m_socket.close();
	}

	void connection_manager::start(const std::shared_ptr<connection>& c)
	{
		m_connections.insert(c);
		c->start();
	}

	void connection_manager::stop(const std::shared_ptr<connection>& c)
	{
		m_connections.erase(c);
		c->stop();
	}

	void connection_manager::stop_all()
	{
		std::unordered_set<std::shared_ptr<connection>> local;
		std::swap(local, m_connections);
		for (auto& c : local)
			c->stop();
	}

	service::service(delegate<void(request& req, response& resp)> onconnection)
		: m_signals { m_service }
		, m_acceptor { m_service }
		, m_manager { onconnection }
	{
		m_signals.add(SIGINT);
		m_signals.add(SIGTERM);
#if defined(SIGQUIT)
		m_signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
		m_signals.async_wait([this](error_code /*ec*/, int /*signo*/)
		{
			// The server is stopped by cancelling all outstanding asynchronous
			// operations. Once all operations have finished the io_service::run()
			// call will exit.
			m_acceptor.close();
			m_manager.stop_all();
		});
	}

	void service::set_server(const std::string& name)
	{
		m_manager.server(name);
	}

	template <typename StreamIter>
	struct stream_t {
		StreamIter b;
	public:
		stream_t(StreamIter it) : b { it } { }
		StreamIter begin() { return b; }
		StreamIter end() { return StreamIter{ }; }
	};
	template <typename StreamIter>
	stream_t<StreamIter> stream(StreamIter it) { return it; }

	void service::setup(int port)
	{
		ip::tcp::resolver resolver(m_service);
		ip::tcp::endpoint endpoint(ip::tcp::v4(), port);

		m_acceptor.open(endpoint.protocol());
		m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
		m_acceptor.bind(endpoint);
		m_acceptor.listen(socket_base::max_connections);
		port = m_acceptor.local_endpoint().port();
		std::string iface = "127.0.0.1";

		try {
			for (auto entry : stream(resolver.resolve({ ip::host_name(), "" }))) {
				auto ep = entry.endpoint();
				if (ep.address().is_v4()) {
					iface = ep.address().to_string();
					break;
				}
			}
		} catch (std::exception&) {
		}

		printf("\nStarting server at http://%s:%u/\n", iface.c_str(), port);
		printf("Quit the server with ^C.\n\n");
		next_accept();
	}

	void service::next_accept()
	{
		auto conn = std::make_shared<connection>(m_service, m_manager);
		m_acceptor.async_accept(conn->socket(), [this, conn](error_code ec) {
			if (!m_acceptor.is_open())
				return;

			if (ec)
				return; // TODO: report error...

			auto endpoint = conn->socket().remote_endpoint();
			// printf("Connected to %s:%u\n", endpoint.address().to_string().c_str(), endpoint.port());
			m_manager.start(conn);

			next_accept();
		});
	}
}}

namespace web {
	server::server()
		: m_svc { { this, &server::on_connection } }
	{
	}

	void server::set_server(const std::string& name)
	{
		m_svc.set_server(name);
	}

	bool server::listen(short port)
	{
		try {
			m_svc.setup(port);
			return true;
		} catch (std::exception& e) {
			fprintf(stderr, "server::listen(): exception: %s\n", e.what());
		}
		return false;
	}

	void server::run()
	{
		m_svc.run();
	}
};