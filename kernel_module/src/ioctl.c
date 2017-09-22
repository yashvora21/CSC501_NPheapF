//// Project 1: Mitkumar Pandya, mhpandya; Rachit Shrivastava, rshriva; Yash Vora, yvora;
//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "npheap.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/list.h>

// this is the node structure of our linked list data structure
struct node_list {
    struct npheap_cmd cmd; // structure given in npheap.h
    struct mutex lock; // mutex lock per node
    //long offset;
    unsigned long phys_addr; // allocated physical address in kernel memory
    //unsigned long size;
    struct list_head list; // kernel linked list head reference // https://isis.poly.edu/kulesh/stuff/src/klist/
};

// accessing external variables defined in core.c file using extern 
// https://en.wikipedia.org/wiki/External_variable 
extern struct node_list ndlist;

//extern struct mutex lock;

// check if node is already locked, return true: 1 , false:2, does not exist:0
long is_locked(__u64 offset) {
    struct node_list *tmp;
    struct list_head *pos, *q;
    list_for_each_safe(pos, q, &ndlist.list) {
        tmp = list_entry(pos, struct node_list, list);
        if((offset >> PAGE_SHIFT) == tmp->cmd.offset) {
            if(tmp->cmd.op == 0) {
                //found but locked
                return 1;
            } else if(tmp->cmd.op == 1) {
                //found but unlocked
                return 2;
            }
        }
    }
    return 0;
}

struct node_list* find_object(struct npheap_cmd cmd) {
    //lock the existing node
    struct node_list *tmp;
    struct list_head *pos, *q;
    // traverse through list and lock the current unocked node
    // https://isis.poly.edu/kulesh/stuff/src/klist/
    list_for_each_safe(pos, q, &ndlist.list) {
        tmp = list_entry(pos, struct node_list, list);
        if((cmd.offset >> PAGE_SHIFT) == tmp->cmd.offset) {
            return tmp;
        }
    }
    return NULL;
}

long npheap_lock(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd cmd;
    // copying user space struct to kernel 
    //https://www.fsl.cs.sunysb.edu/kernel-api/re257.html
    if(copy_from_user(&cmd, user_cmd, sizeof(*user_cmd))) {
        return -1;
    }
    // check if node is locked
    long isLock = is_locked(cmd.offset);
    if(isLock == 0) {
        // node does not exist, create one and lock it
        //creade new node
        struct node_list *tmp;
        tmp = (struct node_list *)kmalloc(sizeof(struct node_list), GFP_KERNEL);
        tmp->cmd.offset = cmd.offset >> PAGE_SHIFT;
        tmp->cmd.op = 0;
        tmp->cmd.size = 0;
        printk("initialized and locked node %zu\n",tmp->cmd.offset);
        mutex_init(&(tmp->lock)); // initializing current node mutex
        mutex_lock(&(tmp->lock)); // lock current node mutex
        list_add(&(tmp->list), &(ndlist.list)); // add current node to linked list
        //https://isis.poly.edu/kulesh/stuff/src/klist/
        return 1;   //pass
    } else if(isLock == 2) {
        //lock the existing node
        struct node_list *tmp;
        struct list_head *pos, *q;
        // traverse through list and lock the current unocked node
        // https://isis.poly.edu/kulesh/stuff/src/klist/
        /*list_for_each_safe(pos, q, &ndlist.list) {
            tmp = list_entry(pos, struct node_list, list);
            if((cmd.offset >> PAGE_SHIFT) == tmp->cmd.offset) {
                printk("node exists and unlocked, now locked %zu\n",tmp->cmd.offset);
                tmp->cmd.op = 0;
                mutex_lock(&(tmp->lock));
                return 1;   //pass
            }
        }*/
        tmp = find_object(cmd);
        if(tmp != NULL) {
            printk("node exists and unlocked, now locked %zu\n",tmp->cmd.offset);
            tmp->cmd.op = 0;
            mutex_lock(&(tmp->lock));
            return 1;
        }
    }
//    mutex_lock(&lock);
    return 0;   //fail
}     

long npheap_unlock(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd cmd;
    if(copy_from_user(&cmd, user_cmd, sizeof(*user_cmd))) {
        return -1;
    }
    struct node_list *tmp;
    struct list_head *pos, *q;
    // iterte through list and unlock the requested node
    /*list_for_each_safe(pos, q, &ndlist.list) {
        tmp = list_entry(pos, struct node_list, list);
        //check if offset is same and it was locked before
        if((cmd.offset >> PAGE_SHIFT) == tmp->cmd.offset && tmp->cmd.op == 0) {
            tmp->cmd.op = 1;
            printk("node %zu , unlocked\n", tmp->cmd.offset);
            mutex_unlock(&(tmp->lock)); // unlocking current node
            return 1;   //pass
        }
    }*/
    tmp = find_object(cmd);
    if(tmp != NULL && tmp->cmd.op == 0) {
        tmp->cmd.op = 1;
        printk("node %zu , unlocked\n", tmp->cmd.offset);
        mutex_unlock(&(tmp->lock)); // unlocking current node
        return 1;
    }
//    mutex_unlock(&lock);
    return 0;   //fail
}

long npheap_getsize(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd cmd;
    if(copy_from_user(&cmd, user_cmd, sizeof(*user_cmd))) {
        return -1;
    }
	struct node_list *tmp;
	struct list_head *pos, *q;
    // iterating through list to check if node associated with requested offset exists, if yes return it's size
	/*list_for_each_safe(pos, q, &ndlist.list) {
        tmp = list_entry(pos, struct node_list, list);
        if ((cmd.offset >> PAGE_SHIFT) == tmp->cmd.offset){
            return tmp->cmd.size; // return current node size
        }
	}*/
    tmp = find_object(cmd);
    if(tmp != NULL) {
        return tmp->cmd.size;
    }
	return 0; // node not found, size is 0
}

long npheap_delete(struct npheap_cmd __user *user_cmd)
{
    struct npheap_cmd cmd;
    if(copy_from_user(&cmd, user_cmd, sizeof(*user_cmd))) {
        return -1;
    }
    struct node_list *tmp;
    struct list_head *pos, *q;
    // iterate through linked list and kfree data of associated offset node
    /*list_for_each_safe(pos, q, &ndlist.list) {
        tmp = list_entry(pos, struct node_list, list);
        //check if offset is same and its unlocked
        // if found and is unlocked
        if((cmd.offset >> PAGE_SHIFT) == tmp->cmd.offset && tmp->cmd.op == 0) {
            //Delete Code
	        printk(KERN_INFO "deleting node %zu\n",tmp->cmd.offset);
	        //list_del(pos);
            // make size 0
	        tmp->cmd.size = 0;
            // free associated memory from kernel memory space
            // https://www.kernel.org/doc/htmldocs/kernel-api/API-kfree.html
            kfree(tmp->cmd.data); 
            tmp->cmd.data = NULL;
            return 1;
        }
    }*/
    tmp = find_object(cmd);
    if(tmp != NULL && tmp->cmd.op == 0) {
        //Delete Code
        printk(KERN_INFO "deleting node %zu\n",tmp->cmd.offset);
        tmp->cmd.size = 0;
        kfree(tmp->cmd.data); 
        tmp->cmd.data = NULL;
        return 1;
    }
    return 0;
}

long npheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case NPHEAP_IOCTL_LOCK:
        return npheap_lock((void __user *) arg);
    case NPHEAP_IOCTL_UNLOCK:
        return npheap_unlock((void __user *) arg);
    case NPHEAP_IOCTL_GETSIZE:
        return npheap_getsize((void __user *) arg);
    case NPHEAP_IOCTL_DELETE:
        return npheap_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}
