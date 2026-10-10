# RaidVol — Software Volume Manager / Software RAID Layer

## 1. Project Overview

**RaidVol** is a software-based volume manager and RAID layer implemented in **C11 on Linux**.

The project creates a virtual block-level storage abstraction backed by multiple disk image files. Instead of accessing individual disks directly, applications can perform read/write operations through the RaidVol layer.

RaidVol supports:

- RAID-0 striping
- RAID-1 mirroring
- RAID-5 striping with distributed XOR parity
- Virtual block to physical disk mapping
- `pread()` / `pwrite()` based disk I/O
- Persistent volume metadata
- RAID-5 degraded reads
- RAID-5 device rebuild
- CRC32-based checksum verification
- Silent corruption detection
- Persistent checksum storage
- Performance benchmarking

---

# 2. Problem Statement

Traditional storage systems may use multiple physical storage devices to provide higher performance, redundancy, or fault tolerance.

The objective of RaidVol is to implement these concepts at the software level by providing a **single logical volume abstraction** over multiple disk image files.

The system must correctly:

1. Map virtual blocks to physical disks.
2. Perform physical read/write operations.
3. Maintain RAID redundancy.
4. Recover data after simulated disk failures.
5. Detect silent data corruption.
6. Store and recover volume metadata.
7. Measure performance of different RAID levels.

---

# 3. Objectives

The major objectives of RaidVol are:

- Implement RAID-0 striping.
- Implement RAID-1 mirroring.
- Implement RAID-5 XOR parity.
- Implement virtual-to-physical block mapping.
- Perform block-level I/O using Linux system calls.
- Verify written data.
- Store persistent RAID metadata.
- Support degraded operation.
- Rebuild failed RAID-5 data.
- Detect silent corruption using checksums.
- Benchmark different storage configurations.

---

# 4. Technology Stack

| Component | Technology |
|---|---|
| Language | C11 |
| Operating System | Linux / WSL |
| Compiler | GCC |
| Build System | GNU Make |
| I/O | `pread()` / `pwrite()` |
| Storage | Disk image files |
| RAID | RAID-0, RAID-1, RAID-5 |
| Checksum | CRC32 |
| Timing | `clock_gettime()` |

---

# 5. System Architecture

```text
                    +----------------------+
                    |      RaidVol CLI      |
                    |      main.c           |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |   Volume Manager     |
                    |      volume.c        |
                    +----------+-----------+
                               |
             +-----------------+-----------------+
             |                 |                 |
             v                 v                 v
        +---------+       +---------+       +---------+
        | RAID-0  |       | RAID-1  |       | RAID-5  |
        | raid0.c |       | raid1.c |       | raid5.c |
        +----+----+       +----+----+       +----+----+
             |                 |                 |
             +-----------------+-----------------+
                               |
                               v
                    +----------------------+
                    |    Device Layer      |
                    |      device.c        |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    | Linux Disk Images    |
                    | disk*.img / raid*.img|
                    +----------------------+

       Metadata --------------------> metadata.c
       Checksums -------------------> checksum.c
       Persistent Checksums --------> checksum_store.c
       Benchmarking ----------------> benchmark.c
```

---

# 6. Project Structure

```text
raidvol/
├── Makefile
├── README.md
│
├── disks/
│   ├── disk0.img
│   ├── disk1.img
│   ├── disk2.img
│   ├── raid0_0.img
│   ├── raid0_1.img
│   ├── raid0_2.img
│   ├── raid5_0.img
│   ├── raid5_1.img
│   ├── raid5_2.img
│   └── raid5_checksums.bin
│
├── include/
│   ├── benchmark.h
│   ├── checksum.h
│   ├── checksum_store.h
│   ├── device.h
│   ├── metadata.h
│   ├── raid0.h
│   ├── raid1.h
│   ├── raid5.h
│   └── volume.h
│
├── src/
│   ├── benchmark.c
│   ├── checksum.c
│   ├── checksum_store.c
│   ├── device.c
│   ├── main.c
│   ├── metadata.c
│   ├── raid0.c
│   ├── raid1.c
│   ├── raid5.c
│   └── volume.c
│
└── tests/
    ├── test_checksum.c
    ├── test_metadata.c
    ├── test_metadata_raid1.c
    └── test_raid1.c
```

---

# 7. Disk Layout

RaidVol reserves the beginning of every disk image for metadata.

```text
Disk Image
+----------------------------+
| Metadata                   |
| 0 - 4095 bytes             |
+----------------------------+
| Data Block 0               |
| 4096 bytes                 |
+----------------------------+
| Data Block 1               |
| 4096 bytes                 |
+----------------------------+
| Data Block 2               |
| 4096 bytes                 |
+----------------------------+
| ...                        |
+----------------------------+
```

Important constants:

```text
Metadata size = 4096 bytes
Data offset   = 4096 bytes
Block size    = 4096 bytes
```

---

# 8. RAID-0

RAID-0 distributes data blocks across multiple disks.

For three disks:

```text
Virtual Block 0 -> Disk 0
Virtual Block 1 -> Disk 1
Virtual Block 2 -> Disk 2
Virtual Block 3 -> Disk 0
Virtual Block 4 -> Disk 1
Virtual Block 5 -> Disk 2
```

The mapping is based on:

```text
disk = virtual_block % device_count
```

The physical block increases after each complete stripe.

### Advantages

- Data is distributed across multiple devices.
- Higher parallelism is possible.
- No parity overhead.

### Limitation

RAID-0 provides **no redundancy**. Failure of one device can make part of the volume unavailable.

---

# 9. RAID-1

RAID-1 mirrors every data block onto two disks.

```text
Virtual Block 0
       |
       +------> Disk 0
       |
       +------> Disk 1
```

A write therefore performs two physical writes.

```text
Virtual Block 0
        |
        +---- Disk 0
        |
        +---- Disk 1
```

During normal reading, data can be read from one mirror.

If one disk fails:

```text
Disk 0 -> FAILED

Read Request
     |
     v
Disk 1 Mirror
     |
     v
Original Data
```

The implementation was tested for:

- Normal read
- Physical mirror consistency
- Disk failure
- Data recovery from the surviving mirror

---

# 10. RAID-5

RaidVol implements a simplified three-disk RAID-5 configuration.

Each stripe contains:

```text
2 Data Blocks
1 Parity Block
```

Parity is calculated using XOR:

```text
P = A XOR B
```

Example:

```text
Stripe 0:

Disk 0 -> Parity
Disk 1 -> Data A
Disk 2 -> Data B
```

The parity disk rotates between stripes.

```text
Stripe 0 -> Disk 0 parity
Stripe 1 -> Disk 1 parity
Stripe 2 -> Disk 2 parity
Stripe 3 -> Disk 0 parity
...
```

---

# 11. RAID-5 Mapping

For three disks, every stripe contains two data blocks.

The virtual block determines:

1. Stripe number
2. Data position
3. Physical disk
4. Physical block

Conceptually:

```text
Virtual Block
      |
      v
+-------------+
| Stripe      |
+-------------+
      |
      v
+-------------+
| Data        |
| Position    |
+-------------+
      |
      v
+-------------+
| Physical    |
| Disk        |
+-------------+
```

Example:

```text
Virtual Block 0 -> Disk 1, Physical Block 0
Virtual Block 1 -> Disk 2, Physical Block 0
Virtual Block 2 -> Disk 0, Physical Block 1
Virtual Block 3 -> Disk 2, Physical Block 1
```

---

# 12. RAID-5 Write

When writing a block, RaidVol:

1. Determines the destination data disk.
2. Determines the other data block in the stripe.
3. Reads the other data block.
4. Calculates new parity.
5. Writes the new data.
6. Writes the new parity.

Formula:

```text
Parity = New Data XOR Other Data
```

---

# 13. RAID-5 Read

A normal RAID-5 read determines the physical location of the requested virtual block and reads the corresponding physical block.

```text
Virtual Block
      |
      v
raid5_map()
      |
      v
Physical Disk + Block
      |
      v
device_read()
```

---

# 14. RAID-5 Degraded Read

If a data disk fails, its data can be reconstructed using:

```text
Missing Data = Surviving Data XOR Parity
```

Example:

```text
Disk 1 -> FAILED
Disk 2 -> Data B
Disk 0 -> Parity

Recovered Data A:

A = B XOR P
```

RaidVol successfully verifies that:

```text
Original Data == Recovered Data
```

---

# 15. RAID-5 Rebuild

After a failed disk is replaced or becomes available again, RaidVol can reconstruct the missing data.

The process is:

```text
Surviving Data
      +
Parity
      |
      v
XOR Reconstruction
      |
      v
Recovered Data
      |
      v
Write to Failed Disk
      |
      v
Byte-for-byte Verification
```

The rebuild test verifies both:

```text
Original Data == Recovered Data
```

and:

```text
Recovered Disk Data == Original Disk Data
```

---

# 16. Metadata System

RaidVol stores persistent metadata inside each disk image.

Metadata contains:

```text
Magic
Version
RAID Level
Device Count
Block Size
Volume Size
```

Example:

```text
Magic        : RAIDVOL
Version      : 1
RAID Level   : 5
Device Count : 3
Block Size   : 4096
Volume Size  : 1048576 bytes
```

The metadata allows RaidVol to discover the configuration when opening an existing volume.

---

# 17. Metadata Validation

When opening a volume, RaidVol verifies:

- Metadata magic
- Version
- RAID level
- Device count
- Block size
- Volume size
- Consistency across all supplied disks

Example:

```text
Metadata discovered successfully.
Detected RAID Level: 5
Detected Device Count: 3
Detected Block Size: 4096
All disk metadata matches.
```

---

# 18. Checksum System

RaidVol implements CRC32 checksums for detecting silent data corruption.

For each block:

```text
Data Block
    |
    v
CRC32 Calculation
    |
    v
Stored Checksum
```

During verification:

```text
Current Data
     |
     v
CRC32 Calculation
     |
     v
Compare with Stored Checksum
```

If they differ:

```text
Checksum mismatch detected!
```

This detects corruption even when the physical disk operation itself succeeds.

---

# 19. Persistent Checksum Storage

Checksums are stored separately in:

```text
disks/raid5_checksums.bin
```

This allows checksum information to persist beyond a single execution of the program.

The system supports:

- Writing checksums
- Reading checksums
- Verifying checksums
- Detecting corrupted blocks
- Restoring corrupted data
- Multi-block checksum verification

---

# 20. Silent Corruption Detection

RaidVol explicitly simulates silent corruption.

Example:

```text
Original Data
     |
     v
Checksum = C
     |
     v
Physical Byte Modified
     |
     v
Read Data
     |
     v
New Checksum != C
     |
     v
CORRUPTION DETECTED
```

The corruption tests successfully detect modified physical data.

---

# 21. Device Layer

The device abstraction is implemented in:

```text
src/device.c
include/device.h
```

Main operations:

```c
int device_open(const char *path);

ssize_t device_read(
    int fd,
    void *buffer,
    size_t size,
    off_t offset
);

ssize_t device_write(
    int fd,
    const void *buffer,
    size_t size,
    off_t offset
);

void device_close(int fd);
```

This separates physical storage operations from RAID logic.

---

# 22. Volume Manager

The volume layer connects metadata discovery with the RAID implementations.

Conceptually:

```text
CLI
 |
 v
Metadata
 |
 v
RAID Level Detection
 |
 +---- RAID-0
 |
 +---- RAID-1
 |
 +---- RAID-5
 |
 v
Physical I/O
```

This allows an existing volume to be opened using its stored metadata.

---

# 23. Benchmarking

RaidVol contains a performance benchmark framework.

The benchmark measures:

- Single-disk write throughput
- Single-disk read throughput
- RAID-0 write/read throughput
- RAID-1 write/read throughput
- RAID-5 write/read throughput

Benchmark configuration:

```text
Block Size : 4096 bytes
Blocks     : 256
Total Data : 1 MB
```

Timing uses:

```c
clock_gettime(CLOCK_MONOTONIC, ...)
```

Throughput is calculated from the amount of data processed and elapsed time.

The benchmark is intended to provide an experimental comparison of the different storage layouts on the current environment.

---

# 24. Build Instructions

From the project directory:

```bash
cd ~/raidvol
```

Clean previous build artifacts:

```bash
make clean
```

Build the project:

```bash
make
```

Run the program:

```bash
./raidvol
```

---

# 25. CLI Commands

### Inspect a disk

```bash
./raidvol inspect disks/raid5_0.img
```

### Create RAID-0

```bash
./raidvol create --raid 0 \
    disks/raid0_0.img \
    disks/raid0_1.img \
    disks/raid0_2.img
```

### Create RAID-1

```bash
./raidvol create --raid 1 \
    disks/disk0.img \
    disks/disk1.img
```

### Create RAID-5

```bash
./raidvol create --raid 5 \
    disks/raid5_0.img \
    disks/raid5_1.img \
    disks/raid5_2.img
```

### Open an existing volume

```bash
./raidvol open disks/raid5_0.img disks/raid5_1.img disks/raid5_2.img
```

### RAID-5 parity test

```bash
./raidvol raid5-test
```

### RAID-0 mapping test

```bash
./raidvol raid0-map-test
```

### Checksum test

```bash
./raidvol checksum-test
```

### RAID-5 checksum test

```bash
./raidvol raid5-checksum-test
```

### Multi-block checksum test

```bash
./raidvol raid5-checksum-multi-test
```

### Benchmark

```bash
./raidvol benchmark
```

---

# 26. Test Coverage

RaidVol has been tested across the following areas:

```text
Build
  |
  +-- Clean compilation
  |
  +-- CLI execution
  |
  +-- RAID-0 mapping
  |
  +-- RAID-0 I/O
  |
  +-- RAID-1 mirroring
  |
  +-- RAID-1 failure recovery
  |
  +-- Metadata persistence
  |
  +-- RAID-5 parity
  |
  +-- RAID-5 mapping
  |
  +-- RAID-5 real I/O
  |
  +-- RAID-5 degraded reads
  |
  +-- RAID-5 rebuild
  |
  +-- CRC32 checksums
  |
  +-- Silent corruption detection
  |
  +-- Multi-block verification
  |
  +-- Performance benchmarking
```

---

# 27. Current Verification Status

The implemented functionality has been verified with PASS results for:

- RAID-0 end-to-end I/O
- RAID-1 normal read
- RAID-1 mirror consistency
- RAID-1 failure recovery
- Metadata write/read
- Metadata validation
- Metadata persistence
- RAID-5 parity calculation
- RAID-5 mapping
- RAID-5 real disk I/O
- RAID-5 physical parity
- RAID-5 degraded operation
- RAID-5 full rebuild
- CRC32 checksum verification
- Silent corruption detection
- Multi-block checksum verification
- Performance benchmarking
- Metadata-driven volume opening

---

# 28. Error Handling

RaidVol checks important storage operations including:

- Disk opening
- Disk reading
- Disk writing
- Metadata validation
- Device count validation
- RAID configuration validation
- Data verification
- Checksum verification

Failed operations return errors instead of silently continuing.

Disk descriptors are also closed after completed operations.

---

# 29. Design Principles

The project follows a modular architecture.

```text
Device Layer
     |
     v
RAID Layer
     |
     v
Volume Layer
     |
     v
Metadata / Checksum
     |
     v
CLI
```

Each major responsibility is separated into its own source/header module.

This makes the project easier to:

- Understand
- Test
- Debug
- Extend
- Demonstrate

---

# 30. Limitations

The current implementation is a software RAID prototype using disk image files.

Important limitations include:

- RAID configurations are currently focused on the implemented project layouts.
- Disk failures are simulated rather than detected from real hardware.
- The implementation does not provide a kernel-level `/dev` block device.
- Hot-spare support is not implemented.
- Online rebuild while serving concurrent I/O is not implemented.
- Advanced concurrency and synchronization are outside the current prototype scope.

---

# 31. Future Enhancements

Possible future improvements include:

1. Hot-spare support.
2. Online RAID-5 rebuild.
3. Real block-device integration.
4. More flexible device counts.
5. Background rebuild.
6. Concurrent I/O.
7. More advanced error reporting.
8. RAID-level performance visualization.
9. Additional integrity metadata.
10. Automated recovery monitoring.

---

# 32. Final Project Summary

RaidVol demonstrates how a software layer can provide a logical storage abstraction over multiple physical storage sources.

The project implements:

```text
Virtual Storage
      |
      +----------------+
      |                |
   RAID-0           RAID-1
      |                |
   Striping         Mirroring
      |
      +----------------+
               |
             RAID-5
               |
       XOR Parity + Recovery
               |
       +-------+-------+
       |               |
   Checksums       Metadata
       |               |
       +-------+-------+
               |
          Verification
```

The project combines **Operating Systems concepts, file I/O, block mapping, RAID algorithms, redundancy, fault recovery, metadata management, data integrity, and performance measurement** into a single software storage system.

---

# 33. One-Line Description

> **RaidVol is a C11-based software volume manager that implements RAID-0, RAID-1, and RAID-5 over disk image files with metadata persistence, checksum-based corruption detection, failure recovery, and performance benchmarking.**
