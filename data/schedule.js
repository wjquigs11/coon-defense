let schedules = [];
let currentScheduleIndex = -1;
let currentAmPm = 'AM';

// Load schedules on page load
document.addEventListener('DOMContentLoaded', function() {
    loadSchedules();
    loadRelayState();
    updateTimeDisplay();
});

function goBack() {
    window.history.back();
}

async function loadSchedules() {
    try {
        const response = await fetch('/api/schedules');
        if (response.ok) {
            schedules = await response.json();
            renderSchedules();
        } else {
            console.error('Failed to load schedules');
        }
    } catch (error) {
        console.error('Error loading schedules:', error);
    }
}

async function saveSchedules() {
    try {
        const response = await fetch('/api/schedules', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(schedules)
        });
        
        if (response.ok) {
            console.log('Schedules saved successfully');
        } else {
            console.error('Failed to save schedules');
        }
    } catch (error) {
        console.error('Error saving schedules:', error);
    }
}

function renderSchedules() {
    const scheduleList = document.getElementById('scheduleList');
    scheduleList.innerHTML = '';

    schedules.forEach((schedule, index) => {
        const scheduleCard = document.createElement('div');
        scheduleCard.className = 'schedule-card';
        scheduleCard.onclick = () => editSchedule(index);

        const daysText = getDaysText(schedule.days);
        const actionText = getActionText(schedule.switches);

        scheduleCard.innerHTML = `
            <div class="schedule-info">
                <div class="schedule-time">${schedule.time} ${schedule.ampm}</div>
                <div class="schedule-details">${daysText}</div>
                <div class="schedule-details">${actionText}</div>
            </div>
            <label class="switch" onclick="event.stopPropagation()">
                <input type="checkbox" ${schedule.enabled ? 'checked' : ''} onchange="toggleSchedule(${index}, this.checked)">
                <span class="slider"></span>
            </label>
        `;

        scheduleList.appendChild(scheduleCard);
    });
}

function getDaysText(days) {
    const dayNames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    if (days.length === 7) {
        return 'Every day';
    } else if (days.length === 5 && days.includes(1) && days.includes(2) && days.includes(3) && days.includes(4) && days.includes(5)) {
        return 'Weekdays';
    } else if (days.length === 2 && days.includes(0) && days.includes(6)) {
        return 'Weekends';
    } else {
        return days.map(day => dayNames[day]).join(', ');
    }
}

function getActionText(switches) {
    const onSwitches = [];
    const offSwitches = [];
    
    for (const [switchNum, state] of Object.entries(switches)) {
        if (state) {
            onSwitches.push(`Switch ${switchNum}`);
        } else {
            offSwitches.push(`Switch ${switchNum}`);
        }
    }
    
    if (onSwitches.length > 0 && offSwitches.length === 0) {
        return onSwitches.join(', ') + ': ON';
    } else if (offSwitches.length > 0 && onSwitches.length === 0) {
        return offSwitches.join(', ') + ': OFF';
    } else if (onSwitches.length === 0 && offSwitches.length === 0) {
        return 'No action';
    } else {
        return 'Mixed actions';
    }
}

function addNewSchedule() {
    currentScheduleIndex = -1;
    document.getElementById('modalTitle').textContent = 'Add Schedule';
    document.getElementById('deleteBtn').style.display = 'none';
    resetModal();
    document.getElementById('scheduleModal').classList.add('active');
}

function editSchedule(index) {
    currentScheduleIndex = index;
    const schedule = schedules[index];
    
    document.getElementById('modalTitle').textContent = 'Edit Schedule';
    document.getElementById('deleteBtn').style.display = 'block';
    
    // Set time
    const [time, ampm] = [schedule.time, schedule.ampm];
    const [hour, minute] = time.split(':');
    document.getElementById('hourInput').value = hour;
    document.getElementById('minuteInput').value = minute;
    setAmPm(ampm);
    
    // Set days
    document.querySelectorAll('.day-btn').forEach(btn => {
        const day = parseInt(btn.dataset.day);
        if (schedule.days.includes(day)) {
            btn.classList.add('active');
        } else {
            btn.classList.remove('active');
        }
    });
    
    // Set switches
    for (let i = 1; i <= 4; i++) {
        const switchElement = document.getElementById(`switch${i}`);
        const state = schedule.switches[i.toString()] || false;
        switchElement.checked = state;
        updateSwitchState(i, state);
    }
    
    updateTimeDisplay();
    document.getElementById('scheduleModal').classList.add('active');
}

function resetModal() {
    // Reset time
    document.getElementById('hourInput').value = '12';
    document.getElementById('minuteInput').value = '0';
    setAmPm('AM');
    
    // Reset days (select all)
    document.querySelectorAll('.day-btn').forEach(btn => {
        btn.classList.add('active');
    });
    
    // Reset switches
    for (let i = 1; i <= 4; i++) {
        document.getElementById(`switch${i}`).checked = false;
        updateSwitchState(i, false);
    }
    
    updateTimeDisplay();
}

function closeModal() {
    document.getElementById('scheduleModal').classList.remove('active');
}

function updateTimeDisplay() {
    const hour = document.getElementById('hourInput').value;
    const minute = document.getElementById('minuteInput').value.padStart(2, '0');
    document.getElementById('timeDisplay').textContent = `${hour}:${minute}`;
}

function setAmPm(ampm) {
    currentAmPm = ampm;
    document.getElementById('amBtn').classList.toggle('active', ampm === 'AM');
    document.getElementById('pmBtn').classList.toggle('active', ampm === 'PM');
}

function toggleDay(button) {
    button.classList.toggle('active');
}

function updateSwitchState(switchNum, state) {
    document.getElementById(`switch${switchNum}State`).textContent = state ? 'ON' : 'OFF';
}

function toggleSchedule(index, enabled) {
    schedules[index].enabled = enabled;
    saveSchedules();
}

function saveSchedule() {
    const hour = document.getElementById('hourInput').value;
    const minute = document.getElementById('minuteInput').value.padStart(2, '0');
    const time = `${hour}:${minute}`;
    
    const selectedDays = [];
    document.querySelectorAll('.day-btn.active').forEach(btn => {
        selectedDays.push(parseInt(btn.dataset.day));
    });
    
    const switches = {};
    for (let i = 1; i <= 4; i++) {
        switches[i.toString()] = document.getElementById(`switch${i}`).checked;
    }
    
    const schedule = {
        time: time,
        ampm: currentAmPm,
        days: selectedDays,
        switches: switches,
        enabled: true
    };
    
    if (currentScheduleIndex === -1) {
        schedules.push(schedule);
    } else {
        schedules[currentScheduleIndex] = schedule;
    }
    
    saveSchedules();
    renderSchedules();
    closeModal();
}

function deleteSchedule() {
    if (currentScheduleIndex !== -1) {
        schedules.splice(currentScheduleIndex, 1);
        saveSchedules();
        renderSchedules();
        closeModal();
    }
}

// Relay control functions
async function loadRelayState() {
    try {
        const response = await fetch('/readings');
        if (response.ok) {
            // For now, we'll assume relay is off. You can implement actual state reading if needed.
            document.getElementById('relayToggle').checked = false;
        }
    } catch (error) {
        console.error('Error loading relay state:', error);
    }
}

async function toggleRelay(checkbox) {
    const state = checkbox.checked ? 1 : 0;
    try {
        const response = await fetch(`/relayupdate?relay=1&state=${state}`);
        if (!response.ok) {
            console.error('Failed to toggle relay');
            // Revert checkbox state on failure
            checkbox.checked = !checkbox.checked;
        }
    } catch (error) {
        console.error('Error toggling relay:', error);
        // Revert checkbox state on failure
        checkbox.checked = !checkbox.checked;
    }
}

// Close modal when clicking outside
document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('scheduleModal').addEventListener('click', function(e) {
        if (e.target === this) {
            closeModal();
        }
    });
});