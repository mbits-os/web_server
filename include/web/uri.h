/*
 * Copyright (C) 2013 midnightBITS
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

#pragma once

/**
URI code.
\file
\author Marcin Zdun <mzdun@midnightbits.com>

This class implements the uri type, a structure allowing
manipulation of resource addresses, with some helper
utilities.
*/

#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>

namespace web {
	/**
	Prepares the input range to be safely used as a part
	of URL.

	Converts all characters to their hex counterparts (%%XX),
	except for the unreserved ones: letters, digits,
	<code>"-"</code>, <code>"."</code>, <code>"_"</code>,
	<code>"~"</code>. This function, together with urldecode,
	can be used for URI normalization.

	\param in address of buffer to encode, out of which
	          <code>in_len</code> must be readable
	\param in_len the length of the buffer
	\return percent-encoded string containing no reserved
	        characters, except for the percent sign.

	\see RFC3986, section 2.1, Percent-Encoding
	\see RFC3986, section 2.3, Unreserved Characters
	\see urldecode(const char*,size_t)
	*/
	std::string urlencode(const char* in, size_t in_len);

	/**
	Removes all percent-encodings from the input range.

	Converts all encoded octets in form of %%XX to their
	raw form.

	\param in address of buffer to decode, out of which
	          <code>in_len</code> must be readable
	\param in_len the length of the buffer
	\return percent-decoded string, which may include some
	        registered characters.

	\see RFC3986, section 2.1, Percent-Encoding
	\see web::urlencode(const char*,size_t)
	*/
	std::string urldecode(const char* in, size_t in_len);

	/**
	Prepares the input string to be safely used as a part
	of URL.

	A version of urlencode(const char*, size_t) taking an
	STL string as input range.

	\param in an STL string to encode
	\return percent-encoded string containing no reserved
	        characters, except for the percent sign.

	\see urlencode(const char*, size_t)
	*/
	inline std::string urlencode(const std::string& in)
	{
		return urlencode(in.c_str(), in.length());
	}

	/**
	Removes all percent-encodings from the input range.

	A version of urldecode(const char*, size_t) taking an
	STL string as input range.

	\param in an STL string to decode
	\return percent-decoded string, which may include some
	        registered characters.

	\see urldecode(const char*, size_t)
	*/
	inline std::string urldecode(const std::string& in)
	{
		return urldecode(in.c_str(), in.length());
	}

	/**
	Prepares the input string to be safely used as a part
	of URL.

	A version of urlencode(const char*, size_t) taking a
	std::string_view as input range.

	\param in a std::string_view to encode
	\return percent-encoded string containing no reserved
	        characters, except for the percent sign.

	\see urlencode(const char*, size_t)
	*/
	inline std::string urlencode(std::string_view in)
	{
		return urlencode(in.data(), in.length());
	}

	/**
	Removes all percent-encodings from the input range.

	A version of urldecode(const char*, size_t) taking a
	std::string_view as input range.

	\param in a std::string_view to decode
	\return percent-decoded string, which may include some
	        registered characters.

	\see urldecode(const char*, size_t)
	*/
	inline std::string urldecode(std::string_view in)
	{
		return urldecode(in.data(), in.length());
	}

	/**
	Unified Resource Identifier class.

	Contains API for:
	- reading and setting URI components,
	- parsing of authority and query components
	- canonization and normalization
	*/
	class uri {
#ifndef USING_DOXYGEN
		std::string m_uri;
		using size_type = std::string::size_type;

		static constexpr size_type ncalc = static_cast<size_type>(-2);
		static constexpr size_type npos = static_cast<size_type>(-1);

		mutable size_type m_scheme = ncalc;
		mutable size_type m_path = ncalc;
		mutable size_type m_query = ncalc;
		mutable size_type m_part = ncalc;

		void ensure_scheme() const;
		void ensure_path() const;
		void ensure_query() const;
		void ensure_fragment() const;

		void invalidate_fragment()
		{
			m_part = ncalc;
		}

		void invalidate_query()
		{
			invalidate_fragment();
			m_query = ncalc;
		}

		void invalidate_path()
		{
			invalidate_query();
			m_path = ncalc;
		}

		void invalidate_scheme()
		{
			invalidate_path();
			m_scheme = ncalc;
		}
#endif

	public:
		uri(); /**< Constructs empty uri */
		uri(const uri&); /**< Copy-constructs an uri */
		uri(uri&&); /**< Move-constructs an uri */
		uri& operator=(const uri&); /**< Copy-assigns an uri */
		uri& operator=(uri&&); /**< Move-assigns an uri */

		/**
		Constructs an uri from the identifier.
		\param ident a string representing the uri
		*/
		uri(std::string_view ident);

		/**
		Constructs an uri from the identifier.
		\param ident a string representing the uri
		*/
		uri(const std::string& ident);

		/**
		Constructs an uri from the identifier.
		\param ident a string representing the uri
		*/
		uri(std::string&& ident);

		/**
		Constructs an uri from the identifier.
		\param ident a string representing the uri
		*/
		uri(const char* ident);

		/** Flags for auth_builder::string */
		enum auth_flag {
			ui_safe, /**< Flag for encoding the user info for UI */
			with_pass, /**< Flag for encoding the user info for transfer (highly unsafe) */
			no_userinfo /**< Flag for encoding the authority with no user info at all */
		};

		/**
		Helper class for reading and setting
		authority component.
		*/
		struct auth_builder {
			std::string user; /**< User information - name */
			std::string password; /**< User information - password */
			std::string host; /**< Host name - either IPv4, IPv6 or registered name */
			std::string port; /**< String representation of the port on host */

			/**
			Parses authority component into its segments.
			\param auth an authority component to be parsed
			\result a parsed authority with parts, if
			        successful; an authority with empty
					host on error
			*/
			static auth_builder parse(std::string_view auth);

			/**
			Generates an authority component from its members.

			Members, if present, are percent-encoded and joined together.

			\param flag chooses, what to do with user password, if present;
			            by default, the password is not placed in authority
			            string
			\result an authority component, which may be placed inside
			        an uri object
			*/
			std::string string(auth_flag flag = ui_safe) const;
		};

		/** Flags for query_builder::string */
		enum query_flag {
			form_urlencoded = false, /**< Flag for encoding fields from a form to place in request body */
			start_with_qmark = true, /**< Flag for encoding fields to be part of an uri */
		};

		/**
		Helper class for parsing, updating and setting
		query component.
		*/
		struct query_builder {
#ifndef USING_DOXYGEN
			std::unordered_map<std::string, std::vector<std::string>> m_values;
#endif
		public:
			/**
			Parses a query component to a builder
			\param query a query component to be parsed
			\result a parsed query with decoded name/value pairs
			*/
			static query_builder parse(std::string_view query);

			/**
			Adds a new name/value pair.
			\param name a name of the field to add
			\param value a value of the field
			\result a builder reference to chain the calls together
			*/
			query_builder& add(const std::string& name, const std::string& value)
			{
				m_values[name].push_back(value);
				return *this;
			}

			/**
			Removes all fields with given name
			\param name a name of the field to remove
			\result a builder reference to chain the calls together
			*/
			query_builder& remove(const std::string& name)
			{
				auto it = m_values.find(name);
				if (it != m_values.end())
					m_values.erase(it);
				return *this;
			}

			/**
			Builds a resulting query string for URI or for form request.
			\param flag chooses, if the resulting string should start
			            with question mark, or not; by default, it does
			\result encoded string created from all fields in the builder
			*/
			std::string string(query_flag flag = start_with_qmark) const;

			/**
			Creates a list of all fields for individual access.
			\result a vector of name/value pairs
			*/
			std::vector<std::pair<std::string, std::string>> list() const;

			std::vector<std::string> const* find(std::string const& key) const {
				auto it = m_values.find(key);
				if (it == m_values.end())
					return nullptr;
				return &it->second;
			}

			std::string const* find_front(std::string const& key) const {
				auto vals = find(key);
				if (!vals || vals->empty())
					return nullptr;
				return &vals->front();
			}
		};

		/**
		\returns the same value, as #web::uri::has_authority()
		\deprecated Original designed mixed "hierarchical" (<i>an URI with a
		            path</i> - AKA all of them) with "URI with an authority"
		            (<i>an URI with <code>://</code></i>).
		*/
		[[deprecated("The name has nothing to do with behavior. "
			"Use has_authority() instead.")]]
		bool hierarchical() const;

		/**
		\returns reverses the return value of #web::uri::has_scheme()
		\deprecated The name suggest something along the line "no-scheme"/"no-auth",
		            while the implementation did, what #web::uri::has_scheme() does,
		            but in reverse.
		*/
		[[deprecated("The name has nothing to do with behavior. "
			"Use has_scheme() instead.")]]
		bool relative() const { return !has_scheme(); }

		/**
		\returns the same value, as #web::uri::has_scheme()
		\deprecated The name suggest something along the line "a full URI, with scheme,
		            auth", while the implementation did, what #web::uri::has_scheme().
		*/
		[[deprecated("The name has nothing to do with behavior. "
			"Use has_scheme() instead.")]]
		bool absolute() const { return has_scheme(); }

		/**
		Checks, if the URI seems to contain a scheme.
		\returns true, if the uri starts with <code>[a-z][-+.a-z0-9]*:</code>
		*/
		bool has_scheme() const;

		/**
		Checks, if the URI seems to contain a non-empty scheme.
		\returns true, if the URI has_scheme(), but is scheme-relative
		*/
		bool is_scheme_relative() const;

		/**
		Checks, if the URI seems to contain an authority.
		\returns true, if the non-scheme part starts with <code>//</code>
		*/
		bool has_authority() const;

		/**
		Checks, if the URI seems to contain a path without authority.
		\returns true, if the non-scheme part starts with anything, but <code>//</code>
		*/
		bool is_opaque() const { return !has_authority(); }

		/**
		Getter for the scheme property.
		\returns scheme, if present
		*/
		std::string_view scheme() const;

		/**
		Getter for the authority property.
		\returns authority, if present
		*/
		std::string_view authority() const;

		/**
		Getter for the path property.
		\returns path, if present
		*/
		std::string_view path() const;

		/**
		Getter for the query property.
		\returns query, if present
		*/
		std::string_view query() const;

		/**
		Getter for the resource property.
		\returns path and query, if present
		*/
		std::string_view resource() const;

		/**
		Getter for the fragment property.
		\returns fragment, if present
		*/
		std::string_view fragment() const;

		/**
		Setter for the scheme property.
		\param value new value of the property
		*/
		void scheme(std::string_view value);

		/**
		Setter for the authority property.
		\param value new value of the property
		*/
		void authority(std::string_view value);

		/**
		Setter for the path property.
		\param value new value of the property
		*/
		void path(std::string_view value);

		/**
		Setter for the query property.
		\param value new value of the property
		*/
		void query(std::string_view value);

		/**
		Setter for the fragment property.
		\param value new value of the property
		*/
		void fragment(std::string_view value);

		/**
		Getter for the underlying object
		\returns an immutable reference to
		         the underlying string
		*/
		const std::string& string() const { return m_uri; }

		/**
		Removes the last component of the path.
		
		Assuming the path uses slashes, this functions
		creates a copy of an uri with last part of the path
		removed. If the path already ends with slash, does
		nothing. Also, the function behaves as if the document
		was an argument supplied to the address bar - it will
		prepend an HTTP protocol, if not protocol is given.

		\param document an uri to convert to base path
		\returns an uri, which can be used in canonical()
		*/
		static uri make_base(const uri& document);

		/**
		Creates a normal uri, as-if the identifier was relative to base.

		If the identifier represents an already absolute hierarchical uri,
		this function just forwards the argument to normal. Otherwise, it
		tries to create an uri as if it was seen from a document with base
		as the address.

		\param identifier an url to expand
		\param base base address to calculate the address against
		\param flag chooses, what to do with user password, if present;
		            by default, the password is not placed in authority
		            string
		\returns a fully-expanded and normalized version of the identifier 
		*/
		static uri canonical(const uri& identifier, const uri& base, auth_flag flag = ui_safe);

		/**
		Normalizes the input.

		All percent-encoded parts are decoded and re-coded.
		Empty parts are removed. Both host and scheme are
		lower-cased. Path is normalized (segments of <code>"."</code>
		and <code>".."</code> are removed). Authority is checked against
		possible valid values:
		- if the host is not IPv4, IPv6 or a string looking
		  like a registered name, the process is aborted
		- the port, if present, is checked to be digits-only
		- the port is checked against few well known ports
		  and removed if not necessary

		\param identifier uri to normalize
		\param flag chooses, what to do with user password, if present;
		            by default, the password is not placed in authority
		            string
		\returns normalized uri
		*/
		static uri normal(uri identifier, auth_flag flag = ui_safe);
	};
}
