# Linux Kernel Virtual Network Interface Card (NIC) Driver & io_uring Transmitter
**PCIe Subsystem Integration, DMA Mapping, and Asynchronous Packet Transmission**

This repository contains a Linux Kernel module (developed for the 6.x series) that implements a complete driver for a simulated PCI-based Virtual Network Interface Card (zsonet). It is accompanied by a high-performance, asynchronous user-space transmitter utilizing the modern `io_uring` API.

## Architectural Highlights
The project operates at the boundary between hardware and the Linux networking stack, handling physical memory constraints, interrupt dispatching, and high-throughput data transfer.

* **Kernel Networking Stack Integration (`net_device`):** Full implementation of the `net_device_ops` structure, seamlessly hooking the custom NIC into the Linux networking core (ifconfig/iproute2 compatibility, packet queuing).
* **PCIe & DMA Memory Management:** Integration with the Linux PCI subsystem (`pci_register_driver`). Configured hardware I/O remapping (`pci_iomap`) and implemented direct, lock-free Direct Memory Access (DMA) allocations for zero-copy packet transfer between the device hardware and RAM.
* **Interrupt Request (IRQ) Handling:** Low-level Top-Half interrupt routing (`request_irq`) mapped to device-specific hardware events (TX completion, RX availability, errors).
* **Asynchronous User-Space I/O (`io_uring`):** Engineered a specialized user-space transmitter that bypasses standard synchronous syscall bottlenecks (like `write()` or `send()`) by batching kernel submissions via `io_uring`, maximizing network throughput.

## Post-Mortem & Technical Debt
This module was an initial foray into kernel-space device drivers. A strict technical review revealed several critical flaws in concurrency and hardware synchronization, serving as a vital baseline for my current understanding of kernel safety protocols:

* **ISR Spinlock Deadlock Hazard:** The driver incorrectly utilizes standard `spin_lock()` primitives in contexts shared with the Interrupt Service Routine (ISR). In a production multiprocessor (SMP) environment, this creates a fatal deadlock vulnerability if an interrupt fires on the same core currently holding the lock. Modern iterations mandate `spin_lock_irqsave()` to mask local interrupts during critical sections.
* **Race Conditions in IRQ Clearing:** Hardware interrupts are cleared before their underlying causes are fully processed in the ring buffers. Under high network loads, this race condition can corrupt the RX state machine, leading to dropped frames and desynchronized buffer rings.
* **Network Endianness Violations:** Frame sizes parsed during RX are not explicitly converted from network byte order (Big-Endian) to host byte order using `ntohs()`. While benign on simulated architectures with matching endianness, this would critically corrupt packet framing on cross-architecture physical deployments.
* **Premature TX Statistic Accounting:** Network transmission statistics (`tx_packets`) are incremented prior to the hardware asserting the `TX_OK` acknowledgment, violating strict network accounting paradigms.
* **Incomplete io_uring Utilization:** The user-space transmitter fails to map 100% of its I/O operations through the `io_uring` submission queue, falling back to synchronous paths for certain setups and bottlenecking the intended asynchronous throughput.

## Tech Stack
* **Domain:** Linux Kernel Space, Network Subsystem, User-space Asynchronous I/O.
* **Language:** C (Kernel standard & POSIX).
* **Key Mechanics:** `struct net_device`, PCI Core, DMA mapping, Hardware Interrupts (IRQs), Spinlocks, `io_uring`.
