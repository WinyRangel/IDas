
-- Creación de la tabla 'sensors'

CREATE TABLE IF NOT EXISTS sensors (

    id INTEGER PRIMARY KEY AUTOINCREMENT,

    sensor_type TEXT,

    location TEXT,

    installation_date DATETIME DEFAULT CURRENT_TIMESTAMP,

    user_id INTEGER,

    FOREIGN KEY (user_id) REFERENCES users(id)

);

INSERT INTO sensors (sensor_type, location, user_id) VALUES ('heartRate', 'Dolores Hidalgo', 1);
INSERT INTO sensors (sensor_type, location, user_id) VALUES ('spO2', 'Dolores Hidalgo', 1);
INSERT INTO sensors (sensor_type, location, user_id) VALUES ('temperature', 'Dolores Hidalgo', 1);
INSERT INTO sensors (sensor_type, location, user_id) VALUES ('humidity', 'Dolores Hidalgo', 1);
INSERT INTO sensors (sensor_type, location, user_id) VALUES ('stepCount', 'Dolores Hidalgo', 1);
INSERT INTO sensors (sensor_type, location, user_id) VALUES ('Giroscopio', 'Dolores Hidalgo', 1);



CREATE TABLE IF NOT EXISTS users (

    id INTEGER PRIMARY KEY AUTOINCREMENT,

    username TEXT,

    email TEXT,

    registration_date DATETIME DEFAULT CURRENT_TIMESTAMP

);

INSERT INTO users (username, email) VALUES ('Daniela', 'danielamanzanorangel@gmail.com');


-- Creación de la tabla 'sensor_details'

CREATE TABLE IF NOT EXISTS sensor_details (

    id INTEGER PRIMARY KEY AUTOINCREMENT,

    sensor_id INTEGER,

    reading REAL,

    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,

    user_id INTEGER,

    FOREIGN KEY (sensor_id) REFERENCES sensors(id),

    FOREIGN KEY (user_id) REFERENCES users(id)

);
