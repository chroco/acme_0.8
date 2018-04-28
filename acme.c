/*
	Chad Coates
	ECE 373
	Homework #8
	June 4, 2017

	This is the ACME: ece_led driver
*/

#include "acme.h"

static dev_t acme_dev_number;
static struct class *acme_class;
static int blink_rate=BLINKRATE;

module_param(blink_rate,int,S_IRUGO|S_IWUSR);

static void service_task(struct work_struct *work){
	int tail,head,count=0;
	struct acme_rx_desc *desc;
	msleep(500);
	writel(LED_OFF,acme_devp->hw.led_addr);
	
	head=readl(acme_devp->hw.hw_addr+RDH);
	tail=readl(acme_devp->hw.hw_addr+RDT);

	count=(head<tail)?acme_devp->rx_ring.count-tail+head:head-tail;
	
	do{
		desc=ACME_RX_DESC(acme_devp->rx_ring,tail);
		printk("desc[%i] (stat,length) ",tail);
		printk("before: (0x%04X,0x%04X) (@Y@) ",desc->status,desc->length);
		desc->status=desc->status & 0xFFFE;
		printk("after: (0x%04X,0x%04X)\n",desc->status,desc->length);
		++tail;
		if(tail==acme_devp->rx_ring.count)tail=0;
	}while(--count>0);

	writel(tail,acme_devp->hw.hw_addr+RDT);
}

static irqreturn_t acme_irq_handler(int irq, void *data){
	u32 cause;
	cause=readl(acme_devp->hw.hw_addr+ICR);
	switch(cause){
	case LSC_INT:
	case RXQ_INT:
		writel(GREEN,acme_devp->hw.led_addr);
		schedule_work(&acme_devp->task);
	}
	writel(LSC|RXQ,acme_devp->hw.hw_addr+IMS);
	return IRQ_HANDLED;
}

static int acme_open(struct inode *inode,struct file *filp){
	int err=0;
	writel(LED_OFF,acme_devp->hw.led_addr);
	return err;
}

static int acme_close(struct inode *inode,struct file *filp){
	int err=0;
	writel(LED_OFF,acme_devp->hw.led_addr);
	return err;
}

static ssize_t 
acme_read(struct file *filp,char __user *buf,size_t len,loff_t *offset){
	int ret;
	struct ring_info info;
	size_t size=sizeof(struct ring_info);
	
	if(*offset >= size)return 0;
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	info.rh=readl(acme_devp->hw.hw_addr+RDBAH);
	info.rl=readl(acme_devp->hw.hw_addr+RDBAL);
	info.len=readl(acme_devp->hw.hw_addr+RDLEN);
	info.head=readl(acme_devp->hw.hw_addr+RDH);
	info.tail=readl(acme_devp->hw.hw_addr+RDT);
	info.icr=readl(acme_devp->hw.hw_addr+ICR);
	info.led=readl(acme_devp->hw.hw_addr+LEDCTL);
	
	if(copy_to_user(buf,&info,size)){
		ret = -EFAULT;
		goto out;
	}
	ret = size;
out:
	return ret;
}

static ssize_t acme_write(struct file *filp,
													const char __user *buf,
													size_t len,loff_t *offset){
	int ret=0, _blink_rate;
	char temp[30];
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	if(copy_from_user(temp,buf,len)){
		ret = -EFAULT;
		goto out;
	}
	kstrtoint(temp,10,&_blink_rate);
	
	if(_blink_rate <= 0){
		ret = -EINVAL;
		printk("Ignorring invalid blink_rate!\n");
		goto out;
	}
out:
	return ret;
}

static const struct file_operations acme_fops = {
	.owner 		= THIS_MODULE,
	.open 		= acme_open,
	.release 	= acme_close,
	.read 		= acme_read,		
	.write 		= acme_write,
};

static int rx_ring_init(struct pci_dev *pdev){
	struct acme_ring *rx_ring = &acme_devp->rx_ring;
	int i,err=-ENOMEM;
	
	rx_ring->count=ACME_RXD;
	rx_ring->size=rx_ring->count*sizeof(struct acme_rx_desc);

	rx_ring->desc=(struct acme_rx_desc *)dma_alloc_coherent(&pdev->dev,rx_ring->size,&rx_ring->dma,GFP_KERNEL);
	if(!rx_ring->desc)goto err;
	
	for(i=0;i<rx_ring->count;++i){
		rx_ring->wtf[i]=kzalloc(DESC_SIZE,GFP_KERNEL);
		if(!rx_ring->wtf[i])goto err_pages;
		rx_ring->desc[i].buff_addr=cpu_to_le64(dma_map_single(&pdev->dev,rx_ring->wtf[i],DESC_SIZE,PCI_DMA_FROMDEVICE));
	}
	rx_ring->next_to_use=0;
	rx_ring->next_to_clean=0;
	return 0;
err_pages:
	for(i=0;i<rx_ring->count;++i){
		dma_unmap_single(&pdev->dev,rx_ring->desc[i].buff_addr,DESC_SIZE,PCI_DMA_FROMDEVICE);
		kfree(rx_ring->wtf[i]);
	}
err:
	dma_free_coherent(&pdev->dev,rx_ring->size,rx_ring->desc,rx_ring->dma);
	return err;
}

static void free_rx_ring(struct pci_dev *pdev){
	int i;
	struct acme_ring *rx_ring=&acme_devp->rx_ring;
	for(i=0;i<rx_ring->count;++i){
		dma_unmap_single(&pdev->dev,rx_ring->desc[i].buff_addr,DESC_SIZE,PCI_DMA_FROMDEVICE);
		kfree(rx_ring->wtf[i]);
	}
	dma_free_coherent(&pdev->dev,rx_ring->size,rx_ring->desc,rx_ring->dma);
}

static int 
amce_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent){
	int bars, err;
	struct acme_ring *rx_ring=&acme_devp->rx_ring;
	resource_size_t mmio_start, mmio_len;

	printk(KERN_INFO "It's dangerous to go alone, take this with you.\n");

	err=pci_enable_device_mem(pdev);
	if(err)return err;

	bars=pci_select_bars(pdev, IORESOURCE_MEM);
	
	err=pci_request_selected_regions(pdev,bars,DEVNAME);
	if(err)goto err_pci_reg;
	
	pci_set_master(pdev);
	
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);
	acme_devp->hw.hw_addr = ioremap(mmio_start, mmio_len);
	acme_devp->hw.led_addr=acme_devp->hw.hw_addr+LEDCTL; 
	
	rx_ring_init(pdev);

	INIT_WORK(&acme_devp->task,service_task);

	writel(0xFFFFFFFF,acme_devp->hw.hw_addr+IMC);
	writel((readl(acme_devp->hw.hw_addr+CTRL) | CTRL_RST),acme_devp->hw.hw_addr+CTRL);
	msleep(25);
	writel(0xFFFFFFFF,acme_devp->hw.hw_addr+IMC);

	writel((readl(acme_devp->hw.hw_addr+GCR) | (1<<22)),acme_devp->hw.hw_addr+GCR);
	writel((readl(acme_devp->hw.hw_addr+GCR2) | 1),acme_devp->hw.hw_addr+GCR2);

	writel(0x1831af08,acme_devp->hw.hw_addr+MDIC);
	
	writel(0,acme_devp->hw.hw_addr+RDH);
	writel((readl(acme_devp->hw.hw_addr+RCTL)|EN|UPE|MPE|BAM),acme_devp->hw.hw_addr+RCTL);
		
	writel((rx_ring->dma>>32) & 0xFFFFFFFF,acme_devp->hw.hw_addr+RDBAH);
	writel(rx_ring->dma & 0xFFFFFFFF,acme_devp->hw.hw_addr+RDBAL);
	writel(sizeof(struct acme_rx_desc)*ACME_RXD,acme_devp->hw.hw_addr+RDLEN);
	writel(ACME_RXD-1,acme_devp->hw.hw_addr+RDT);
	writel(LED_OFF,acme_devp->hw.hw_addr+LEDCTL);
	
	writel(LSC|RXQ,acme_devp->hw.hw_addr+IMS);
	
	pci_enable_msi(pdev);
	err=request_irq(pdev->irq,acme_irq_handler,0,"acme_int",NULL);
	if(err){
		err=-EBUSY;
		goto err_irq;
	}
	return 0;
err_irq:
	free_rx_ring(pdev);
	return err;
err_pci_reg:
	pci_disable_device(pdev);
	return err;
}

static void amce_pci_remove(struct pci_dev *pdev){
	free_irq(pdev->irq,NULL);
	pci_disable_msi(pdev);
	cancel_work_sync(&acme_devp->task);
	free_rx_ring(pdev);
	pci_release_selected_regions(pdev,pci_select_bars(pdev,IORESOURCE_MEM));
	pci_disable_device(pdev);
	printk(KERN_INFO "So long!!\n");
}

static DEFINE_PCI_DEVICE_TABLE(amce_pci_tbl) = {
	{ PCI_DEVICE(0x8086, 0x150c) },
	{ }, 
};

static struct pci_driver acme_pci_driver = {
	.name = DEVNAME,
	.id_table = amce_pci_tbl,
	.probe = amce_pci_probe,
	.remove = amce_pci_remove,
};

static int __init amce_init(void){
	acme_class = class_create(THIS_MODULE,DEVNAME);
	
	if(alloc_chrdev_region(&acme_dev_number,0,DEVCOUNT,DEVNAME) < 0) {
		printk(KERN_DEBUG "Can't register device\n"); 
		return -ENODEV;
	}
	
	acme_devp = kmalloc(sizeof(struct acme_dev), GFP_KERNEL);
	if(!acme_devp){
		printk("Bad Kmalloc\n"); 
		return -ENOMEM;
	}
	
	cdev_init(&acme_devp->cdev,&acme_fops);
	acme_devp->led_ctl=LED_OFF;
	if(blink_rate<0)return -EINVAL;
	if(blink_rate==0){
		printk("blink_rate > 0 required, resetting to default\n");
		blink_rate = BLINKRATE;
	}
	acme_devp->blink_rate=blink_rate;

	if(cdev_add(&acme_devp->cdev,acme_dev_number,DEVCOUNT)){
		printk(KERN_NOTICE "cdev_add() failed");
		unregister_chrdev_region(acme_dev_number,DEVCOUNT);
		return -1;
	}

	device_create(acme_class,NULL,acme_dev_number,NULL,DEVNAME);
	
	printk("ACME: character device %s loaded!\n",DEVNAME);
	
	return pci_register_driver(&acme_pci_driver);
}

static void __exit amce_exit(void){
	writel(0xFFFFFFFF,acme_devp->hw.hw_addr+IMC);
	writel((readl(acme_devp->hw.hw_addr+RCTL) & ~EN),acme_devp->hw.hw_addr+RCTL);
	
	cdev_del(&acme_devp->cdev);
	unregister_chrdev_region(acme_dev_number,DEVCOUNT);
	kfree(acme_devp);
	device_destroy(acme_class,acme_dev_number);
	class_destroy(acme_class);
	pci_unregister_driver(&acme_pci_driver);
	printk("ACME: character device %s unloaded.\n",DEVNAME);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chad Coates");
MODULE_VERSION("0.5");

module_init(amce_init);
module_exit(amce_exit);

