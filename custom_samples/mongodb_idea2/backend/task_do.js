export class TaskDO {
  constructor(state, env) {
    this.state = state;
    this.env = env;
  }

  async getTasks() {
    const data = await this.state.storage.get("tasks");
    return data ? JSON.parse(data) : {};
  }

  async saveTasks(tasks) {
    await this.state.storage.put("tasks", JSON.stringify(tasks));
  }

  async fetch(request) {
    const url = new URL(request.url);
    const path = url.pathname.split("/").filter(Boolean);
    const method = request.method.toUpperCase();

    let tasks = await this.getTasks();

    if (method === "POST" && path[0] === "tasks") {
      const body = await request.json();

      let existingId = Object.keys(tasks).find((id) => {
        return tasks[id].name.toLowerCase() === body.name.toLowerCase();
      });
      if (existingId) {
        return new Response(JSON.stringify(tasks[existingId]), {
          status: 200,
          headers: { "Content-Type": "application/json" },
        });
      }

      let counter = (await this.state.storage.get("counter")) || 0;
      counter++;
      await this.state.storage.put("counter", counter);
      let newId = counter.toString();

      tasks[newId] = { id: newId, ...body, completed: false };
      await this.saveTasks(tasks);

      return new Response(JSON.stringify(tasks[newId]), {
        status: 201,
        headers: { "Content-Type": "application/json" },
      });
    }

    else if (method === "GET" && path[0] === "tasks") {
      if (path.length === 1) {
        return new Response(JSON.stringify(tasks, null, 2), {
          headers: { "Content-Type": "application/json" },
        });
      } else {
        let id = path[1];
        let task = tasks[id];
        if (!task) {
          return new Response("Not found", { status: 404 });
        }
        return new Response(JSON.stringify(task, null, 2), {
          headers: { "Content-Type": "application/json" },
        });
      }
    }

    else if (method === "PUT" && path[0] === "tasks" && path[1]) {
      let id = path[1];
      if (!tasks[id]) {
        return new Response("Not found", { status: 404 });
      }
      const body = await request.json();
      tasks[id] = { ...tasks[id], ...body };
      await this.saveTasks(tasks);
      return new Response(JSON.stringify(tasks[id]), {
        headers: { "Content-Type": "application/json" },
      });
    }

    else if (method === "DELETE" && path[0] === "tasks" && path[1]) {
      let id = path[1];
      if (!tasks[id]) {
        return new Response("Not found", { status: 404 });
      }
      delete tasks[id];
      await this.saveTasks(tasks);
      return new Response(JSON.stringify({ message: `Task ${id} deleted.` }), {
        headers: { "Content-Type": "application/json" },
      });
    }

    return new Response("Bad request to TaskDO", { status: 400 });
  }
}
