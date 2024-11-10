// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0
#include "module.h"

#include <workerd/io/features.h>
#include <workerd/jsg/url.h>

namespace workerd::api::node {

bool ModuleUtil::isBuiltin(kj::String specifier) {
  return jsg::checkNodeSpecifier(specifier) != kj::none;
}

jsg::JsValue ModuleUtil::createRequire(jsg::Lock& js, kj::String path) {
  // Node.js requires that the specifier path is a File URL or an absolute
  // file path string. To be compliant, we will convert whatever specifier
  // is into a File URL if possible, then take the path as the actual
  // specifier to use.
  auto parsed = JSG_REQUIRE_NONNULL(jsg::Url::tryParse(path.asPtr(), "file:///"_kj), TypeError,
      "The argument must be a file URL object, "
      "a file URL string, or an absolute path string.");

  // We do not currently handle specifiers as URLs, so let's treat any
  // input that has query string params or hash fragments as errors.
  if (parsed.getSearch().size() > 0 || parsed.getHash().size() > 0) {
    JSG_FAIL_REQUIRE(
        Error, "The specifier must not have query string parameters or hash fragments.");
  }

  // The specifier must be a file: URL
  JSG_REQUIRE(parsed.getProtocol() == "file:"_kj, TypeError, "The specifier must be a file: URL.");

  return jsg::JsValue(js.wrapReturningFunction(js.v8Context(),
      [referrer = kj::str(parsed.getPathname())](
          jsg::Lock& js, const v8::FunctionCallbackInfo<v8::Value>& args) -> v8::Local<v8::Value> {
    auto registry = jsg::ModuleRegistry::from(js);

    // TODO(soon): This will need to be updated to support the new module registry
    // when that is fully implemented.
    JSG_REQUIRE(registry != nullptr, Error, "Module registry not available.");

    auto ref = ([&] {
      try {
        return kj::Path::parse(referrer.slice(1));
      } catch (kj::Exception& e) {
        JSG_FAIL_REQUIRE(Error, kj::str("Invalid referrer path: ", referrer.slice(1)));
      }
    })();

    auto spec = kj::str(args[0]);

    if (jsg::isNodeJsCompatEnabled(js)) {
      KJ_IF_SOME(nodeSpec, jsg::checkNodeSpecifier(spec)) {
        spec = kj::mv(nodeSpec);
      }
    }

    static const kj::Path kRoot = kj::Path::parse("");

    kj::Path targetPath = ([&] {
      // If the specifier begins with one of our known prefixes, let's not resolve
      // it against the referrer.
      try {
        if (spec.startsWith("node:") || spec.startsWith("cloudflare:") ||
            spec.startsWith("workerd:")) {
          return kj::Path::parse(spec);
        }

        return ref == kRoot ? kj::Path::parse(spec) : ref.parent().eval(spec);
      } catch (kj::Exception&) {
        JSG_FAIL_REQUIRE(Error, kj::str("Invalid specifier path: ", spec));
      }
    })();

    // require() is only exposed to worker bundle modules so the resolve here is only
    // permitted to require worker bundle or built-in modules. Internal modules are
    // excluded.
    auto& info = JSG_REQUIRE_NONNULL(
        registry->resolve(js, targetPath, ref, jsg::ModuleRegistry::ResolveOption::DEFAULT,
            jsg::ModuleRegistry::ResolveMethod::REQUIRE, spec.asPtr()),
        Error, "No such module \"", targetPath.toString(), "\".");

    bool isEsm = info.maybeSynthetic == kj::none;

    auto module = info.module.getHandle(js);

    if (module->GetStatus() == v8::Module::Status::kEvaluating ||
        module->GetStatus() == v8::Module::Status::kInstantiating) {
      KJ_IF_SOME(synth, info.maybeSynthetic) {
        KJ_IF_SOME(cjs, synth.tryGet<jsg::ModuleRegistry::CommonJsModuleInfo>()) {
          return cjs.moduleContext->getExports(js);
        }
        KJ_IF_SOME(cjs, synth.tryGet<jsg::ModuleRegistry::NodeJsModuleInfo>()) {
          return cjs.moduleContext->getExports(js);
        }
      }
    }

    auto features = FeatureFlags::get(js);
    jsg::InstantiateModuleOptions options = jsg::InstantiateModuleOptions::DEFAULT;
    if (features.getNoTopLevelAwaitInRequire()) {
      options = jsg::InstantiateModuleOptions::NO_TOP_LEVEL_AWAIT;

      // If the module was already evaluated, let's check if it is async.
      // If it is, we will throw an error. This case can happen if a previous
      // attempt to require the module failed because the module was async.
      if (module->GetStatus() == v8::Module::kEvaluated) {
        JSG_REQUIRE(!module->IsGraphAsync(), Error,
            "Top-level await in module is not permitted at this time.");
      }
    }

    jsg::instantiateModule(js, module, options);

    if (isEsm) {
      // If the import is an esm module, we will return the namespace object.
      jsg::JsObject obj(module->GetModuleNamespace().As<v8::Object>());
      if (obj.get(js, "__cjsUnwrapDefault"_kj) == js.boolean(true)) {
        return obj.get(js, "default"_kj);
      }
      return obj;
    }

    return jsg::JsValue(js.v8Get(module->GetModuleNamespace().As<v8::Object>(), "default"_kj));
  }));
}

}  // namespace workerd::api::node
