#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <linux/device.h>
#include <linux/cdev.h> 
#include <linux/slab.h>

#include <mach/sys_config.h>


#define CHRDEV_NAME      "ls_sysconfig"
#define CLASS_NAME       "ls_sysconfig"
#define DEVICE_NAME      "ls_sysconfig"

static dev_t         devid;
static struct cdev*  ls_sysconfig_cdev;
static struct class* ls_sysconfig_class;
static DEFINE_MUTEX(sysconfig_lock);

#define DEV_DBG_EN   		0
#if(DEV_DBG_EN == 1)		
#define vfe_dev_dbg(x,arg...) printk(KERN_INFO"[LANDSEM_DEBUG][ls_sysconfig]"x,##arg)
#else
#define vfe_dev_dbg(x,arg...) 
#endif

#define vfe_dev_err(x,arg...) printk(KERN_ERR"[LANDSEM_ERR][ls_sysconfig]"x,##arg)
#define vfe_dev_info(x,arg...) printk(KERN_INFO"[LANDSEM_INFO][ls_sysconfig]"x,##arg)

typedef enum {
    LS_SYSCONFIG_CMD_MAIN_KEYS = 0,
	LS_SYSCONFIG_CMD_SUB_KEYS
}ls_sysconfig_cmd_t;

typedef enum {
    LS_SYSCONFIG_TYPE_GPIO = 0,
	LS_SYSCONFIG_TYPE_STR,
	LS_SYSCONFIG_TYPE_INT,
	LS_SYSCONFIG_TYPE_NONE
}ls_sysconfig_type_t;

#define SYS_CONFIG_VALUE_LENTH    250

typedef struct {
   ls_sysconfig_type_t type;
   char          value[SYS_CONFIG_VALUE_LENTH];
}ls_sysconfig_value_t;


#define MAX_KEY_NAME_LENTH    50
char   *main_key = NULL;
char   *sub_key = NULL;

static int ls_sysconfig_open(struct inode *inode,struct file *file)  {
    vfe_dev_dbg("open ls_sysconfig device!");

    return 0;
}

static int ls_sysconfig_release(struct inode *inode, struct file *file)  
{	
    vfe_dev_dbg("close ls_sysconfig device!");
	
    return 0;
}

static long ls_sysconfig_ioctl(struct file *file, unsigned int cmd, unsigned long value)  {
   // lock 
   mutex_lock(&sysconfig_lock);
   // set key.
   switch(cmd)  {
       case LS_SYSCONFIG_CMD_MAIN_KEYS:  {
		   copy_from_user(main_key,(void __user *)value,strlen((char*)value));
		   main_key[strlen((char*)value)] = '\0';
		   vfe_dev_dbg("set main key is %s\n",main_key);
           break;
	   }
	   case LS_SYSCONFIG_CMD_SUB_KEYS:   {
		   copy_from_user(sub_key,(void __user *)value,strlen((char*)value));
           sub_key[strlen((char*)value)] = '\0';
		   vfe_dev_dbg("set sub key is %s\n",sub_key);
           break;
	   }
   }
   //unlock
   mutex_unlock(&sysconfig_lock);
   return 0;
}

static ssize_t ls_sysconfig_read(struct file *file, char __user *buf, size_t count, loff_t *offset)  {
	int  ret = 0;
	script_item_u   item_val;
	script_item_value_type_e  item_type;
	ls_sysconfig_value_t result;

    vfe_dev_dbg("set key is (%s:%s)\n",main_key,sub_key);
	//lock.
	mutex_lock(&sysconfig_lock);
	//get config.
	memset(result.value,0x00,SYS_CONFIG_VALUE_LENTH);
	item_type = script_get_item(main_key, sub_key, &item_val);
    if(SCIRPT_ITEM_VALUE_TYPE_PIO == item_type)  {//get gpio params.
       result.type = LS_SYSCONFIG_TYPE_GPIO;
       sprintf(result.value,"gpio:%d,mul_sel:%d,pull:%d,drv_level:%d,data:%d",item_val.gpio.gpio,item_val.gpio.mul_sel,item_val.gpio.pull,item_val.gpio.drv_level,item_val.gpio.data);	   
	}
	else if(SCIRPT_ITEM_VALUE_TYPE_STR == item_type) {//get string params.
	    result.type = LS_SYSCONFIG_TYPE_STR;
		sprintf(result.value,"%s",item_val.str);
	}
	else if(SCIRPT_ITEM_VALUE_TYPE_INT == item_type)  {//get int params.
	    result.type = LS_SYSCONFIG_TYPE_INT;
	    sprintf(result.value,"%d",item_val.val);
	}
	else if(SCIRPT_ITEM_VALUE_TYPE_INVALID == item_type)  {//get invalid value params.
	    result.type = LS_SYSCONFIG_TYPE_NONE;
        sprintf(result.value,"%s","none");
	}
    //copy to user.
	ret = copy_to_user(buf,&result,sizeof(result));//
	if(ret)  {
		vfe_dev_err("copy result to user space error!\n"); 
		return -1;
	}
	//unlock.
	mutex_unlock(&sysconfig_lock);
	
	return sizeof(result);
}

static struct file_operations fops_ls_sysconfig = {
   .owner          = THIS_MODULE,
   .open           = ls_sysconfig_open,
   .release        = ls_sysconfig_release,
   .read           = ls_sysconfig_read,
   .unlocked_ioctl = ls_sysconfig_ioctl,
};

/*
* @function:ls_sysconfig_init
* This function is a init function of this modules,it such as module_init.
* @return unknown.
*/
static int __init ls_sysconfig_init(void)  {
	int ret;

	//get dev number.
	ret = alloc_chrdev_region(&devid,0,1,CHRDEV_NAME);
	if(ret)  {
		vfe_dev_err("alloc char driver error!\n");
		return ret;
	}
	//register char dev.
	ls_sysconfig_cdev = cdev_alloc();
	cdev_init(ls_sysconfig_cdev,&fops_ls_sysconfig);
	ls_sysconfig_cdev->owner = THIS_MODULE;
	ret = cdev_add(ls_sysconfig_cdev,devid,1);
	if(ret)  {
		vfe_dev_err("cdev create error!\n");
		unregister_chrdev_region(devid,1);
		return ret;
	}
	//create device.
	ls_sysconfig_class = class_create(THIS_MODULE,CLASS_NAME);
	if(IS_ERR(ls_sysconfig_class))  {
        vfe_dev_err("create class error\n");
		cdev_del(ls_sysconfig_cdev);
		unregister_chrdev_region(devid,1);
        return -1;
	}
	device_create(ls_sysconfig_class,NULL,devid,NULL,DEVICE_NAME);
    //malloc mem.
	main_key = (char*)kmalloc(MAX_KEY_NAME_LENTH,GFP_KERNEL);
	if(!main_key)  {
		vfe_dev_err("malloc main key point error!");		
		goto err_clean;
	}
	sub_key = (char*)kmalloc(MAX_KEY_NAME_LENTH,GFP_KERNEL);
	if(!sub_key)  {
		vfe_dev_err("malloc sub key point error!");
		if(main_key)  {
           kfree(main_key);
		}
        goto err_clean;
	}	
	vfe_dev_info("ls_sysconfig driver init is ok!\n");

    return 0;

	err_clean:
		device_destroy(ls_sysconfig_class,devid);
		class_destroy(ls_sysconfig_class);		
		cdev_del(ls_sysconfig_cdev);		
		unregister_chrdev_region(devid,1);

		return -ENOMEM;

}

/**
* @function:ls_sysconfig_exit
* This function is a clean function of this module.
*/
static void __exit ls_sysconfig_exit(void) {
    if(sub_key)  {
        kfree(sub_key);
	}
    if(main_key)  {
        kfree(main_key);
	}
	
	device_destroy(ls_sysconfig_class,devid);
	class_destroy(ls_sysconfig_class);	
	cdev_del(ls_sysconfig_cdev);	
	unregister_chrdev_region(devid,1);

    vfe_dev_info("ls_sysconfig driver exit is ok!\n");
}


late_initcall(ls_sysconfig_init);
module_exit(ls_sysconfig_exit);

MODULE_AUTHOR("arvinfei@landsem.com.cn");
MODULE_DESCRIPTION("landsem sysconfig driver.");
MODULE_LICENSE("GPL");

