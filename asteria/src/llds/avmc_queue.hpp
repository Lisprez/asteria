// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_LLDS_AVMC_QUEUE_HPP_
#define ASTERIA_LLDS_AVMC_QUEUE_HPP_

#include "../fwd.hpp"
#include "../runtime/enums.hpp"
#include "../source_location.hpp"

namespace Asteria {

class AVMC_Queue
  {
  public:
    // This union can be used to encapsulate trivial information in solidified nodes.
    // At most 48 btis can be stored here. You may make appropriate use of them.
    // Fields of each struct here share a unique prefix. This helps you ensure that you don't
    // access fields of different structs at the same time.
    union Uparam
      {
        struct {
          uint16_t x_DO_NOT_USE_;
          uint16_t x16;
          uint32_t x32;
        };
        struct {
          uint16_t y_DO_NOT_USE_;
          uint8_t y8s[2];
          uint32_t y32;
        };
        struct {
          uint16_t z_DO_NOT_USE_;
          uint16_t z16s[3];
        };
        struct {
          uint16_t v_DO_NOT_USE_;
          uint8_t v8s[6];
        };
      };

    static_assert(sizeof(Uparam) == 8);

    // Symbols are optional. If no symbol is given, no backtrace frame is appended.
    // The source location is used to generate backtrace frames.
    struct Symbols
      {
        Source_Location sloc;
      };

    static_assert(::std::is_nothrow_copy_constructible<Symbols>::value);

    // These are prototypes for callbacks.
    using Constructor  = void (Uparam uparam, void* sparam, intptr_t ctor_arg);
    using Move_Ctor    = void (Uparam uparam, void* sparam, void* sp_old);
    using Destructor   = void (Uparam uparam, void* sparam);
    using Executor     = AIR_Status (Executive_Context& ctx, Uparam uparam, const void* sparam);
    using Enumerator   = Variable_Callback& (Variable_Callback& callback, Uparam uparam, const void* sparam);

  private:
    struct Vtable
      {
        Move_Ctor* mvctor_opt;  // if null then bitwise copy is performed
        Destructor* dtor_opt;  // if null then no cleanup is performed
        Executor* executor;  // not nullable [!]
        Enumerator* vnum_opt;  // if null then no variables shall exist
      };

    struct Header;

  private:
    Header* m_bptr = nullptr;  // beginning of raw storage
    uint32_t m_rsrv = 0;  // size of raw storage, in number of `Header`s [!]
    uint32_t m_used = 0;  // size of used storage, in number of `Header`s [!]

  public:
    constexpr
    AVMC_Queue()
    noexcept
      { }

    AVMC_Queue(AVMC_Queue&& other)
    noexcept
      { this->swap(other);  }

    AVMC_Queue&
    operator=(AVMC_Queue&& other)
    noexcept
      { return this->swap(other);  }

    ~AVMC_Queue()
      {
        if(this->m_used)
          this->do_destroy_nodes();

        if(this->m_bptr)
          ::operator delete(this->m_bptr);

#ifdef ROCKET_DEBUG
        ::std::memset(static_cast<void*>(this), 0xCA, sizeof(*this));
#endif
      }

  private:
    void
    do_destroy_nodes()
    noexcept;

    void
    do_reallocate(uint32_t nadd);

    // Reserve storage for the next node.
    inline
    Header*
    do_reserve_one(Uparam uparam, const Symbols* syms_opt, size_t nbytes);

    // This function returns a vtable struct that is allocated statically.
    template<Executor execT, Enumerator* qvnumT, typename SparamT>
    static
    const Vtable*
    do_get_vtable()
    noexcept
      {
        static_assert(::std::is_object<SparamT>::value);
        static_assert(::std::is_same<SparamT, typename ::std::decay<SparamT>::type>::value);
        static_assert(::std::is_nothrow_move_constructible<SparamT>::value);

        // The vtable must have static storage duration.
        // As it is defined `constexpr` here, we need 'real' function pointers.
        // Those converted from non-capturing lambdas are not an option.
        struct Sfn
          {
            static
            void
            move_construct(Uparam, void* sparam, void* sp_old)
            noexcept
              { ::rocket::construct_at((SparamT*)sparam, ::std::move(*(SparamT*)sp_old));  }

            static
            void
            destroy(Uparam, void* sparam)
            noexcept
              { ::rocket::destroy_at((SparamT*)sparam);  }
          };

        // Trivial operations can be optimized.
        constexpr auto qmvct = ::std::is_trivially_move_constructible<SparamT>::value
                                   ? nullptr : Sfn::move_construct;
        constexpr auto qdtor = ::std::is_trivially_destructible<SparamT>::value
                                   ? nullptr : Sfn::destroy;
        static constexpr Vtable s_vtbl[1] = {{ qmvct, qdtor, execT, qvnumT }};
        return s_vtbl;
      }

    // Append a new node to the end. `nbytes` is the size of `sparam` to initialize.
    // If `src_opt` is specified, it should point to the buffer containing data to copy.
    // Otherwise, `sparam` is filled with zeroes.
    void
    do_append_trivial(Executor* exec, Uparam uparam, const Symbols* syms_opt,
                      const void* src_opt, size_t nbytes);

    // Append a new node to the end. `nbytes` is the size of `sparam` to initialize.
    // If `ctor_opt` is specified, it is called to initialize `sparam`.
    // Otherwise, `sparam` is filled with zeroes.
    void
    do_append_nontrivial(const Vtable* vtbl, Uparam uparam, const Symbols* syms_opt,
                         size_t nbytes, Constructor* ctor_opt, intptr_t ctor_arg);

    template<Executor execT, Enumerator* qvnumT, typename XSparamT>
    void
    do_append_nontrivial(Uparam uparam, const Symbols* syms_opt, XSparamT&& xsparam)
      {
        using Sparam = typename ::std::decay<XSparamT>::type;

        this->do_append_nontrivial(
            this->do_get_vtable<execT, qvnumT, Sparam>(), uparam, syms_opt,
            sizeof(Sparam),
            +[](Uparam /*uparam*/, void* sparam, intptr_t ctor_arg)
              { ::rocket::construct_at((Sparam*)sparam, ::std::forward<XSparamT>(*(Sparam*)ctor_arg));  },
            (intptr_t)(const volatile void*)::std::addressof(xsparam));
      }

  public:
    bool
    empty()
    const noexcept
      { return this->m_used == 0;  }

    AVMC_Queue&
    clear()
    noexcept
      {
        if(this->m_used)
          this->do_destroy_nodes();

        // Clean invalid data up.
        this->m_used = 0;
        return *this;
      }

    AVMC_Queue&
    shrink_to_fit()
      {
        if(this->m_used == this->m_rsrv)
          return *this;

        this->do_reallocate(0);
        return *this;
      }

    AVMC_Queue&
    swap(AVMC_Queue& other)
    noexcept
      {
        ::std::swap(this->m_bptr, other.m_bptr);
        ::std::swap(this->m_rsrv, other.m_rsrv);
        ::std::swap(this->m_used, other.m_used);
        return *this;
      }

    // Append a trivial node. This allows you to bind an arbitrary function.
    // If `sparam_opt` is a null pointer, `nbytes` zero bytes are allocated.
    // Call `append()` if the parameter is non-trivial.
    AVMC_Queue&
    append_trivial(Executor& exec, Uparam uparam = { })
      {
        this->do_append_trivial(exec, uparam, nullptr, nullptr, 0);
        return *this;
      }

    AVMC_Queue&
    append_trivial(Executor& exec, const Symbols& syms, Uparam uparam = { })
      {
        this->do_append_trivial(exec, uparam, ::std::addressof(syms), nullptr, 0);
        return *this;
      }

    AVMC_Queue&
    append_trivial(Executor& exec, const void* sparam_opt, size_t nbytes)
      {
        this->do_append_trivial(exec, Uparam(), nullptr, sparam_opt, nbytes);
        return *this;
      }

    AVMC_Queue&
    append_trivial(Executor& exec, const Symbols& syms, const void* sparam_opt, size_t nbytes)
      {
        this->do_append_trivial(exec, Uparam(), ::std::addressof(syms), sparam_opt, nbytes);
        return *this;
      }

    AVMC_Queue&
    append_trivial(Executor& exec, Uparam uparam, const void* sparam_opt, size_t nbytes)
      {
        this->do_append_trivial(exec, uparam, nullptr, sparam_opt, nbytes);
        return *this;
      }

    AVMC_Queue&
    append_trivial(Executor& exec, const Symbols& syms, Uparam uparam, const void* sparam_opt, size_t nbytes)
      {
        this->do_append_trivial(exec, uparam, ::std::addressof(syms), sparam_opt, nbytes);
        return *this;
      }

    // Append a node with type-generic semantics.
    // Both trivial and non-trivial parameter types are supported.
    // However, as this may result in a virtual call, the executor function has to be specified as
    // a template argument.
    template<Executor execT, nullptr_t /*qvnumT*/>
    AVMC_Queue&
    append(Uparam uparam = { })
      {
        this->do_append_trivial(execT, uparam, nullptr, nullptr, 0);
        return *this;
      }

    template<Executor execT, nullptr_t /*qvnumT*/>
    AVMC_Queue&
    append(const Symbols& syms, Uparam uparam = { })
      {
        this->do_append_trivial(execT, uparam, ::std::addressof(syms), nullptr, 0);
        return *this;
      }

    template<Executor execT, Enumerator* qvnumT, typename XSparamT,
    ROCKET_DISABLE_IF(::std::is_convertible<XSparamT&&, Uparam>::value)>
    AVMC_Queue&
    append(XSparamT&& sparam)
      {
        using Sparam = typename ::std::decay<XSparamT>::type;
        if(::std::is_trivial<Sparam>::value)
          this->do_append_trivial(execT, Uparam(), nullptr,
                                  ::std::addressof(sparam), sizeof(sparam));
        else
          this->do_append_nontrivial<execT, qvnumT>(Uparam(), nullptr,
                                                    ::std::forward<XSparamT>(sparam));
        return *this;
      }

    template<Executor execT, Enumerator* qvnumT, typename XSparamT,
    ROCKET_DISABLE_IF(::std::is_convertible<XSparamT&&, Uparam>::value)>
    AVMC_Queue&
    append(const Symbols& syms, XSparamT&& sparam)
      {
        using Sparam = typename ::std::decay<XSparamT>::type;
        if(::std::is_trivial<Sparam>::value)
          this->do_append_trivial(execT, Uparam(), ::std::addressof(syms),
                                  ::std::addressof(sparam), sizeof(sparam));
        else
          this->do_append_nontrivial<execT, qvnumT>(Uparam(), ::std::addressof(syms),
                                                    ::std::forward<XSparamT>(sparam));
        return *this;
      }

    template<Executor execT, Enumerator* qvnumT, typename XSparamT>
    AVMC_Queue&
    append(Uparam uparam, XSparamT&& sparam)
      {
        using Sparam = typename ::std::decay<XSparamT>::type;
        if(::std::is_trivial<Sparam>::value)
          this->do_append_trivial(execT, uparam, nullptr,
                                  ::std::addressof(sparam), sizeof(sparam));
        else
          this->do_append_nontrivial<execT, qvnumT>(Uparam(), nullptr,
                                                    ::std::forward<XSparamT>(sparam));
        return *this;
      }

    template<Executor execT, Enumerator* qvnumT, typename XSparamT>
    AVMC_Queue&
    append(const Symbols& syms, Uparam uparam, XSparamT&& sparam)
      {
        using Sparam = typename ::std::decay<XSparamT>::type;
        if(::std::is_trivial<Sparam>::value)
          this->do_append_trivial(execT, uparam, ::std::addressof(syms),
                                  ::std::addressof(sparam), sizeof(sparam));
        else
          this->do_append_nontrivial<execT, qvnumT>(uparam, ::std::addressof(syms),
                                                    ::std::forward<XSparamT>(sparam));
        return *this;
      }

    // These are interfaces called by the runtime.
    AVMC_Queue&
    reload(const cow_vector<AIR_Node>& code);

    AIR_Status
    execute(Executive_Context& ctx)
    const;

    Variable_Callback&
    enumerate_variables(Variable_Callback& callback)
    const;
  };

inline
void
swap(AVMC_Queue& lhs, AVMC_Queue& rhs)
noexcept
  { lhs.swap(rhs);  }

}  // namespace Asteria

#endif
