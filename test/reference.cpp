// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "_test_init.hpp"
#include "../asteria/src/runtime/reference.hpp"

using namespace Asteria;

int main()
  {
    auto ref = Reference(Reference_root::S_constant { D_string(rocket::cow_string::shallow("meow")) });
    auto val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_string);
    ASTERIA_TEST_CHECK(val.check<D_string>() == "meow");
    ASTERIA_TEST_CHECK_CATCH(ref.open() = D_boolean(true));
    auto ref2 = ref;
    val = ref2.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_string);
    ASTERIA_TEST_CHECK(val.check<D_string>() == "meow");
    ASTERIA_TEST_CHECK_CATCH(ref.open() = D_boolean(true));

    ref = Reference_root::S_temporary { D_integer(42) };
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_integer);
    ASTERIA_TEST_CHECK(val.check<D_integer>() == 42);
    ASTERIA_TEST_CHECK_CATCH(ref.open() = D_boolean(true));
    val = ref2.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_string);
    ASTERIA_TEST_CHECK(val.check<D_string>() == "meow");
    ASTERIA_TEST_CHECK_CATCH(ref.open() = D_boolean(true));

    ref = Reference_root::S_variable { rocket::make_refcounted<Variable>(Source_location(rocket::cow_string::shallow("nonexistent"), 42), D_null(), false) };
    ref.zoom_in(Reference_modifier::S_array_index { -3 });
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_null);
    ref.open() = D_integer(36);
    ref.zoom_out();
    ref.zoom_in(Reference_modifier::S_array_index { 0 });
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_integer);
    ASTERIA_TEST_CHECK(val.check<D_integer>() == 36);
    ref.zoom_out();

    ref.zoom_in(Reference_modifier::S_array_index { 2 });
    ref.zoom_in(Reference_modifier::S_object_key { rocket::prehashed_string(rocket::cow_string::shallow("my_key")) });
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_null);
    ref.open() = D_real(10.5);
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_real);
    ASTERIA_TEST_CHECK(val.check<D_real>() == 10.5);
    ref.zoom_out();
    ref.zoom_out();
    ref.zoom_in(Reference_modifier::S_array_index { -1 });
    ref.zoom_in(Reference_modifier::S_object_key { rocket::prehashed_string(rocket::cow_string::shallow("my_key")) });
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_real);
    ASTERIA_TEST_CHECK(val.check<D_real>() == 10.5);
    ref.zoom_in(Reference_modifier::S_object_key { rocket::prehashed_string(rocket::cow_string::shallow("invalid_access")) });
    ASTERIA_TEST_CHECK_CATCH(val = ref.read());
    ref.zoom_out();

    val = ref.unset();
    ASTERIA_TEST_CHECK(val.type() == Value::type_real);
    ASTERIA_TEST_CHECK(val.check<D_real>() == 10.5);
    val = ref.read();
    ASTERIA_TEST_CHECK(val.type() == Value::type_null);
    val = ref.unset();
    ASTERIA_TEST_CHECK(val.type() == Value::type_null);
  }
