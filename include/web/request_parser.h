/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once
#include <web/headers.h>

namespace web {
	enum class parsing {
		separator,
		error
	};

	struct data_src {
		virtual ~data_src() = default;
		virtual size_t get(char* data, size_t s) = 0;
	};

	class field_parser {
	public:
		static parsing read_line(data_src& src, std::vector<char>& dst);

		parsing decode(data_src&);
		bool rearrange(headers& dst);
	private:
		class span {
		public:
			constexpr span() = default;
			constexpr span(size_t offset, size_t length)
				: m_offset(offset)
				, m_length(length)
			{
			}

			constexpr size_t offset() const { return m_offset; }
			constexpr size_t length() const { return m_length; }
		private:
			size_t m_offset = 0;
			size_t m_length = 0;
			size_t m_hash = 0;
		};

		std::vector<char> m_contents;
		std::vector<std::tuple<span, span>> m_field_list;
		size_t m_last_line_end = 0;

		std::string get(span s)
		{
			return { m_contents.data() + s.offset(), s.length() };
		}
	};

	template <typename Final>
	class http_parser_base {
	public:
		parsing decode(data_src&);
	protected:
		http_version_t m_proto;
		field_parser m_fields;
	};

	template <typename Final>
	inline parsing http_parser_base<Final>::decode(data_src& src)
	{
		auto& thiz = static_cast<Final&>(*this);
		auto ret = thiz.first_line(src);

		if (ret != parsing::separator)
			return ret;

		return m_fields.decode(src);
	}

	class request;

	class request_parser : public http_parser_base<request_parser> {
		friend class http_parser_base<request_parser>;
		parsing first_line(data_src&);
	public:
		bool extract(bool secure, request& req, short unsigned port, const std::string& host_1_0 = { });
	private:
		std::string m_method;
		std::string m_resource;
	};
}
