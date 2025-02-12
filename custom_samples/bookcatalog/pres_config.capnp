using Workerd = import "/workerd/workerd.capnp";

const config :Workerd.Config = (
  services = [
    ( name = "diskStore",
      disk = (
        path = "/workspaces/workerd/storage",
        writable = true,
        allowDotfiles = false
      )
    ),
    ( name = "book-service", worker = .libraryWorker )
  ],

  sockets = [
    ( name = "http",
      address = "*:8080",
      http = (),
      service = "book-service"
    )
  ]
);

const libraryWorker :Workerd.Worker = (
  compatibilityDate = "2023-12-31",

  modules = [
    ( name = "library.js", esModule = embed "library.js" ),
    ( name = "index.html", text = embed "index.html" )
  ],

  durableObjectNamespaces = [
    ( className = "BookStorageDO", uniqueKey = "catalog-unique-key-1234" )
  ],

  durableObjectStorage = ( localDisk = "diskStore" ),

  bindings = [
    ( name = "BOOK_DB", durableObjectNamespace = "BookStorageDO" )
  ],
);
