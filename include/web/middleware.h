/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <web/request.h>
#include <web/response.h>

namespace web {
	struct middleware_base {
		enum result {
			carry_on = true,
			finished = false
		};
		virtual ~middleware_base() = default;
		virtual result handle(request& req, response& resp) = 0;
	};
}
