// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#include "urlpattern.h"

#include "ada.h"

#include <kj/vector.h>

namespace workerd::api {
URLPattern::URLPatternInit create(const ada::url_pattern_init& other) {
  URLPattern::URLPatternInit result{};
#define V(_, name)                                                                                 \
  if (auto v = other.name) {                                                                       \
    result.name = kj::String(kj::heapArray(v->c_str(), v->size()));                                \
  }
  URL_PATTERN_COMPONENTS(V)
#undef V

  if (auto v = other.base_url) {
    result.baseURL = kj::String(kj::heapArray(v->c_str(), v->size()));
  }
  return result;
}

ada::url_pattern_options URLPattern::URLPatternOptions::toAdaType() const {
  ada::url_pattern_options options;
  options.ignore_case = ignoreCase.orDefault(false);
  return options;
}

ada::url_pattern_init URLPattern::URLPatternInit::toAdaType() {
  ada::url_pattern_init init{};
#define V(_, name)                                                                                 \
  KJ_IF_SOME(v, name) {                                                                            \
    init.name = std::string(v.begin(), v.size());                                                  \
  }
  URL_PATTERN_COMPONENTS(V)
#undef V
  KJ_IF_SOME(b, baseURL) {
    init.base_url = std::string(b.begin(), b.size());
  }
  return init;
}
URLPattern::URLPatternComponentResult create(
    jsg::Lock& js, const ada::url_pattern_component_result& other) {
  auto result = URLPattern::URLPatternComponentResult{
    .input = kj::String(kj::heapArray(other.input.c_str(), other.input.size())),
    .groups = js.obj(),
  };

  for (const auto& [key, value]: other.groups) {
    result.groups.set(js, js.str(kj::heapArray(key.c_str(), key.size())),
        js.str(kj::heapArray(key.c_str(), key.size())));
  }

  return result;
}

URLPattern::URLPatternResult create(jsg::Lock& js, const ada::url_pattern_result& other) {
  URLPattern::URLPatternResult result{
#define V(_, name) .name = URLPattern::URLPatternComponentResult::create(js, other.name),
    URL_PATTERN_COMPONENTS(V)
#undef V
  };

  auto vecInputs = kj::Vector<URLPattern::URLPatternInput>(2);
  for (const auto& input: other.inputs) {
    if (std::holds_alternative<std::string_view>(input)) {
      auto i = std::get<std::string_view>(input);
      vecInputs.add(kj::String(kj::heapArray(i.data(), i.size())));
    } else {
      KJ_DASSERT(std::holds_alternative<ada::url_pattern_init>(input));
      auto obj = std::get<ada::url_pattern_init>(input);
      vecInputs.add(URLPattern::URLPatternInit::create(obj));
    }
  }
  result.inputs = vecInputs.releaseAsArray();
  return result;
}

#define V(uppercase, lowercase)                                                                    \
  kj::StringPtr URLPattern::get##uppercase() const {                                               \
    auto value = inner.get_##lowercase();                                                          \
    return kj::StringPtr(value.data(), value.size());                                              \
  }
URL_PATTERN_COMPONENTS(V)
#undef V

jsg::Ref<URLPattern> URLPattern::constructor(jsg::Lock& js,
    jsg::Optional<URLPatternInput> maybeInput,
    jsg::Optional<kj::OneOf<kj::String, URLPatternOptions>> maybeBase,
    jsg::Optional<URLPatternOptions> maybeOptions) {
  ada::url_pattern_input input = ada::url_pattern_init{};
  std::string_view* base = nullptr;
  std::optional<ada::url_pattern_options> options{};

  KJ_IF_SOME(i, maybeInput) {
    KJ_SWITCH_ONEOF(i) {
      KJ_CASE_ONEOF(str, kj::String) {
        input = std::string_view(str.begin(), str.size());
      }
      KJ_CASE_ONEOF(init, URLPatternInit) {
        input = init.toAdaType();
      }
    }
  }

  KJ_IF_SOME(b, maybeBase) {
    KJ_SWITCH_ONEOF(b) {
      KJ_CASE_ONEOF(str, kj::String) {
        *base = std::string_view(str.begin(), str.size());
      }
      KJ_CASE_ONEOF(o, URLPatternOptions) {
        options = o.toAdaType();
      }
    }
  }

  if (!options.has_value()) {
    KJ_IF_SOME(o, maybeOptions) {
      options = o.toAdaType();
    }
  }

  if (auto result = ada::parse_url_pattern<URLPatternRegexEngine>(
          input, base, options.has_value() ? &*options : nullptr)) {
    return jsg::alloc<URLPattern>(kj::mv(*result));
  }

  JSG_FAIL_REQUIRE(TypeError, "Failed to construct URLPattern");
}

bool URLPattern::test(
    jsg::Lock& js, jsg::Optional<URLPatternInput> maybeInput, jsg::Optional<kj::String> maybeBase) {
  ada::url_pattern_input input = ada::url_pattern_init{};
  std::string_view* base = nullptr;

  KJ_IF_SOME(i, maybeInput) {
    KJ_SWITCH_ONEOF(i) {
      KJ_CASE_ONEOF(string, kj::String) {
        input = std::string_view(string.begin(), string.size());
      }
      KJ_CASE_ONEOF(pi, URLPattern::URLPatternInit) {
        input = pi.toAdaType();
      }
    }
  }

  KJ_IF_SOME(b, maybeBase) {
    *base = std::string_view(b.begin(), b.size());
  }

  if (auto result = inner.test(input)) {
    return *result;
  }

  JSG_FAIL_REQUIRE(TypeError, "Failed to test URLPattern");
}

kj::Maybe<URLPattern::URLPatternResult> URLPattern::exec(
    jsg::Lock& js, jsg::Optional<URLPatternInput> maybeInput, jsg::Optional<kj::String> maybeBase) {

  ada::url_pattern_input input = ada::url_pattern_init{};
  std::string_view* base = nullptr;

  KJ_IF_SOME(i, maybeInput) {
    KJ_SWITCH_ONEOF(i) {
      KJ_CASE_ONEOF(string, kj::String) {
        input = std::string_view(string.begin(), string.size());
      }
      KJ_CASE_ONEOF(pi, URLPattern::URLPatternInit) {
        input = pi.toAdaType();
      }
    }
  }

  KJ_IF_SOME(b, maybeBase) {
    *base = std::string_view(b.begin(), b.size());
  }

  if (auto result = inner.exec(input, base)) {
    if (result.has_value()) {
      return URLPatternResult::create(js, result->value());
    }

    // Return null
    return kj::none;
  }
  // If result does not exist, we should throw.
  JSG_FAIL_REQUIRE(TypeError, "Failed to exec URLPattern");
}
}  // namespace workerd::api
