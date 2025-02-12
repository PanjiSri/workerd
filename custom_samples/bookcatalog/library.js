import FRONT_HTML from "./index.html";

export class BookStorageDO {
  constructor(state, env) {
    this.state = state;
    this.env = env;
  }

  async fetch(request) {
    let url = new URL(request.url);
    let path = url.pathname.replace(/^\/+|\/+$/g, "").split("/");

    let catalog = (await this.state.storage.get("catalog")) || {};
    let method = request.method.toUpperCase();

    if (path[0] !== "api" || path[1] !== "books") {
      return new Response("Not found in DO", { status: 404 });
    }

    switch (method) {
      case "GET":

        if (path.length === 2) {
          return new Response(JSON.stringify(catalog, null, 2), {
            headers: { "Content-Type": "application/json" },
          });

        } else if (path.length === 3) {

          let id = path[2];
          let book = catalog[id];

          if (!book) {
            return new Response(`Book ${id} not found`, { status: 404 });
          }

          return new Response(JSON.stringify(book, null, 2), {
            headers: { "Content-Type": "application/json" },
          });

        }

        return new Response("Not found", { status: 404 });

      case "POST":
        if (path.length === 2) {

          let body = await request.json();
          let newId = Math.random().toString(36).slice(2, 10);
          catalog[newId] = { id: newId, ...body };

          await this.state.storage.put("catalog", catalog);

          return new Response(JSON.stringify(catalog[newId]), {
            status: 201,
            headers: { "Content-Type": "application/json" },
          });

        }

        return new Response("Not found", { status: 404 });

      case "PUT":

        if (path.length === 3) {
          let id = path[2];

          if (!catalog[id]) {
            return new Response(`Book ${id} not found`, { status: 404 });
          }

          let body = await request.json();

          catalog[id] = { ...catalog[id], ...body };

          await this.state.storage.put("catalog", catalog);

          return new Response(JSON.stringify(catalog[id]), {
            headers: { "Content-Type": "application/json" },
          });
        }

        return new Response("Not found", { status: 404 });

      case "DELETE":
        if (path.length === 3) {
          let id = path[2];

          if (!catalog[id]) {
            return new Response(`Book ${id} not found`, { status: 404 });
          }

          delete catalog[id];

          await this.state.storage.put("catalog", catalog);

          return new Response(`Book ${id} removed`);
        }

        return new Response("Not found", { status: 404 });

      default:

        return new Response("Method not allowed", { status: 405 });
    }
  }
}


export default {
  async fetch(request, env) {

    let url = new URL(request.url);

    let path = url.pathname.replace(/^\/+|\/+$/g, "").split("/");

    if (path[0] === "api") {

      let id = env.BOOK_DB.idFromName("global-catalog");
      
      let stub = env.BOOK_DB.get(id);

      return stub.fetch(request);

    } else {

      return new Response(FRONT_HTML, {
        headers: { "Content-Type": "text/html" },
      });

    }
  }
};
