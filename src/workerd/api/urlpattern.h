// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#pragma once

#include "ada.h"

#include <workerd/jsg/jsg.h>

#include <optional>
#include <string_view>

namespace workerd::api {

#define URL_PATTERN_COMPONENTS(V)                                                                  \
  V(Protocol, protocol)                                                                            \
  V(Username, username)                                                                            \
  V(Password, password)                                                                            \
  V(Hostname, hostname)                                                                            \
  V(Port, port)                                                                                    \
  V(Pathname, pathname)                                                                            \
  V(Search, search)                                                                                \
  V(Hash, hash)

class URLPatternRegexEngine {
 public:
  URLPatternRegexEngine() = default;
  using regex_type = jsg::JsRef<jsg::JsRegExp>;
  static std::optional<regex_type> create_instance(std::string_view pattern, bool ignore_case) {
    return std::nullopt;
  }
  static std::optional<std::vector<std::optional<std::string>>> regex_search(
      std::string_view input, const regex_type& pattern) {
    return std::nullopt;
  }
  static bool regex_match(std::string_view input, const regex_type& pattern) {
    return false;
  }
};

// URLPattern is a Web Platform standard API for matching URLs against a
// pattern syntax (think of it as a regular expression for URLs). It is
// defined in https://wicg.github.io/urlpattern.
// More information about the URL Pattern syntax can be found at
// https://developer.mozilla.org/en-US/docs/Web/API/URL_Pattern_API
class URLPattern final: public jsg::Object {
 public:
  explicit URLPattern(ada::url_pattern<URLPatternRegexEngine>&& i): inner(kj::mv(i)) {};

  // A structure providing matching patterns for individual components
  // of a URL. When a URLPattern is created, or when a URLPattern is
  // used to match or test against a URL, the input can be given as
  // either a string or a URLPatternInit struct. If a string is given,
  // it will be parsed to create a URLPatternInit. The URLPatternInit
  // API is defined as part of the URLPattern specification.
  struct URLPatternInit final {
#define V(_, name) jsg::Optional<kj::String> name;
    URL_PATTERN_COMPONENTS(V)
#undef V
    jsg::Optional<kj::String> baseURL;

    JSG_STRUCT(protocol, username, password, hostname, port, pathname, search, hash, baseURL);

    static URLPatternInit create(const ada::url_pattern_init& other);
    ada::url_pattern_init toAdaType();
  };

  using URLPatternInput = kj::OneOf<kj::String, URLPatternInit>;

  // A struct providing the URLPattern matching results for a single
  // URL component. The URLPatternComponentResult is only ever used
  // as a member attribute of a URLPatternResult struct. The
  // URLPatternComponentResult API is defined as part of the URLPattern
  // specification.
  struct URLPatternComponentResult final {
    kj::String input;
    jsg::JsObject groups;

    JSG_STRUCT(input, groups);

    static URLPatternComponentResult create(
        jsg::Lock& js, const ada::url_pattern_component_result& other);
  };

  // A struct providing the URLPattern matching results for all
  // components of a URL. The URLPatternResult API is defined as
  // part of the URLPattern specification.
  struct URLPatternResult final {
    kj::Array<URLPatternInput> inputs;
#define V(_, name) URLPatternComponentResult name;
    URL_PATTERN_COMPONENTS(V)
#undef V

    JSG_STRUCT(inputs, protocol, username, password, hostname, port, pathname, search, hash);

    static URLPatternResult create(jsg::Lock& js, const ada::url_pattern_result& other);
  };

  struct URLPatternOptions final {
    jsg::Optional<bool> ignoreCase;

    JSG_STRUCT(ignoreCase);

    ada::url_pattern_options toAdaType() const;
  };

  static jsg::Ref<URLPattern> constructor(jsg::Lock& js,
      jsg::Optional<URLPatternInput> input,
      jsg::Optional<kj::OneOf<kj::String, URLPatternOptions>> baseURL,
      jsg::Optional<URLPatternOptions> patternOptions);

  kj::Maybe<URLPatternResult> exec(
      jsg::Lock& js, jsg::Optional<URLPatternInput> input, jsg::Optional<kj::String> baseURL);

  bool test(jsg::Lock& js, jsg::Optional<URLPatternInput> input, jsg::Optional<kj::String> baseURL);

#define V(name, _) kj::StringPtr get##name() const;
  URL_PATTERN_COMPONENTS(V)
#undef V

  JSG_RESOURCE_TYPE(URLPattern) {
#define V(Name, name) JSG_READONLY_PROTOTYPE_PROPERTY(name, get##Name);
    URL_PATTERN_COMPONENTS(V)
#undef V
    JSG_METHOD(test);
    JSG_METHOD(exec);
  }

 private:
  ada::url_pattern<URLPatternRegexEngine> inner;
};

#define EW_URLPATTERN_ISOLATE_TYPES                                                                \
  api::URLPattern, api::URLPattern::URLPatternInit, api::URLPattern::URLPatternComponentResult,    \
      api::URLPattern::URLPatternResult, api::URLPattern::URLPatternOptions

}  // namespace workerd::api
