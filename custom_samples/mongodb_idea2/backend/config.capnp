using Workerd = import "/workerd/workerd.capnp";

const config :Workerd.Config = (
  services = [
    ( name = "task-service", worker = .backendWorker )
  ],

  sockets = [
    ( name = "http",
      address = "*:8080",
      http = (),
      service = "task-service"
    )
  ]
);

const backendWorker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",

  modules = [
    ( name = "server.js", esModule = embed "server.js" ),
    ( name = "task_do.js", esModule = embed "task_do.js" )
  ],

  durableObjectNamespaces = [
    ( className = "TaskDO", uniqueKey = "tasks-unique-key-1234" )
  ],

  durableObjectStorage = ( inMemory = void ),

  bindings = [
    ( name = "TASKS_DO_NAMESPACE", durableObjectNamespace = "TaskDO" )
  ]
);
