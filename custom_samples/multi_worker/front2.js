import FRONT2_HTML from "front2.html";

export default {
  async fetch(request, env) {
    let url = new URL(request.url);

    if (url.pathname === "/") {
      return new Response(FRONT2_HTML, {
        headers: { "Content-Type": "text/html" }
      });
    }

    else if (url.pathname.startsWith("/api/")) {

      let pathPart = request.url.replace(/^.*\/api\//, "");
      let id = env.REMOTE_BOOK_DO.idFromName("mySharedBookDB");
      let stub = env.REMOTE_BOOK_DO.get(id);

      let doUrl = `https://fake-host/${pathPart}`;

      let doReq = new Request(doUrl, {
        method: request.method,
        headers: request.headers,
        body: request.method !== "GET" ? await request.text() : undefined
      });

      return stub.fetch(doReq);
    }

    return new Response("front2-service 404 not found", { status:404 });
  }
};
