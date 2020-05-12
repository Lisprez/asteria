// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_CONDITION_VARIABLE_HPP_
#  error Please include <rocket/condition_variable.hpp> instead.
#endif

namespace details_condition_variable {

inline
void
do_cond_wait(::pthread_cond_t& cond, ::pthread_mutex_t& mutex)
  {
    int r = ::pthread_cond_wait(&cond, &mutex);
    ROCKET_ASSERT(r != EINVAL);
  }

inline
void
do_cond_timedwait(::pthread_cond_t& cond, ::pthread_mutex_t& mutex, const ::timespec& abstime)
  {
    int r = ::pthread_cond_timedwait(&cond, &mutex, &abstime);
    ROCKET_ASSERT(r != EINVAL);
  }

inline
void
do_cond_signal(::pthread_cond_t& cond)
noexcept
  {
    int r = ::pthread_cond_signal(&cond);
    ROCKET_ASSERT(r == 0);
  }

inline
void
do_cond_broadcast(::pthread_cond_t& cond)
noexcept
  {
    int r = ::pthread_cond_broadcast(&cond);
    ROCKET_ASSERT(r == 0);
  }

inline
void
do_cond_destroy(::pthread_cond_t& cond)
noexcept
  {
    int r = ::pthread_cond_destroy(&cond);
    ROCKET_ASSERT(r == 0);
  }

}  // namespace details_condition_variable
