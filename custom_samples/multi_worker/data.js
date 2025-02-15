export class BookDO {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.initialized = false;
  }

  async initialize() {
    if (this.initialized) return;

    this.initialized = true;
    this.books = (await this.state.storage.get("books")) || {};
    this.nextId = (await this.state.storage.get("nextId")) || 1;
  }

  async fetch(request) {

  try {
    await this.initialize();

    let url = new URL(request.url);
    let path = url.pathname.split("/").filter(Boolean);
    let method = request.method.toUpperCase();

    if (method === "POST" && path[0] === "books") {
      let body = await request.json();

      let existingId = Object.keys(this.books).find((id) => {
        return (
          this.books[id].title.toLowerCase() === body.title.toLowerCase() &&
          this.books[id].author.toLowerCase() === body.author.toLowerCase()
        );
      });

      if (existingId) {
        return new Response(
          JSON.stringify(this.books[existingId]),
          { status: 200, headers: { "Content-Type": "application/json" } }
        );
      }

      let newId = this.nextId;
      this.nextId++;

      let newBook = { id: newId, title: body.title, author: body.author };
      this.books[newId] = newBook;

      await this.state.storage.put("books", this.books);
      await this.state.storage.put("nextId", this.nextId);

      return new Response(
        JSON.stringify(newBook),
        { status: 201, headers: { "Content-Type": "application/json" } }
      );
    }

    if (method === "GET" && path[0] === "books") {
      if (path.length === 1) {
        let all = Object.values(this.books);
        return new Response(
          JSON.stringify(all),
          { headers: { "Content-Type": "application/json" } }
        );
      } else {
        let id = parseInt(path[1]);
        let b = this.books[id];
        if (!b) {
          return new Response(
            JSON.stringify({ error: "Not found" }),
            { status: 404, headers: { "Content-Type": "application/json" } }
          );
        }

        return new Response(
          JSON.stringify(b),
          { headers: { "Content-Type": "application/json" } }
        );
      }
    }

    if (method === "PUT" && path[0] === "books" && path[1]) {
      let id = parseInt(path[1]);
      if (!this.books[id]) {
        return new Response(
          JSON.stringify({ error: "Not found" }),
          { status: 404, headers: { "Content-Type": "application/json" } }
        );
      }

      let body = await request.json();
      this.books[id] = { ...this.books[id], ...body };

      await this.state.storage.put("books", this.books);

      return new Response(
        JSON.stringify(this.books[id]),
        { headers: { "Content-Type": "application/json" } }
      );
    }

    if (method === "DELETE" && path[0] === "books" && path[1]) {
      let id = parseInt(path[1]);
      if (!this.books[id]) {
        return new Response(
          JSON.stringify({ error: "Not found" }),
          { status: 404, headers: { "Content-Type": "application/json" } }
        );
      }

      let deletedBook = this.books[id];
      delete this.books[id];

      await this.state.storage.put("books", this.books);

      return new Response(
        JSON.stringify({ message: `Book ${deletedBook.id} deleted.` }),
        { headers: { "Content-Type": "application/json" } }
      );
    }

    return new Response(
      JSON.stringify({ error: "Bad request to BookDO" }),
      { status: 400, headers: { "Content-Type": "application/json" } }
    );

    }

  catch (err){

      console.log("Caught an error in fetch():", err);

      return new Response(
        JSON.stringify({ error: err.message }),
        { status: 500, headers: { "Content-Type": "application/json" } }
      );
    }
  }
}
