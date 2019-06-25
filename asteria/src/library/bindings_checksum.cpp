// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "bindings_checksum.hpp"
#include "argument_reader.hpp"
#include "simple_binding_wrapper.hpp"
#include "../runtime/global_context.hpp"
#include "../runtime/collector.hpp"
#include "../utilities.hpp"

namespace Asteria {

    namespace {
    namespace CRC32 {

    template<std::uint32_t valueT, std::uint32_t divisorT, int roundT> struct Generator : Generator<(valueT >> 1) ^ (-(valueT & 1) & divisorT), divisorT, roundT + 1>
      {
      };
    template<std::uint32_t valueT, std::uint32_t divisorT> struct Generator<valueT, divisorT, 8> : std::integral_constant<std::uint32_t, valueT>
      {
      };
    template<std::uint32_t divisorT, std::size_t... indicesT> constexpr std::array<std::uint32_t, sizeof...(indicesT)> do_generate_table_impl(const std::index_sequence<indicesT...>&) noexcept
      {
        return {{ Generator<static_cast<std::uint8_t>(indicesT), divisorT, 0>::value... }};
      }
    template<std::uint32_t divisorT> constexpr std::array<std::uint32_t, 256> do_generate_table() noexcept
      {
        return do_generate_table_impl<divisorT>(std::make_index_sequence<256>());
      }
    constexpr auto s_iso3309_table = do_generate_table<0xEDB88320>();

    class Hasher : public Abstract_Opaque
      {
      private:
        std::uint32_t m_reg;

      public:
        Hasher() noexcept
          : m_reg(UINT32_MAX)
          {
          }

      public:
        void describe(std::ostream& os) const override
          {
            os << "CRC-32 hasher";
          }
        void enumerate_variables(const Abstract_Variable_Callback& /*callback*/) const override
          {
            // There is nothing to do.
          }

        void write(const G_string& data) noexcept
          {
            const auto p = reinterpret_cast<const std::uint8_t*>(data.data());
            const auto n = data.size();
            auto r = this->m_reg;
            // Hash bytes one by one.
            for(std::size_t i = 0; i != n; ++i) {
              std::size_t b = (r ^ p[i]) & 0xFF;
              r >>= 8;
              r ^= s_iso3309_table[b];
            }
            this->m_reg = r;
          }
        G_integer finish() noexcept
          {
            // Get the checksum.
            auto ck = ~this->m_reg;
            // Reset internal states.
            this->m_reg = UINT32_MAX;
            return ck;
          }
      };

    }
    }

G_object std_checksum_crc32_new()
  {
    G_object r;
    r.insert_or_assign(rocket::sref("!h"),  // details
      G_opaque(
        rocket::make_refcnt<CRC32::Hasher>()
      ));
    r.insert_or_assign(rocket::sref("write"),
      G_function(
        make_simple_binding(
          // Description
          rocket::sref("<std.checksum.crc32_new()>.write"),
          // Opaque parameter
          G_null(),
          // Definition
          [](const Value& /*opaque*/, const Global_Context& /*global*/, Reference&& self, Cow_Vector<Reference>&& args) -> Reference
            {
              Argument_Reader reader(rocket::sref("<std.checksum.crc32_new()>.write"), args);
              // Get the hasher.
              Reference_Modifier::S_object_key xmod = { rocket::sref("!h") };
              self.zoom_in(rocket::move(xmod));
              auto& h = dynamic_cast<CRC32::Hasher&>(self.open().mut_opaque().mut());
              // Parse arguments.
              G_string data;
              if(reader.start().g(data).finish()) {
                h.write(data);
                return Reference_Root::S_null();
              }
              reader.throw_no_matching_function_call();
            }
        )
      ));
    r.insert_or_assign(rocket::sref("finish"),
      G_function(
        make_simple_binding(
          // Description
          rocket::sref("<std.checksum.crc32_new()>.finish"),
          // Opaque parameter
          G_null(),
          // Definition
          [](const Value& /*opaque*/, const Global_Context& /*global*/, Reference&& self, Cow_Vector<Reference>&& args) -> Reference
            {
              Argument_Reader reader(rocket::sref("<std.checksum.crc32_new()>.finish"), args);
              // Get the hasher.
              Reference_Modifier::S_object_key xmod = { rocket::sref("!h") };
              self.zoom_in(rocket::move(xmod));
              auto& h = dynamic_cast<CRC32::Hasher&>(self.open().mut_opaque().mut());
              // Parse arguments.
              if(reader.start().finish()) {
                Reference_Root::S_temporary xref = { h.finish() };
                return rocket::move(xref);
              }
              reader.throw_no_matching_function_call();
            }
        )
      ));
    return r;
  }

G_integer std_checksum_crc32(const G_string& data)
  {
    CRC32::Hasher h;
    h.write(data);
    return h.finish();
  }

void create_bindings_checksum(G_object& result, API_Version /*version*/)
  {
    //===================================================================
    // `std.checksum.crc32_new()`
    //===================================================================
    result.insert_or_assign(rocket::sref("crc32_new"),
      G_function(make_simple_binding(
        // Description
        rocket::sref
          (
            "\n"
            "`std.checksum.crc32_new()`\n"
            "\n"
            "  * Creates a CRC-32 hasher according to ISO/IEC 3309. The divisor\n"
            "    is `0x04C11DB7` (or `0xEDB88320` in reverse form).\n"
            "\n"
            "  * Returns the hasher as an `object` consisting of the following\n"
            "    members:\n"
            "\n"
            "    * `write(data)`\n"
            "    * `finish()`\n"
            "\n"
            "    The function `write()` is used to put data into the hasher,\n"
            "    which shall be of type `string`. After all data have been put,\n"
            "    the function `finish()` extracts the checksum as an `integer`\n"
            "    (whose high-order 32 bits are always zeroes), then resets the\n"
            "    hasher, making it suitable for further data as if it had just\n"
            "    been created.\n"
          ),
        // Opaque parameter
        G_null
          (
            nullptr
          ),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Reference&& /*self*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.checksum.crc32_new"), args);
            // Parse arguments.
            if(reader.start().finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_checksum_crc32_new() };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          }
      )));
    //===================================================================
    // `std.checksum.crc32()`
    //===================================================================
    result.insert_or_assign(rocket::sref("crc32"),
      G_function(make_simple_binding(
        // Description
        rocket::sref
          (
            "\n"
            "`std.checksum.crc32(data)`\n"
            "\n"
            "  * Calculates the CRC-32 checksum of `data` which must be of type\n"
            "    `string`, as if this function was defined as\n"
            "\n"
            "    ```\n"
            "      std.checksum.crc32 = func(data) {\n"
            "        var h = this.crc32_new();\n"
            "        h.write(data);\n"
            "        return h.finish();\n"
            "      };\n"
            "    ```\n"
            "\n"
            "    This function is expected to be both more efficient and easier\n"
            "    to use.\n"
            "\n"
            "  * Returns the CRC-32 checksum as an `integer`. The high-order 32\n"
            "    bits are always zeroes.\n"
          ),
        // Opaque parameter
        G_null
          (
            nullptr
          ),
        // Definition
        [](const Value& /*opaque*/, const Global_Context& /*global*/, Reference&& /*self*/, Cow_Vector<Reference>&& args) -> Reference
          {
            Argument_Reader reader(rocket::sref("std.checksum.crc32"), args);
            // Parse arguments.
            G_string data;
            if(reader.start().g(data).finish()) {
              // Call the binding function.
              Reference_Root::S_temporary xref = { std_checksum_crc32(data) };
              return rocket::move(xref);
            }
            // Fail.
            reader.throw_no_matching_function_call();
          }
      )));
    //===================================================================
    // End of `std.checksum`
    //===================================================================
  }

}  // namespace Asteria
