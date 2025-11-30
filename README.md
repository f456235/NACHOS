# 🖥️ NachOS Operating System Implementation

Repository Content Note: This repository highlights the implementation of the File System and Threading (Pthreads) subsystems, which represent the most complex architectural challenges in the NachOS kernel. While the documentation below covers the full scope of the operating system extensions I developed (including System Calls and Scheduling), the source code provided here focuses on the persistence and concurrency layers to demonstrate C++ systems programming proficiency.

![C++](https://img.shields.io/badge/Language-C++-blue)
![System](https://img.shields.io/badge/Domain-Operating%20System%20Kernel-red)
![Concurrency](https://img.shields.io/badge/Feature-Multi--Level%20Scheduling%20%7C%20File%20System-green)

> **⚠️ Academic Integrity Disclaimer:**
> Due to course policies regarding academic integrity at National Tsing Hua University (NTHU), the **source code for this project is not publicly available**. This repository serves as a documentation of my system architecture, design choices, and implementation details based on the NachOS 4.0 kernel.

## 📖 Project Overview
This project involves extending **NachOS** to support industrial-grade OS features. The implementation spans across **Kernel Multi-threading**, **Priority Scheduling**, **Virtual Memory**, and a **UNIX-style File System**, transforming a basic educational kernel into a fully functional operating system capable of running concurrent user programs.

---

## 🏗️ System Architecture & Key Features

### 1. Advanced File System (MP4)
Implemented a robust file system supporting dynamic growth and hierarchical directories.
* **Multi-level Indexed Allocation:**
    * Extended the file header to support **4-level indirect addressing**.
    * Capable of handling file sizes up to **100 MB** (originally limited to 4KB), utilizing a tree-based block allocation scheme.
* **Hierarchical Directory Management:**
    * Implemented **Recursive Operations**: `mkdir -p`, `cp -r`, `rm -r` for deep directory trees.
    * Path parsing logic to handle nested paths (e.g., `/home/user/docs/file.txt`).
* **Persistence:**
    * Implemented `SynchDisk` operations to ensure metadata (Bitmap, FileHeaders) consistency during write-backs.

### 2. Multi-level Feedback Queue Scheduler (MP3)
Replaced the default FIFO scheduler with a preemptive **Multi-level Feedback Queue (MLFQ)** to optimize throughput and response time.
* **Priority Levels:**
    * **L1 (High Priority):** Short-burst tasks (SJFs).
    * **L2 (Medium Priority):** Interactive tasks.
    * **L3 (Low Priority):** Background tasks (Round Robin).
* **Aging Mechanism:** Implemented dynamic priority boosting to prevent **Starvation**. Threads waiting in L3 for too long are promoted to L2/L1.
* **Preemption:** Kernel checks for higher-priority threads during `TimerInterrupt` ticks to enforce fairness.

### 3. Kernel Multi-threading & Context Switching
* **Thread Lifecycle Management:**
    * Implemented core thread operations: `Fork` (allocate stack), `Yield` (voluntarily relinquish CPU), `Sleep` (block on I/O), and `Finish` (cleanup).
    * Managed thread states (`READY`, `RUNNING`, `BLOCKED`) to coordinate transitions within the **Multi-level Feedback Queue (MLFQ)**.
* **Assembly-Level Context Switch:**
    * Engineered the `SWITCH` routine in assembly to manually save/restore CPU context.
    * Persisted critical registers (`EAX`, `ESP`, `EBP`, `PC`) and callee-saved registers to the kernel stack, ensuring seamless execution flow resumption .
* **Concurrency & Synchronization:**
    * Implemented **Semaphores** (P/V operations) to manage access to shared resources like the `Console` output.
    *  Utilized **Interrupt Masking** (`SetLevel(IntOff)`) to guarantee atomicity during critical kernel updates (e.g., modifying `ReadyList`), preventing race conditions.

### 4. System Calls & User-Kernel Boundary
Established a secure protection mechanism to isolate User Mode from Kernel Mode.
* **Trap Handling Mechanism:**
    * Implemented the **MIPS Exception Handler** to intercept user requests (`SyscallException`).
    * Designed the argument passing logic: reading parameters from registers `r4-r7` and writing return values to `r2` .
* **Implemented System Calls:**
    * **Process Control:** `Exec`, `Exit`, `Halt`.
    * **File I/O:** `Create`, `Open`, `Read`, `Write`, `Close` (integrated with the custom File System) .
* **Robust Exception Handling:**
    * Introduced `MemoryLimitException` to gracefully handle Out-Of-Memory (OOM) scenarios.
    * Implemented safety checks for `Page Faults` and invalid virtual address access, terminating offending user processes without crashing the entire kernel.

---

## 🔧 Engineering Challenges & Solutions

### Challenge 1: The "Broadcast Storm" of Recursion (File System)
* **Problem:** Implementing `rm -r` (recursive delete) was tricky because deleting a directory requires first emptying its contents. A naive implementation caused memory corruption when traversing deep directory trees.
* **Solution:** I architected a **Depth-First Search (DFS)** traversal mechanism. The kernel recursively enters directories, deletes files bottom-up, and updates the `FreeMap` and `DirectoryTable` only after the child nodes are successfully cleared.

### Challenge 2: Starvation in Priority Scheduling
* **Problem:** In early tests, low-priority (L3) threads were never executed when a high-priority loop was running.
* **Solution:** I introduced an **Aging Algorithm**. Every 1500 ticks, the scheduler scans the ready queue and increments the priority of waiting threads, ensuring eventually every thread gets CPU time.

---

## 💻 Tech Stack
* **Language:** C++ (Kernel Logic), MIPS Assembly (Context Switch)
* **Platform:** Linux (Cross-compiled for MIPS architecture)
* **Tools:** GDB (Debugging), Makefile

---

## 🏆 Results
* **File System:** Successfully stored and retrieved files > 64MB using 4-level indirection.
* **Scheduling:** Demonstrated fair CPU distribution across 10+ concurrent threads with varying priorities.
* **Stability:** Passed all stress tests for memory leaks and race conditions.
