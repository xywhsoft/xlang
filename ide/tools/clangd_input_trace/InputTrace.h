// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef XLANG_CLANGD_INPUT_TRACE_H
#define XLANG_CLANGD_INPUT_TRACE_H
#include "llvm/Support/JSON.h"
#include "llvm/Support/VirtualFileSystem.h"
#include <optional>
namespace clang { namespace clangd {
void enableInputTrace();
bool inputTraceEnabled();
llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> wrapInputTraceFS(
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS);
// Called under the server's existing transport writer mutex. Never sends I/O.
std::optional<llvm::json::Value> takeInputTrace();
} }
#endif
