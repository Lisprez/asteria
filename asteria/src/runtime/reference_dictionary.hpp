// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_RUNTIME_REFERENCE_DICTIONARY_HPP_
#define ASTERIA_RUNTIME_REFERENCE_DICTIONARY_HPP_

#include "../fwd.hpp"
#include "reference.hpp"
#include "../rocket/cow_vector.hpp"
#include "../rocket/static_vector.hpp"

namespace Asteria {

class Reference_dictionary
  {
  private:
    struct Bucket
      {
        union { std::size_t size /* of the first bucket */; Bucket *prev /* of the rest */; };
        union { std::size_t reserved /* of the last bucket */; Bucket *next /* of the rest */; };
        rocket::prehashed_string name;
        rocket::static_vector<Reference, 1> refv;
        std::uintptr_t padding[3];

        Bucket() noexcept
          : prev(nullptr), next(nullptr),
            name(), refv()
          {
          }
        ROCKET_MOVABLE_DESTRUCTOR(Bucket)
          {
          }

        explicit operator bool () const noexcept
          {
            return !this->name.empty();
          }
      };

    // The first and last buckets are permanently reserved.
    rocket::cow_vector<Bucket> m_stor;

  public:
    Reference_dictionary() noexcept
      : m_stor()
      {
      }
    ROCKET_NONCOPYABLE_DESTRUCTOR(Reference_dictionary);

  private:
    void do_clear() noexcept;
    void do_rehash(std::size_t res_arg);
    void do_check_relocation(Bucket *from, Bucket *to);

  public:
    bool empty() const noexcept
      {
        if(this->m_stor.empty()) {
          return true;
        }
        return this->m_stor.front().size == 0;
      }
    std::size_t size() const noexcept
      {
        if(this->m_stor.empty()) {
          return 0;
        }
        return this->m_stor.front().size;
      }
    void clear() noexcept
      {
        if(this->m_stor.empty()) {
          return;
        }
        this->do_clear();
      }

    const Reference * get_opt(const rocket::prehashed_string &name) const noexcept;
    Reference & open(const rocket::prehashed_string &name);
    bool unset(const rocket::prehashed_string &name) noexcept;
  };

}

#endif
