CREATE TABLE "sensor_mapping" (
    "sensor_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL
);
CREATE TABLE "prog"
(
  "id" INTEGER PRIMARY KEY,
  "description" TEXT NOT NULL,
  "sensor_id" INTEGER NOT NULL,
  "cycle_duration_sec" INTEGER NOT NULL,
  "cycle_duration_nsec" INTEGER NOT NULL,

  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
