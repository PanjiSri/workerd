import { MongoClient } from 'mongodb';

const MONGODB_URI = 'mongodb://127.0.0.1:27017';
const DB_NAME = 'mern-simple';

let db;

export async function connectDB() {
  try {
    const client = new MongoClient(MONGODB_URI);
    await client.connect();
    db = client.db(DB_NAME);
    console.log('Connected to MongoDB');
  } catch (error) {
    console.error('MongoDB connection error:', error);
    process.exit(1);
  }
}

export function getCollection(name) {
  return db.collection(name);
}
