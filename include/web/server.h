/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#ifdef HTTP_USE_ASIO
#include <web/bits/asio.h>
#endif

#include <web/router.h>

namespace web {
	class server {
		router::compiled m_routes;
#ifdef HTTP_USE_ASIO
		asio::service m_svc;
#endif
	public:
		server();
		void set_server(const std::string&);
		void set_routes(router& router);
		bool listen(short port);
		void run();
		void on_connection(request& req, response& resp);
	};
};
