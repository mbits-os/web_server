/*
 * Copyright (C) 2015 midnightBITS
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <web/uri.h>
#include <cctype>

namespace web {
	namespace {
		inline std::string to_string(std::string_view sv) {
			return { sv.data(), sv.length() };
		}

		int default_port(std::string_view scheme)
		{
#define KNOWN(proto, port) if (scheme == #proto) return port;

			KNOWN(http, 80); // ~100% of use cases...
			KNOWN(https, 443);

			KNOWN(ftp, 21);
			KNOWN(ssh, 22);
			KNOWN(telnet, 23);

#undef KNOWN
			return -1;
		}

		std::string tolower(std::string s)
		{
			for (auto& c : s)
				c = (char)std::tolower((uint8_t)c);

			return s;
		}

		std::string tolower(std::string_view s)
		{
			return tolower(to_string(s));
		}

		std::vector<std::string_view> path_split(std::string_view path)
		{
			auto length = 1;
			for (auto c : path) {
				if (c == '/')
					++length;
			}

			std::vector<std::string_view> out;
			out.reserve(length);

			auto slash = path.find('/');
			decltype(slash) prev = 0;
			while (slash != std::string_view::npos) {
				out.emplace_back(path.substr(prev, slash - prev));
				prev = slash + 1;
				slash = path.find('/', prev);
			}
			out.emplace_back(path.substr(prev));

			return out;
		}

		std::string path_join(const std::vector<std::string>& chunks)
		{
			if (chunks.empty())
				return { };

			auto length = chunks.size() - 1;
			for (auto& ch : chunks)
				length += ch.length();

			std::string out;
			out.reserve(length);

			bool first = true;
			for (auto& ch : chunks) {
				if (first) first = false;
				else out.push_back('/');

				out.append(ch);
			}

			return out;
		}

		inline bool issafe(unsigned char c)
		{
			return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
		}

		inline bool host_issafe(unsigned char c)
		{
			return issafe(c) || c == ':' || c == '[' || c == ']';
		}

		template <typename Pred>
		inline std::string urlencode(const char* in, size_t in_len, Pred&& safe)
		{
			size_t length = in_len;

			auto b = in;
			auto e = b + in_len;

			for (auto it = b; it != e; ++it) {
				if (!safe(*it))
					length += 2;
			}

			static constexpr char hexes[] = "0123456789ABCDEF";
			std::string out;
			out.reserve(length);

			for (auto it = b; it != e; ++it) {
				auto c = *it;
				if (safe(c)) {
					out += c;
					continue;
				}
				out += '%';
				out += hexes[(c >> 4) & 0xF];
				out += hexes[(c) & 0xF];
			}
			return out;
		}

		inline char hex(char c)
		{
			switch (c) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return c - '0';
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				return c - 'a' + 10;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				return c - 'A' + 10;
			}
			return 0;
		}

		inline std::string host_urlencode(const char* in, size_t in_len)
		{
			return urlencode(in, in_len, host_issafe);
		}

		inline std::string host_urlencode(std::string_view in)
		{
			return host_urlencode(in.data(), in.length());
		}
	}

	std::string urlencode(const char* in, size_t in_len)
	{
		return urlencode(in, in_len, issafe);
	}

	std::string urldecode(const char* in, size_t in_len)
	{
		std::string out;
		out.reserve(in_len);

		for (size_t i = 0; i < in_len; ++i) {
			// go inside only, if there is enough space
			if (in[i] == '%' && (i < in_len - 2) &&
				isxdigit(in[i + 1]) && isxdigit(in[i + 2])) {
				unsigned char c = (hex(in[i + 1]) << 4) | hex(in[i + 2]);
				out += c;
				i += 2;
				continue;
			}
			out += in[i];
		}
		return out;
	}

	uri::auth_builder uri::auth_builder::parse(std::string_view auth)
	{
		auto pos = auth.find('@');
		auto host = pos == std::string_view::npos ? 0 : pos + 1;

		auto colon = auth.rfind(':');
		if (auth.length() > host && auth[host] == '[') {
			// IPv6/IPVFuture...
			auto end = auth.find(']', host);
			if (end == std::string_view::npos)
				return { };

			if (auth.length() > (end + 1) && auth[end + 1] != ':')
				return { };

			colon = end + 1;
			if (colon >= auth.length())
				colon = std::string_view::npos;
		}
		else if (colon < host)
			colon = std::string_view::npos;

		auto host_count = colon == std::string_view::npos ? std::string_view::npos : colon - host;

		auth_builder out{ };

		if (host) {
			auto userInfo = auth.substr(0, pos);
			auto colon = userInfo.find(':');
			out.user = urldecode(userInfo.substr(0, colon));
			if (colon != std::string_view::npos)
				out.password = urldecode(userInfo.substr(colon + 1));
		}

		out.host = urldecode(auth.substr(host, host_count));
		if (colon != std::string_view::npos)
			out.port = urldecode(auth.substr(colon + 1));

		return out;
	}

	std::string uri::auth_builder::string(auth_flag flag) const
	{
		auto auser_name = urlencode(user);
		auto auser_pass = host_urlencode(password);
		auto ahost = host_urlencode(host);
		auto aport = urlencode(port);

		if (flag == no_userinfo)
			auser_name.clear();
		else if (flag == ui_safe)
			auser_pass.clear();

		if (aport.empty() && auser_name.empty())
			return ahost;

		size_t length = ahost.length();
		if (!auser_name.empty()) {
			length += auser_name.length() + 1;
			if (!auser_pass.empty())
				length += auser_pass.length() + 1;
		}
		if (!aport.empty())
			length += aport.length() + 1;

		std::string out;
		out.reserve(length);

		if (!auser_name.empty()) {
			out.append(auser_name);
			if (!auser_pass.empty()) {
				out.push_back(':');
				out.append(auser_pass);
			}
			out.push_back('@');
		}

		out.append(ahost);

		if (!aport.empty()) {
			out.push_back(':');
			out.append(aport);
		}
		return out;
	}


	std::string uri::query_builder::string(query_flag flag) const
	{
		std::string out;
		bool first = true;
		for (auto& pair : m_values) {
			auto name = urlencode(pair.first) + "=";
			for (auto& value : pair.second) {
				if (first) {
					first = false;
					if (flag == start_with_qmark)
						out += "?";
				}
				else out += "&";

				out += name + urlencode(value);
			}
		}
		return out;
	}

	std::vector<std::pair<std::string, std::string>> uri::query_builder::list() const
	{
		size_t length = 0;
		for (auto& pair : m_values) {
			length += pair.second.size();
		}

		std::vector<std::pair<std::string, std::string>> out;
		out.reserve(length);

		for (auto& pair : m_values) {
			for (auto& value : pair.second)
				out.emplace_back(pair.first, value);
		}

		return out;
	}

#define WS() do { while (isspace((unsigned char)*c) && c < end) ++c; } while(0)
#define LOOK_FOR(ch) do { while (!isspace((unsigned char)*c) && *c != (ch) && c < end) ++c; } while(0)
#define LOOK_FOR2(ch1, ch2) do { while (!isspace((unsigned char)*c) && *c != (ch1) && *c != (ch2) && c < end) ++c; } while(0)
#define IS(ch) (c < end && *c == (ch))

	uri::query_builder uri::query_builder::parse(std::string_view query)
	{
		uri::query_builder out;
		const char* c = query.data();
		const char* end = c + query.length();

		if (c < end && *c == '?')
			++c;

		while (c < end) {
			WS();
			const char* name_start = c;
			LOOK_FOR2('=', '&');
			std::string name = urldecode(name_start, c - name_start);
			WS();

			if (IS('=')) {
				++c;
				WS();
				const char* value_start = c;
				LOOK_FOR('&');
				out.add(name, urldecode(value_start, c - value_start));
				WS();
			}
			else
				out.add(name, { });

			if (!IS('&')) break;

			++c;
		}
		return out;
	}

#undef WS
#undef LOOK_FOR
#undef LOOK_FOR2
#undef IS

	uri::uri() = default;
	uri::uri(const uri&) = default;
	uri& uri::operator=(const uri&) = default;

	uri::uri(std::string_view ident)
		: m_uri{ ident.data(), ident.length() }
	{
	}
	uri::uri(const std::string& ident)
		: m_uri{ ident }
	{
	}

	uri::uri(std::string&& ident)
		: m_uri{ std::move(ident) }
	{
	}

	uri::uri(const char* ident)
		: m_uri{ ident }
	{
	}

	uri::uri(uri&& other)
		: m_uri{ std::move(other.m_uri) }
	{
		other.invalidate_scheme();
	}

	uri& uri::operator=(uri&& other)
	{
		m_uri = std::move(other.m_uri);
		other.invalidate_scheme();
		invalidate_scheme();
		return *this;
	}

	void uri::ensure_scheme() const
	{
		if (m_scheme != ncalc)
			return;

		m_scheme = npos;
		auto length = m_uri.length();

		auto b = m_uri.data();
		auto c = b;
		auto e = b + length;

		if (c != e && (c + 1) != e && *c == '/' && c[1] == '/') {
			m_scheme = 0;
			return;
		}

		if (c == e || !isalpha((unsigned char)*c))
			return;

		++c;
		while (c != e && (isalnum((unsigned char)*c) || *c == '+' || *c == '-' || *c == '.'))
			++c;

		if (c == e || *c != ':')
			return;
		++c;

		m_scheme = c - b;
	}

	void uri::ensure_path() const
	{
		if (m_path != ncalc)
			return;

		ensure_scheme();

		if (m_scheme == npos) {
			m_path = 0;
			return;
		}

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_path = m_scheme;

		if (m_scheme + 1 >= length || c[m_scheme] != '/' || c[m_scheme + 1] != '/')
			return;

		m_path = m_scheme + 2;
		while (m_path < length) {
			switch (c[m_path]) {
			case '/': case '?': case '#':
				return;
			}
			++m_path;
		}
	}

	void uri::ensure_query() const
	{
		if (m_query != ncalc)
			return;

		ensure_path();

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_query = m_path;
		while (m_query < length) {
			switch (c[m_query]) {
			case '?': case '#':
				return;
			}
			++m_query;
		}

	}

	void uri::ensure_fragment() const
	{
		if (m_part != ncalc)
			return;

		ensure_query();

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_part = m_query;
		while (m_part < length) {
			if (c[m_part] == '#')
				return;
			++m_part;
		}
	}

	bool uri::hierarchical() const
	{
		ensure_path();

		if (m_scheme == npos)
			return true;

		if (m_path - m_scheme <= 2)
			return false;

		auto c = m_uri.data();
		return c[m_scheme + 1] == '/' && c[m_scheme + 2] == '/';
	}

	bool uri::has_scheme() const
	{
		ensure_scheme();
		return m_scheme != npos;
	}

	bool uri::is_scheme_relative() const
	{
		ensure_scheme();
		return m_scheme == 0;
	}

	bool uri::has_authority() const
	{
		ensure_path();

		// standard behaviour of browsers is
		// that the non-scheme URIs start with path-only
		if (m_scheme == npos)
			return false;

		if (m_path - m_scheme <= 2)
			return false; // no space for //

		auto c = m_uri.data();
		return c[m_scheme] == '/' && c[m_scheme + 1] == '/';
	}

	std::string_view substr(const std::string& s, size_t off, size_t len)
	{
		return { s.data() + off, len };
	}

	std::string_view substr(const std::string& s, size_t off)
	{
		return { s.data() + off, s.length() - off };
	}

	std::string_view uri::scheme() const
	{
		if (!has_scheme() || !m_scheme)
			return {};

		return substr(m_uri, 0, m_scheme - 1);
	}

	std::string_view uri::authority() const
	{
		if (!has_scheme())
			return {};

		if (is_opaque())
			return {};

		auto start = m_scheme + 2;
		return substr(m_uri, start, m_path - start);
	}

	std::string_view uri::path() const
	{
		ensure_query();
		return substr(m_uri, m_path, m_query - m_path);
	}

	std::string_view uri::query() const
	{
		ensure_fragment();
		return substr(m_uri, m_query, m_part - m_query);
	}

	std::string_view uri::resource() const
	{
		ensure_fragment();
		return substr(m_uri, m_path, m_part - m_path);
	}

	std::string_view uri::fragment() const
	{
		ensure_fragment();
		return substr(m_uri, m_part);
	}

	void uri::scheme(std::string_view value)
	{
		ensure_scheme();
		if (m_scheme == npos)
			return;

		if (m_scheme)
			m_uri.replace(0, m_scheme - 1, value.data(), value.length());
		else {
			m_uri.replace(0, 0, value.data(), value.length());
			m_uri.insert(value.length(), 1, ':');
		}
		invalidate_scheme();
	}

	void uri::authority(std::string_view value)
	{
		if (is_opaque())
			return;

		auto start = m_scheme + 2;
		m_uri.replace(start, m_path - start, value.data(), value.length());
		invalidate_path();
	}

	void uri::path(std::string_view value)
	{
		ensure_query();
		if (has_authority() && (value.empty() || value[0] != '/')) {
			m_uri.replace(m_path, m_query - m_path, "/");
			++m_path;
			m_query = m_path;
		}
		m_uri.replace(m_path, m_query - m_path, value.data(), value.length());
		invalidate_path(); // query -> path due to having possibly taken the '/' branch and having ++m_path
	}

	void uri::query(std::string_view value)
	{
		ensure_fragment();
		m_uri.replace(m_query, m_part - m_query, value.data(), value.length());
		invalidate_fragment();
	}

	void uri::fragment(std::string_view value)
	{
		ensure_fragment();
		m_uri.replace(m_part, m_uri.length() - m_part, value.data(), value.length());
	}

	std::string_view remove_filename(std::string_view path)
	{
		auto find = path.rfind('/');
		if (find == std::string_view::npos)
			return path;
		return path.substr(0, find + 1);
	}

	uri uri::make_base(const uri& document)
	{
		if (document.string().empty())
			return document;

		auto tmp = document;
		if (!tmp.has_scheme())
			tmp = uri{ "http://" + tmp.string() };

		tmp.fragment(std::string_view());
		tmp.query(std::string_view());
		tmp.path(remove_filename(tmp.path()));
		tmp.ensure_query();
		return tmp;
	}


	uri uri::canonical(const uri& identifier, const uri& base, auth_flag flag)
	{
		if (identifier.has_authority()) {
			if (identifier.has_scheme() && !identifier.is_scheme_relative())
				return normal(identifier, flag);

			if (!base.has_scheme())
				return normal(identifier, flag);
			// base-scheme://ident-auth/ident-path?ident-query#ident-frag
			auto temp = identifier;
			temp.scheme(base.scheme());
			return normal(std::move(temp), flag);
		}

		if (identifier.has_scheme()) {
			if (!base.has_scheme() || tolower(identifier.scheme()) != tolower(base.scheme()))
				return normal(identifier, flag);
		}

		// base-scheme://base-auth/base-path?ident-query#ident-frag
		auto temp = base;
		temp.fragment(identifier.fragment());
		temp.query(identifier.query());

		auto path = identifier.path();
		if (!path.empty() && path[0] == '/')
			return temp.path(path), normal(std::move(temp), flag);

		auto bpath = base.path();
		if (!bpath.empty())
			return temp.path(to_string(base.path()) + "/" + to_string(path)), normal(std::move(temp), flag);
		return temp.path(path), normal(std::move(temp), flag);
	}

	uri uri::normal(uri tmp, auth_flag flag)
	{
		tmp.ensure_path();
		if (tmp.m_scheme != npos) {
			auto scheme = tolower(to_string(tmp.scheme()));
			tmp.scheme(scheme);
		}

		if (tmp.has_authority()) {
			auto auth = auth_builder::parse(tmp.authority());
			if (auth.host.empty()) // decoding failed
				return { };

			// HOST: =======================================================================================
			auth.host = tolower(auth.host);

			// is IPv4, IPv6 or reg-name?
			for (auto c : auth.host) {
				if (!isalnum((uint8_t)c) && c != '-' && c != '.' && c != '[' && c != ']' && c != ':')
					return { };
			}

			// PORT: =======================================================================================
			// empty or digits
			for (auto c : auth.port) {
				if (!isdigit((uint8_t)c))
					return { };
			}

			// if default for the scheme, remove
			if (!auth.port.empty()) {
				auto port = atoi(auth.port.data());
				auto def = default_port(tmp.scheme());
				if (port == def)
					auth.port.clear();
			}

			tmp.authority(auth.string(flag));
		}
		// PATH: =======================================================================================
		if (tmp.has_authority() || !tmp.has_scheme()) {
			// the "://" uris will have paths with slashes for sure;
			// the "no-scheme, no-auth" URIs start with path with slashes (see has_authority());
			// cannot say that about other paths (e.g. user@server in mailto:user@server)

			auto path = path_split(tmp.path());
			std::vector<std::string> recoded;
			recoded.reserve(path.size());
			for (auto& part : path)
				recoded.push_back(urlencode(urldecode(part)));

			bool absolute_path = (recoded.size() > 1) && recoded.front().empty();

			// URL ended with slash; should still end with slash
			// Also, URL path ended with either xxxx/. or xxxx/..
			// -> after resolving the result should be a "dir"
			bool empty_at_end = (recoded.size() > 1) &&
				(recoded.back().empty() || recoded.back() == "." || recoded.back() == "..");
			decltype(recoded) canon;
			canon.reserve(recoded.size());

			decltype(recoded) overshots;
			for (auto& p : recoded) {
				if (p.empty() || p == ".")
					continue;

				if (p == "..") {
					if (canon.empty())
						overshots.push_back(std::move(p));
					else
						canon.pop_back();
					continue;
				}

				canon.push_back(std::move(p));
			}
			if (empty_at_end)
				canon.emplace_back();
			if (absolute_path)
				tmp.path("/" + path_join(canon));
			else {
				canon.insert(canon.begin(), overshots.begin(), overshots.end());
				tmp.path(path_join(canon));
			}
		}

		return tmp;
	}
}
