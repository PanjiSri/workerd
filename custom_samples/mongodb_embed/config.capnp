using Workerd = import "/workerd/workerd.capnp";

const config :Workerd.Config = (
  services = [
    (name = "task-service", worker = .taskWorker),
  ],
  sockets = [
    (name = "http", address = "*:5000", http = (), service = "task-service")
  ]
);

const taskWorker :Workerd.Worker = (
  modules = [
    (name = "worker.js", esModule = embed "backend/dist/worker.js"),
    (name = "mongo.js", esModule = embed "backend/src/mongo.js")
  ],
  compatibilityDate = "2024-03-01",
  compatibilityFlags = ["nodejs_compat"]
);
