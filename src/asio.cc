/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/server.h>
#include <mutex>
#include <condition_variable>

unsigned get_thid();

namespace web { namespace asio {

	void connection::asio_stream::write_data(RX& rx, unsigned tid, unsigned conn)
	{
		async_write(m_socket, buffer(rx.data, rx.transferred), [&, this, tid, conn](error_code ec, std::size_t bytes_transferred) {
			if (ec) {
				rx.status = failed;
				LOG_NFO() << "asio_stream::write_data(this:" << this << ", rx, #" << tid << "." << conn << ") -- failed: " << ec.message();
			} else {
				rx.transferred = bytes_transferred;
				rx.status = succeeded;
				LOG_NFO() << "asio_stream::write_data(this:" << this << ", rx, #" << tid << "." << conn << ") --> " << bytes_transferred;
			}
			m_cv.notify_one();
		});
	}

	void connection::asio_stream::read_data(TX& tx, unsigned tid, unsigned conn)
	{
		m_socket.async_read_some(buffer(tx.data), [&, this, tid, conn](error_code ec, std::size_t bytes_transferred) {
			if (ec) {
				tx.status = failed;
				LOG_NFO() << "asio_stream::read_data(this:" << this << ", tx, #" << tid << "." << conn << ") -- failed: " << ec.message();
			} else {
				tx.transferred = bytes_transferred;
				tx.status = succeeded;
				LOG_NFO() << "asio_stream::read_data(this:" << this << ", tx, #" << tid << "." << conn << ") --> " << bytes_transferred;
			}
			m_cv.notify_one();
		});
	}

	void connection::asio_stream::shutdown(stream* src)
	{
		if (socket_aborting) {
			LOG_WRN() << "asio_stream::shutdown(this:" << this << ", src:" << src << ") -- while aborting";
			return;
		}

		auto shared = m_parent->shared_from_this();
		m_socket.get_io_service().post([parent = m_parent, shared, this, src] {
			LOG_NFO() << "asio_stream::shutdown(this:" << this << ", src:" << src << ")::lambda";
			parent->shutdown();
			LOG_NFO() << "asio_stream::shutdown(this:" << this << ", src:" << src << ") -- done";
		});
	}

	bool connection::asio_stream::overflow(stream* src, const void* data, size_t size, unsigned conn)
	{
		std::unique_lock<std::mutex> lock { m_mtx };

		LOG_NFO() << "asio_stream::overflow(this:" << this << ", src:" << src << ", data:" << data << ", size:" << size << ")";

		RX rx;
		rx.data = data;
		rx.transferred = size;

		auto shared = m_parent->shared_from_this();
		auto thid = get_thid();
		m_socket.get_io_service().post([&, this, shared, thid, conn] {
			write_data(rx, thid, conn);
		});

		m_cv.wait(lock, [&] { return rx.status != running; });
		if (rx.status == succeeded) {
			LOG_NFO() << "asio_stream::overflow(this:" << this << ", src:" << src << ", data:" << data << ", size:" << size << ") -- success";
			src->flushed_write();
			return rx.transferred == size;
		} else {
			LOG_NFO() << "asio_stream::overflow(this:" << this << ", src:" << src << ", data:" << data << ", size:" << size << ") -- failed, rx.status:" << static_cast<int>(rx.status);
			return false;
		}
	}

	bool connection::asio_stream::underflow(stream* src, unsigned conn)
	{
		std::unique_lock<std::mutex> lock { m_mtx };

		LOG_NFO() << "asio_stream::undeflow(this:" << this << ", src:" << src << ")";

		TX tx;
		auto shared = m_parent->shared_from_this();
		auto thid = get_thid();
		m_socket.get_io_service().post([&, this, shared, conn] {
			read_data(tx, thid, conn);
		});

		m_cv.wait(lock, [&] { return tx.status != running; });
		if (tx.status == succeeded) {
			LOG_NFO() << "asio_stream::undeflow(this:" << this << ", src:" << src << ") -- success";
			src->refill_read(tx.data.data(), tx.transferred);
			return !!tx.transferred;
		} else {
			LOG_NFO() << "asio_stream::undeflow(this:" << this << ", src:" << src << ") -- failed, tx.status:" << static_cast<int>(tx.status);
			return false;
		}
	}

	bool connection::asio_stream::is_open(stream* src)
	{
		return m_socket.is_open();
	}

	endpoint_t connection::asio_stream::remote_endpoint(stream*)
	{
		auto ep = m_socket.remote_endpoint();
		return { ep.address().to_string(), ep.port() };
	}

	endpoint_t connection::asio_stream::local_endpoint(stream*)
	{
		return { ip::host_name(), m_socket.local_endpoint().port() };
	}

	void connection::asio_stream::close()
	{
		LOG_NFO() << "asio_stream::close(this:" << this << ") -------------------------------------------------------";
		socket_aborting = true;
		m_socket.close();
	}

	connection::connection(io_service& io, connection_manager& manager)
		: m_stream { io, this }
		, m_connection_manager { manager }
	{
		LOG_NFO() << "connection::connection(this:" << this << ")";
	}

	connection::~connection()
	{
		LOG_NFO() << "connection::~connection(this:" << this << ")";
	}

	void connection::start()
	{
		LOG_NFO() << "connection::start(this:" << this << ")";
		auto shared = shared_from_this();
		m_th = std::thread([this, shared] {
			LOG_NFO() << "START =============================================";
			handle(false);
			LOG_NFO() << "STOP ----------------------------------------------";
		});
	}

	void connection::handle(bool secure)
	{
		stream io { m_stream };
		m_connection_manager.on_connection(io, secure);
	}

	void connection::stop()
	{
		LOG_NFO() << "connection::stop(this:" << this << ")";
		auto shared = shared_from_this();
		m_stream.close();
		m_stream.socket().get_io_service().post([this, shared] {
			LOG_NFO() << "connection::stop(this:" << this << ")::lambda -- "
					  << (m_th.joinable() ? "" : "non ") << "joinable";
			if (m_th.joinable())
				m_th.join();
			LOG_NFO() << "connection::stop(this:" << this << ")::lambda/done";
		});
	}

	void connection::shutdown()
	{
		LOG_NFO() << "connection::shutdown(this:" << this << ")";
		m_connection_manager.stop(shared_from_this());
	}

	void connection_manager::start(const std::shared_ptr<connection>& c)
	{
		LOG_NFO() << "connection_manager::start(this:" << this << ", c:" << c.get() << ":" << c.use_count() << ")";
		m_connections.insert(c);
		c->start();
	}

	void connection_manager::stop(const std::shared_ptr<connection>& c)
	{
		LOG_NFO() << "connection_manager::stop(this:" << this << ", c:" << c.get() << ":" << c.use_count() << ")";
		m_connections.erase(c);
		c->stop();
	}

	void connection_manager::stop_all()
	{
		LOG_NFO() << "connection_manager::stop_all(this:" << this << ")";
		std::unordered_set<std::shared_ptr<connection>> local;
		std::swap(local, m_connections);
		for (auto& c : local) {
			LOG_NFO() << "connection_manager::stop_all(this:" << this << ") -- c:" << c.get() << ":" << c.use_count();
			c->stop();
		}
	}

	service::service(delegate<void(stream&, bool)> onconnection)
		: m_signals { m_service }
		, m_acceptor { m_service }
		, m_manager { onconnection }
	{
		LOG_NFO() << "service::service(this:" << this << ")";
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
			printf("\nShutting the server down\n");
			m_acceptor.close();
			m_manager.stop_all();
		});
	}

	service::~service() {
		LOG_NFO() << "service::~service(this:" << this << ")";
	}

	endpoint service::setup(unsigned short port)
	{
		LOG_NFO() << "service::setup(this:" << this << ", port:" << port << ")";

		ip::tcp::resolver resolver(m_service);
		ip::tcp::endpoint endpoint(ip::tcp::v4(), port);

		m_acceptor.open(endpoint.protocol());
		m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
		m_acceptor.bind(endpoint);
		m_acceptor.listen(socket_base::max_connections);

		next_accept();
		return web::asio::endpoint{ ip::host_name(), m_acceptor.local_endpoint().port() };
	}

	void service::next_accept()
	{
		auto conn = std::make_shared<connection>(m_service, m_manager);
		LOG_NFO() << "service::next_accept(this:" << this << ") -- conn:" << conn.get() << ":" << conn.use_count();
		m_acceptor.async_accept(conn->socket(), [this, conn](error_code ec) {
			if (!m_acceptor.is_open()) {
				LOG_NFO() << "service::next_accept(this:" << this << ")::lambda -- acceptor closed";
				return;
			}

			if (ec) {
				LOG_ERR() << "service::next_accept(this:" << this << ")::lambda -- acceptor error: " << ec.message();
				return;
			}

			LOG_NFO() << "service::next_accept(this:" << this << ")::lambda -- starting conn:" << conn.get() << ":" << conn.use_count();
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

	std::optional<endpoint> server::listen(unsigned short port)
	{
		try {
			return m_svc.setup(port);
		} catch (std::exception& e) {
			fprintf(stderr, "server::listen(): exception: %s\n", e.what());
		}
		return std::nullopt;
	}

	void server::run()
	{
		m_svc.run();
	}
};