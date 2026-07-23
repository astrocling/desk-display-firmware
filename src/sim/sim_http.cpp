#include "sim_http.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <cstring>

namespace sim {
namespace {

constexpr long kTimeoutSeconds = 8;

struct WriteCtx {
  char* buf;
  std::size_t cap;
  std::size_t len;
  bool overflowed;
};

std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* ctx = static_cast<WriteCtx*>(userdata);
  const std::size_t total = size * nmemb;
  const std::size_t space = ctx->cap > ctx->len ? ctx->cap - ctx->len : 0;
  const std::size_t copy = total < space ? total : space;
  if (copy > 0) {
    std::memcpy(ctx->buf + ctx->len, ptr, copy);
    ctx->len += copy;
  }
  if (copy < total) {
    ctx->overflowed = true;
  }
  // Always report the full size consumed so libcurl doesn't abort the
  // transfer; overflow is surfaced to the caller via ctx.overflowed.
  return total;
}

void ensureGlobalInit() {
  static const bool kInitted = [] {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return true;
  }();
  (void)kInitted;
}

}  // namespace

bool simHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen) {
  bodyLen = 0;
  if (!url || !body || bodyCap == 0) {
    return false;
  }

  ensureGlobalInit();

  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  WriteCtx ctx{body, bodyCap, 0, false};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "desk-display-sim/1.0");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  const CURLcode res = curl_easy_perform(curl);
  long httpStatus = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::fprintf(stderr, "sim_http: GET %s failed: %s\n", url, curl_easy_strerror(res));
    return false;
  }
  if (httpStatus < 200 || httpStatus >= 300) {
    std::fprintf(stderr, "sim_http: GET %s -> HTTP %ld\n", url, httpStatus);
    return false;
  }
  if (ctx.overflowed) {
    std::fprintf(stderr, "sim_http: response for %s exceeded buffer, dropping\n", url);
    return false;
  }

  bodyLen = ctx.len;
  return true;
}

bool simAdsbHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
                    void* /*user*/) {
  return simHttpGet(url, body, bodyCap, bodyLen);
}

}  // namespace sim
