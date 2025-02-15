import FRONT2_HTML from "front2.html";

export default {
  async fetch(request, env) {
    let url = new URL(request.url);

    if (url.pathname === "/") {
      return new Response(FRONT2_HTML, {
        headers: { "Content-Type": "text/html; charset=utf-8" },
      });
    }

    else if (url.pathname.startsWith("/api/")) {
      return handleApiRequest(request, env);
    }

    return new Response("front2-service 404 not found", { status: 404 });
  },
};

async function handleApiRequest(request, env) {
  let pathPart = request.url.replace(/^.*\/api\//, "");

  let id = env.REMOTE_BOOK_DO.idFromName("mySharedBookDB");

  let stub = env.REMOTE_BOOK_DO.get(id);

  let doUrl = "https://fake-host/" + pathPart;

  let body = null;
  if (request.method !== "GET") {
    body = await request.text();
    // console.log("[front2] Non-GET request body =>", body);
  }

  let doReq = new Request(doUrl, {
    method: request.method,
    headers: request.headers,
    body,
  });

  let doResp;
  try {
    doResp = await stub.fetch(doReq);
    // console.log("[front2] stub.fetch() => status:", doResp.status);
  } catch (err) {
    // console.log("[front2] ERROR calling stub.fetch():", err);
    return new Response("Failed to contact DO: " + err.message, { status: 502 });
  }

  return doResp;
}
