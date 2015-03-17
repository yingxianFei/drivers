#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/types.h> 
#include <linux/fs.h> 
#include <linux/proc_fs.h> 
#include <linux/device.h> 
#include <asm/uaccess.h> 
#include <mach/carconfig.h>
#include "lsconfig.h" 
//#include <string.h>


/*
主设备和从设备号变量
*/ 
static int lsconfig_major = 0; 
static int lsconfig_minor = 0; 
  
/*
设备类别和设备变量
*/  
static struct class* lsconfig_class = NULL;  
static struct lsconfig_android_dev* lsconfig_dev = NULL; 
static char buf_config[MAX_ITEM][MAX_LENTGH];
static  int active_item = 0;
static unsigned  int  need_item = MAGIC_ITEM;

/*
传统的设备文件操作方法
*/  
static int lsconfig_open(struct inode* inode, struct file* filp); 
static int lsconfig_release(struct inode* inode, struct file* filp);  
static ssize_t lsconfig_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos); 
static ssize_t lsconfig_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos); 
static int lsconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void config_item_read(const char * filename);

static CarConfig *gCarConfig;


/*以下为车型配置文件接口*/
char * obtain_item(int id)
{
	id += KCONFIGOFFSET;
	if((id < 0) || (id > active_item)){
		printk("[config]ERROR : wrong id!!!\n");
		return NULL;
	}

	return strchr(buf_config[id], SEPARATOR)+1;
}
EXPORT_SYMBOL_GPL(obtain_item);
CarConfig *get_gcarconfig()
{
	if(gCarConfig == NULL){
		printk("[config]ERROR: we do not init carconfig infomation something serious happened!!\n");
		return NULL;
	}
	return gCarConfig;
}
EXPORT_SYMBOL_GPL(get_gcarconfig);
static int init_gcarconfig()
{
	gCarConfig = kmalloc(sizeof(CarConfig), GFP_KERNEL);
	if(gCarConfig == NULL){
		printk("[config]ERROR: kmalloc error , we may out of memory!!\n");
		return -1;
	}
	memset(gCarConfig, 0, sizeof(CarConfig));
	config_item_read("/dev/block/mmcblk0p8");//read config file 
	return 0;
}
/*以上为车型配置文件接口*/
/*模块加载方法*/


void config_item_read(const char * filename)
{
    struct file *filp;
    struct inode *inode;
    mm_segment_t fs;
    off_t fsize, tTempFileLen;
    char *buf = NULL;
    int i = 0, j = 0, m = 0, k = 0, t = 0;
    
    unsigned long magic;
    printk(KERN_INFO "<1>start config station\n");
     
    filp=filp_open(filename,O_RDWR,0);

    if(IS_ERR(filp))
    {
          printk(KERN_INFO "<2>start error filp ***************************************************...\n");
          return;
    }
    
    tTempFileLen=filp->f_op->llseek(filp,0,SEEK_END);   
    filp->f_op->llseek(filp,0,SEEK_SET); 

    inode=filp->f_dentry->d_inode; 
    magic=inode->i_sb->s_magic;
    //printk(KERN_INFO "<1>file system magic:%li \n",magic);
    //printk(KERN_INFO "<1>super blocksize:%li \n",inode->i_sb->s_blocksize);
    //printk(KERN_INFO "<1>inode %li \n",inode->i_ino);
    
    fsize=inode->i_size;
 //   printk(KERN_INFO "<1>file size:%i \n",(int)fsize);

    fsize = (MAX_LENTGH * MAX_ITEM);
    buf=(char *) kmalloc(fsize+1,GFP_ATOMIC); 
    if(NULL == buf)
    {
        printk(KERN_INFO "<1> buf alloc error \n");
        return;
    }

    fs=get_fs();
    set_fs(KERNEL_DS);
    filp->f_op->read(filp,buf,fsize,&(filp->f_pos));
    set_fs(fs); 
   // printk("<1>buf = %s\n",buf); 

    for(j = 0; j < fsize; j++)
    {   
        //ZZ:;  90 90 58 59 end mark
        if((buf[j] == 90)  && (buf[j+1] == 90) &&   (buf[j+2]  == 58) && (buf[j+3] == 59))
        {
             buf_config[k][m] = buf[j];
             buf_config[k][m+1] = buf[j+1];
             buf_config[k][m+2] = buf[j+2];
             buf_config[k][m+3] = buf[j+3];
             break;
        }

        buf_config[k][m] = buf[j];
        ++m;

		//printk("buf_config[%d][%d] = buf[%d] ->%c\n", k, m, j, buf[j]);

        //  \n  \r
        if(buf[j] == 10 && buf[j-1] == 13)
        {
             m =0;
             k++;
        }
    }
    active_item = k - 1;

	gCarConfig->carname = obtain_item(CARNAME_ID);
	gCarConfig->tptype = obtain_item(TPTYPE_ID);
	gCarConfig->tpsize = obtain_item(TPSIZE_ID);
	gCarConfig->wheel_mode = obtain_item(WHEEL_ID);

	printk("gCarConfig->carname=%s gCarConfig->tptype=%s gCarConfig->tpsize=%s\n",
		gCarConfig->carname, gCarConfig->tptype, gCarConfig->tpsize);
    filp_close(filp,NULL);
    kfree(buf);
} 


void file_copy_back(const char * filename)
{
    struct file *filp;
    mm_segment_t fs;
    char *buf =NULL;
    int i = 0, j = 0, m = 0, k = 0, t = 0;
    
    unsigned long magic;
//    printk(KERN_INFO "<1>start.copy file back\n");
     
    filp=filp_open(filename,O_RDWR,0);

    if(IS_ERR(filp))
    {
         printk(KERN_INFO "<2>open error copy file back.\n");
         return;
    }
   
    //just for test
    //buf_config[0][4] = buf_config[0][4] + 1;
    //buf_config[1][4] = buf_config[1][4] + 1;
     
    for(i = 0; i < MAX_ITEM; i++)
    {
        k = strlen(buf_config[i]);

        if(k)
        {
            m += k;  
//           printk(KERN_INFO "<2> copy file back fsize = %d.\n", m);              
        }
        else
                {
                       break;
                }
    }

    buf=(char *) kmalloc(m,GFP_ATOMIC); 
    if(NULL == buf)
    {
        printk(KERN_INFO "<2>copy file kmalloc failed.\n");
        return;
    }

    //printk(KERN_INFO "--------------------------- copy file back  *** buf = %s\n",  buf);    
    memset(buf, 0, m);
    for(i = 0; i < MAX_ITEM; i++)
    {
             k = strlen(buf_config[i]);
             
             if(0 == k)
             {
                 break;
             }

             for(j = 0; j < k; j++)
             {
                 buf[t] = buf_config[i][j];
                 ++t;
             }           
     }

//     printk(KERN_INFO "<2> copy file back t = %d *** buf = %s\n",  t, buf);
      
     fs=get_fs();
     set_fs(KERNEL_DS);
     filp->f_op->write(filp,buf, t ,&(filp->f_pos));
     set_fs(fs); 
    
    filp_close(filp,NULL);
    kfree(buf);
}


/*
设备文件操作方法表
*/  
static struct file_operations lsconfig_fops = { 
 .owner = THIS_MODULE, 
 .open = lsconfig_open, 
 .release = lsconfig_release, 
 .read = lsconfig_read, 
 .write = lsconfig_write, 
 .unlocked_ioctl = lsconfig_ioctl,
}; 
  
/*
访问设置属性方法
*/  
//static ssize_t lsconfig_val_show(struct device* dev, struct device_attribute* attr, char* buf);  
//static ssize_t lsconfig_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count); 

/*
定义设备属性
*/  
//static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, lsconfig_val_show, lsconfig_val_store);  
 
/*
定义传统的设备文件访问方法，主要是定义
lsconfig_open
lsconfig_release
lsconfig_read 
lsconfig_write 
这四个打开、释放、读和写
设备文件的方法：
/*
打开设备方法
*/  
static int lsconfig_open(struct inode* inode, struct file* filp) { 
    
    struct lsconfig_android_dev* dev; 
  
    /*将自定义设备结构体保存在文件指针的私有数据域中，以便访问设备时拿来用*/ 
    //dev = container_of(inode->i_cdev, struct lsconfig_android_dev, dev); 
    //filp->private_data = dev; 

   // config_item_read("/dev/block/mmcblk0p8");
    
     //config_item_read("/dev/block/nandf");
    return 0; 
} 
  
/*
设备文件释放时调用，空实现
*/  
static int lsconfig_release(struct inode* inode, struct file* filp) { 

    file_copy_back("/dev/block/mmcblk0p8");
    //file_copy_back("/dev/block/nandf");

    return 0; 
} 
  
/*
读取设备的寄存器
val 
的值
*/  
static ssize_t lsconfig_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos) { 
    
    ssize_t err = 0;  
#if 0
    struct lsconfig_android_dev* dev = filp->private_data; 
  
    /*同步访问*/  
    if(down_interruptible(&(dev->sem))) { 
        return -ERESTARTSYS; 
    } 
  
    if(count < sizeof(dev->val)) { 
       goto out; 
    } 
  
/*将寄存器val 的值拷贝到用户提供的缓冲区*/ 

    if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) { 
        err = -EFAULT; 
        goto out; 
    } 
  
    err = sizeof(dev->val);  
 
    out:  
    up(&(dev->sem)); 
#endif
    return err; 
} 
static int stoi(char *str)//change string to int 
{   
	int i,sum = 0;
	
	printk("convert str = %s\n", str);
	
	for(i = 0; str[i] != '\0'; i++){
		sum += sum*10 + str[i]-'0' - sum;
	}
	return sum;
};
static int preprocess(char *str)
{
	printk("preprocess str = %s\n", str);
	if(str[0] != '-'){
		return stoi(str);
	}else{
		return -(stoi(&str[1]));
	}
}
int analysis(MYSIZE * val, char *buf, int size)
{
	char *tempval = buf;
	char *tempstr = NULL;
	printk("buf--->%s\n", buf);
	tempstr = strchr(buf, SEPARATOR);
	printk("tempstr--->%s\n", tempstr);
	(*tempstr) = '\0';
	tempstr++;
	val->line = preprocess(tempval) + KCONFIGOFFSET;//we add 32 items for coreinit config
	printk("val->line = %d\n", val->line);
	printk("tempstr = %s\n", tempstr);
	if(strlen(tempstr) > (MAX_LENTGH - 2)){
		printk("error: the string is too long max length = %d", MAX_LENTGH);
		return -1;
	}
	memset(val->msize, 0, MAX_LENTGH);
	memcpy(val->msize, tempstr, strlen(tempstr) + 1);
	val->msize[strlen(tempstr)] = '\r';
	val->msize[strlen(tempstr) + 1] = '\n';
	
	return 0;
	
}
/*写设备的寄存器值val*/  
static ssize_t lsconfig_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos) 
{ 
    
    MYSIZE val;
    int i = 0;
    int div = 0;
    ssize_t err = 0;
	char * kbuf = kmalloc(count, GFP_KERNEL);
	if(kbuf == NULL){
		err = -ENOMEM;
		goto outm;
	}
    if(copy_from_user(kbuf, buf, count)) { 
        err = -EFAULT; 
        goto out; 
    }
	
	printk("kbuf = %s count = %d\n", kbuf, count);
	*(kbuf + (count-1)) = '\0';//delete '\n'
	err = analysis(&val, kbuf, count);
	printk("val.line = %d val.msize = %s\n", val.line, val.msize);
	for(i = 0; i < strlen(buf_config[val.line]);  i++){     
	     // 58 = ':' 
		if(buf_config[val.line][i] == 58){
			div = i + 1;
			break;
		}
	}

	memset(&buf_config[val.line][div], 0, MAX_LENTGH -div);

	for(i = 0; i < strlen(val.msize);  i++){
		
		if((i + div)  >=  MAX_LENTGH){
		    break;
		}
	
		buf_config[val.line][i + div] = val.msize[i];
	}
	file_copy_back("/dev/block/mmcblk0p8");
out:
	kfree(kbuf);
outm:
    return count; 
}

static int lsconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg) 
{ 
    void __user *argp = (void __user *)arg;
    MYSIZE val;
    int i = 0;
    int mark_len = 2;
    int length = 0;
    int div = 0;
    int act_len = 0;
//    printk(KERN_INFO "config: CMD=%d \n",cmd); 

    switch(cmd) 
    { 
        
         case WRITE_DATA: 
                    
             if (copy_from_user(&val, (MYSIZE *)argp, sizeof(val)))
             {
                printk(KERN_INFO "config:lsconfig_ioctl --WRITE_DATA failed\n"); 
                return -EFAULT;
             }

//             printk(KERN_INFO "config:lsconfig_ioctl --val.msize =%s  **  val.line = %d **  size = %d  \n",val.msize, val.line,  strlen(val.msize)); 
             
             length = strlen(val.msize);
             if ((val.line > active_item ) || (length <= mark_len) || (length > MAX_LENTGH) || (val.msize[length-mark_len] != '\r' && val.msize[length-1] != '\n') )
             {
                 printk(KERN_INFO "config: WRITE_DATA cannot handle the invalid item number.\n"); 
                 return -EFAULT;
             }
             
             

             for(i = 0; i < strlen(buf_config[val.line]);  i++)
                          {     
                                 // 58 = ':' 
                                 
                                 if(buf_config[val.line][i] == 58)
                                 {
                                       div = i + 1;
                                       break;
                                 }
                          }

//             printk(KERN_INFO "config: before write buf config=%s :: div = %d\n",buf_config[val.line], div);
          
            // memset(buf_config[val.line][div], 0, MAX_LENTGH -div);*(buf_config + val.line*MAX_LENTGH + div)
             memset(&buf_config[val.line][div], 0, MAX_LENTGH -div);
             
             for(i = 0; i < strlen(val.msize);  i++)
             {
                 if((i + div)  >=  MAX_LENTGH)
                                  {
                                        break;
                                  }
                 buf_config[val.line][i + div] = val.msize[i];
             }
//             printk(KERN_INFO "config: after  write buf config=%s \n",buf_config[val.line]); 
            
             break; 

         case READ_DATA: 
        
             if(need_item > active_item)
             {
                  printk(KERN_INFO "config:  READ_DATA cannot handle the invalid item number.\n"); 
                  return -EFAULT;
             }

             for(i = 0; i < strlen(buf_config[need_item]);  i++)
             {     
                 // 58 = : 
                 if(buf_config[need_item][i] == 58)
                 {
                     div = i + 1;
                     break;
                 }
             }
             
             act_len = strlen(buf_config[need_item]) - div;

//             printk(KERN_INFO "config: READ_DATA div =%d,  act = %d \n",div, act_len); 

             for(i = 0; i < act_len;  i++)
             {
                 val.msize[i] = buf_config[need_item][i + div];
             }

             val.line = act_len; //strlen(val.msize);  ////need_item;
  
             if(copy_to_user((MYSIZE *)argp, &val, sizeof(val))) 
             { 
                printk(KERN_INFO "<1>READ_DATA Failed\n"); 
                return -EFAULT;
             } 
             
             need_item = MAGIC_ITEM;
             break; 
 
         case SET_NUM: 
          
             if (copy_from_user(&need_item,(int *)argp, sizeof(need_item)))
             {
                printk(KERN_INFO "config: --SET_NUM failed\n"); 
                return -EFAULT;
             }
//             printk(KERN_INFO "<1>GET_NUM --need_item=%d\n", need_item); 
             break; 

         case FLUSH_DATA:
          
             //if (copy_from_user(&need_item,(int *)argp, sizeof(need_item)))
             //{
             //   printk(KERN_INFO "config: --SET_NUM failed\n"); 
             //   return -EFAULT;
             //}
            
             file_copy_back("/dev/block/mmcblk0p8");
             //file_copy_back("/dev/block/nandf");
//             printk(KERN_INFO "<1>flush data into mmc blk config ok\n"); 
             break; 

         default:
             break; 
    } 
} 

/*初始化设备*/
static int __lsconfig_setup_dev(struct lsconfig_android_dev *dev){
    int err;
    dev_t devno = MKDEV(lsconfig_major, lsconfig_minor);
    
    memset(dev, 0, sizeof(struct lsconfig_android_dev));
    
    cdev_init(&(dev->dev), &lsconfig_fops);
    dev->dev.owner = THIS_MODULE;
    dev->dev.ops = &lsconfig_fops;
    
    /*注册字符设备*/
    err = cdev_add(&(dev->dev), devno, 1);
    if(err){
        return err;
    }
    
    /*初始化信号联合寄存器VAL的值*/
    //init_MUTEX(&(dev->sem));  old function delete
    sema_init(&(dev->sem), 1);
    //dev->val = 0;
    
    return 0;
}

static int __init lsconfig_init(void){
	
    int err = -1;
    dev_t dev = 0;
    struct device *temp = NULL;
	
	//    printk(KERN_ALERT"Initializing lsconfig device./n");
	init_gcarconfig();
	
    /*动态分配主设备和从设备号*/
    err = alloc_chrdev_region(&dev, 0, 1, TCLCONFIG_DEVICE_NODE_NAME);
    if(err < 0){
    	
        printk(KERN_ALERT"Failed to alloc char dev region./n");
        goto fail;
    }
    lsconfig_major = MAJOR(dev);
    lsconfig_minor = MINOR(dev);
    
    /*分配lsconfig设备结构体变量*/
    lsconfig_dev = kmalloc(sizeof(struct lsconfig_android_dev),GFP_KERNEL);
    if(!lsconfig_dev){
    	err = -ENOMEM;
    	printk(KERN_ALERT"Failed to alloc lsconfig_dev./n");
    	goto unregister;
    }
    
    /*初始化设备*/
    err = __lsconfig_setup_dev(lsconfig_dev);
    if(err){
        printk(KERN_ALERT"Failed to setup dev: %d./n", err);
    	  goto cleanup;
    }
   	
    /*在sys/class目录下创建设备类别目录lsconfig*/	
    lsconfig_class = class_create(THIS_MODULE, TCLCONFIG_DEVICE_CLASS_NAME);
    if(IS_ERR(lsconfig_class)){
        err = PTR_ERR(lsconfig_class);
        printk(KERN_ALERT"Failed to create lsconfig class./n");
        goto destroy_cdev;
    }
   
    /*dev目录 sys/class/lsconfig 目录下分别创建设备文件lsconfig*/
    temp = device_create(lsconfig_class, NULL, dev, "%s", TCLCONFIG_DEVICE_FILE_NAME);
    //temp = device_create(NULL, NULL, dev, "%s", TCLCONFIG_DEVICE_FILE_NAME);
    if(IS_ERR(temp)){
        err = PTR_ERR(temp);
        printk(KERN_ALERT"Failed to create lsconfig device.");
        goto destroy_class;
    }
#if 0
    /*在sys/class/lsconfig/lsconfig 目录下创建属性文件val*/
    err = device_create_file(temp, &dev_attr_val);
    if(err < 0){
    	
        printk(KERN_ALERT"Failed to create attribute val.");
        goto destroy_device;
    }
#endif
    dev_set_drvdata(temp, lsconfig_dev);
    
#if 0
    /*创建 proc/lsconfig 文件*/
    lsconfig_create_proc();
#endif
    return 0;
    
    destroy_device:
		device_destroy(lsconfig_class, dev);
     
    destroy_class:
		class_destroy(lsconfig_class);  
        
    destroy_cdev:
		cdev_del(&lsconfig_dev->dev);
    	  
    cleanup:
		kfree(lsconfig_dev);
    	  
     unregister:
		unregister_chrdev_region(MKDEV(lsconfig_major, lsconfig_minor), 1);
     	   
     fail:
		kfree(gCarConfig);
     	return err;
    
}

/*模块卸载方法*/
static void __exit lsconfig_exit(void){
    dev_t devno = MKDEV(lsconfig_major, lsconfig_minor);
    printk(KERN_ALERT"Destroy lsconfig device./n");
    
#if 0
    /*删除proc/lsconfig文件*/
    lsconfig_remove_proc();
    
    /*销毁设备类别和设备*/
    if(lsconfig_class){
    
        device_destroy(lsconfig_class, MKDEV(lsconfig_major, lsconfig_minor));
        class_destroy(lsconfig_class);
    }
#endif 
    /*删除字符设备和释放设备内存*/
    if(lsconfig_dev){
    	
    	 cdev_del(&(lsconfig_dev->dev));
    	 kfree(lsconfig_dev);
    }
    kfree(gCarConfig);
    /*释放设备号*/
    unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("First Android Driver");

late_initcall(lsconfig_init);
module_exit(lsconfig_exit);
