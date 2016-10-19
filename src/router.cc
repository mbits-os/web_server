/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/router.h>
#include <cassert>

namespace web {
	void router::add(const std::string& path, const endpoint_type& et, method m)
	{
		assert(m != method::other);
		m_handlers[m].push_back({ path, et });
	}

	void router::add(const std::string& path, const endpoint_type& et, const std::string& other_method)
	{
		auto textual = other_method;
		auto m = make_method(textual);
		if (m == method::other)
			m_shandlers[textual].push_back({ path, et });
		else
			m_handlers[m].push_back({ path, et });
	}

	void router::append(const std::string& path, const std::shared_ptr<router>& sub)
	{
		m_routers.push_back({ path, sub });
	}

	void router::use(const std::string& path, const std::shared_ptr<middleware>& filt)
	{
		m_middleware.push_back({ path, filt });
	}

	std::shared_ptr<route> router::compile(handler& src, int options)
	{
		return std::make_shared<route>(src.mask, std::move(src.endpoint), options);
	}

	router::compiled router::compile(int options)
	{
		for (auto& sub : m_routers)
			sub.sub->surrender(sub.mask, m_handlers, m_shandlers, m_middleware);
		m_routers.clear();

		route_list out;
		for (auto& pair : m_handlers) {
			auto& dst = out[pair.first];
			dst.reserve(pair.second.size());
			for (auto& handler : pair.second)
				dst.push_back(compile(handler, options));
		}
		sroute_list sout;
		for (auto& pair : m_shandlers) {
			auto& dst = sout[pair.first];
			dst.reserve(pair.second.size());
			for (auto& handler : pair.second)
				dst.push_back(compile(handler, options));
		}
		m_handlers.clear();
		m_shandlers.clear();

		return compiled { std::move(out), std::move(sout), std::move(m_middleware) };
	}

	void router::surrender(const std::string& prefix, handlers& handlers, shandlers& shandlers, filter_list& middlewares)
	{
		for (auto& pair : m_handlers) {
			auto& dst = handlers[pair.first];
			for (auto& handler : pair.second) {
				handler.mask = prefix + handler.mask;
				dst.push_back(std::move(handler));
			}
		}
		for (auto& pair : m_shandlers) {
			auto& dst = shandlers[pair.first];
			for (auto& handler : pair.second) {
				handler.mask = prefix + handler.mask;
				dst.push_back(std::move(handler));
			}
		}
		middlewares.reserve(middlewares.size() + m_middleware.size());
		for (auto& pair : m_middleware) {
			middlewares.emplace_back(prefix + pair.first, std::move(pair.second));
		}

		m_handlers.clear();
		m_shandlers.clear();
		m_middleware.clear();

		for (auto& sub : m_routers)
			sub.sub->surrender(prefix + sub.mask, handlers, shandlers, middlewares);
		m_routers.clear();
	}

	std::shared_ptr<route> router::compiled::find(method m, const std::string& path, std::vector<param>& params)
	{
		auto it = m_routes.find(m);
		if (it == m_routes.end())
			return { };

		for (auto& route : it->second) {
			if (route->matcher().matches(path, params))
				return route;
		}

		return { };
	}

	std::shared_ptr<route> router::compiled::find(const std::string& other_method, const std::string& path, std::vector<param>& params)
	{
		auto it = m_sroutes.find(other_method);
		if (it == m_sroutes.end())
			return { };

		for (auto& route : it->second) {
			if (route->matcher().matches(path, params))
				return route;
		}

		return { };
	}
}