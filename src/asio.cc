/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>
#include <web/stream.h>
#include <mutex>
#include <condition_variable>

namespace web { namespace asio {

	connection::connection(io_service& io, connection_manager& manager)
		: m_socket { io }
		, m_connection_manager { manager }
	{
	}

	void connection::start()
	{
		m_th = std::thread([this] { handle(false); });
	}

	class asio_stream : public stream::impl {
		ip::tcp::socket& m_socket;
		connection* m_parent;
		std::mutex m_mtx;
		std::condition_variable m_cv;
		bool socket_aborting = false;

		enum {
			succeeded,
			aborting,
			failed,
			running
		};

		struct RX {
			const void* data = nullptr;
			size_t transferred = 0;
			int status = running;
		};

		struct TX {
			std::array<char, 8192> data;
			size_t transferred = 0;
			int status = running;
		};

		void write_data(RX& rx) {
			async_write(m_socket, buffer(rx.data, rx.transferred), [&, this](error_code ec, std::size_t bytes_transferred) {
				if (ec) {
					if (ec != boost::asio::error::operation_aborted)
						rx.status = aborting;
					else
						rx.status = failed;
				} else {
					rx.transferred = bytes_transferred;
					rx.status = succeeded;
				}
				m_cv.notify_one();
			});
		}

		void read_data(TX& tx)
		{
			m_socket.async_read_some(buffer(tx.data), [&, this](error_code ec, std::size_t bytes_transferred) {
				if (ec) {
					if (ec != boost::asio::error::operation_aborted)
						tx.status = aborting;
					else
						tx.status = failed;
				} else {
					tx.transferred = bytes_transferred;
					tx.status = succeeded;
				}
				m_cv.notify_one();
				// m_connection_manager.stop(shared_from_this());
			});
		}
	public:
		asio_stream(ip::tcp::socket& socket, connection* parent)
			: m_socket { socket }
			, m_parent { parent }
		{
		}

		void shutdown(stream*)
		{
			if (socket_aborting)
				return;
			m_parent->shutdown();
		}

		bool overflow(stream* src, const void* data, size_t size) override
		{
			std::unique_lock<std::mutex> lock { m_mtx };

			RX rx;
			rx.data = data;
			rx.transferred = size;

			auto& service = m_socket.get_io_service();
			service.dispatch([&, this] { write_data(rx); });

			m_cv.wait(lock, [&] { return rx.status != running; });
			if (rx.status == succeeded) {
				src->flushed_write();
				return rx.transferred == size;
			} else {
				socket_aborting = (rx.status == aborting);
				return false;
			}
		}

		bool underflow(stream* src) override
		{
			std::unique_lock<std::mutex> lock { m_mtx };

			TX tx;
			auto& service = m_socket.get_io_service();
			service.dispatch([&, this] { read_data(tx); });

			m_cv.wait(lock, [&] { return tx.status != running; });
			if (tx.status == succeeded) {
				src->refill_read(tx.data.data(), tx.transferred);
				return !!tx.transferred;
			} else {
				socket_aborting = (tx.status == aborting);
				return false;
			}
		}

		bool is_open(stream*) override
		{
			return m_socket.is_open();
		}

		endpoint_t remote_endpoint(stream*) override
		{
			auto ep = m_socket.remote_endpoint();
			return { ep.address().to_string(), ep.port() };
		}

		endpoint_t local_endpoint(stream*) override
		{
			return { ip::host_name(), m_socket.local_endpoint().port() };
		}
	};

	void connection::handle(bool secure)
	{
		asio_stream asio_io { m_socket, this };
		stream io { asio_io };
		m_connection_manager.on_connection(io, secure);
	}

	void connection::stop()
	{
		m_socket.close();
		if (m_th.joinable())
			m_th.join();
	}

	void connection::shutdown()
	{
		m_connection_manager.stop(shared_from_this());
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

	service::service(delegate<void(stream&, bool)> onconnection)
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

	template <typename StreamIter>
	struct stream_t {
		StreamIter b;
	public:
		stream_t(StreamIter it) : b { it } { }
		StreamIter begin() { return b; }
		StreamIter end() { return StreamIter{ }; }
	};
	template <typename StreamIter>
	stream_t<StreamIter> mk_stream(StreamIter it) { return it; }

	void service::setup(int port)
	{
		ip::tcp::resolver resolver(m_service);
		ip::tcp::endpoint endpoint(ip::tcp::v4(), port);

		m_acceptor.open(endpoint.protocol());
		m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
		m_acceptor.bind(endpoint);
		m_acceptor.listen(socket_base::max_connections);
		port = m_acceptor.local_endpoint().port();
		std::string iface = ip::host_name();

#if 0
		try {
			for (auto entry : mk_stream(resolver.resolve({ ip::host_name(), "" }))) {
				auto ep = entry.endpoint();
				if (ep.address().is_v4()) {
					iface = ep.address().to_string();
					break;
				}
			}
		} catch (std::exception&) {
			iface = "127.0.0.1";
		}
#endif

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
		m_svc.server(name);
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