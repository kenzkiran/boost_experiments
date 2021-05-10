#pragma once

#include "pch.h"

#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <winrt/base.h>

#include <string>

struct NamedPipeWin {
 public:
  static constexpr unsigned kDefaultBufferSize = 1024 * 4;
  // We can take more input arguments when we need them
  [[nodiscard]] static NamedPipeWin CreateServerPipe(
      const std::string& name,
      unsigned buf_size = kDefaultBufferSize);
  [[nodiscard]] static NamedPipeWin CreateClientPipe(const std::string& name);
  NamedPipeWin() = default;
  ~NamedPipeWin();
  NamedPipeWin(NamedPipeWin&& other) = default;
  NamedPipeWin& operator=(NamedPipeWin&& other) = default;
  // May be Optional is a over kill
  std::optional<bool> IsServer() const;
  bool DisconnectAndClose() noexcept;
  bool IsValid() const;
  // TODO: is it possible to even control what the user of this class
  // can do with handle ?
  HANDLE handle() const { return pipe_.get(); }

 private:
  explicit NamedPipeWin(wil::unique_handle handle);
  NamedPipeWin(const NamedPipeWin&) = delete;             // disable copy
  NamedPipeWin& operator=(const NamedPipeWin&) = delete;  // disable assign
  wil::unique_handle pipe_;
};
