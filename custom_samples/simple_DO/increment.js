export class IncrementDurableObject {
  constructor(state, env) {
    this.state = state;
    this.env = env;
  }

  async fetch(request) {
    let count = (await this.state.storage.get("count")) || 0;
    count++;
    await this.state.storage.put("count", count);
    return new Response(`Count = ${count}\n`);
  }
}

export default {
  async fetch(request, env, ctx) {
    let id = env.COUNTER.idFromName("globalCounter");
    let stub = env.COUNTER.get(id);

    return stub.fetch(request);
  }
};
