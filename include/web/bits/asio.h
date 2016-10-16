#pragma once

#include <boost/asio.hpp>
#include <unordered_set>
#include <web/delegate.h>
#include <web/request.h>
#include <web/response.h>
#include <web/request_parser.h>

namespace web { namespace asio {
	using namespace boost::asio;
	using boost::system::error_code;

	class connection_manager;
	class connection : public std::enable_shared_from_this<connection> {
		ip::tcp::socket m_socket;
		connection_manager& m_connection_manager;
		std::array<char, 8192> m_buffer;

		request_parser m_parser;
		response m_response;

		void async_read();
		void on_data(size_t read);
		void write_response(request& request);
		void write_done(error_code ec, std::size_t, bool keep_alive);
	public:
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		explicit connection(io_service& io, connection_manager& manager);

		ip::tcp::socket& socket() { return m_socket; }

		void start();
		void stop();
	};

	class connection_manager {
		std::unordered_set<std::shared_ptr<connection>> m_connections;
		delegate<void(request& req, response& resp)> m_onconnection;
		std::string m_server;
	public:
		connection_manager(const connection_manager&) = delete;
		connection_manager& operator=(const connection_manager&) = delete;
		connection_manager(delegate<void(request& req, response& resp)> onconnection)
			: m_onconnection(std::move(onconnection))
		{
		}

		void server(const std::string& value) { m_server = value; }
		const std::string& server() const { return m_server; }
		void start(const std::shared_ptr<connection>& c);
		void stop(const std::shared_ptr<connection>& c);
		void stop_all();
		void on_connection(request& req, response& resp)
		{
			m_onconnection(req, resp);
		}
	};

	class service {
		io_service m_service;
		signal_set m_signals;
		ip::tcp::acceptor m_acceptor;
		connection_manager m_manager;

		void next_accept();
	public:
		service(delegate<void(request& req, response& resp)> onconnection);
		void set_server(const std::string&);
		void setup(int port);

		void run()
		{
			m_service.run();
		}
	};
}} // web::asio
