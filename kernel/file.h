#pragma once

struct file {
    long (*read)(struct file *f, unsigned long buf, unsigned long len);
    long (*write)(struct file *f, unsigned long buf, unsigned long len);
};
