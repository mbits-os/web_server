/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace web {
	enum class header {
		empty,
		extension_header,
		// [Request header](https://tools.ietf.org/html/rfc2616#section-5.3)
		Accept,
		Accept_Charset,
		Accept_Encoding,
		Accept_Language,
		Authorization,
		Expect,
		From,
		Host,
		If_Match,
		If_Modified_Since,
		If_None_Match,
		If_Range,
		If_Unmodified_Since,
		Max_Forwards,
		Proxy_Authorization,
		Range,
		Referer,
		TE,
		User_Agent,

		// [Response header](https://tools.ietf.org/html/rfc2616#section-6.2)
		Accept_Ranges,
		Age,
		ETag,
		Location,
		Proxy_Authenticate,
		Retry_After,
		Server,
		Vary,
		WWW_Authenticate,

		// [Entity header](https://tools.ietf.org/html/rfc2616#section-7.1)
		Allow,
		Content_Encoding,
		Content_Language,
		Content_Length,
		Content_Location,
		Content_MD5,
		Content_Range,
		Content_Type,
		Expires,
		Last_Modified,

		// [General header](https://tools.ietf.org/html/rfc2616#section-14)
		Cache_Control,
		Connection,
		Date,
		Pragma,
		Trailer,
		Transfer_Encoding,
		Upgrade,
		Via,
		Warning,

		// Cookie headers:
		Cookie,
		Set_Cookie
	};

	class header_key {
		header m_header = header::empty;
		std::string m_extension;
		header_key(const std::string& ex)
			: m_header(header::extension_header), m_extension(ex)
		{
		}
	public:
		header_key(header h)
			: m_header(h)
		{
		}

		header value() const { return m_header; }
		const std::string& extension() const { return m_extension; }

		static header_key make(std::string);
		static const char* name(header);
		const char* name() const;

		bool empty() const { return m_header == header::empty; }
		bool extension_header() const { return m_header == header::extension_header; }
		bool operator == (const header_key& oth) const
		{
			return m_header == header::extension_header
				? (oth.m_header == header::extension_header && m_extension == oth.m_extension)
				: m_header == oth.m_header;
		}
		bool operator != (const header_key& oth) const
		{
			return !(*this == oth);
		}

		bool operator < (const header_key& oth) const
		{
			if (m_header == oth.m_header) {
				if (m_header == header::extension_header)
					return m_extension < oth.m_extension;
				return false;
			}
			return m_header < oth.m_header;
		}
	};
}

namespace std {
	template <>
	struct hash<web::header_key> {
		size_t operator()(const web::header_key& key) const noexcept(
			noexcept(hash<web::header>{}(web::header::Accept)) &&
			noexcept(hash<std::string>{}(std::string { }))
			)
		{
			if (key == web::header::extension_header)
				return hash<web::header>{}(key.value());
			return hash<std::string>{}(key.extension());
		}
	};
}

namespace web {
	class headers {
		std::unordered_map<header_key, std::vector<std::string>> m_headers;
	public:
		void add(const header_key& key, const std::string& value)
		{
			m_headers[key].emplace_back(value);
		}
		void add(const header_key& key, std::string&& value)
		{
			m_headers[key].emplace_back(std::move(value));
		}
		void erase(const header_key& key)
		{
			m_headers.erase(key);
		}

		auto empty() const { return m_headers.empty(); }
		auto size() const { return m_headers.size(); }
		auto find(const header_key& key) const { return m_headers.find(key); }
		auto begin() const { return m_headers.begin(); }
		auto end() const { return m_headers.end(); }
		void clear() { m_headers.clear(); }

		bool has(const header_key& key) const
		{
			auto it = find(key);
			return it != end() && !it->second.empty();
		}

		const std::string* find_front(const header_key& key) const
		{
			auto it = find(key);
			if (it != end() && !it->second.empty())
				return &it->second.front();
			return nullptr;
		}
	};

	namespace http_version {
		struct version_t {
			version_t() = default;
			constexpr version_t(uint16_t M_ver, uint16_t m_ver)
				: version { (static_cast<uint32_t>(M_ver) << 16) | static_cast<uint32_t>(m_ver) }
			{
			}

			constexpr inline uint16_t M_ver() const
			{
				return static_cast<uint16_t>((version >> 16) & 0xFFFF);
			}

			constexpr inline uint16_t m_ver() const
			{
				return static_cast<uint16_t>((version) & 0xFFFF);
			}

			bool operator==(const version_t& rhs) const { return version == rhs.version; }
			bool operator!=(const version_t& rhs) const { return version != rhs.version; }
			bool operator>=(const version_t& rhs) const { return version >= rhs.version; }
			bool operator<=(const version_t& rhs) const { return version <= rhs.version; }
			bool operator>(const version_t& rhs) const { return version > rhs.version; }
			bool operator<(const version_t& rhs) const { return version < rhs.version; }
		private:
			uint32_t version = 0;
		};


		static constexpr version_t http_none { 0, 0 };
		static constexpr version_t http_1_0 { 1, 0 };
		static constexpr version_t http_1_1 { 1, 1 };
	}
	using http_version_t = http_version::version_t;
}
