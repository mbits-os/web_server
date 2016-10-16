/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/route.h>
#include <unordered_map>

namespace web {
	class router {
		struct sub_route {
			std::string mask;
			std::shared_ptr<router> sub;
		};

		struct handler {
			std::string mask;
			endpoint_type endpoint;
		};

		std::vector<sub_route> m_routers;
		using handlers = std::unordered_map<method, std::vector<handler>>;
		using shandlers = std::unordered_map<std::string, std::vector<handler>>;
		handlers m_handlers;
		shandlers m_shandlers;

		void surrender(const std::string& prefix, handlers& handlers, shandlers& shandlers);
		std::shared_ptr<route> compile(handler& src, int options);
	public:
		static std::shared_ptr<router> make()
		{
			return std::make_shared<router>();
		}

		using route_list = std::unordered_map<method, std::vector<std::shared_ptr<route>>>;
		using sroute_list = std::unordered_map<std::string, std::vector<std::shared_ptr<route>>>;
		class compiled {
			route_list m_routes;
			sroute_list m_sroutes;
		public:
			compiled() = default;
			explicit compiled(route_list&& routes, sroute_list&& sroutes)
				: m_routes(std::move(routes))
				, m_sroutes(std::move(sroutes))
			{
			}

			std::shared_ptr<route> find(method m, const std::string& route, std::vector<param>& params);
			std::shared_ptr<route> find(const std::string& other_method, const std::string& route, std::vector<param>& params);
			const route_list& routes() { return m_routes; }
			const sroute_list& sroutes() { return m_sroutes; }
		};

		void add(const std::string& path, const endpoint_type& et, method m = method::get);
		void add(const std::string& path, const endpoint_type& et, const std::string& other_method);
		void append(const std::string& path, const std::shared_ptr<router>& sub);
		compiled compile(int options = COMPILE_DEFAULT);
	};
}