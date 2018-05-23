// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_PARAMETER_HPP_
#define ASTERIA_PARAMETER_HPP_

#include "fwd.hpp"

namespace Asteria {

class Parameter {
private:
	String m_identifier;
	Sptr<const Variable> m_default_argument_opt;

public:
	Parameter(String identifier, Sptr<const Variable> default_argument_opt)
		: m_identifier(std::move(identifier)), m_default_argument_opt(std::move(default_argument_opt))
	{ }
	Parameter(Parameter &&) noexcept;
	Parameter & operator=(Parameter &&) noexcept;
	~Parameter();

public:
	const String & get_identifier() const {
		return m_identifier;
	}
	const Sptr<const Variable> & get_default_argument_opt() const {
		return m_default_argument_opt;
	}
};

using Parameter_vector = T_vector<Parameter>;

extern void prepare_function_arguments(Xptr_vector<Reference> &arguments_inout, Sparg<const Parameter_vector> parameters_opt);

}

#endif
