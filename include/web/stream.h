/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once
#include <memory>
#include <array>
#include <vector>
#include <cassert>
#include <cstring>

namespace web {
	template <typename Final, size_t BufferLength = 4192>
	class ostream {
		size_t m_write_ptr = 0;
		std::array<char, BufferLength> m_output;

		bool store(const void* data, size_t size)
		{
			assert((m_write_ptr + size) <= BufferLength);

			std::memcpy(m_output.data() + m_write_ptr, data, size);
			m_write_ptr += size;

			if (m_write_ptr == BufferLength)
				return static_cast<Final*>(this)->overflow();
			return true;
		}
	protected:
		std::pair<const void*, size_t> write_data()
		{
			return { m_output.data(), m_write_ptr };
		}

		void flushed()
		{
			m_write_ptr = 0;
		}
	public:
		size_t write(const void* data, size_t size)
		{
			size_t written = 0;
			auto ptr = (const char*)data;
			while (size) {
				auto chunk = size;
				auto rest = BufferLength - m_write_ptr;
				if (chunk > rest)
					chunk = rest;

				if (!store(ptr, chunk))
					break;

				size -= chunk;
				written += chunk;
				ptr += chunk;
			}
			return written;
		}
		bool overflow() { return false; }
	};

	template <typename Final>
	class istream {
		size_t m_read_ptr = 0;
		std::vector<char> m_input;

		bool load(void* data, size_t size)
		{
			if (m_read_ptr == m_input.size()) {
				if (!static_cast<Final*>(this)->underflow())
					return false;
			}
			assert((m_read_ptr + size) <= m_input.size());

			memcpy(data, m_input.data() + m_read_ptr, size);
			m_read_ptr += size;

			return true;
		}
	protected:
		void refill(const char* ptr, size_t len)
		{
			m_read_ptr = 0;
			m_input.assign(ptr, ptr + len);
		}
	public:
		size_t read(void* data, size_t size)
		{
			size_t read_amount = 0;
			auto ptr = (char*)data;
			while (size) {
				auto chunk = size;
				auto rest = m_input.size() - m_read_ptr;
				if (chunk > rest)
					chunk = rest;
				if (!load(ptr, chunk))
					break;
				size -= chunk;
				read_amount += chunk;
				ptr += chunk;
			}
			return read_amount;
		}

		bool underflow()
		{
			return false;
		}
	};

	struct endpoint_t {
		std::string host;
		unsigned short port;
	};

	class stream
		: public istream<stream>
		, public ostream<stream> {
	public:
		struct impl {
			virtual ~impl();
			virtual void shutdown(stream* src) = 0;
			virtual bool overflow(stream* src, const void* data, size_t size, unsigned conn) = 0;
			virtual bool underflow(stream* src, unsigned conn) = 0;
			virtual bool is_open(stream* src) = 0;
			virtual endpoint_t local_endpoint(stream* src) = 0;
			virtual endpoint_t remote_endpoint(stream* src) = 0;
		};

		stream(impl& ref) : m_impl { ref } { }
		void flushed_write() { ostream<stream>::flushed(); }
		void refill_read(const void* data, size_t length)
		{
			istream<stream>::refill((const char*)data, length);
		}

		void shutdown()
		{
			m_impl.shutdown(this);
		}
		void conn_no(unsigned val) { conn_ = val; }
		bool overflow()
		{
			auto data = write_data();
			return m_impl.overflow(this, std::get<0>(data), std::get<1>(data), conn_);
		}

		bool underflow()
		{
			return m_impl.underflow(this, conn_);
		}

		bool is_open()
		{
			return m_impl.is_open(this);
		}

		endpoint_t local_endpoint()
		{
			return m_impl.local_endpoint(this);
		}

		endpoint_t remote_endpoint()
		{
			return m_impl.remote_endpoint(this);
		}
	private:
		impl& m_impl;
		unsigned conn_{ 0 };
	};
}
