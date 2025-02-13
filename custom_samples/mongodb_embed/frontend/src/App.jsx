import TaskList from "./components/TaskList"
import "./App.css"

function App() {
  return (
    <div className="App">
      <h1 className="text-2xl font-bold mb-4">Simple Task Manager</h1>
      <TaskList />
    </div>
  )
}

export default App