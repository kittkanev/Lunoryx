#pragma once
static const char* TestEnvironment(const char* name) {
#ifdef LUNORYX_TESTING
  return std::getenv(name);
#else
  (void)name; return nullptr;
#endif
}
