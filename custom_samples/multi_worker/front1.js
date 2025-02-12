import FRONT1_HTML from "front1.html";

export default {
  async fetch(request, env) {
    let url = new URL(request.url);

    if (url.pathname === "/") {

      return new Response(FRONT1_HTML, {
        headers: { "Content-Type": "text/html" }
      });

    }

    else if (url.pathname.startsWith("/api/")) {
      return handleApiRequest(request, env);
    }

    return new Response("front1-service 404 not found", { status:404 });
  }
};

async function handleApiRequest(request, env) {
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
