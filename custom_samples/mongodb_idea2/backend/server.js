export { TaskDO } from './task_do.js';

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname.startsWith("/api/tasks")) {
      const id = env.TASKS_DO_NAMESPACE.idFromName("singleton");
      const stub = env.TASKS_DO_NAMESPACE.get(id);

      url.pathname = url.pathname.replace("/api", "");
      const newRequest = new Request(url.toString(), request);
      return stub.fetch(newRequest);
    }

    return new Response("Not Found", { status: 404 });
  }
};
