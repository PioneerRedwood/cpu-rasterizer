#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

inline void Log(const char* message) noexcept {
  if (message == nullptr) {
    return;
  }

  const size_t length = std::strlen(message);
  if (length > 0) {
    std::fwrite(message, 1, length, stderr);
  }
  std::fputc('\n', stderr);

#if defined(_WIN32)
  OutputDebugStringA(message);
  OutputDebugStringA("\n");
#endif
}

inline void LogF(const char* format, ...) noexcept {
  if (format == nullptr) {
    return;
  }

  char buffer[1024];
  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (written <= 0) {
    return;
  }

  const size_t length =
      static_cast<size_t>(written < static_cast<int>(sizeof(buffer))
                              ? written
                              : static_cast<int>(sizeof(buffer) - 1));

  std::fwrite(buffer, 1, length, stderr);
  std::fputc('\n', stderr);

#if defined(_WIN32)
  OutputDebugStringA(buffer);
  OutputDebugStringA("\n");
#endif
}
