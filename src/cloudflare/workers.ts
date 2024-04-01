// Copyright (c) 2024 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

// TODO(cleanup): C++ built-in modules do not yet support named exports, so we must define this
//   wrapper module that simply re-exports the classes from the built-in module.

import entrypoints from 'cloudflare-internal:workers';

export import WorkerEntrypoint = entrypoints.WorkerEntrypoint;
export import DurableObject = entrypoints.DurableObject;
export import RpcStub = entrypoints.RpcStub;
export import RpcTarget = entrypoints.RpcTarget;
