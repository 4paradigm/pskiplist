// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, 4Paradigm Inc. */

#pragma once

#include "libpmemobj++/p.hpp"
#include "libpmemobj++/persistent_ptr.hpp"
#include "libpmemobj++/pool.hpp"
#include "libpmemobj++/utils.hpp"
#include "libpmemobj++/make_persistent.hpp"
#include "libpmemobj++/make_persistent_array.hpp"
#include "libpmemobj++/transaction.hpp"
#include <atomic>
#include <cassert>

namespace fourpd
{
using pmem::obj::persistent_ptr;
using pmem::obj::delete_persistent;
using pmem::obj::make_persistent;
using pmem::obj::p;
using pmem::obj::pool;
using pmem::obj::pool_by_vptr;
using pmem::obj::pool_by_pptr;
using pmem::obj::pool_base;
using pmem::obj::transaction;

using size_type = size_t;
using offset_type = uint64_t;

static const uint64_t kDeleteFlag = 1;
static const uint64_t kDirtyFlag = 1 << 1;
static const uint64_t mask = 1 | (1 << 1);

template<class T>
class SmartPPtr {
private:
    offset_type val;
    //uint8_t refcount;
public:
    SmartPPtr() = default;
    SmartPPtr(pool_base& pop, T * ref, bool deleted, bool dirty) {
        val = (((offset_type) ref - (offset_type) pop.handle()) & ~mask)
              | (deleted ? kDeleteFlag : 0)
              | (dirty ? kDirtyFlag : 0);
    }
    SmartPPtr(offset_type offset, bool deleted, bool dirty):
        val(offset | (deleted ? kDeleteFlag : 0) | (dirty ? kDirtyFlag : 0)) {}
    explicit SmartPPtr(persistent_ptr<T> pptr): val(pptr.raw().off) {}
    explicit SmartPPtr(offset_type offset): val(offset) {}

    offset_type getOffset(void) {
        return (val & ~mask);
    }
    T * getVptr() {
        return (val == 0) ? nullptr : (T *) (((offset_type) pmemobj_pool_by_ptr(this) + val ) & ~mask);
    }
    persistent_ptr<T> getPptr() {
        return persistent_ptr<T>(PMEMoid{pmemobj_oid(this).pool_uuid_lo, val & ~mask});
    }

    bool isDelete(void)  { return (val & kDeleteFlag); }
    bool isDirty(void)   { return (val & kDirtyFlag);  }
    void clearDirty(void)  { val &= ~kDirtyFlag;  return; }
};

// template<class T>
// inline T* OFFSET2PTR(PMEMobjpool * base_ptr_, offset_type offset) {
//     return reinterpret_cast<T *>((offset_type) (base_ptr_) + (offset_type) (offset));
// }

// template<class T>
// inline offset_type PTR2OFFSET(PMEMobjpool * base_ptr_, T* ptr) {
//     return ((offset_type) (ptr) - (offset_type) base_ptr_);
// }

// template<class T>
// inline void PERSISTREAD(PMEMobjpool * base_ptr_, std::atomic<SmartPPtr<T>>* addr) {
//     while (true) {
//         auto expected = addr->load();
//         if (expected.isDirty()) {
//             LOG_("expected is dirty");
//             auto target = SmartPPtr<T>(base_ptr_, expected.getRef(base_ptr_), false, false);
//             if (!(addr->compare_exchange_strong(expected, target)))
//                 continue;
//             pmemobj_persist(base_ptr_, addr, sizeof(std::atomic<SmartPPtr<T>>));
//             break;
//         }
//         break;
//     }
// }
} /* namespace fourpd */
