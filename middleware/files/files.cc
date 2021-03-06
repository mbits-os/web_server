/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include "files.h"
#include <cassert>
#include <sys/stat.h>

#ifdef WIN32
constexpr char DIRSEP = '\\';
#else
constexpr char DIRSEP = '/';
#endif

namespace web { namespace middleware {
	files::files(const std::string& root)
		: m_root(root)
	{
		assert(!m_root.empty());
		auto last = m_root[m_root.length() - 1];
		if (last == DIRSEP) {
			m_root.pop_back();
			assert(!m_root.empty());
		}
	}

	middleware_base::result files::file_helper(std::string const & path, request& req, response& resp)
	{
		struct stat st;
		if (!stat(path.c_str(), &st)) {
			auto mode = st.st_mode & S_IFMT;

			auto m = req.method();
			if (m != method::get && m != method::head) {
				resp.add(header::Allow, "GET,HEAD");
				resp.stock_response(status::method_not_allowed);
				return finished;
			}
			if ((mode & S_IFDIR) == S_IFDIR) {
				// try index:
				bool expanded = false;
				auto ndx = path;
				if (ndx[ndx.length() - 1] != DIRSEP) {
					expanded = true;
					ndx.push_back(DIRSEP);
				}
				ndx += "index.html";
				if (!stat(ndx.c_str(), &st)) {
					mode = st.st_mode & S_IFMT;
					if ((mode & S_IFDIR) != S_IFDIR) {
						if (expanded) {
							auto uri = req.uri();
							auto const path_view = uri.path();
							uri.path(std::string{ path_view.data(), path_view.length() } + "/");
							resp.add(header::Location, uri.string());
							resp.stock_response(status::moved_permanently);
						} else {
							resp.send_file(ndx);
						}
						return finished;
					}
				}
				return carry_on;
			}
			resp.send_file(path);
			return finished;
		}

		return carry_on;
	}

	middleware_base::result files::handle(request& req, response& resp)
	{
		auto const res_view = req.uri().path();
#ifdef WIN32
		auto resource = std::string{ res_view.data(), res_view.length() };
		for (auto& c : resource) {
			if (c == '/')
				c = '\\';
		}
		auto path = m_root + resource;
#else
		auto path = m_root + std::string{ res_view.data(), res_view.length() };
#endif

		return file_helper(path, req, resp);
	}
}}
