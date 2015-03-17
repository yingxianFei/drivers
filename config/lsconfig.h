#ifndef _LSLCONFIG_ANDROID_H_ 
#define _LSCONFIG_ANDROID_H_ 

#include <linux/cdev.h> 
#include <linux/semaphore.h>  

#define TCLCONFIG_DEVICE_NODE_NAME  "lsconfig" 
#define TCLCONFIG_DEVICE_FILE_NAME  "lsconfig" 
#define TCLCONFIG_DEVICE_PROC_NAME  "lsconfig" 
#define TCLCONFIG_DEVICE_CLASS_NAME "lsconfig" 

struct lsconfig_android_dev { 
 //int val;  
 struct semaphore sem; 
 struct cdev dev; 
}; 

#define MAX_ITEM 500
#define MAX_LENTGH 40
#define MAGIC_ITEM (MAX_ITEM*MAX_LENTGH)

typedef struct{
   int line;
   char msize[MAX_LENTGH];
}MYSIZE;

#define CONFIG_MAGIC  'C'
#define WRITE_DATA  _IOW(CONFIG_MAGIC, 0, MYSIZE)
#define READ_DATA  _IOR(CONFIG_MAGIC, 1, MYSIZE)
#define SET_NUM _IOW(CONFIG_MAGIC, 2, int)
#define FLUSH_DATA _IO(CONFIG_MAGIC, 3)
#endif  
