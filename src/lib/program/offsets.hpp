#pragma once
#include <common.hpp>
#include <tuple>
#include "version.hpp"

namespace exl::reloc {
using VersionType = util::UserVersion;
template <VersionType Version, impl::LookupEntry... Entries>
using UserTableType = VersionedTable<Version, Entries...>;
using UserTableSet = TableSet<VersionType,
    UserTableType<VersionType::DEFAULT,
#include "syms_exl_reloc.inc"
    >>;
static_assert(UserTableSet::s_Count == 1, "standalone overlay requires the TotK symbol table");
}
