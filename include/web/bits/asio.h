/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable:4242)
#pragma warning (disable:4265)
#pragma warning (disable:4619)
#pragma warning (disable:4834)
#endif
#include <boost/asio.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <unordered_set>
#include <web/delegate.h>
#include <web/log.h>
#include <web/request.h>
#include <web/response.h>
#include <web/stream.h>
#include <web/request_parser.h>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace web { namespace asio {
	using namespace boost::asio;
	using boost::system::error_code;

	class connection_manager;
	class connection : public std::enable_shared_from_this<connection> {
		class asio_stream : public stream::impl {
			ip::tcp::socket m_socket;
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

			void write_data(RX& rx, unsigned tid, unsigned conn);
			void read_data(TX& tx, unsigned tid, unsigned conn);
		public:
			asio_stream(io_service& io, connection* parent)
				: m_socket { io }
				, m_parent { parent }
			{
				LOG_NFO() << "asio_stream::asio_stream(this:" << this << ")";
			}
			~asio_stream() {
				LOG_NFO() << "asio_stream::~asio_stream(this:" << this << ")";
			}

			void shutdown(stream*) override;
			bool overflow(stream* src, const void* data, size_t size, unsigned conn) override;
			bool underflow(stream* src, unsigned conn) override;
			bool is_open(stream*) override;
			endpoint_t remote_endpoint(stream*) override;
			endpoint_t local_endpoint(stream*) override;

			ip::tcp::socket& socket() { return m_socket; }
			void close();
		};

		asio_stream m_stream;

		std::thread m_th;
		connection_manager& m_connection_manager;
		std::array<char, 8192> m_buffer;

		void handle(bool secure);
	public:
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		explicit connection(io_service& io, connection_manager& manager);
		~connection();

		ip::tcp::socket& socket() { return m_stream.socket(); }

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

	struct endpoint {
		std::string interface{};
		unsigned short port{};
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
		~service();
		endpoint setup(unsigned short port);
		void server(const std::string& value) { m_server = value; }
		const std::string& server() const { return m_server; }

		void run()
		{
			m_service.run();
		}
	};
}} // web::asio
