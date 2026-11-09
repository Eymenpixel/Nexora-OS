# BlackBit security model

## Getallow

`getallow` is the kernel's capability policy gate. Kernel code must call `getallow_check` at the point where it performs a protected operation; hiding a command in the shell is not an authorization check. The policy starts with no implicit privileged grants. The built-in kernel console may inspect system and memory status and start only compiled test images. The demo's kernel-assigned ring-3 identity may request `system.info`; `memory.inspect` is denied by default and can be granted temporarily by an authenticated admin. `system.halt`, filesystem writes, process execution, and unknown capabilities are denied to the demo.

Every policy decision records the kernel-assigned caller, capability, decision, and timer tick in a bounded 16-record RAM audit log. The log is useful for the current session but is not persistent or tamper-proof. It is lost on reboot and should not be treated as forensic evidence.

## Persistent administrator credential

The first boot setup accepts one 12–63 character password and confirms it. The kernel stores a random 16-byte RDRAND salt and a PBKDF2-HMAC-SHA-256 verifier using 600,000 iterations, matching the current [OWASP password storage guidance](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html). PBKDF2 is CPU-hard rather than memory-hard, so a stolen disk still permits offline password guessing; use a strong, unique passphrase. The current implementation requires RDRAND and refuses enrollment when the CPU does not provide it.

Successful login creates a five-minute kernel session. Five failed attempts cause a 30-second delay, but both the delay and failure counter are in RAM and reset at reboot. Grants are also temporary and are cleared at logout/session expiry. There is one administrator record, no password reset or account recovery, and a corrupt store is locked rather than silently overwritten. Back up `blackbit-auth.img` privately; never commit it to GitHub.

The two-sector checksum detects accidental damage, not malicious edits. Anyone who can replace the disk can replace the password verifier and checksum, or erase the disk to trigger first-time setup. BlackBit has no secure boot, TPM-bound key, disk encryption, or physical-tamper protection. The current password login therefore controls this prototype's running console and policy flow; it does not defend the machine against an attacker with physical boot or disk access.

## Current trust boundary

The built-in command shell and kernel run at x86 ring 0. A bug in any kernel code can bypass `getallow`. Fixed built-in test images run at ring 3 with separate mapped code and stack pages and can enter the kernel only through the `int 0x80` syscall gate. The `userfault` image tries to read a supervisor-only page; the page fault is contained by terminating the task and returning to the shell. This is not yet a general process model: there is no executable loader, scheduler, or multi-user account database. It deliberately does not ship a default password.

Before running third-party programs, BlackBit needs user mode and validated syscalls; each process must receive an identity assigned by the kernel, and each sensitive kernel operation must authorize that identity through `getallow`. Credential enrollment and secure storage must be designed before interactive grants are enabled.
