// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "statement.hpp"
#include "xprunit.hpp"
#include "../runtime/air_node.hpp"
#include "../runtime/reference_stack.hpp"
#include "../runtime/executive_context.hpp"
#include "../runtime/analytic_context.hpp"
#include "../runtime/global_context.hpp"
#include "../runtime/function_analytic_context.hpp"
#include "../runtime/instantiated_function.hpp"
#include "../runtime/traceable_exception.hpp"
#include "../utilities.hpp"

namespace Asteria {

    namespace {

    template<typename XrefT> void do_safe_set_named_reference(Cow_Vector<PreHashed_String> *names_out_opt, Abstract_Context &ctx_io,
                                                              const char *desc, const PreHashed_String &name, XrefT &&xref)
      {
        if(name.empty()) {
          return;
        }
        if(name.rdstr().starts_with("__")) {
          ASTERIA_THROW_RUNTIME_ERROR("The name `", name, "` for this ", desc, " is reserved and cannot be used.");
        }
        if(names_out_opt) {
          names_out_opt->emplace_back(name);
        }
        ctx_io.open_named_reference(name) = std::forward<XrefT>(xref);
      }

    Rcptr<Variable> do_safe_create_variable(Cow_Vector<PreHashed_String> *names_out_opt, Abstract_Context &ctx_io,
                                            const char *desc, const PreHashed_String &name, const Global_Context &global)
      {
        auto var = global.create_variable();
        Reference_Root::S_variable ref_c = { var };
        do_safe_set_named_reference(names_out_opt, ctx_io, desc, name, rocket::move(ref_c));
        return var;
      }

    Cow_Vector<Air_Node> do_generate_code_statement_list(Cow_Vector<PreHashed_String> *names_out_opt, Analytic_Context &ctx_io,
                                                             const Cow_Vector<Statement> &stmts)
      {
        Cow_Vector<Air_Node> code;
        rocket::for_each(stmts, [&](const Statement &stmt) { stmt.generate_code(code, names_out_opt, ctx_io);  });
        return code;
      }

    Cow_Vector<Air_Node> do_generate_code_block(const Analytic_Context &ctx, const Cow_Vector<Statement> &block)
      {
        Analytic_Context ctx_next(&ctx);
        auto code = do_generate_code_statement_list(nullptr, ctx_next, block);
        return code;
      }

    Air_Node::Status do_execute_statement_list(Reference_Stack &stack, Executive_Context &ctx_io,
                                               const Cow_Vector<Air_Node> &code, const Cow_String &func, const Global_Context &global)
      {
        auto status = Air_Node::status_next;
        rocket::any_of(code, [&](const Air_Node &node) { return (status = node.execute(stack, ctx_io, func, global))
                                                                 != Air_Node::status_next;  });
        return status;
      }

    Air_Node::Status do_execute_block(Reference_Stack &stack,
                                      const Executive_Context &ctx, const Cow_Vector<Air_Node> &code, const Cow_String &func, const Global_Context &global)
      {
        Executive_Context ctx_next(&ctx);
        auto status = do_execute_statement_list(stack, ctx_next, code, func, global);
        return status;
      }

    Cow_Vector<Air_Node> do_generate_code_expression(const Analytic_Context &ctx, const Cow_Vector<Xprunit> &expr)
      {
        Cow_Vector<Air_Node> code;
        rocket::for_each(expr, [&](const Xprunit &xpru) { xpru.generate_code(code, ctx);  });
        return code;
      }

    void do_evaluate_expression(Reference_Stack &stack, Executive_Context &ctx,
                                const Cow_Vector<Air_Node> &code, const Cow_String &func, const Global_Context &global)
      {
        // We push a null reference in case of empty expressions.
        stack.clear();
        stack.push(Reference_Root::S_null());
        // Evaluate the expression.
        // If it is empty, the result will be pushed on `stack`; otherwise the aforementioned null reference will be on the top.
        rocket::for_each(code, [&](const Air_Node &node) { node.execute(stack, ctx, func, global);  });
      }

    Air_Node::Status do_execute_clear_stack(Reference_Stack &stack, Executive_Context & /*ctx_io*/,
                                            const Cow_Vector<Air_Node::Variant> & /*p*/, const Cow_String & /*func*/, const Global_Context & /*global*/)
      {
        // We push a null reference in case of empty expressions.
        stack.clear();
        stack.push(Reference_Root::S_null());
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_block_callback(Reference_Stack &stack, Executive_Context &ctx_io,
                                               const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &code = p.at(0).as<Cow_Vector<Air_Node>>();
        // Execute the block without affecting `ctx_io`.
        auto status = do_execute_block(stack, ctx_io, code, func, global);
        return status;
      }

    Air_Node::Status do_define_uninitialized_variable(Reference_Stack & /*stack*/, Executive_Context &ctx_io,
                                                      const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context &global)
      {
        // Decode arguments.
        const auto &sloc = p.at(0).as<Source_Location>();
        const auto &name = p.at(1).as<PreHashed_String>();
        const auto &immutable = static_cast<bool>(p.at(2).as<std::int64_t>());
        // Allocate a variable.
        auto var = do_safe_create_variable(nullptr, ctx_io, "variable", name, global);
        // Initialize the variable.
        var->reset(sloc, D_null(), immutable);
        return Air_Node::status_next;
      }

   Air_Node::Status do_declare_variable_and_clear_stack(Reference_Stack &stack, Executive_Context &ctx_io,
                                                        const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context &global)
      {
        // Decode arguments.
        const auto &name = p.at(0).as<PreHashed_String>();
        // Allocate a variable.
        auto var = do_safe_create_variable(nullptr, ctx_io, "variable placeholder", name, global);
        // Note that the initializer must not be empty for this code.
        stack.clear();
        // Push the variable on the stack.
        Reference_Root::S_variable ref_c = { var };
        stack.push(std::move(ref_c));
        return Air_Node::status_next;
      }

    Air_Node::Status do_initialize_variable(Reference_Stack &stack, Executive_Context & /*ctx_io*/,
                                            const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context & /*global*/)
      {
        // Decode arguments.
        const auto &sloc = p.at(0).as<Source_Location>();
        const auto &immutable = static_cast<bool>(p.at(1).as<std::int64_t>());
        // Read the value of the initializer.
        // Note that the initializer must not have been empty for this code.
        auto value = stack.top().read();
        stack.pop();
        auto var = stack.top().get_variable_opt();
        ROCKET_ASSERT(var);
        var->reset(sloc, std::move(value), immutable);
        return Air_Node::status_next;
      }

    Air_Node::Status do_define_function(Reference_Stack & /*stack*/, Executive_Context &ctx_io,
                                        const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context &global)
      {
        // Decode arguments.
        const auto &sloc = p.at(0).as<Source_Location>();
        const auto &name = p.at(1).as<PreHashed_String>();
        const auto &params = p.at(2).as<Cow_Vector<PreHashed_String>>();
        const auto &body = p.at(3).as<Cow_Vector<Statement>>();
        // Create a dummy reference for further name lookups.
        // A function becomes visible before its definition, where it is initialized to `null`.
        auto var = do_safe_create_variable(nullptr, ctx_io, "function", name, global);
        // Generate code for the function body.
        Cow_Vector<Air_Node> code_next;
        Function_Analytic_Context ctx_next(&ctx_io, params);
        rocket::for_each(body, [&](const Statement &stmt) { stmt.generate_code(code_next, nullptr, ctx_next);  });
        // Instantiate the function.
        rocket::insertable_ostream nos;
        nos << name << "("
            << rocket::ostream_implode(params.begin(), params.size(), ", ")
            <<")";
        Rcobj<Instantiated_Function> closure(sloc, nos.extract_string(), params, rocket::move(code_next));
        // Initialized the function variable.
        var->reset(sloc, D_function(rocket::move(closure)), true);
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_branch(Reference_Stack &stack, Executive_Context &ctx_io,
                                       const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &negative = static_cast<bool>(p.at(0).as<std::int64_t>());
        const auto &code_true = p.at(1).as<Cow_Vector<Air_Node>>();
        const auto &code_false = p.at(2).as<Cow_Vector<Air_Node>>();
        // Pick a branch basing on the condition.
        if(stack.top().read().test() != negative) {
          // Execute the true branch. Forward any status codes unexpected to the caller.
          auto status = do_execute_block(stack, ctx_io, code_true, func, global);
          if(rocket::is_none_of(status, { Air_Node::status_next })) {
            return status;
          }
          return Air_Node::status_next;
        }
        // Execute the false branch. Forward any status codes unexpected to the caller.
        auto status = do_execute_block(stack, ctx_io, code_false, func, global);
        if(rocket::is_none_of(status, { Air_Node::status_next })) {
          return status;
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_select(Reference_Stack &stack, Executive_Context &ctx_io,
                                       const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // This is different from a C `switch` statement where `case` labels must have constant operands.
        // Evaluate the control expression.
        const auto ctrl_value = stack.top().read();
        // `ctx_fr` is used before a `default` clause is encountered; `ctx_bk` is used after it.
        Executive_Context ctx_fr(&ctx_io);
        Executive_Context ctx_bk(&ctx_fr);
        auto qctx_cur = &ctx_fr;
        // Iterate over all `case` labels and evaluate them. Stop if the result value compares equal with `switch_value`.
        std::size_t index_def = SIZE_MAX;
        std::size_t index_case = 0;
        for(;;) {
          // No more clauses?
          if(index_case >= p.size()) {
            // No `case` label equals `ctrl_value`.
            if(index_def == SIZE_MAX) {
              // There is no `default` label. Skip this block.
              return Air_Node::status_next;
            }
            // A `default` label exists. Jump back to it.
            qctx_cur = &ctx_fr;
            index_case = index_def;
            break;
          }
          // Decode arguments.
          const auto &code_case = p.at(index_case).as<Cow_Vector<Air_Node>>();
          const auto &names = p.at(index_case + 2).as<Cow_Vector<PreHashed_String>>();
          if(!code_case.empty()) {
            // This is a `case` clause.
            // Evaluate the operand and check whether it equals `ctrl_value`.
            do_evaluate_expression(stack, *qctx_cur, code_case, func, global);
            if(stack.top().read().compare(ctrl_value) == Value::compare_equal) {
              // Found a `case` label.
              break;
            }
          } else {
            // This is a `default` clause.
            if(qctx_cur != &ctx_fr) {
              ASTERIA_THROW_RUNTIME_ERROR("Multiple `default` clauses have been found in this `switch` statement.");
            }
            // Record it.
            qctx_cur = &ctx_bk;
            index_def = index_case;
          }
          // Inject all names into this scope.
          rocket::for_each(names, [&](const PreHashed_String &name) { do_safe_set_named_reference(nullptr, *qctx_cur, "skipped variable", name, Reference_Root::S_null());  });
          // Try the next clause.
          index_case += 3;
        }
        for(;;) {
          // Decode arguments.
          const auto &code_clause = p.at(index_case + 1).as<Cow_Vector<Air_Node>>();
          // Execute the clause. Break out of the block if requested. Forward any status codes unexpected to the caller.
          auto status = do_execute_statement_list(stack, *qctx_cur, code_clause, func, global);
          if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_switch })) {
            break;
          }
          if(rocket::is_none_of(status, { Air_Node::status_next })) {
            return status;
          }
          // Execute the next clause.
          index_case += 3;
          // No more clauses?
          if(index_case >= p.size()) {
            break;
          }
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_do_while(Reference_Stack &stack, Executive_Context &ctx_io,
                                         const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &code_body = p.at(0).as<Cow_Vector<Air_Node>>();
        const auto &negative = static_cast<bool>(p.at(1).as<std::int64_t>());
        const auto &code_cond = p.at(2).as<Cow_Vector<Air_Node>>();
        // This is the same as a `do...while` loop in C.
        for(;;) {
          // Execute the body. Break out of the loop if requested. Forward any status codes unexpected to the caller.
          auto status = do_execute_block(stack, ctx_io, code_body, func, global);
          if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_while })) {
            break;
          }
          if(rocket::is_none_of(status, { Air_Node::status_next, Air_Node::status_continue_unspec, Air_Node::status_continue_while })) {
            return status;
          }
          // Check the condition.
          do_evaluate_expression(stack, ctx_io, code_cond, func, global);
          if(stack.top().read().test() == negative) {
            break;
          }
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_while(Reference_Stack &stack, Executive_Context &ctx_io,
                                      const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &negative = static_cast<bool>(p.at(0).as<std::int64_t>());
        const auto &code_cond = p.at(1).as<Cow_Vector<Air_Node>>();
        const auto &code_body = p.at(2).as<Cow_Vector<Air_Node>>();
        // This is the same as a `while` loop in C.
        for(;;) {
          // Check the condition.
          do_evaluate_expression(stack, ctx_io, code_cond, func, global);
          if(stack.top().read().test() == negative) {
            break;
          }
          // Execute the body. Break out of the loop if requested. Forward any status codes unexpected to the caller.
          auto status = do_execute_block(stack, ctx_io, code_body, func, global);
          if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_while })) {
            break;
          }
          if(rocket::is_none_of(status, { Air_Node::status_next, Air_Node::status_continue_unspec, Air_Node::status_continue_while })) {
            return status;
          }
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_for(Reference_Stack &stack, Executive_Context &ctx_io,
                                    const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &code_init = p.at(0).as<Cow_Vector<Air_Node>>();
        const auto &code_cond = p.at(1).as<Cow_Vector<Air_Node>>();
        const auto &code_step = p.at(2).as<Cow_Vector<Air_Node>>();
        const auto &code_body = p.at(3).as<Cow_Vector<Air_Node>>();
        // This is the same as a `for` loop in C.
        Executive_Context ctx_for(&ctx_io);
        do_execute_statement_list(stack, ctx_for, code_init, func, global);
        for(;;) {
          // Treat an empty condition as being always true.
          if(!code_cond.empty()) {
            // Check the condition.
            do_evaluate_expression(stack, ctx_for, code_cond, func, global);
            if(stack.top().read().test() == false) {
              break;
            }
          }
          // Execute the body. Break out of the loop if requested. Forward any status codes unexpected to the caller.
          auto status = do_execute_block(stack, ctx_for, code_body, func, global);
          if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_for })) {
            break;
          }
          if(rocket::is_none_of(status, { Air_Node::status_next, Air_Node::status_continue_unspec, Air_Node::status_continue_for })) {
            return status;
          }
          // Evaluate the step expression and discard its value.
          do_evaluate_expression(stack, ctx_for, code_step, func, global);
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_for_each(Reference_Stack &stack, Executive_Context &ctx_io,
                                         const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &key_name = p.at(0).as<PreHashed_String>();
        const auto &mapped_name = p.at(1).as<PreHashed_String>();
        const auto &code_init = p.at(2).as<Cow_Vector<Air_Node>>();
        const auto &code_body = p.at(3).as<Cow_Vector<Air_Node>>();
        // This is the same as a ranged-`for` loop in C++.
        Executive_Context ctx_for(&ctx_io);
        auto key_var = do_safe_create_variable(nullptr, ctx_for, "key variable", key_name, global);
        do_safe_set_named_reference(nullptr, ctx_for, "mapped reference", mapped_name, Reference_Root::S_null());
        // Evaluate the range initializer.
        do_evaluate_expression(stack, ctx_for, code_init, func, global);
        auto range_ref = rocket::move(stack.mut_top());
        auto range_value = range_ref.read();
        // Iterate over the range.
        switch(rocket::weaken_enum(range_value.type())) {
        case type_array:
          {
            const auto &array = range_value.check<D_array>();
            for(auto it = array.begin(); it != array.end(); ++it) {
              // Create a fresh context for the loop body.
              Executive_Context ctx_body(&ctx_for);
              // Set up the key variable, which is immutable.
              key_var->reset(Source_Location(rocket::sref("<builtin>"), 0), D_integer(it - array.begin()), true);
              // Set up the mapped reference.
              Reference_Modifier::S_array_index refm_c = { it - array.begin() };
              range_ref.zoom_in(rocket::move(refm_c));
              do_safe_set_named_reference(nullptr, ctx_body, "mapped reference", mapped_name, range_ref);
              range_ref.zoom_out();
              // Execute the loop body.
              auto status = do_execute_statement_list(stack, ctx_body, code_body, func, global);
              if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_for })) {
                break;
              }
              if(rocket::is_none_of(status, { Air_Node::status_next, Air_Node::status_continue_unspec, Air_Node::status_continue_for })) {
                return status;
              }
            }
            break;
          }
        case type_object:
          {
            const auto &object = range_value.check<D_object>();
            for(auto it = object.begin(); it != object.end(); ++it) {
              // Create a fresh context for the loop body.
              Executive_Context ctx_body(&ctx_for);
              // Set up the key variable, which is immutable.
              key_var->reset(Source_Location(rocket::sref("<builtin>"), 0), D_string(it->first), true);
              // Set up the mapped reference.
              Reference_Modifier::S_object_key refm_c = { it->first };
              range_ref.zoom_in(rocket::move(refm_c));
              do_safe_set_named_reference(nullptr, ctx_body, "mapped reference", mapped_name, range_ref);
              range_ref.zoom_out();
              // Execute the loop body.
              auto status = do_execute_statement_list(stack, ctx_body, code_body, func, global);
              if(rocket::is_any_of(status, { Air_Node::status_break_unspec, Air_Node::status_break_for })) {
                break;
              }
              if(rocket::is_none_of(status, { Air_Node::status_next, Air_Node::status_continue_unspec, Air_Node::status_continue_for })) {
                return status;
              }
            }
            break;
          }
        default:
          ASTERIA_THROW_RUNTIME_ERROR("The `for each` statement does not accept a range of type `", Value::get_type_name(range_value.type()), "`.");
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_try(Reference_Stack &stack, Executive_Context &ctx_io,
                                    const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context &global)
      {
        // Decode arguments.
        const auto &code_try = p.at(0).as<Cow_Vector<Air_Node>>();
        const auto &except_name = p.at(1).as<PreHashed_String>();
        const auto &code_catch = p.at(2).as<Cow_Vector<Air_Node>>();
        // This is the same as a `try...catch` block in C++.
        try {
          // Execute the `try` clause. If no exception is thrown, this will have little cost.
          auto status = do_execute_block(stack, ctx_io, code_try, func, global);
          if(rocket::is_none_of(status, { Air_Node::status_next })) {
            return status;
          }
        } catch(std::exception &stdex) {
          // Translate the exception.
          // The exception object shall not outlast the `catch` body.
          Executive_Context ctx_catch(&ctx_io);
          auto traceable = trace_exception(rocket::move(stdex));
          Reference_Root::S_temporary exref_c = { traceable.get_value() };
          do_safe_set_named_reference(nullptr, ctx_catch, "exception reference", except_name, rocket::move(exref_c));
          // Provide backtrace information.
          D_array backtrace;
          for(std::size_t i = 0; i < traceable.get_frame_count(); ++i) {
            const auto &frame = traceable.get_frame(i);
            D_object elem;
            elem.try_emplace(rocket::sref("file"), D_string(frame.source_file()));
            elem.try_emplace(rocket::sref("line"), D_integer(frame.source_line()));
            elem.try_emplace(rocket::sref("func"), D_string(frame.function_signature()));
            backtrace.emplace_back(rocket::move(elem));
          }
          ASTERIA_DEBUG_LOG("Exception backtrace:\n", Value(backtrace));
          Reference_Root::S_temporary btref_c = { rocket::move(backtrace) };
          ctx_catch.open_named_reference(rocket::sref("__backtrace")) = rocket::move(btref_c);
          // Execute the `catch` body.
          auto status = do_execute_statement_list(stack, ctx_catch, code_catch, func, global);
          if(rocket::is_none_of(status, { Air_Node::status_next })) {
            return status;
          }
        }
        return Air_Node::status_next;
      }

    Air_Node::Status do_return_status_simple(Reference_Stack & /*stack*/, Executive_Context & /*ctx_io*/,
                                             const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context & /*global*/)
      {
        // Decode arguments.
        const auto &status = static_cast<Air_Node::Status>(p.at(0).as<std::int64_t>());
        // Return the status as is.
        return status;
      }

    [[noreturn]] Air_Node::Status do_execute_throw(Reference_Stack &stack, Executive_Context & /*ctx_io*/,
                                                   const Cow_Vector<Air_Node::Variant> &p, const Cow_String &func, const Global_Context & /*global*/)
      {
        // Decode arguments.
        const auto &sloc = p.at(0).as<Source_Location>();
        // Throw the value; we don't throw by reference.
        throw Traceable_Exception(stack.top().read(), sloc, func);
      }

    Air_Node::Status do_execute_convert_to_temporary(Reference_Stack &stack, Executive_Context & /*ctx_io*/,
                                                     const Cow_Vector<Air_Node::Variant> & /*p*/, const Cow_String & /*func*/, const Global_Context & /*global*/)
      {
        // Replace the result with a temporary value, if it isn't.
        if(stack.top().is_temporary()) {
          return Air_Node::status_next;
        }
        Reference_Root::S_temporary ref_c = { stack.top().read() };
        stack.mut_top() = rocket::move(ref_c);
        return Air_Node::status_next;
      }

    Air_Node::Status do_execute_assert(Reference_Stack &stack, Executive_Context & /*ctx_io*/,
                                       const Cow_Vector<Air_Node::Variant> &p, const Cow_String & /*func*/, const Global_Context & /*global*/)
      {
        // Decode arguments.
        const auto &sloc = p.at(0).as<Source_Location>();
        const auto &negative = static_cast<bool>(p.at(1).as<std::int64_t>());
        const auto &msg = p.at(2).as<PreHashed_String>();
        // If the assertion succeeds, there is no effect.
        if(ROCKET_EXPECT(stack.top().read().test() != negative)) {
          return Air_Node::status_next;
        }
        // Throw a `Runtime_Error` if the assertion fails.
        rocket::insertable_ostream mos;
        mos << "Assertion failed at \'" << sloc << "\'";
        if(msg.empty()) {
          mos << "!";
        } else {
          mos << ": " << msg;
        }
        throw_runtime_error(__func__, mos.extract_string());
      }

    }  // namespace

void Statement::generate_code(Cow_Vector<Air_Node> &code_out, Cow_Vector<PreHashed_String> *names_out_opt, Analytic_Context &ctx_io) const
  {
    switch(static_cast<Index>(this->m_stor.index())) {
    case index_expression:
      {
        const auto &alt = this->m_stor.as<S_expression>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the expression.
        rocket::for_each(alt.expr, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        return;
      }
    case index_block:
      {
        const auto &alt = this->m_stor.as<S_block>();
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(do_generate_code_block(ctx_io, alt.body));  // 0
        code_out.emplace_back(&do_execute_block_callback, rocket::move(p));
        return;
      }
    case index_variable:
      {
        const auto &alt = this->m_stor.as<S_variable>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(names_out_opt, ctx_io, "variable placeholder", alt.name, Reference_Root::S_null());
        // Distinguish uninitialized variables from initialized ones.
        if(alt.init.empty()) {
          Cow_Vector<Air_Node::Variant> p;
          p.emplace_back(alt.sloc);  // 0
          p.emplace_back(alt.name);  // 1
          p.emplace_back(static_cast<std::int64_t>(alt.immutable));  // 2
          code_out.emplace_back(&do_define_uninitialized_variable, rocket::move(p));
          return;
        }
        // A variable becomes visible before its initializer, where it is initialized to `null`.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(alt.name);  // 0
        code_out.emplace_back(&do_declare_variable_and_clear_stack, rocket::move(p));
        // Generate inline code for the initializer.
        rocket::for_each(alt.init, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        // Generate code to initialize the variable.
        p.clear();
        p.emplace_back(alt.sloc);  // 0
        p.emplace_back(static_cast<std::int64_t>(alt.immutable));  // 1
        code_out.emplace_back(&do_initialize_variable, rocket::move(p));
        return;
      }
    case index_function:
      {
        const auto &alt = this->m_stor.as<S_function>();
        // Create a dummy reference for further name lookups.
        do_safe_set_named_reference(names_out_opt, ctx_io, "function placeholder", alt.name, Reference_Root::S_null());
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(alt.sloc);  // 0
        p.emplace_back(alt.name);  // 1
        p.emplace_back(alt.params);  // 2
        p.emplace_back(alt.body);  // 3
        code_out.emplace_back(&do_define_function, rocket::move(p));
        return;
      }
    case index_if:
      {
        const auto &alt = this->m_stor.as<S_if>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the condition expression.
        rocket::for_each(alt.cond, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        // Encode arguments.
        p.clear();
        p.emplace_back(static_cast<std::int64_t>(alt.negative));  // 0
        p.emplace_back(do_generate_code_block(ctx_io, alt.branch_true));  // 1
        p.emplace_back(do_generate_code_block(ctx_io, alt.branch_false));  // 2
        code_out.emplace_back(&do_execute_branch, rocket::move(p));
        return;
      }
    case index_switch:
      {
        const auto &alt = this->m_stor.as<S_switch>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the condition expression.
        rocket::for_each(alt.ctrl, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        // Create a fresh context for the `switch` body.
        // Note that all clauses inside a `switch` statement share the same context.
        Analytic_Context ctx_switch(&ctx_io);
        // Encode arguments.
        p.clear();
        // Note that this node takes variable number of arguments.
        for(auto it = alt.clauses.begin(); it != alt.clauses.end(); ++it) {
          p.emplace_back(do_generate_code_expression(ctx_switch, it->first));  // n * 3 + 0
          Cow_Vector<PreHashed_String> names;
          p.emplace_back(do_generate_code_statement_list(&names, ctx_switch, it->second));  // n * 3 + 1
          p.emplace_back(rocket::move(names));  // n * 3 + 2
        }
        code_out.emplace_back(&do_execute_select, rocket::move(p));
        return;
      }
    case index_do_while:
      {
        const auto &alt = this->m_stor.as<S_do_while>();
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(do_generate_code_block(ctx_io, alt.body));  // 0
        p.emplace_back(static_cast<std::int64_t>(alt.negative));  // 1
        p.emplace_back(do_generate_code_expression(ctx_io, alt.cond));  // 2
        code_out.emplace_back(&do_execute_do_while, rocket::move(p));
        return;
      }
    case index_while:
      {
        const auto &alt = this->m_stor.as<S_while>();
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(static_cast<std::int64_t>(alt.negative));  // 0
        p.emplace_back(do_generate_code_expression(ctx_io, alt.cond));  // 1
        p.emplace_back(do_generate_code_block(ctx_io, alt.body));  // 2
        code_out.emplace_back(&do_execute_while, rocket::move(p));
        return;
      }
    case index_for:
      {
        const auto &alt = this->m_stor.as<S_for>();
        // Create a fresh context for the `for` loop.
        Analytic_Context ctx_for(&ctx_io);
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(do_generate_code_statement_list(nullptr, ctx_for, alt.init));  // 0
        p.emplace_back(do_generate_code_expression(ctx_for, alt.cond));  // 1
        p.emplace_back(do_generate_code_expression(ctx_for, alt.step));  // 2
        p.emplace_back(do_generate_code_statement_list(nullptr, ctx_for, alt.body));  // 3
        code_out.emplace_back(&do_execute_for, rocket::move(p));
        return;
      }
    case index_for_each:
      {
        const auto &alt = this->m_stor.as<S_for_each>();
        // Create a fresh context for the `for` loop.
        Analytic_Context ctx_for(&ctx_io);
        do_safe_set_named_reference(nullptr, ctx_for, "key placeholder", alt.key_name, Reference_Root::S_null());
        do_safe_set_named_reference(nullptr, ctx_for, "mapped placeholder", alt.mapped_name, Reference_Root::S_null());
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(alt.key_name);  // 0
        p.emplace_back(alt.mapped_name);  // 1
        p.emplace_back(do_generate_code_expression(ctx_for, alt.init));  // 2
        p.emplace_back(do_generate_code_statement_list(nullptr, ctx_for, alt.body));  // 3
        code_out.emplace_back(&do_execute_for_each, rocket::move(p));
        return;
      }
    case index_try:
      {
        const auto &alt = this->m_stor.as<S_try>();
        // Create a fresh context for the `catch` clause.
        Analytic_Context ctx_catch(&ctx_io);
        do_safe_set_named_reference(nullptr, ctx_catch, "exception placeholder", alt.except_name, Reference_Root::S_null());
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        p.emplace_back(do_generate_code_block(ctx_catch, alt.body_try));  // 0
        p.emplace_back(alt.except_name);  // 1
        p.emplace_back(do_generate_code_statement_list(nullptr, ctx_catch, alt.body_catch));  // 2
        code_out.emplace_back(&do_execute_try, rocket::move(p));
        return;
      }
    case index_break:
      {
        const auto &alt = this->m_stor.as<S_break>();
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        switch(alt.target) {
        case Statement::target_unspec:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_break_unspec));  // 0
            break;
          }
        case Statement::target_switch:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_break_switch));  // 0
            break;
          }
        case Statement::target_while:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_break_while));  // 0
            break;
          }
        case Statement::target_for:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_break_for));  // 0
            break;
          }
        default:
          ASTERIA_TERMINATE("An unknown target scope type `", alt.target, "` has been encountered.");
        }
        code_out.emplace_back(&do_return_status_simple, rocket::move(p));
        return;
      }
    case index_continue:
      {
        const auto &alt = this->m_stor.as<S_continue>();
        // Encode arguments.
        Cow_Vector<Air_Node::Variant> p;
        switch(alt.target) {
        case Statement::target_unspec:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_continue_unspec));  // 0
            break;
          }
        case Statement::target_switch:
          {
            ASTERIA_TERMINATE("`target_switch` is not allowed to follow `continue`.");
          }
        case Statement::target_while:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_continue_while));  // 0
            break;
          }
        case Statement::target_for:
          {
            p.emplace_back(static_cast<std::int64_t>(Air_Node::status_continue_for));  // 0
            break;
          }
        default:
          ASTERIA_TERMINATE("An unknown target scope type `", alt.target, "` has been encountered.");
        }
        code_out.emplace_back(&do_return_status_simple, rocket::move(p));
        return;
      }
    case index_throw:
      {
        const auto &alt = this->m_stor.as<S_throw>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the operand.
        rocket::for_each(alt.expr, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        // Encode arguments.
        p.clear();
        p.emplace_back(alt.sloc);  // 0
        code_out.emplace_back(&do_execute_throw, rocket::move(p));
        return;
      }
    case index_return:
      {
        const auto &alt = this->m_stor.as<S_return>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the operand.
        rocket::for_each(alt.expr, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        if(!alt.by_ref) {
          // If the result is to be passed by value, convert it as necessary.
          p.clear();
          code_out.emplace_back(&do_execute_convert_to_temporary, rocket::move(p));
        }
        // Encode arguments.
        p.clear();
        p.emplace_back(static_cast<std::int64_t>(Air_Node::status_return));  // 0
        code_out.emplace_back(&do_return_status_simple, rocket::move(p));
        return;
      }
    case index_assert:
      {
        const auto &alt = this->m_stor.as<S_assert>();
        // Generate preparation code.
        Cow_Vector<Air_Node::Variant> p;
        code_out.emplace_back(&do_execute_clear_stack, rocket::move(p));
        // Generate inline code for the operand.
        rocket::for_each(alt.expr, [&](const Xprunit &xpru) { xpru.generate_code(code_out, ctx_io);  });
        // Encode arguments.
        p.clear();
        p.emplace_back(alt.sloc);  // 0
        p.emplace_back(static_cast<std::int64_t>(alt.negative));  // 1
        p.emplace_back(PreHashed_String(alt.msg));  // 2
        code_out.emplace_back(&do_execute_assert, rocket::move(p));
        return;
      }
    default:
      ASTERIA_TERMINATE("An unknown statement type enumeration `", this->m_stor.index(), "` has been encountered.");
    }
  }

}  // namespace Asteria
