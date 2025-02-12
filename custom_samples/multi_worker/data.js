export class BookDO {
  constructor(state, env) {
    this.state = state;
    this.env = env;
  }

  async fetch(request) {
    let url = new URL(request.url);
    let path = url.pathname.split("/").filter(Boolean);
    let method = request.method.toUpperCase();

    let books = (await this.state.storage.get("books")) || {};

    if (method === "POST" && path[0] === "books") {
      let body = await request.json();

      let existingId = Object.keys(books).find((id) => {
        return (
          books[id].title.toLowerCase() === body.title.toLowerCase() &&
          books[id].author.toLowerCase() === body.author.toLowerCase()
        );
      });

      if (existingId) {
        return new Response(JSON.stringify(books[existingId]), {
          status: 200,
          headers: { "Content-Type": "application/json" }
        });
      }

      let newId = Math.random().toString(36).slice(2, 10);

      books[newId] = { id: newId, ...body };

      await this.state.storage.put("books", books);

      return new Response(JSON.stringify(books[newId]), {
        status: 201,
        headers: { "Content-Type": "application/json" }
      });

    } else if (method === "GET" && path[0] === "books") {
      if (path.length === 1) {

        return new Response(JSON.stringify(books, null, 2), {
          headers: { "Content-Type": "application/json" }
        });

      } else {
        let id = path[1];
        let b = books[id];

        if (!b) {
          return new Response("Not found", { status: 404 });
        }

        return new Response(JSON.stringify(b, null, 2), {
          headers: { "Content-Type": "application/json" }
        });

      }

    } else if (method === "PUT" && path[0] === "books" && path[1]) {
      let id = path[1];

      if (!books[id]) {
        return new Response("Not found", { status: 404 });
      }

      let body = await request.json();
      books[id] = { ...books[id], ...body };

      await this.state.storage.put("books", books);

      return new Response(JSON.stringify(books[id]), {
        headers: { "Content-Type": "application/json" }
      });

    } else if (method === "DELETE" && path[0] === "books" && path[1]) {
      let id = path[1];

      if (!books[id]) {
        return new Response("Not found", { status: 404 });
      }

      delete books[id];

      await this.state.storage.put("books", books);

      return new Response(`Book ${id} deleted.`);
    }

    return new Response("Bad request to BookDO", { status: 400 });
  }
}
