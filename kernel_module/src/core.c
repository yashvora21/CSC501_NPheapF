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

extern struct miscdevice npheap_dev;

// this is the node structure of our linked list data structure
struct node_list {
	struct npheap_cmd cmd; // structure given in npheap.h
	struct mutex lock; // mutex lock per node
	//long offset;
	unsigned long phys_addr; // allocated physical address in kernel memory
	//unsigned long size;
	struct list_head list; // kernel linked list head reference // https://isis.poly.edu/kulesh/stuff/src/klist/
};

// initializing pointers
struct node_list ndlist;
//struct mutex lock;
struct node_list *tmp;
struct list_head *pos, *q;

// method to find if node exists in linked list and is deleted or not
int find(struct vm_area_struct *vma) {
	// traversing linked list https://isis.poly.edu/kulesh/stuff/src/klist/
	list_for_each_safe(pos, q, &ndlist.list) {
		tmp= list_entry(pos, struct node_list, list);
		// check if offset matches and size is not zero
		// size zero indicates deleted node from kmemory
		if (vma->vm_pgoff == tmp->cmd.offset && tmp->cmd.size > 0){
			printk(KERN_INFO "found %zu %zu \n",tmp->cmd.offset, vma->vm_pgoff);
			return 1; // found
		}
  	}
  	return 0; // not found
}

int npheap_mmap(struct file *filp, struct vm_area_struct *vma)
{
 // struct node_list pos;
  //pos = ndlist;
  int found = 0;
  // variables to store size and allocated memory address
  unsigned long phys_addr;
  unsigned long size = vma->vm_end - vma->vm_start;

  found = find(vma);
  // if not found create a new node, allocate kernel memory and add it to linked list
  if ( found == 0) {
  	// allocating kernel memory https://www.kernel.org/doc/htmldocs/kernel-api/API-kmalloc.html
	  void *kmemory = kmalloc(size, GFP_KERNEL);
	  printk(KERN_INFO "Node %zu got: %zu bytes of memory\n",vma->vm_pgoff, ksize(kmemory));
	  // find physical address using page_shift
	  phys_addr = (unsigned long)virt_to_phys((void *)kmemory) >> PAGE_SHIFT;
	  //	        printk(KERN_INFO "VMA offset: %zu", vma->vm_pgoff);

	  int ret;
	  // mapping allocated kernel memory to user virtual space address
	  // http://www.makelinux.net/ldd3/chp-15-sect-2
	  if((ret = remap_pfn_range(vma,
			  vma->vm_start,
			  phys_addr,
			  size,
			  vma->vm_page_prot)) < 0) {
		  printk(KERN_ERR "ret is %d\n", ret);
		  return ret;
	  }
	  //tmp = (struct node_list *)kmalloc(sizeof(struct node_list), GFP_KERNEL);
	  //mutex_init(tmp->lock);
	  //tmp->cmd.offset = vma->vm_pgoff;
	  // assigning memory physical address and allocated size to linked list desired node
	  	struct node_list *dummy;
		list_for_each_safe(pos, q, &ndlist.list) {
		    dummy = list_entry(pos, struct node_list, list);
		    // if found assign values
		    if(dummy->cmd.offset == vma->vm_pgoff) {
		    	dummy->cmd.data = kmemory;
				dummy->phys_addr = phys_addr;
				dummy->cmd.size = size;
				break;
		    }
		}
	  //list_add(&(tmp->list), &(ndlist.list));

  } else if (found == 1){
  	// if node found, reassign it the previously allocated memory address and size
	  int ret;
	  printk(KERN_INFO "Actual size vs new size  %zu %zu ", tmp->cmd.size, size);
	  if((ret = remap_pfn_range(vma,
			  vma->vm_start,
			  tmp->phys_addr,
			  tmp->cmd.size,
			  vma->vm_page_prot)) < 0) {
		  printk(KERN_ERR "ret is %d\n", ret);
		  return ret;
	  }

  }

	return 0;
}

int npheap_init(void)
{
	int ret;
	if ((ret = misc_register(&npheap_dev)))
		printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
	else{
		// initializing kernel linked list
		// https://isis.poly.edu/kulesh/stuff/src/klist/
		INIT_LIST_HEAD(&ndlist.list);
		//mutex_init(&lock);  // removed global lock
		printk(KERN_ERR "\"npheap\" misc device installed\n");
	}
	return ret;
}

void npheap_exit(void)
{
    misc_deregister(&npheap_dev);
}

