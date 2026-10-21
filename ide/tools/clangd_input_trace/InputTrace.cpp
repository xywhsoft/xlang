// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "InputTrace.h"
#include "NativeInput.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SHA256.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>

namespace clang { namespace clangd { namespace {
constexpr size_t RecordLimit = 65536, FrameLimit = 512 * 1024;
std::atomic<bool> Enabled{false};
struct TraceState {
  std::mutex Mutex;
  std::map<std::string, llvm::json::Object> Records;
  std::set<std::string> Pending;
  uint64_t Sequence = 0, ContentBytes = 0, MetadataBytes = 0;
  std::string Failure;
};
TraceState &state() { static TraceState S; return S; }
void failed(llvm::StringRef Why) {
  auto &S = state();
  std::lock_guard<std::mutex> Lock(S.Mutex);
  if (S.Failure.empty()) S.Failure = Why.str();
}
void record(llvm::json::Object Row, uint64_t Bytes = 0) {
  auto &S = state();
  std::lock_guard<std::mutex> Lock(S.Mutex);
  if (!S.Failure.empty()) return;
  auto Path = Row.getString("path");
  size_t EncodedBytes = llvm::formatv("{0}", llvm::json::Value(llvm::json::Object(Row))).str().size();
  if (!Path || Path->empty() || Path->size() > 32768 || EncodedBytes > FrameLimit / 2) {
    S.Failure = "Invalid or oversized physical input record.";
    return;
  }
  std::string Key = Path->str();
  auto I = S.Records.find(Key);
  if (I != S.Records.end()) {
    for (const auto &Field : Row) {
      // A directory status query depends on presence and identity. Child
      // creation times do not affect that query. Enumeration has its own
      // exact captured entry list below.
      if (Row.getString("kind") == "directory" &&
          (Field.first == "size" || Field.first == "modified100ns")) continue;
      if (auto *Old = I->second.get(Field.first)) {
        if (*Old != Field.second) {
          S.Failure = "Physical input changed while the compiler consumed it.";
          return;
        }
      }
    }
    bool Changed = false;
    for (const auto &Field : Row) if (!I->second.get(Field.first)) {
      I->second[Field.first] = Field.second;
      Changed = true;
    }
    if (Changed) { S.ContentBytes += Bytes; S.MetadataBytes += EncodedBytes; S.Pending.insert(Key); }
  } else {
    if (S.Records.size() == RecordLimit) {
      S.Failure = "Physical input graph exceeds 65536 records.";
      return;
    }
    S.ContentBytes += Bytes;
    S.MetadataBytes += EncodedBytes;
    S.Records.emplace(Key, std::move(Row));
    S.Pending.insert(Key);
  }
  if (S.ContentBytes > 1024ULL * 1024 * 1024)
    S.Failure = "Physical input buffers exceed 1 GiB.";
  if (S.MetadataBytes > 128ULL * 1024 * 1024)
    S.Failure = "Physical input metadata exceeds 128 MiB.";
}
llvm::json::Object stampRow(llvm::StringRef Path, const xlang_native_stamp &S) {
  return llvm::json::Object{{"path", Path.str()},
          {"kind", (S.attributes & 0x10) ? "directory" : "file"},
          {"index", std::to_string(S.index)}, {"volume", std::to_string(S.volume)},
          {"size", std::to_string(S.size)}, {"modified100ns", std::to_string(S.modified)},
          {"attributes", std::to_string(S.attributes)}};
}
llvm::vfs::Status compilerStatus(llvm::StringRef Path, const xlang_native_stamp &S) {
  using Tick = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
  auto Time = llvm::sys::TimePoint<>(std::chrono::duration_cast<llvm::sys::TimePoint<>::duration>(
      Tick(static_cast<int64_t>(S.modified) - 116444736000000000LL)));
  return llvm::vfs::Status(Path, llvm::sys::fs::UniqueID(S.volume, S.index), Time,
      0, 0, S.size, (S.attributes & 0x10) ? llvm::sys::fs::file_type::directory_file :
      llvm::sys::fs::file_type::regular_file,
      (S.attributes & 1) ? (llvm::sys::fs::all_read | llvm::sys::fs::all_exe) : llvm::sys::fs::all_all);
}
std::error_code errorCode(unsigned long E) {
  if (E == 2 || E == 3 || E == 267) return std::make_error_code(std::errc::no_such_file_or_directory);
  if (E == 5 || E == 32 || E == 33) return std::make_error_code(std::errc::permission_denied);
  if (E == 8 || E == 14) return std::make_error_code(std::errc::not_enough_memory);
  return std::make_error_code(std::errc::io_error);
}
void recordError(llvm::StringRef Path, unsigned long E) {
  if (E == 2 || E == 3 || E == 267)
    record(llvm::json::Object{{"path", Path.str()}, {"kind", "missing"}});
  else failed("Physical input could not be observed completely.");
}
class TraceFile : public llvm::vfs::File {
  void *Handle;
  std::string Path;
public:
  TraceFile(void *H, std::string P) : Handle(H), Path(std::move(P)) {}
  ~TraceFile() override { close(); }
  llvm::ErrorOr<llvm::vfs::Status> status() override {
    xlang_native_stamp S; unsigned long E = 0;
    if (!xlangNativeStatus(Handle, &S, &E)) {
      recordError(Path, E); return errorCode(E);
    }
    record(stampRow(Path, S));
    return compilerStatus(Path, S);
  }
  llvm::ErrorOr<std::string> getName() override { return Path; }
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> getBuffer(
      const llvm::Twine &Name, int64_t FileSize, bool RequiresNullTerminator,
      bool IsVolatile) override {
    char *Data = nullptr; size_t Size = 0; xlang_native_stamp S; unsigned long E = 0;
    if (!xlangNativeRead(Handle, &Data, &Size, &S, &E)) {
      recordError(Path, E); return errorCode(E);
    }
    auto Buffer = llvm::MemoryBuffer::getMemBufferCopy(llvm::StringRef(Data, Size), Name.str());
    xlangNativeFree(Data);
    llvm::SHA256 Digest; Digest.update(Buffer->getBuffer());
    auto Bytes = Digest.final();
    static const char Hex[] = "0123456789abcdef";
    std::string Hash; Hash.reserve(64);
    for (uint8_t B : Bytes) { Hash.push_back(Hex[B >> 4]); Hash.push_back(Hex[B & 15]); }
    auto Row = stampRow(Path, S); Row["sha256"] = std::move(Hash);
    record(std::move(Row), Size);
    return Buffer;
  }
  std::error_code close() override {
    xlangNativeClose(Handle); Handle = nullptr; return {};
  }
};
class CapturedDir : public llvm::vfs::detail::DirIterImpl {
  std::vector<llvm::vfs::directory_entry> Entries;
  size_t Index = 0;
public:
  explicit CapturedDir(std::vector<llvm::vfs::directory_entry> E) : Entries(std::move(E)) {
    if (!Entries.empty()) CurrentEntry = Entries[0];
  }
  std::error_code increment() override {
    CurrentEntry = ++Index < Entries.size() ? Entries[Index] : llvm::vfs::directory_entry();
    return {};
  }
};
class TraceFS : public llvm::vfs::ProxyFileSystem {
  std::string absolute(const llvm::Twine &Path) {
    llvm::SmallString<256> P; Path.toVector(P);
    if (getUnderlyingFS().makeAbsolute(P)) { failed("Cannot resolve physical input path."); return {}; }
    llvm::sys::path::remove_dots(P, true);
    for (char &C : P) if (C == '\\') C = '/';
    return P.str().str();
  }
public:
  explicit TraceFS(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS) : ProxyFileSystem(std::move(FS)) {}
  llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine &Path) override {
    if (!inputTraceEnabled()) return getUnderlyingFS().status(Path);
    std::string P = absolute(Path); void *H = nullptr; xlang_native_stamp S; unsigned long E = 0;
    if (!xlangNativeOpen(P.c_str(), 0, &H, &S, &E)) { recordError(P, E); return errorCode(E); }
    xlangNativeClose(H); record(stampRow(P, S)); return compilerStatus(P, S);
  }
  llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>> openFileForRead(const llvm::Twine &Path) override {
    if (!inputTraceEnabled()) return getUnderlyingFS().openFileForRead(Path);
    std::string P = absolute(Path); void *H = nullptr; xlang_native_stamp S; unsigned long E = 0;
    if (!xlangNativeOpen(P.c_str(), 1, &H, &S, &E)) { recordError(P, E); return errorCode(E); }
    record(stampRow(P, S));
    return std::make_unique<TraceFile>(H, std::move(P));
  }
  llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>> openFileForReadBinary(const llvm::Twine &Path) override {
    return openFileForRead(Path);
  }
  bool exists(const llvm::Twine &Path) override {
    if (!inputTraceEnabled()) return getUnderlyingFS().exists(Path);
    auto S = status(Path); return S && S->exists();
  }
  std::error_code setCurrentWorkingDirectory(const llvm::Twine &Path) override {
    std::string P = inputTraceEnabled() ? absolute(Path) : std::string();
    auto E = getUnderlyingFS().setCurrentWorkingDirectory(Path);
    if (inputTraceEnabled()) {
      auto S = status(P);
      if (E || !S || !S->isDirectory()) failed("Compiler working directory is unavailable.");
    }
    return E;
  }
  std::error_code getRealPath(const llvm::Twine &Path, llvm::SmallVectorImpl<char> &Output) override {
    auto E = getUnderlyingFS().getRealPath(Path, Output);
    if (inputTraceEnabled()) {
      auto S = status(Path);
      if (!E && S) {
        std::string Real(Output.begin(), Output.end());
        for (char &C : Real) if (C == '\\') C = '/';
        xlang_native_stamp Native; unsigned long Code = 0; void *H = nullptr;
        std::string P = absolute(Path);
        if (xlangNativeOpen(P.c_str(), 0, &H, &Native, &Code)) {
          xlangNativeClose(H);
          auto Row = stampRow(P, Native); Row["realPath"] = Real; record(std::move(Row));
        } else recordError(P, Code);
      } else if (E && S) failed("Physical real-path query failed.");
    }
    return E;
  }
  llvm::vfs::directory_iterator dir_begin(const llvm::Twine &Path, std::error_code &E) override {
    auto Iterator = getUnderlyingFS().dir_begin(Path, E);
    if (!inputTraceEnabled()) return Iterator;
    std::string P = absolute(Path);
    auto S = status(P);
    if (E || !S) {
      if (E != std::errc::no_such_file_or_directory && E != std::errc::not_a_directory)
        failed("Physical directory enumeration failed.");
      return Iterator;
    }
    std::vector<llvm::vfs::directory_entry> Entries;
    llvm::json::Array Names;
    for (; Iterator != llvm::vfs::directory_iterator(); Iterator.increment(E)) {
      if (E || Entries.size() >= 4096) { failed("Physical directory enumeration is incomplete or oversized."); E = std::make_error_code(std::errc::io_error); return {}; }
      Entries.push_back(*Iterator);
    }
    if (E) { failed("Physical directory enumeration is incomplete."); return {}; }
    auto Sorted = Entries;
    std::sort(Sorted.begin(), Sorted.end(), [](const auto &A, const auto &B) { return A.path() < B.path(); });
    for (const auto &Entry : Sorted)
      Names.push_back(llvm::json::Object{{"name", llvm::sys::path::filename(Entry.path()).str()}, {"type", static_cast<int64_t>(Entry.type())}});
    xlang_native_stamp Native; unsigned long Code = 0; void *H = nullptr;
    if (xlangNativeOpen(P.c_str(), 0, &H, &Native, &Code)) {
      xlangNativeClose(H); auto Row = stampRow(P, Native);
      Row["entries"] = std::move(Names); record(std::move(Row));
    } else recordError(P, Code);
    return llvm::vfs::directory_iterator(std::make_shared<CapturedDir>(std::move(Entries)));
  }
};
} // namespace
void enableInputTrace() { Enabled.store(true, std::memory_order_release); }
bool inputTraceEnabled() { return Enabled.load(std::memory_order_acquire); }
llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> wrapInputTraceFS(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS) {
  return new TraceFS(std::move(FS));
}
std::optional<llvm::json::Value> takeInputTrace() {
  if (!inputTraceEnabled()) return std::nullopt;
  auto &S = state(); std::lock_guard<std::mutex> Lock(S.Mutex);
  if (S.Pending.empty() && S.Failure.empty()) return std::nullopt;
  llvm::json::Array Rows; size_t Bytes = 0;
  while (!S.Pending.empty() && Rows.size() < 128) {
    auto I = S.Pending.begin(); llvm::json::Value Row(llvm::json::Object(S.Records.at(*I)));
    size_t Size = llvm::formatv("{0}", Row).str().size();
    if (!Rows.empty() && Bytes + Size > FrameLimit) break;
    Bytes += Size; Rows.push_back(std::move(Row)); S.Pending.erase(I);
  }
  auto Previous = S.Sequence++;
  return llvm::json::Object{{"version", 1}, {"sequence", std::to_string(S.Sequence)},
      {"previous", std::to_string(Previous)}, {"stable", S.Failure.empty()},
      {"complete", S.Pending.empty()}, {"error", S.Failure}, {"records", std::move(Rows)}};
}
} } // namespace clang::clangd
