#pragma once
#include <web/headers.h>

namespace web {
	enum class parsing {
		reading,
		separator,
		error
	};

	class field_parser {
	public:
		std::pair<size_t, parsing> append(const char* data, size_t length);
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
		std::pair<size_t, parsing> append(const char* data, size_t length);
	protected:
		http_version_t m_proto;
		field_parser m_fields;
	private:
		bool m_needs_first_line = true;
	};

	template <typename Final>
	inline std::pair<size_t, parsing> http_parser_base<Final>::append(const char* data, size_t length)
	{
		if (m_needs_first_line) {
			auto& thiz = static_cast<Final&>(*this);
			auto ret = thiz.first_line(data, length);

			if (std::get<parsing>(ret) != parsing::separator)
				return ret;

			m_needs_first_line = false;
			auto offset = std::get<size_t>(ret);
			ret = m_fields.append(data + offset, length - offset);
			std::get<size_t>(ret) += offset;
			return ret;
		}
		return m_fields.append(data, length);
	}

	class request;
	class request_parser : public http_parser_base<request_parser> {
		friend class http_parser_base<request_parser>;
		std::pair<size_t, parsing> first_line(const char* data, size_t length);
	public:
		bool extract(request& req);
	private:
		std::string m_method;
		std::string m_resource;
	};
}
