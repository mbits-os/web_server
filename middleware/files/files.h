/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <string>
#include <web/middleware.h>

namespace web { namespace middleware {
	class files : public middleware_base {
		std::string m_root{};
	protected:
		static result file_helper(std::string const & root, request& req, response& resp);
		files() = default;
	public:
		files(const std::string& root);
		result handle(request& req, response& resp) override;
	};
}}
