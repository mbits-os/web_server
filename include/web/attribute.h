/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once
#include <type_traits>
#include <unordered_map>
#include <memory>

namespace web {
	struct attribute
	{
		using typeid_t = size_t;

		template <typename A>
		using is_attribute = std::is_base_of<attribute, typename std::remove_cv<A>::type>;

		virtual ~attribute() = default;
		template <typename A>
		static inline typeid_t id()
		{
			static_assert(is_attribute<A>{}, "Attribute type must inherit from web::attribute.");
			static typeid_t next_id = 0;
			static typeid_t type_id = next_id++;
			return type_id;
		}
	};

	class attributes {
		std::unordered_map<attribute::typeid_t, std::shared_ptr<attribute>> m_list;

		template <typename A>
		using is_attribute = std::is_base_of<attribute, typename std::remove_cv<A>::type>;

		template <typename A, typename Ret = void>
		using require_attribute = typename std::enable_if<is_attribute<A>::value, Ret>::type;
	public:
		template <typename Attr>
		require_attribute<Attr, bool> has_attr() const
		{
			static_assert(is_attribute<Attr>{}, "Attribute type must inherit from web::attribute.");
			auto it = m_list.find(attribute::id<Attr>());
			if (it == m_list.end())
				return false;
			return !!it->second;
		}

		template <typename Attr>
		require_attribute<Attr> set_attr(const std::shared_ptr<Attr>& attr)
		{
			static_assert(is_attribute<Attr>{}, "Attribute type must inherit from web::attribute.");
			if (attr)
				m_list[attribute::id<Attr>()] = attr;
		}

		template <typename Attr>
		require_attribute<Attr> set_attr(std::shared_ptr<Attr>&& attr)
		{
			static_assert(is_attribute<Attr>{}, "Attribute type must inherit from web::attribute.");
			if (attr)
				m_list[attribute::id<Attr>()] = std::move(attr);
		}

		template <typename Attr, typename... Args>
		require_attribute<Attr> make_attr(Args&&... args)
		{
			static_assert(is_attribute<Attr>{}, "Attribute type must inherit from web::attribute.");
			auto ptr = std::make_shared<Attr>(std::forward<Args>(args)...);
			if (ptr)
				m_list[attribute::id<Attr>()] = std::move(ptr);
		}

		template <typename Attr>
		require_attribute<Attr, std::shared_ptr<Attr>> get_attr() const
		{
			static_assert(is_attribute<Attr>{}, "Attribute type must inherit from web::attribute.");
			auto it = m_list.find(attribute::id<Attr>());
			if (it == m_list.end())
				return { };
			return it->second;
		}
	};
}
