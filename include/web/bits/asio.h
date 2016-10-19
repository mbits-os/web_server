/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <boost/asio.hpp>
#include <unordered_set>
#include <web/delegate.h>
#include <web/request.h>
#include <web/response.h>
#include <web/request_parser.h>
#include <thread>

namespace web { namespace asio {
	using namespace boost::asio;
	using boost::system::error_code;

	class connection_manager;
	class connection : public std::enable_shared_from_this<connection> {
		ip::tcp::socket m_socket;
		std::thread m_th;
		connection_manager& m_connection_manager;
		std::array<char, 8192> m_buffer;

		void handle(bool secure);
	public:
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		explicit connection(io_service& io, connection_manager& manager);

		ip::tcp::socket& socket() { return m_socket; }

		void start();
		void stop();

		void shutdown();
	};

	class connection_manager {
		std::unordered_set<std::shared_ptr<connection>> m_connections;
		delegate<void(stream&, bool)> m_onconnection;
	public:
		connection_manager(const connection_manager&) = delete;
		connection_manager& operator=(const connection_manager&) = delete;
		connection_manager(delegate<void(web::stream&, bool)> onconnection)
			: m_onconnection(std::move(onconnection))
		{
		}

		void start(const std::shared_ptr<connection>& c);
		void stop(const std::shared_ptr<connection>& c);
		void stop_all();
		void on_connection(stream& io, bool secure)
		{
			m_onconnection(io, secure);
		}
	};

	class service {
		io_service m_service;
		signal_set m_signals;
		ip::tcp::acceptor m_acceptor;
		connection_manager m_manager;
		std::string m_server;

		void next_accept();
	public:
		service(delegate<void(stream&, bool)> onconnection);
		void setup(int port);
		void server(const std::string& value) { m_server = value; }
		const std::string& server() const { return m_server; }

		void run()
		{
			m_service.run();
		}
	};
}} // web::asio
