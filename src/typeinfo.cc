#include "typeinfo.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using std::type_info;

type_info::~type_info() {}

bool type_info::operator==(const type_info &other) const
{
	return __type_name == other.__type_name;
}
bool type_info::operator!=(const type_info &other) const
{
	return __type_name != other.__type_name;
}
bool type_info::before(const type_info &other) const
{
	return __type_name < other.__type_name;
}
const char* type_info::name() const
{
	return __type_name;
}
type_info::type_info (const type_info& rhs)
{
	__type_name = rhs.__type_name;
}
type_info& type_info::operator= (const type_info& rhs) 
{
	return *new type_info(rhs);
}

ABI_NAMESPACE::__fundamental_type_info::~__fundamental_type_info() {}
ABI_NAMESPACE::__array_type_info::~__array_type_info() {}
ABI_NAMESPACE::__function_type_info::~__function_type_info() {}
ABI_NAMESPACE::__enum_type_info::~__enum_type_info() {}
ABI_NAMESPACE::__class_type_info::~__class_type_info() {}
ABI_NAMESPACE::__si_class_type_info::~__si_class_type_info() {}
ABI_NAMESPACE::__vmi_class_type_info::~__vmi_class_type_info() {}
ABI_NAMESPACE::__pbase_type_info::~__pbase_type_info() {}
ABI_NAMESPACE::__pointer_type_info::~__pointer_type_info() {}
ABI_NAMESPACE::__pointer_to_member_type_info::~__pointer_to_member_type_info() {}

