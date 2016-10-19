/*
 * Copyright (C) 2016 midnightBITS
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

/**
Non-owning string.
\file
\author Marcin Zdun <mzdun@midnightbits.com>

This class implements the cstring type, an iterable non-mutable
string of characters.
*/

#pragma once

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

namespace web {
	class cstring {
		static constexpr size_t get_length(const char* s)
		{
			return *s ? 1 + get_length(s + 1) : 0;
		}
	public:
		using size_type = size_t;
		using reference = const char&;
		using pointer = const char*;
		using const_iterator = pointer;
		using traits_type = std::char_traits<char>;

		static const size_type dynamic_range = (size_type)-1;
		static const size_type npos = (size_type)-1;

		constexpr cstring() noexcept
			: m_data(nullptr)
			, m_size(0)
		{
		}

		constexpr cstring(const char* d, size_type s) noexcept
			: m_data(d)
			, m_size(s)
		{
		}

		template <size_type s>
		constexpr cstring(const char (&d)[s]) noexcept
			: m_data(d)
			, m_size(s - 1)
		{
		}

		constexpr cstring(const char* d) noexcept
			: m_data(d)
			, m_size(d ? get_length(d) : 0)
		{
		}

		cstring(const std::string& s) noexcept
			: m_data(s.data())
			, m_size(s.length())
		{
		}

		constexpr cstring subspan(size_type offset, size_type count = npos) const noexcept
		{
			return (offset < length()
				? (count <= (length() - offset)
					? cstring { data() + offset, count }
					: cstring { data() + offset, length() - offset })
				: cstring { });
		}

		constexpr cstring first(size_type count) const noexcept { return subspan(0, count); }
		constexpr cstring last(size_type count) const noexcept { return subspan(length() - count, count); }

		constexpr bool empty() const noexcept { return !m_size; }
		constexpr const char* data() const noexcept { return m_data; }
		const char* c_str() const noexcept { return m_data; }
		constexpr size_type size() const noexcept { return m_size; }
		constexpr size_type length() const noexcept { return m_size; }
		constexpr reference operator[](size_type ndx) const noexcept { return m_data[ndx]; }

		constexpr const_iterator begin() const noexcept { return m_data; }
		constexpr const_iterator end() const noexcept { return m_data + m_size; }

		std::string str() const { return { data(), length() }; }

		int compare(const cstring& rhs) const
		{
			auto hint = std::strncmp(c_str(), rhs.c_str(), std::min(length(), rhs.length()));
			if (hint)
				return hint;
			return ((int)length()) - ((int)rhs.length());
		}
		bool operator == (const cstring& rhs) const { return compare(rhs) == 0; }
		bool operator != (const cstring& rhs) const { return compare(rhs) != 0; }
		bool operator < (const cstring& rhs) const { return compare(rhs) < 0; }
		bool operator > (const cstring& rhs) const { return compare(rhs) > 0; }

		size_type find(char c, size_type offset = 0) const
		{
			return find(&c, offset, 1);
		}
		size_type find(const char* s, size_type offset = 0) const
		{
			return find(s, offset, traits_type::length(s));
		}
		size_type find(const char* s, size_type off, size_type len) const
		{
			if (!len && off < length())
				return off; // empty string always matches

			size_type space = off < length() ? length() - off : 0;
			if (len <= space) { // enough space?
				space -= len - 1;
				auto from = data() + off;
				auto test = traits_type::find(from, space, *s);
				while (test) {
					if (!traits_type::compare(test, s, len))
						return test - data(); // match found
					space -= test - from + 1;
					from = test + 1;
					test = traits_type::find(from, space, *s);
				}
			}

			return npos; // no match
		}
		size_type rfind(char c, size_type offset = npos) const
		{
			return rfind(&c, offset, 1);
		}
		size_type rfind(const char* s, size_type offset = npos) const
		{
			return rfind(s, offset, traits_type::length(s));
		}
		size_type rfind(const char* s, size_type off, size_type len) const
		{
			if (!len) // empty string always matches
				return off < length() ? off : length();

			if (len <= length()) { // enough space?
				auto max_off = length() - len;
				auto test = data() + (off < max_off ? off : max_off);
				while (true) {
					if (*test == *s && !traits_type::compare(test, s, len))
						return test - data();
					else if (test == data())
						break;
					--test;
				}
			}

			return npos; // no match
		}

		friend inline std::ostream& operator<<(std::ostream& o, const web::cstring& s)
		{
			std::ios_base::iostate state = std::ios_base::goodbit;

			const std::ostream::sentry ok(o);
			if (!ok) state |= std::ios_base::badbit;

			if (state == std::ios_base::goodbit &&
				o.rdbuf()->sputn(s.data(), s.length()) != s.length()) {
				state |= std::ios_base::badbit;
			};

			o.setstate(state);
			return o;
		}

		friend inline std::string operator+(const web::cstring& rhs, const web::cstring& lhs)
		{
			auto out = rhs.str();
			out.append(lhs.c_str(), lhs.length());
			return out;
		}

		friend inline std::string operator+(const std::string& rhs, const web::cstring& lhs)
		{
			auto out = rhs;
			out.append(lhs.c_str(), lhs.length());
			return out;
		}

	private:
		const char* m_data;
		size_type m_size;
	};

	inline std::string to_string(const cstring& h)
	{
		return h.str();
	}
}
