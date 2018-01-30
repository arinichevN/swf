
delete from sensor_mapping;
INSERT INTO sensor_mapping (sensor_id,peer_id,remote_id) VALUES (1,'obj_1',1);
INSERT INTO sensor_mapping (sensor_id,peer_id,remote_id) VALUES (2,'obj_1',2);
INSERT INTO sensor_mapping (sensor_id,peer_id,remote_id) VALUES (3,'obj_1',3);
INSERT INTO sensor_mapping (sensor_id,peer_id,remote_id) VALUES (4,'obj_1',4);

delete from prog;
INSERT INTO prog(id,description,sensor_id,cycle_duration_sec,cycle_duration_nsec,enable,load) VALUES 
(1,'камера1',1,1,0,1,1);
INSERT INTO prog(id,description,sensor_id,cycle_duration_sec,cycle_duration_nsec,enable,load) VALUES 
(2,'камера2',2,1,0,1,1);
INSERT INTO prog(id,description,sensor_id,cycle_duration_sec,cycle_duration_nsec,enable,load) VALUES 
(3,'камера3',3,1,0,1,1);
INSERT INTO prog(id,description,sensor_id,cycle_duration_sec,cycle_duration_nsec,enable,load) VALUES 
(4,'камера4',4,1,0,1,1);


