import { useState, useEffect } from "react"
import axios from "axios"

function TaskList() {
  const [tasks, setTasks] = useState({})  
  const [taskName, setTaskName] = useState("")

  useEffect(() => {
    fetchTasks()
  }, [])

  const fetchTasks = async () => {
    try {
      const response = await axios.get("/api/tasks")
      setTasks(response.data)
    } catch (error) {
      console.error("Error fetching tasks:", error)
    }
  }

  const addTask = async () => {
    if (!taskName.trim()) return
    try {
      const response = await axios.post("/api/tasks", { name: taskName })
      setTasks(prev => ({...prev, [response.data.id]: response.data}))
      setTaskName("")
    } catch (error) {
      console.error("Error adding task:", error)
    }
  }

  const deleteTask = async (id) => {
    try {
      await axios.delete(`/api/tasks/${id}`)
      setTasks(prev => {
        const newTasks = {...prev}
        delete newTasks[id]
        return newTasks
      })
    } catch (error) {
      console.error("Error deleting task:", error)
    }
  }

  const toggleTask = async (id) => {
    try {
      const task = tasks[id]
      const response = await axios.put(`/api/tasks/${id}`, {
        ...task,
        completed: !task.completed
      })
      setTasks(prev => ({...prev, [id]: response.data}))
    } catch (error) {
      console.error("Error updating task:", error)
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
        {Object.values(tasks).map((task) => (
          // From _id to id
          <li key={task.id}>
            <div className="task-content">
              <input
                type="checkbox"
                checked={task.completed}
                onChange={() => toggleTask(task.id)}
              />
              <span style={{ textDecoration: task.completed ? 'line-through' : 'none' }}>
                {task.name}
              </span>
            </div>
            {/* From _id to id */}
            <button onClick={() => deleteTask(task.id)}>Delete</button>
          </li>
        ))}
      </ul>
    </div>
  )
}

export default TaskList
