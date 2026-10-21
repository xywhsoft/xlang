// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "Transport.h"
#include "support/InputTrace.h"
namespace clang { namespace clangd { namespace {
class TraceTransport : public Transport, private Transport::MessageHandler {
  std::unique_ptr<Transport> Inner;
  Transport::MessageHandler *Handler = nullptr;
  bool InitializeReply = false;
  llvm::json::Value InitializeID{nullptr};
  void flush() {
    while (auto Delta = takeInputTrace()) {
      bool Stable = Delta->getAsObject()->getBoolean("stable").value_or(false);
      bool Complete = Delta->getAsObject()->getBoolean("complete").value_or(false);
      Inner->notify("xlang/inputTrace", std::move(*Delta));
      if (!Stable || Complete) break;
    }
  }
  bool onNotify(llvm::StringRef Method, llvm::json::Value Params) override {
    return Handler->onNotify(Method, std::move(Params));
  }
  bool onCall(llvm::StringRef Method, llvm::json::Value Params, llvm::json::Value ID) override {
    if (Method == "initialize") {
      auto *Object = Params.getAsObject();
      auto *Capabilities = Object ? Object->getObject("capabilities") : nullptr;
      auto *Experimental = Capabilities ? Capabilities->getObject("experimental") : nullptr;
      if (Experimental && Experimental->getInteger("xlangInputTrace") == 1) {
        enableInputTrace(); InitializeReply = true; InitializeID = ID;
      }
    }
    return Handler->onCall(Method, std::move(Params), std::move(ID));
  }
  bool onReply(llvm::json::Value ID, llvm::Expected<llvm::json::Value> Result) override {
    return Handler->onReply(std::move(ID), std::move(Result));
  }
public:
  explicit TraceTransport(std::unique_ptr<Transport> T) : Inner(std::move(T)) {}
  void notify(llvm::StringRef Method, llvm::json::Value Params) override {
    flush(); Inner->notify(Method, std::move(Params));
  }
  void call(llvm::StringRef Method, llvm::json::Value Params, llvm::json::Value ID) override {
    flush(); Inner->call(Method, std::move(Params), std::move(ID));
  }
  void reply(llvm::json::Value ID, llvm::Expected<llvm::json::Value> Result) override {
    if (InitializeReply && ID == InitializeID) {
      InitializeReply = false;
      if (Result) if (auto *Object = Result->getAsObject()) {
        if (auto *Capabilities = Object->getObject("capabilities")) {
          if (auto *Experimental = Capabilities->getObject("experimental"))
            (*Experimental)["xlangInputTrace"] = 1;
          else (*Capabilities)["experimental"] = llvm::json::Object{{"xlangInputTrace", 1}};
        }
      }
    }
    flush(); Inner->reply(std::move(ID), std::move(Result));
  }
  llvm::Error loop(Transport::MessageHandler &H) override { Handler = &H; return Inner->loop(*this); }
};
}
std::unique_ptr<Transport> newInputTraceTransport(std::unique_ptr<Transport> Inner) {
  return std::make_unique<TraceTransport>(std::move(Inner));
}
} }
