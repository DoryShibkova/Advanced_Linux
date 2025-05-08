#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define DEVICE_NAME "int_stack"
#define IOCTL_SET_SIZE _IOW('s', 1, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daria Shibkova");

// Stack data structure with mutex protection
struct stack {
    int *data;
    int top;
    int size;
    struct mutex lock;
};

static struct stack *stack;
static int major_number;
static struct class *stack_class;
static struct device *stack_device;

// Initialize device on open
static int stack_open(struct inode *inode, struct file *file) {
    return 0;
}

// Cleanup on device close
static int stack_release(struct inode *inode, struct file *file) {
    return 0;
}

// Pop operation - returns value from top of stack
static ssize_t stack_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    int value;
    
    if (count != sizeof(int))
        return -EINVAL;
        
    mutex_lock(&stack->lock);
    
    if (stack->top == 0) {
        mutex_unlock(&stack->lock);
        return 0; // Return NULL for empty stack
    }
    
    value = stack->data[--stack->top];
    mutex_unlock(&stack->lock);
    
    if (copy_to_user(buf, &value, sizeof(int)))
        return -EFAULT;
        
    return sizeof(int);
}

// Push operation - adds value to stack
static ssize_t stack_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    int value;
    
    if (count != sizeof(int))
        return -EINVAL;
        
    if (copy_from_user(&value, buf, sizeof(int)))
        return -EFAULT;
        
    mutex_lock(&stack->lock);
    
    if (stack->top >= stack->size) {
        mutex_unlock(&stack->lock);
        return -ERANGE;
    }
    
    stack->data[stack->top++] = value;
    mutex_unlock(&stack->lock);
    
    return sizeof(int);
}

// Configure stack size via ioctl
static long stack_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int new_size;
    int *new_data;
    
    if (cmd != IOCTL_SET_SIZE)
        return -ENOTTY;
        
    if (copy_from_user(&new_size, (int __user *)arg, sizeof(int)))
        return -EFAULT;
        
    if (new_size <= 0)
        return -EINVAL;
        
    mutex_lock(&stack->lock);
    
    // Allocate new memory for the stack
    new_data = kmalloc(new_size * sizeof(int), GFP_KERNEL);
    if (!new_data) {
        mutex_unlock(&stack->lock);
        return -ENOMEM;
    }
    
    // Copy existing elements to the new stack
    if (stack->data) {
        int elements_to_copy = (stack->top < new_size) ? stack->top : new_size;
        memcpy(new_data, stack->data, elements_to_copy * sizeof(int));
        kfree(stack->data);
    }
    
    stack->data = new_data;
    stack->size = new_size;
    if (stack->top > new_size) {
        stack->top = new_size; // Adjust top if new size is smaller
    }
    mutex_unlock(&stack->lock);
    
    return 0;
}

// File operations structure
static const struct file_operations stack_fops = {
    .owner = THIS_MODULE,
    .open = stack_open,
    .release = stack_release,
    .read = stack_read,
    .write = stack_write,
    .unlocked_ioctl = stack_ioctl,
};

// Module initialization
static int __init stack_init(void) {
    stack = kmalloc(sizeof(struct stack), GFP_KERNEL);
    if (!stack)
        return -ENOMEM;
        
    mutex_init(&stack->lock);
    stack->data = NULL;
    stack->size = 0;
    stack->top = 0;
    
    major_number = register_chrdev(0, DEVICE_NAME, &stack_fops);
    if (major_number < 0) {
        kfree(stack);
        return major_number;
    }
    
    stack_class = class_create(DEVICE_NAME);
    if (IS_ERR(stack_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(stack);
        return PTR_ERR(stack_class);
    }
    
    stack_device = device_create(stack_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(stack_device)) {
        class_destroy(stack_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(stack);
        return PTR_ERR(stack_device);
    }
    
    printk(KERN_INFO "Stack module loaded\n");
    return 0;
}

// Module cleanup
static void __exit stack_exit(void) {
    device_destroy(stack_class, MKDEV(major_number, 0));
    class_destroy(stack_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    
    if (stack) {
        if (stack->data)
            kfree(stack->data);
        kfree(stack);
    }
    
    printk(KERN_INFO "Stack module unloaded\n");
}

module_init(stack_init);
module_exit(stack_exit);
