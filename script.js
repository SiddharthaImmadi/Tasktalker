document.addEventListener('DOMContentLoaded', () => {
    const taskForm = document.getElementById('taskForm');
    const taskList = document.getElementById('taskList');
    const taskCount = document.getElementById('task_count');
    const searchInput = document.getElementById('searchInput');
    const themeToggle = document.getElementById('themeToggle');
    const formTitle = document.querySelector('.left-panel h1');
    const submitButton = document.querySelector('.btn-primary');
    const cancelButton = document.getElementById('btnCancel');

    let allTasks = [];
    let editingTaskId = null;

    // --- Theme Logic ---
    let isLight = localStorage.getItem('theme') === 'light';
    updateTheme();

    themeToggle.addEventListener('click', () => {
        isLight = !isLight;
        localStorage.setItem('theme', isLight ? 'light' : 'dark');
        updateTheme();
    });

    function updateTheme() {
        if (isLight) {
            document.body.classList.add('light-theme');
            themeToggle.textContent = '🌙';
        } else {
            document.body.classList.remove('light-theme');
            themeToggle.textContent = '☀️';
        }
    }

    // --- Data Logic ---

    // Fetch and display tasks
    function fetchTasks() {
        fetch('/tasks')
            .then(res => res.json())
            .then(data => {
                allTasks = data;
                filterAndRender();
            })
            .catch(err => console.error('Error fetching tasks:', err));
    }

    // Filter tasks based on search input
    function filterAndRender() {
        const query = searchInput.value.toLowerCase();
        const filtered = allTasks.filter(task =>
            task.name.toLowerCase().includes(query) ||
            (task.description && task.description.toLowerCase().includes(query))
        );
        renderTasks(filtered);
    }

    searchInput.addEventListener('input', filterAndRender);

    // Render tasks to DOM
    function renderTasks(tasks) {
        taskList.innerHTML = '';
        taskCount.textContent = `${tasks.length} Tasks`;

        // Sort tasks: High > Medium > Low
        const priorityOrder = { 'High': 3, 'Medium': 2, 'Low': 1 };
        tasks.sort((a, b) => priorityOrder[b.priority] - priorityOrder[a.priority]);

        if (tasks.length === 0) {
            taskList.innerHTML = '<div style="text-align:center; color: var(--text-secondary); padding: 2rem;">No tasks found</div>';
            return;
        }

        tasks.forEach(task => {
            const card = document.createElement('div');
            card.className = 'task-card';

            let badgeClass = 'badge-low';
            if (task.priority === 'High') badgeClass = 'badge-high';
            else if (task.priority === 'Medium') badgeClass = 'badge-medium';

            card.innerHTML = `
                <div class="task-info">
                    <div class="task-name">${task.name}</div>
                    <div class="task-desc">${task.description}</div>
                    <div class="task-meta">
                        <span>📅 ${task.due_date ? task.due_date : 'No Date'} ${task.due_time ? 'at ' + task.due_time : ''}</span>
                    </div>
                </div>
                <div class="task-actions-col">
                    <span class="badge ${badgeClass}">${task.priority}</span>
                    <div class="action-buttons">
                        <button class="btn-edit" data-id="${task.id}" title="Edit Task">✏️</button>
                        <button class="btn-delete" data-id="${task.id}" title="Delete Task">🗑️</button>
                    </div>
                </div>
            `;

            // Add listeners
            const editBtn = card.querySelector('.btn-edit');
            const deleteBtn = card.querySelector('.btn-delete');

            editBtn.addEventListener('click', () => loadTaskIntoForm(task));
            deleteBtn.addEventListener('click', () => deleteTask(task.id));

            taskList.appendChild(card);
        });
    }

    function loadTaskIntoForm(task) {
        editingTaskId = task.id;
        document.getElementById('name').value = task.name;
        document.getElementById('description').value = task.description;
        document.getElementById('due_date').value = task.due_date;
        document.getElementById('due_time').value = task.due_time;
        document.getElementById('priority').value = task.priority;

        formTitle.textContent = 'Edit Task';
        submitButton.textContent = 'Update Task';
        cancelButton.style.display = 'block';
    }

    function deleteTask(id) {
        if (!confirm('Are you sure you want to delete this task?')) return;

        fetch(`/tasks?id=${id}`, { method: 'DELETE' })
            .then(res => {
                if (res.ok) fetchTasks();
                else console.error('Failed to delete');
            })
            .catch(err => console.error('Error deleting:', err));
    }

    // Handle Form Submit
    taskForm.addEventListener('submit', (e) => {
        e.preventDefault();

        const formData = new FormData(taskForm);
        const data = Object.fromEntries(formData.entries());

        const url = '/tasks';
        let method = 'POST';

        if (editingTaskId !== null) {
            method = 'PUT';
            data.id = editingTaskId; // Include ID for update
        }

        fetch(url, {
            method: method,
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        })
            .then(res => {
                if (res.ok) {
                    resetForm();
                    fetchTasks(); // Refresh list
                }
            })
            .catch(err => console.error('Error adding/updating task:', err));
    });

    cancelButton.addEventListener('click', resetForm);

    function resetForm() {
        taskForm.reset();
        editingTaskId = null;
        formTitle.textContent = 'New Task';
        submitButton.textContent = 'Add Task';
        cancelButton.style.display = 'none';
        // Reset priority due to select default?
        document.getElementById('priority').value = 'Low';
    }

    // Initial Load
    fetchTasks();

    // Poll for updates (e.g. priority changes based on time) every minute
    setInterval(fetchTasks, 60000);
});
