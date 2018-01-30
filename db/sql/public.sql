CREATE TABLE "peer" (
    "id" TEXT NOT NULL,
    "port" INTEGER NOT NULL,
    "ip_addr" TEXT NOT NULL
);
CREATE UNIQUE INDEX "peer_ind" on peer (id ASC);
CREATE TABLE "phone_number" (
    "group_id" INTEGER NOT NULL,
    "value" TEXT NOT NULL
);
