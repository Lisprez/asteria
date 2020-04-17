// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "avmc_queue.hpp"
#include "../runtime/air_node.hpp"
#include "../runtime/variable_callback.hpp"
#include "../runtime/runtime_error.hpp"
#include "../utilities.hpp"

namespace Asteria {

struct AVMC_Queue::Header
  {
    uint8_t nphdrs;  // size of `sparam`, in number of `Header`s [!]
    uint8_t has_vtbl : 1;  // vtable exists?
    uint8_t has_syms : 1;  // symbols exist?
    uint16_t uparam_x16;  // user-defined data [1]
    uint32_t uparam_x32;  // user-defined data [2]
    union {
      const Vtable* vtable;  // active if `has_vtbl`
      Executor* exec;  // active otherwise
    };
    alignas(max_align_t) char alignment[0];  // user-defined data [3]

    Move_Ctor*
    mvctor_opt()
    const noexcept
      { return this->has_vtbl ? this->vtable->mvctor_opt : nullptr;  }

    Destructor*
    dtor_opt()
    const noexcept
      { return this->has_vtbl ? this->vtable->dtor_opt : nullptr;  }

    Executor*
    executor()
    const noexcept
      { return this->has_vtbl ? this->vtable->executor : this->exec;  }

    Enumerator*
    vnum_opt()
    const noexcept
      { return this->has_vtbl ? this->vtable->vnum_opt : nullptr;  }

    constexpr
    Uparam
    uparam()
    const noexcept
      { return {{ 0xDEAD, this->uparam_x16, this->uparam_x32 }};  }

    constexpr
    void*
    skip(uint32_t offset)
    const noexcept
      { return const_cast<Header*>(this) + 1 + offset;  }

    Symbols*
    syms_opt()
    const noexcept
      { return this->has_syms ? static_cast<Symbols*>(this->skip(0)) : nullptr;  }

    constexpr
    uint32_t
    symbol_size_in_headers()
    const noexcept
      { return this->has_syms * uint32_t((sizeof(Symbols) - 1) / sizeof(Header) + 1);  }

    void*
    sparam()
    const noexcept
      { return this->skip(this->symbol_size_in_headers());  }

    constexpr
    uint32_t
    total_size_in_headers()
    const noexcept
      { return 1 + this->symbol_size_in_headers() + this->nphdrs;  }
  };

void
AVMC_Queue::
do_destroy_nodes()
noexcept
  {
    auto next = this->m_bptr;
    while(ROCKET_EXPECT(next != this->m_bptr + this->m_used)) {
      auto qnode = ::std::exchange(next, next + next->total_size_in_headers());

      // Destroy user-defined data.
      if(auto qdtor = qnode->dtor_opt())
        (*qdtor)(qnode->uparam(), qnode->sparam());

      // Destroy symbols.
      if(auto ksyms = qnode->syms_opt())
        ::rocket::destroy_at(ksyms);
    }

#ifdef ROCKET_DEBUG
    this->m_used = 0xDEADBEEF;
#endif
  }

void
AVMC_Queue::
do_reallocate(uint32_t nadd)
  {
    constexpr size_t nhdrs_max = UINT32_MAX / sizeof(Header);
    if(nhdrs_max - this->m_used < nadd)
      throw ::std::bad_array_new_length();
    uint32_t rsrv = this->m_used + nadd;
    auto bptr = static_cast<Header*>(::operator new(rsrv * sizeof(Header)));

    // Performa bitwise copy of all contents of the old block.
    // This copies all existent headers and trivial data. Note that the size is unchanged.
    auto bold = ::std::exchange(this->m_bptr, bptr);
    this->m_rsrv = rsrv;
    ::std::memcpy(bptr, bold, this->m_used * sizeof(Header));

    // Move old non-trivial nodes if any.
    // Warning: No exception shall be thrown from the code below.
    uint32_t off = 0;
    while(ROCKET_EXPECT(off != this->m_used)) {
      auto qnode = bptr + off;
      auto qxold = bold + off;
      off += qnode->total_size_in_headers();

      // Move-construct new user-defined data.
      if(auto qmvct = qnode->mvctor_opt())
        (*qmvct)(qnode->uparam(), qnode->sparam(), qxold->sparam());

      // Move-construct new symbols.
      if(auto ksyms = qnode->syms_opt())
        ::rocket::construct_at(ksyms, ::std::move(*(qxold->syms_opt())));

      // Destroy old user-defined data.
      if(auto qdtor = qxold->dtor_opt())
        (*qdtor)(qxold->uparam(), qxold->sparam());

      // Destroy old symbols.
      if(auto ksyms = qxold->syms_opt())
        ::rocket::destroy_at(ksyms);
    }
    // Deallocate the old block.
    if(bold)
      ::operator delete(bold);
  }

AVMC_Queue::Header*
AVMC_Queue::
do_reserve_one(Uparam uparam, const Symbols* syms_opt, size_t nbytes)
  {
    constexpr size_t nbytes_max = UINT8_MAX * sizeof(Header) - 1;
    if(nbytes > nbytes_max)
      ASTERIA_THROW("invalid AVMC node size (`$1` > `$2`)", nbytes, nbytes_max);

    // Create a dummy header for calculation.
    Header temph;
    temph.nphdrs      = static_cast<uint8_t>((nbytes + sizeof(Header) - 1) / sizeof(Header));
    temph.has_syms    = syms_opt != nullptr;
    temph.uparam_x16  = uparam.x16;
    temph.uparam_x32  = uparam.x32;

    // Allocate a new memory block as needed.
    uint32_t nadd = temph.total_size_in_headers();
    if(ROCKET_UNEXPECT(this->m_rsrv - this->m_used < nadd)) {
#ifndef ROCKET_DEBUG
      // Reserve more space for non-debug builds.
      nadd |= this->m_used * 4;
#endif
      this->do_reallocate(nadd);
    }
    ROCKET_ASSERT(this->m_rsrv - this->m_used >= nadd);

    // Append a new node.
    auto qnode = this->m_bptr + this->m_used;
    ::std::memcpy(qnode, &temph, sizeof(temph));
    return qnode;
  }

void
AVMC_Queue::
do_append_trivial(Executor* exec, Uparam uparam, const Symbols* syms_opt,
                  const void* src_opt, size_t nbytes)
  {
    auto qnode = this->do_reserve_one(uparam, syms_opt, nbytes);
    qnode->has_vtbl = false;
    qnode->exec = exec;

    // Copy source data if `src_opt` is non-null. Fill zeroes otherwise.
    // This operation will not throw exceptions.
    if(src_opt)
      ::std::memcpy(qnode->sparam(), src_opt, nbytes);
    else
      ::std::memset(qnode->sparam(), 0, nbytes);

    // Set up symbols.
    // This operation will not throw exceptions.
    if(auto qsyms = qnode->syms_opt())
      ::rocket::construct_at(qsyms, *syms_opt);

    // Accept this node.
    this->m_used += qnode->total_size_in_headers();
  }

void
AVMC_Queue::
do_append_nontrivial(const Vtable* vtbl, Uparam uparam, const Symbols* syms_opt,
                     size_t nbytes, Constructor* ctor_opt, intptr_t ctor_arg)
  {
    auto qnode = this->do_reserve_one(uparam, syms_opt, nbytes);
    qnode->has_vtbl = true;
    qnode->vtable = vtbl;

    // Invoke the constructor if `ctor_opt` is non-null. Fill zeroes otherwise.
    // If an exception is thrown, there is no effect.
    if(ctor_opt)
      (*ctor_opt)(uparam, qnode->sparam(), ctor_arg);
    else
      ::std::memset(qnode->sparam(), 0, nbytes);

    // Set up symbols.
    // This operation will not throw exceptions.
    if(auto qsyms = qnode->syms_opt())
      ::rocket::construct_at(qsyms, *syms_opt);

    // Accept this node.
    this->m_used += qnode->total_size_in_headers();
  }

AVMC_Queue&
AVMC_Queue::
reload(const cow_vector<AIR_Node>& code)
  {
    this->clear();

    // Solidify all ndoes with regard to dead code elimination.
    for(const auto& node : code)
      if(!node.solidify(*this))
        break;

    // Reduce memory usage. TODO: Is this really necessary?
    this->shrink_to_fit();
    return *this;
  }

AIR_Status
AVMC_Queue::
execute(Executive_Context& ctx)
const
  {
    auto status = air_status_next;
    auto next = this->m_bptr;
    while(ROCKET_EXPECT(next != this->m_bptr + this->m_used)) {
      auto qnode = ::std::exchange(next, next + next->total_size_in_headers());

      // Call the executor function for this node.
      auto qexec = qnode->executor();
      ASTERIA_RUNTIME_TRY {
        status = (*qexec)(ctx, qnode->uparam(), qnode->sparam());
      }
      ASTERIA_RUNTIME_CATCH(Runtime_Error& except) {
        if(auto qsyms = qnode->syms_opt())
          except.push_frame_plain(qsyms->sloc, ::rocket::sref(""));
        throw;
      }
      if(ROCKET_UNEXPECT(status != air_status_next))
        break;
    }
    return status;
  }

Variable_Callback&
AVMC_Queue::
enumerate_variables(Variable_Callback& callback)
const
  {
    auto next = this->m_bptr;
    while(ROCKET_EXPECT(next != this->m_bptr + this->m_used)) {
      auto qnode = ::std::exchange(next, next + next->total_size_in_headers());

      // Enumerate variables in this node.
      if(auto qvnum = qnode->has_vtbl ? qnode->vtable->vnum_opt : nullptr)
        (*qvnum)(callback, qnode->uparam(), qnode->sparam());
    }
    return callback;
  }

}  // namespace Asteria
