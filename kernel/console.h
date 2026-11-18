#pragma once

// The console file, shared by every thread that has it open. Never freed.
struct file *console_file(void);
