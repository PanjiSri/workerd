using Workerd = import "/workerd/workerd.capnp";

const config :Workerd.Config = (
  services = [
    (
      name = "internet",
      network = (
        allow = [
          "public",
          "private",
          "local",
          "unix",
          "unix-abstract"
        ]
      )
    ),

    ( name = "data-service", worker = .dataWorker ),

    ( name = "front1-service", worker = .front1Worker ),

    ( name = "front2-service", worker = .front2Worker ),
  ],

  sockets = [
    ( name = "dataSocket",   address = "*:8081", http = (), service = "data-service" ),

    ( name = "front1Socket", address = "*:8082", http = (), service = "front1-service" ),

    ( name = "front2Socket", address = "*:8083", http = (), service = "front2-service" ),
  ]
);

# ------------------- data-service (DO) -------------------
const dataWorker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",
  modules = [
    ( name = "data.js", esModule = embed "data.js" )
  ],

  durableObjectNamespaces = [
    ( className = "BookDO", uniqueKey = "unique-bookdo-key" )
  ],

  durableObjectStorage = (inMemory = void),
);

# ------------------- front1-service (Full CRUD) -------------------
const front1Worker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",

  modules = [
    ( name = "front1.js",   esModule = embed "front1.js" ),
    ( name = "front1.html", text    = embed "front1.html" )
  ],

  globalOutbound = "internet",

  bindings = [
    (
      name = "REMOTE_BOOK_DO",
      durableObjectNamespace = (
        className = "BookDO",
        serviceName = "data-service"
      )
    )
  ],
);

# ------------------- front2-service (Read-Only) -------------------
const front2Worker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",

  modules = [
    ( name = "front2.js",   esModule = embed "front2.js" ),
    ( name = "front2.html", text    = embed "front2.html" )
  ],

  globalOutbound = "internet",

  bindings = [
    (
      name = "REMOTE_BOOK_DO",
      durableObjectNamespace = (
        className = "BookDO",
        serviceName = "data-service"
      )
    )
  ],
);
