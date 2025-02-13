import { Hono } from 'hono';
import { connectDB, getCollection } from './mongo.js';

await connectDB();
const tasksCollection = getCollection('tasks');

const app = new Hono();

app.use('*', async (c, next) => {
  c.header('Access-Control-Allow-Origin', '*');
  c.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE');
  await next();
});

app.get('/api/tasks', async (c) => {
  try {
    const tasks = await tasksCollection.find().toArray();
    return c.json(tasks);
  } catch (error) {
    return c.json({ error: 'Failed to fetch tasks' }, 500);
  }
});

app.post('/api/tasks', async (c) => {
  try {
    const task = await c.req.json();
    const result = await tasksCollection.insertOne(task);
    return c.json({ ...task, _id: result.insertedId }, 201);
  } catch (error) {
    return c.json({ error: 'Failed to create task' }, 500);
  }
});

export default {
  fetch: app.fetch
};
