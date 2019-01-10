/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/route.h>
#include <web/middleware.h>
#include <unordered_map>

namespace web {
	class router {
	public:
		using route_list = std::unordered_map<method, std::vector<std::shared_ptr<route>>>;
		using sroute_list = std::unordered_map<std::string, std::vector<std::shared_ptr<route>>>;
		using filter_list = std::vector<std::pair<std::string, std::shared_ptr<middleware_base>>>;

		class compiled {
			route_list m_routes;
			sroute_list m_sroutes;
			filter_list m_middleware;
		public:
			compiled() = default;
			explicit compiled(route_list&& routes, sroute_list&& sroutes, filter_list&& middleware)
				: m_routes(std::move(routes))
				, m_sroutes(std::move(sroutes))
				, m_middleware(std::move(middleware))
			{
			}

			std::shared_ptr<route> find(method m, const std::string& route, std::vector<param>& params);
			std::shared_ptr<route> find(const std::string& other_method, const std::string& route, std::vector<param>& params);
			const route_list& routes() { return m_routes; }
			const sroute_list& sroutes() { return m_sroutes; }
			const auto& filters() { return m_middleware; }
		};
	private:
		struct sub_route {
			std::string mask;
			std::shared_ptr<router> sub;
		};

		struct handler {
			std::string mask;
			endpoint_type endpoint;
			int options;
		};

		using handlers = std::unordered_map<method, std::vector<handler>>;
		using shandlers = std::unordered_map<std::string, std::vector<handler>>;
		handlers m_handlers;
		shandlers m_shandlers;
		filter_list m_middleware;
		std::vector<sub_route> m_routers;

		void surrender(const std::string& prefix, handlers& handlers, shandlers& shandlers, filter_list& middlewares);
		std::shared_ptr<route> compile(handler& src);
	public:
		static std::shared_ptr<router> make()
		{
			return std::make_shared<router>();
		}

		void add(const std::string& path, const endpoint_type& et, method m = method::get, int options = COMPILE_DEFAULT);
		void add(const std::string& path, const endpoint_type& et, const std::string& other_method, int options = COMPILE_DEFAULT);
		void append(const std::string& path, const std::shared_ptr<router>& sub);

		template <typename Class, typename... Args>
		void filter(Args&&... args)
		{
			use("/", std::make_shared<Class>(std::forward<Args>(args)...));
		}

		void use(const std::string& path, const std::shared_ptr<middleware_base>& filt);

		compiled compile();
	};
}