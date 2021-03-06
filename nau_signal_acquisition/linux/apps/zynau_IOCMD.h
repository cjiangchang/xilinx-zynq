#ifndef Z7NAU_IOCMD_H_
#define Z7NAU_IOCMD_H_

/* IOCTL defines */
#define AXILITE_IOCTL_BASE		'W'
#define AXILITE_RDALENGTH	        _IO(AXILITE_IOCTL_BASE, 0)
#define AXILITE_RADLENGTH		_IO(AXILITE_IOCTL_BASE, 1)
#define AXILITE_RTRIGINT		_IO(AXILITE_IOCTL_BASE, 2)
#define AXILITE_RTRIGSEL	        _IO(AXILITE_IOCTL_BASE, 3)
#define AXILITE_RADRSTN		        _IO(AXILITE_IOCTL_BASE, 4)
#define AXILITE_RDARSTN		        _IO(AXILITE_IOCTL_BASE, 5)
#define AXILITE_RLEDON_TIME	        _IO(AXILITE_IOCTL_BASE, 12)
#define AXILITE_RLEDOFF_TIME	        _IO(AXILITE_IOCTL_BASE, 13)


#define AXILITE_WDALENGTH		_IO(AXILITE_IOCTL_BASE, 6)
#define AXILITE_WADLENGTH		_IO(AXILITE_IOCTL_BASE, 7)
#define AXILITE_WTRIGINT		_IO(AXILITE_IOCTL_BASE, 8)
#define AXILITE_WTRIGSEL	        _IO(AXILITE_IOCTL_BASE, 9)
#define AXILITE_WADRSTN		        _IO(AXILITE_IOCTL_BASE, 10)
#define AXILITE_WDARSTN		        _IO(AXILITE_IOCTL_BASE, 11)
#define AXILITE_WLEDON_TIME	        _IO(AXILITE_IOCTL_BASE, 14)
#define AXILITE_WLEDOFF_TIME	        _IO(AXILITE_IOCTL_BASE, 15)


/* IOCTL defines */
#define Z7AXIGPIO_IOCTL_BASE		'W'
#define Z7AXIGPIO_RDATA	        _IO(Z7AXIGPIO_IOCTL_BASE, 20)
#define Z7AXIGPIO_RTRI		_IO(Z7AXIGPIO_IOCTL_BASE, 21)
#define Z7AXIGPIO2_RDATA	_IO(Z7AXIGPIO_IOCTL_BASE, 22)
#define Z7AXIGPIO2_RTRI	        _IO(Z7AXIGPIO_IOCTL_BASE, 23)
#define Z7AXIGPIO_RGIER 	_IO(Z7AXIGPIO_IOCTL_BASE, 24)
#define Z7AXIGPIO_RIER		_IO(Z7AXIGPIO_IOCTL_BASE, 25)
#define Z7AXIGPIO_RISR		_IO(Z7AXIGPIO_IOCTL_BASE, 26)

#define Z7AXIGPIO_WDATA	        _IO(Z7AXIGPIO_IOCTL_BASE, 27)
#define Z7AXIGPIO_WTRI		_IO(Z7AXIGPIO_IOCTL_BASE, 28)
#define Z7AXIGPIO2_WDATA	_IO(Z7AXIGPIO_IOCTL_BASE, 29)
#define Z7AXIGPIO2_WTRI	        _IO(Z7AXIGPIO_IOCTL_BASE, 30)
#define Z7AXIGPIO_WGIER 	_IO(Z7AXIGPIO_IOCTL_BASE, 31)
#define Z7AXIGPIO_WIER		_IO(Z7AXIGPIO_IOCTL_BASE, 32)
#define Z7AXIGPIO_WISR		_IO(Z7AXIGPIO_IOCTL_BASE, 33)

#endif /* Z7NAU_IOCMD_H_ */
