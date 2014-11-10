#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/ktime.h>
// Preprocessor defined constants
#define DRIVER_AUTHOR "Nash Kaminski"
#define DRIVER_DESC   "Digital Encoder Driver for Raspberry Pi"
#define MOD_VERSION "0.0.1"
#define SHORT_DESC "encoder"
#define INIT_INTERRUPT_PIN 20
// text below will be seen in 'cat /proc/interrupt' command
#define INTERRUPT_DESC "piEncoder interrupt"

struct timespec ts_start,ts_end,test_of_time;
short int piEncoder_open=0;
int piEncoder_tick_irq=0;
//static struct timer_list timer;
//Major device number
static int major;
//uDev data structures
static struct class *piEncoder_class;
static struct device *piEncoder_device;
typedef struct {
    long ticks;
    long prevTicks;
    int tickrate;
    } enc_out_t;
static struct timer_list timer;
static enc_out_t output;
//File operation functions
static int piEncoder_dev_open(struct inode *inode, struct file *file)
{
	piEncoder_open++;
	return 0;
}

static int piEncoder_dev_release(struct inode *inode, struct file *file)
{
	piEncoder_open--;
	return 0;
}

static ssize_t piEncoder_dev_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	size_t nbytes;
//calculate number of bytes to return
	nbytes = sizeof(enc_out_t);
	//printk( "piEncoder: Copying %d bytes of data to userland\n", (int)nbytes);
	if(copy_to_user(buf, &output, nbytes)!=0){
		printk(KERN_NOTICE "piEncoder: Unable to copy data to userland\n");
	}	
	return nbytes;  
}

//pointers to functions for file ops
static struct file_operations f = { .read = piEncoder_dev_read, .open = piEncoder_dev_open, .release = piEncoder_dev_release };

// initializes character device
int piEncoder_init_devnode(void)
{
	void *ptr_err;
	//Register actual character device
	if ((major = register_chrdev(0, SHORT_DESC, &f)) < 0)
		return major;
	//Create udev class
	piEncoder_class = class_create(THIS_MODULE, SHORT_DESC);
	if (IS_ERR(ptr_err = piEncoder_class))
		goto err2;
	//Create device file via udev
	piEncoder_device = device_create(piEncoder_class, NULL, MKDEV(major, 0), NULL, SHORT_DESC);
	if (IS_ERR(ptr_err = piEncoder_device))
		goto err;

	
	return 0;
// Handle errors by undoing whatever has been done
err:
	class_destroy(piEncoder_class);
err2:
	unregister_chrdev(major, SHORT_DESC);
	return PTR_ERR(ptr_err);
}

//Cleans up character device on exit/unload
void piEncoder_cleanup_devnode(void)
{
	device_destroy(piEncoder_class, MKDEV(major, 0));
	class_destroy(piEncoder_class);
	return unregister_chrdev(major, SHORT_DESC);
}

void timer_callback( unsigned long data )
{
    printk( "my_timer_callback called (%ld).\n", jiffies );
    mod_timer(timer, jiffies + msecs_to_jiffies(1000));
}
static irqreturn_t tick_ISR(int irq, void *dev_id, struct pt_regs *regs) {
	unsigned long flags;
	// disable hard interrupts (remember them in flag 'flags')
	local_irq_save(flags);
   	output.ticks++;
    // restore hard interrupts
	local_irq_restore(flags);
	return IRQ_HANDLED;
}
//Sets up a GPIO pin change interrupt 
short int attachInterrupt(int pin, const char *desc, const char *devdesc, int trigger, void *m_ISR){
	short int irq;
	//Request GPIO pin
	if (gpio_request(pin, desc)) {
		printk(KERN_NOTICE "piEncoder: GPIO request faiure: %s\n", desc);
		return -1;
	}
	//Set direction to input
	if (gpio_direction_input(pin)){
		printk(KERN_NOTICE "piEncoder: Failed to set GPIO direction");
		return -1;
	}
	//Get IRQ corresponding to that GPIO pin
	if ( (irq = gpio_to_irq(pin)) < 0 ) {
		printk(KERN_NOTICE "piEncoder: GPIO to IRQ mapping faiure %s\n", desc);
		return -1;
	}

	printk(KERN_NOTICE "piEncoder: Mapped GPIO# %d to HW IRQ, registering IRQ...%d\n", pin ,irq);
	// Actually register the IRQ
	// RISING, FALLING, HIGH, and LOW supported as trigger, precede with IRQF_TRIGGER_

	if (request_irq(irq,
				(irq_handler_t ) m_ISR,
				trigger,
				desc,
				(char *)devdesc)) {
		printk(KERN_NOTICE "piEncoder: IRQ Request failure\n");
		return -1;
	}

	return irq;
}

//Setup interrupts for input pins
int piEncoder_irq_config(void) {
	if((piEncoder_tick_irq = attachInterrupt(INIT_INTERRUPT_PIN, INTERRUPT_DESC, NULL, IRQF_TRIGGER_RISING, tick_ISR)) > 0)
        return 0;
    return -1;
}

//Detach interrupts and free GPIO pins on exit/unload
void piEncoder_irq_release(int pin, short int irq, const char *devdesc) {
	free_irq(irq, (void *)devdesc);
	gpio_free(pin);
	return;
}
int piEncoder_timer_init(void){
setup_timer(timer,timer_callback,0);
mod_timer(timer, jiffies + msecs_to_jiffies(1000));
return 0;
}
int piEncoder_timer_cleanup(void){
return 0;
}

//Called on insert/load, initializes everything via initializer function and checks for error in the process
int piEncoder_init(void) {
    output.ticks=0;
    output.prevTicks=0;
    output.tickrate=0;
	printk(KERN_NOTICE "piEncoder: Module version " MOD_VERSION ", initializing...\n");
    if(!piEncoder_init_devnode())
	goto init3;
    if(!piEncoder_irq_config())
        goto init2;
    if(!piEncoder_timer_init()){
        goto init1;
        return 0;
        }
    init1:
        piEncoder_timer_cleanup();
    init2: 
        if(piEncoder_tick_irq)
            piEncoder_irq_release(INIT_INTERRUPT_PIN, piEncoder_tick_irq, INTERRUPT_DESC);
    init3:
    return -1;

}
//Main cleanup function, cleans up and frees all resources.
//Kernel has already ensured that nobody is accessing our device.
void piEncoder_cleanup(void) {
	printk(KERN_NOTICE "piEncoder: Exiting...\n");
    piEncoder_cleanup_devnode();
    del_timer(&timer);
    if(piEncoder_tick_irq){
     piEncoder_irq_release(INIT_INTERRUPT_PIN, piEncoder_tick_irq, INTERRUPT_DESC);
     }
   }

// Kernel functions that must be supplied with a modules init and exit function pointers
module_init(piEncoder_init);
module_exit(piEncoder_cleanup);


/****************************************************************************/
/* Module licensing/description block.                                      */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
