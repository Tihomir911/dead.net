CREATE TABLE daily_sessions (
    id INTEGER PRIMARY KEY,
    session_date TEXT
    
);

CREATE TABLE measurements (
    id INTEGER PRIMARY KEY,
    latitude REAL,
    longitude REAL,
    download_mbps REAL,
    upload_mbps REAL,
    ping_ms INTEGER,
    jitter_ms INTEGER,
    packet_loss_percent REAL,
    connection_type TEXT,
    measured_at TEXT,
    session_id INTEGER,
    FOREIGN KEY(session_id) REFERENCES daily_sessions(id)
);





