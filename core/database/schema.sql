CREATE TABLE daily_sessions (
    id INTEGER PRIMARY KEY,
    session_date TEXT NOT NULL,
    UNIQUE (session_date)
);

CREATE TABLE measurements (
    id INTEGER PRIMARY KEY,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL,
    download_mbps REAL,
    upload_mbps REAL,
    ping_ms INTEGER,
    jitter_ms INTEGER,
    packet_loss_percent REAL,
    connection_type TEXT,
    measured_at TEXT NOT NULL,
    status TEXT NOT NULL,
    session_id INTEGER NOT NULL,
    CHECK (status = 'success' OR status = 'no_connection' OR  status = 'test_error'),
    CHECK (download_mbps >= 0),
    CHECK (upload_mbps >= 0),
    CHECK (jitter_ms >= 0),
    CHECK (ping_ms >= 0),
    CHECK (packet_loss_percent >= 0 AND packet_loss_percent <= 100 ),
    CHECK (latitude >= -90 AND latitude <= 90),
    CHECK (longitude >= -180 AND longitude <= 180),
    FOREIGN KEY(session_id) REFERENCES daily_sessions(id)
    
);
CREATE INDEX session_index
ON measurements (session_id);
CREATE INDEX measured_index
ON measurements (measured_at);

CREATE TABLE dead_zones (
    id INTEGER PRIMARY KEY,
    latitude REAL,
    longitude REAL,
    measured_at TEXT,
    measurement_id INTEGER NOT NULL,
    UNIQUE (measurement_id),
    FOREIGN KEY(measurement_id) REFERENCES measurements(id)
);

CREATE TABLE map_aggregates (
    id INTEGER PRIMARY KEY,
    latitude REAL,
    longitude REAL,
    avg_download_mbps REAL,
    avg_upload_mbps REAL,
    avg_ping_ms REAL,
    avg_jitter_ms REAL,
    avg_packet_loss_percent REAL,
    connection_type TEXT,
    measured_at TEXT,
    measurement_count INTEGER NOT NULL,
    CHECK (measurement_count > 0),
    CHECK (connection_type = '4G' OR connection_type = 'WI-FI' OR connection_type = '5G')
);

CREATE TABLE settings (
    id INTEGER PRIMARY KEY,
    background_monitoring INTEGER NOT NULL DEFAULT 0,
    measurement_interval INTEGER NOT NULL DEFAULT 120,
    CHECK(measurement_interval > 0), 
    CHECK (background_monitoring = 0 OR background_monitoring = 1)
);

CREATE TABLE schema_migrations (
    id INTEGER PRIMARY KEY,
    version_migrations INTEGER NOT NULL,
    data_migrations TEXT NOT NULL,
    UNIQUE (version_migrations)
);