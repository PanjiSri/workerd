import { useState, useEffect } from "react"
import axios from "axios"

function TaskList() {
  const [tasks, setTasks] = useState([])
  const [taskName, setTaskName] = useState("")

  useEffect(() => {
    fetchTasks()
  }, [])

  const fetchTasks = async () => {
    try {
      const response = await axios.get("http://localhost:5000/api/tasks")
      setTasks(response.data)
    } catch (error) {
      console.error("Error fetching tasks:", error)
    }
  }

  const addTask = async () => {
    if (!taskName.trim()) return
    try {
      const response = await axios.post("http://localhost:5000/api/tasks", { name: taskName })
      setTasks([...tasks, response.data])
      setTaskName("")
    } catch (error) {
      console.error("Error adding task:", error)
    }
  }

  const deleteTask = async (id) => {
    try {
      await axios.delete(`http://localhost:5000/api/tasks/${id}`)
      setTasks(tasks.filter((task) => task._id !== id))
    } catch (error) {
      console.error("Error deleting task:", error)
    }
  }

  const handleKeyPress = (e) => {
    if (e.key === "Enter") {
      addTask()
    }
  }

  return (
    <div>
      <div className="flex">
        <input
          type="text"
          value={taskName}
          onChange={(e) => setTaskName(e.target.value)}
          onKeyPress={handleKeyPress}
          placeholder="Add a new task"
          className="task-input"
        />
        <button onClick={addTask}>Add Task</button>
      </div>
      <ul className="task-list">
        {tasks.map((task) => (
          <li key={task._id}>
            {task.name}
            <button onClick={() => deleteTask(task._id)}>Delete</button>
          </li>
        ))}
      </ul>
    </div>
  )
}

export default TaskList