// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_VALUE_HPP_
#define ASTERIA_VALUE_HPP_

#include "fwd.hpp"
#include "rocket/variant.hpp"
#include "abstract_opaque.hpp"
#include "abstract_function.hpp"

namespace Asteria {

class Value
  {
  public:
    enum Type : std::uint8_t
      {
        type_null      = 0,
        type_boolean   = 1,
        type_integer   = 2,
        type_double    = 3,
        type_string    = 4,
        type_opaque    = 5,
        type_function  = 6,
        type_array     = 7,
        type_object    = 8,
      };
    using Variant = rocket::variant<
      ROCKET_CDR(
        , D_null      // 0,
        , D_boolean   // 1,
        , D_integer   // 2,
        , D_double    // 3,
        , D_string    // 4,
        , D_opaque    // 5,
        , D_function  // 6,
        , D_array     // 7,
        , D_object    // 8,
      )>;

    enum Compare : std::uint8_t
      {
        compare_unordered  = 0,
        compare_less       = 1,
        compare_equal      = 2,
        compare_greater    = 3,
      };

  private:
    Variant m_variant;

  public:
    Value() noexcept
      : m_variant()  // Initialize to `null`.
      {
      }
    template<typename AltT, typename std::enable_if<std::is_constructible<Variant, AltT &&>::value>::type * = nullptr>
      Value(AltT &&alt)
        : m_variant(std::forward<AltT>(alt))
        {
        }
    ~Value();

    Value(const Value &) noexcept;
    Value & operator=(const Value &) noexcept;
    Value(Value &&) noexcept;
    Value & operator=(Value &&) noexcept;

  public:
    Type type() const noexcept
      {
        return static_cast<Type>(m_variant.index());
      }
    template<typename AltT>
      const AltT * opt() const noexcept
        {
          return m_variant.get<AltT>();
        }
    template<typename AltT>
      AltT * opt() noexcept
        {
          return m_variant.get<AltT>();
        }
    template<typename AltT>
      const AltT & check() const
        {
          return m_variant.as<AltT>();
        }
    template<typename AltT>
      AltT & check()
        {
          return m_variant.as<AltT>();
        }
    template<typename AltT>
      AltT & set(AltT &&alt)
        {
          return m_variant.set(std::forward<AltT>(alt));
        }
  };

extern const char * get_type_name(Value::Type type) noexcept;

extern bool test_value(const Value &value);
extern Value::Compare compare_values(const Value &lhs, const Value &rhs) noexcept;

extern void dump_value(std::ostream &os, const Value &value, std::size_t indent_next = 0, std::size_t indent_increment = 2);
extern std::ostream & operator<<(std::ostream &os, const Value &value);

}

#endif
