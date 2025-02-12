using Workerd = import "/workerd/workerd.capnp";

const config :Workerd.Config = (
  services = [
    (name = "my-increment-service", worker = .incrementWorker)
  ],
  sockets = [
    (name = "http",
     address = "*:8080",
     http = (),
     service = "my-increment-service")
  ]
);

const incrementWorker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",

  modules = [
    ( name = "increment.js", esModule = embed "increment.js" )
  ],

  durableObjectNamespaces = [
    (
      className = "IncrementDurableObject",
      uniqueKey = "my-secret-key-1234"
    )
  ],

  durableObjectStorage = ( inMemory = void ),

  bindings = [
    ( name = "COUNTER", durableObjectNamespace = "IncrementDurableObject" )
  ]
);
