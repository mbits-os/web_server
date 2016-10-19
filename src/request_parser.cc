/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <web/request_parser.h>
#include <web/request.h>
#include <algorithm>
#include <cctype>

namespace web {
	namespace {
		template <typename V>
		auto find(V& contents, const char* sub)
		{
			auto cur = std::begin(contents);
			auto end = std::end(contents);

			if (!sub || !*sub)
				return end;

			auto len = strlen(sub);
			do {
				cur = std::find(cur, end, *sub);
				if (cur == end) break;
				if (size_t(end - cur) < len) break;
				if (!memcmp(&*cur, sub, len))
					return cur;
				++cur;
			} while (true);
			return end;
		}

		inline size_t report_read(size_t prev, size_t position)
		{
			return (position > prev) ? prev - position : 0;
		}

		std::string printable(std::string&& s)
		{
			for (auto& c : s)
				c = isprint((uint8_t)c) ? c : '.';
			return s;
		}

		std::string produce(const std::string& cs_)
		{
			auto ptr = cs_.c_str();
			auto len = cs_.length();
			auto end = ptr + len;

			while (ptr != end && std::isspace((uint8_t)*ptr)) {
				++ptr;
				--len;
			}
			while (ptr != end && std::isspace((uint8_t)end[-1])) {
				--end;
				--len;
			}

			bool in_cont = false;
			for (auto cur = ptr; cur != end; ++cur) {
				uint8_t uc = *cur;
				if (in_cont) {
					in_cont = !!std::isspace(uc);
					if (in_cont)
						--len;
					continue;
				}
				if (uc == '\r') {
					in_cont = true;
					// while (ptr != cur && std::isspace((uint8_t)cur[-1])) {
					// 	--cur;
					// 	--len;
					// }
					continue;
				}
			}

			std::string out;
			out.reserve(len);

			in_cont = false;
			for (auto cur = ptr; cur != end; ++cur) {
				uint8_t uc = *cur;
				if (in_cont) {
					in_cont = !!std::isspace(uc);
					if (in_cont)
						--len;
					else
						out.push_back(uc);
					continue;
				}
				if (uc == '\r') {
					in_cont = true;
					while (ptr != cur && std::isspace((uint8_t)cur[-1])) {
						--cur;
						out.pop_back();
					}
					out.push_back(' ');
					continue;
				}
				out.push_back(uc);
			}

			out.shrink_to_fit();
			return out;
		}

		std::string lower(std::string&& in)
		{
			for (auto& c : in)
				c = std::tolower((uint8_t)c);
			return in;
		}

		template <typename It, typename Int>
		It parse_number(It cur, It end, Int& val)
		{
			val = 0;
			while (cur != end) {
				switch (*cur) {
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					val *= 10;
					val += *cur++ - '0';
					break;
				default:
					return cur;
				}
			}
			return cur;
		}

#define TEST_CHAR(c) if (cur == end || *cur != (c)) return false; ++cur;
		template <typename It>
		bool parse_proto(It cur, It end, http_version_t& var)
		{
			TEST_CHAR('H');
			TEST_CHAR('T');
			TEST_CHAR('T');
			TEST_CHAR('P');
			TEST_CHAR('/');

			int16_t http_major = 0, http_minor = 0;
			auto next = parse_number(cur, end, http_major);
			if (next == cur) return false;
			cur = next;

			TEST_CHAR('.');

			next = parse_number(cur, end, http_minor);

			var = http_version_t(http_major, http_minor);
			return next != cur && next == end;
		}
#undef TEST_CHAR
	}

	std::pair<size_t, parsing> field_parser::append(const char* data, size_t length)
	{
		if (!length)
			return { 0, parsing::reading };

		auto prev = m_contents.size();
		m_contents.insert(m_contents.end(), data, data + length);

		auto begin = std::begin(m_contents);
		auto cur = std::next(begin, m_last_line_end);
		auto end = std::end(m_contents);

		while (cur != end) {
			auto it = std::find(cur, end, '\r');
			if (it == end) break;
			if (std::next(it) == end) break;
			if (*std::next(it) != '\n') // mid-line \r? - check with RFC if ignore, or error
				return { report_read(prev, std::distance(begin, it)), parsing::error };

			if (it == cur) { // empty line
				//if (!rearrange(dst))
				//	return { report_read(prev, std::distance(begin, it)), parsing::error };
				return { report_read(prev, std::distance(begin, it)), parsing::separator };
			}

			std::advance(it, 2);
			if (isspace((uint8_t)*cur)) {
				if (m_field_list.empty())
					return { report_read(prev, std::distance(begin, it)), parsing::error };

				m_last_line_end = std::distance(begin, it);
				auto& fld = std::get<1>(m_field_list.back());
				fld = span(fld.offset(), m_last_line_end - fld.offset());
			} else {
				auto colon = std::find(cur, it, ':');
				if (colon == it) // no colon in field's first line
					return { report_read(prev, std::distance(begin, it)), parsing::error };

				m_last_line_end = std::distance(begin, it);
				m_field_list.emplace_back(
					span(std::distance(begin, cur), std::distance(cur, colon)),
					span(std::distance(begin, colon) + 1, std::distance(colon, it) - 1)
				);
			}

			cur = it;
		}
		return { length, parsing::reading };
	}

	bool field_parser::rearrange(headers& dst)
	{
		dst.clear();

		for (auto& pair : m_field_list) {
			auto key = header_key::make(produce(get(std::get<0>(pair))));
			auto value = produce(get(std::get<1>(pair)));
			dst.add(key, std::move(value));
		}

		m_field_list.clear();
		m_field_list.shrink_to_fit();
		m_contents.clear();
		m_contents.shrink_to_fit();
		m_last_line_end = 0;
		return true;
	}

	std::pair<size_t, parsing> request_parser::first_line(const char* data, size_t length)
	{
		// Method SP Request-URI SP HTTP-Verson CRLF
		auto cur = data;
		auto end = data + length;
		size_t len = 1;

		if (m_resource.empty() || m_resource.back() != '\r') {
			auto it = std::find(cur, end, '\r');
			if (it == end || it == (end - 1)) {
				m_resource.append(data, length);
				return { length, parsing::reading };
			}
			if (it[1] != '\n')
				return { (it - data), parsing::error };
			m_resource.append(data, it + 1);
			len += it - data + 1;
		} else if (!length)
			return { 0, parsing::reading };
		else if (*data != '\n')
			return { 0, parsing::error };

		m_resource.pop_back();

		auto proto_pos = m_resource.find_last_of(' ');
		if (proto_pos == std::string::npos)
			return { len, parsing::error };
		auto method_pos = m_resource.find_first_of(' ');
		if (proto_pos == method_pos)
			return { len, parsing::error };

		if (!parse_proto(
			std::next(m_resource.begin(), proto_pos + 1),
			m_resource.end(), m_proto)) {
			return { len, parsing::error };
		}

		m_method = m_resource.substr(0, method_pos);

		while (method_pos < proto_pos && m_resource[method_pos] == ' ')
			++method_pos;
		while (method_pos < proto_pos && m_resource[proto_pos - 1] == ' ')
			--proto_pos;

		if (method_pos == proto_pos)
			return { len, parsing::error };

		m_resource = m_resource.substr(method_pos, proto_pos - method_pos);
		return { len, parsing::separator };
	}

	bool request_parser::extract(bool secure, request& req, short unsigned port, const std::string& host_1_0)
	{
		auto m = make_method(m_method);
		if (m == method::other)
			req.m_smethod = std::move(m_method);
		else
			m_method.clear(), req.m_method = m;

		if (!m_fields.rearrange(req.m_headers))
			return false;

		std::string uri { "http" };
		if (secure)
			uri.push_back('s');
		uri.append("://");
		if (m_proto.M_ver() < 1 || (m_proto.M_ver() == 1 && m_proto.m_ver() == 0)) {
			uri.append(host_1_0);
		} else if (m_proto == http_version::http_1_1) {
			auto host = req.host();
			if (!host)
				return false;
			uri.append(*host);
		}
		auto host = web::uri { uri };
		auto auth = web::uri::auth_builder::parse(host.authority());
		auth.port = std::to_string(port);
		host.authority(auth.string(web::uri::with_pass));

		req.m_uri = web::uri::canonical(m_resource, host, web::uri::with_pass);
		req.m_version = m_proto;
		return true;
	}
}
