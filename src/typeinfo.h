#include <stddef.h>
#include "abi_namespace.h"
#include "typeinfo"

namespace ABI_NAMESPACE
{
	// Primitive type info
	struct __fundamental_type_info : public std::type_info
	{
		virtual ~__fundamental_type_info();
	};
	struct __array_type_info : public std::type_info
	{
		virtual ~__array_type_info();
	};
	struct __function_type_info : public std::type_info
	{
		virtual ~__function_type_info();
	};
	struct __enum_type_info : public std::type_info
	{
		virtual ~__enum_type_info();
	};

	// Base class for class type info.  Used only for tentative definitions.
	struct __class_type_info : public std::type_info
	{
		virtual ~__class_type_info();
		virtual void *cast_to(void *obj, const struct __class_type_info *other) const;
        virtual bool can_cast_to(const struct __class_type_info *other) const;
	};

	// Single-inheritance class.  
	struct __si_class_type_info : public __class_type_info
	{
		virtual ~__si_class_type_info();
		const __class_type_info *__base_type;
		virtual void *cast_to(void *obj, const struct __class_type_info *other) const;
        virtual bool can_cast_to(const struct __class_type_info *other) const;
	};

	struct __base_class_type_info
	{
		const __class_type_info *__base_type;
		private:
			long __offset_flags;

			enum __offset_flags_masks
			{
				__virtual_mask = 0x1,
				__public_mask = 0x2,
				__offset_shift = 8
			};
		public:
			long offset() const
			{
				return __offset_flags >> __offset_shift;
			}
			long flags() const
			{
				return __offset_flags & ((1 << __offset_shift) - 1);
			}
			bool isPublic() const { return flags() & __public_mask; }
			bool isVirtual() const { return flags() & __virtual_mask; }
	};


	struct __vmi_class_type_info : public __class_type_info
	{
		virtual ~__vmi_class_type_info();
		unsigned int __flags;
		unsigned int __base_count;
		__base_class_type_info __base_info[1];

		enum __flags_masks
		{
			__non_diamond_repeat_mask = 0x1,
			__diamond_shaped_mask = 0x2
		};
		virtual void *cast_to(void *obj, const struct __class_type_info *other) const;
        virtual bool can_cast_to(const struct __class_type_info *other) const;
	};

	struct __pbase_type_info : public std::type_info
	{
		virtual ~__pbase_type_info();
		unsigned int __flags;
		const std::type_info *__pointee;

		enum __masks
		{
			__const_mask = 0x1,
			__volatile_mask = 0x2,
			__restrict_mask = 0x4,
			__incomplete_mask = 0x8,
			__incomplete_class_mask = 0x10
		};
	};

	struct __pointer_type_info : public __pbase_type_info
	{
		virtual ~__pointer_type_info();
	};

	struct __pointer_to_member_type_info : public __pbase_type_info
	{
		virtual ~__pointer_to_member_type_info();
		const __class_type_info *__context;
	};

}
