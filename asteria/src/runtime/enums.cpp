// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "enums.hpp"

namespace asteria {

const char*
describe_xop(Xop xop)
noexcept
  {
    switch(xop) {
      case xop_inc_post:
        return "postfix `++`";

      case xop_dec_post:
        return "postfix `--`";

      case xop_subscr:
        return "postfix `[]`";

      case xop_pos:
        return "prefix `+`";

      case xop_neg:
        return "prefix `-`";

      case xop_notb:
        return "prefix `~`";

      case xop_notl:
        return "prefix `!`";

      case xop_inc_pre:
        return "prefix `++`";

      case xop_dec_pre:
        return "prefix `--`";

      case xop_unset:
        return "prefix `unset`";

      case xop_countof:
        return "prefix `countof`";

      case xop_typeof:
        return "prefix `typeof`";

      case xop_sqrt:
        return "prefix `__sqrt`";

      case xop_isnan:
        return "prefix `__isnan`";

      case xop_isinf:
        return "prefix `__isinf`";

      case xop_abs:
        return "prefix `__abs`";

      case xop_sign:
        return "prefix `__sign`";

      case xop_round:
        return "prefix `__round`";

      case xop_floor:
        return "prefix `__floor`";

      case xop_ceil:
        return "prefix `__ceil`";

      case xop_trunc:
        return "prefix `__trunc`";

      case xop_roundi:
        return "prefix `__roundi`";

      case xop_floori:
        return "prefix `__floori`";

      case xop_ceili:
        return "prefix `__ceili`";

      case xop_trunci:
        return "prefix `__trunci`";

      case xop_cmp_eq:
        return "infix `==`";

      case xop_cmp_ne:
        return "infix `!=`";

      case xop_cmp_lt:
        return "infix `<`";

      case xop_cmp_gt:
        return "infix `>`";

      case xop_cmp_lte:
        return "infix `<=`";

      case xop_cmp_gte:
        return "infix `>=`";

      case xop_cmp_3way:
        return "infix `<=>`";

      case xop_add:
        return "infix `+`";

      case xop_sub:
        return "infix `-`";

      case xop_mul:
        return "infix `*`";

      case xop_div:
        return "infix `/`";

      case xop_mod:
        return "infix `%`";

      case xop_sll:
        return "infix `<<<`";

      case xop_srl:
        return "infix `>>>`";

      case xop_sla:
        return "infix `<<`";

      case xop_sra:
        return "infix `>>`";

      case xop_andb:
        return "infix `&`";

      case xop_orb:
        return "infix `|`";

      case xop_xorb:
        return "infix `^`";

      case xop_assign:
        return "infix `=`";

      case xop_fma:
        return "prefix `__fma`";

      case xop_head:
        return "postfix `[^]`";

      case xop_tail:
        return "postfix `[$]`";

      default:
        return "<unknown operator>";
    }
  }

}  // namespace asteria
