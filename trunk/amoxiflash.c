/*
amoxiflash -- NAND Flash chip programmer utility, using the Infectus 1 / 2 chip
Copyright (C) 2008  bushing

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

For more information, contact bushing@gmail.com, or see http://code.google.com/p/amoxiflash
*/

#include <stdio.h>
#include <math.h>
#include <usb.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#define INFECTUS_NAND_CMD 0x4e
#define INFECTUS_NAND_SEND 0x1
#define INFECTUS_NAND_RECV 0x2

#define NAND_RESET 0xff
#define NAND_CHIPID 0x90
#define NAND_GETSTATUS 0x70
#define NAND_ERASE_PRE 0x60
#define NAND_ERASE_POST 0xd0
#define NAND_READ_PRE 0x00
#define NAND_READ_POST 0x30
#define NAND_WRITE_PRE 0x80
#define NAND_WRITE_POST 0x10

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long int u64;

usb_dev_handle *locate_infectus(void);

char *pld_ids[] = {
	  "O2MOD",
      "Globe Hitachi",
      "Globe Samsung",
      "Infectus 78",
      "NAND Programmer",
      "2 NAND Programmer",
      "SPI Programmer",
      "XDowngrader"
};

struct usb_dev_handle *h;
struct timeval tv1, tv2;

int run_fast = 0;
int block_size = 0x2c0;
int verify_after_write = 1;
int debug_mode = 0;
int test_mode = 0;
int check_status = 0;
int start_block = 0;
int quick_check = 0;

u32 start_time = 0;
u32 blocks_done = 0;

void timer_start(void) {
	gettimeofday(&tv1, NULL);
}

unsigned long long timer_end(void) {
	unsigned long long retval;
	gettimeofday(&tv2, NULL);
	retval = (tv2.tv_sec - tv1.tv_sec) * 1000000ULL;
	retval += tv2.tv_usec - tv1.tv_usec;
	return retval;
}

char ascii(char s) {
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(void *d, int len) {
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16) {
    printf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len) printf("   ");
      else printf("%02x ",data[off+i]);

    printf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) printf(" ");
      else printf("%c",ascii(data[off+i]));
    printf("\n");
  }
}

int infectus_sendcommand(u8 *buf, int len, int maxsize) {
	if (debug_mode) {
		printf("> "); hexdump(buf, len);
	}
	
	int ret = 0;
start:
		while (ret < len) {
		ret = usb_bulk_write(h,1,(char *)buf,len,500);
		if (ret < 0) {
			printf("Error %d sending command: %s\n", ret, usb_strerror());
			return ret;
		}
		if (ret != len) {
			printf("Error: short write (%d < %d)\n", ret, len);
		}
	}

	ret=usb_bulk_read(h,1,(char *)buf,maxsize,500);	
	if(ret < 0) {
		printf("Error reading reply: %d\n", ret);
		return ret;		
	}
	
	if (debug_mode) {
		printf("< ");
		hexdump(buf, ret);
	}
	
	if(buf[0] != 0xFF) {
		printf("Reply began with %02x, expected ff\n", buf[0]);
		goto start;
	}
	buf++; // skip initial FF
	
	if (debug_mode && ret > 0) hexdump(buf, ret);
//	usleep(1000);
	return ret;
}

int infectus_nand_command(u8 *command, unsigned int len, ...) {
	int i;
	va_list ap;
    va_start(ap, len);
	memset(command, 0, len+9);
	command[0]=INFECTUS_NAND_CMD;
	command[7]=len;
	for(i=0; i<=len; i++) {
		command[i+8]=va_arg(ap,int) & 0xff;
	}
    va_end(ap);
	return len+9;
}

int infectus_nand_receive(u8 *buf, int len) {
	memset(buf, 0, 8);
	buf[0] = INFECTUS_NAND_CMD;
	buf[1] = INFECTUS_NAND_RECV;
	buf[6] = (len >> 8) & 0xff;
	buf[7] = len & 0xff;
	return infectus_sendcommand(buf, 8, len+3);
}

int infectus_reset() {
	u8 buf[128];
	int ret;
	
	ret = usb_set_configuration(h,1);
	if (ret) printf("conf_stat=%d\n",ret);

	if (ret == -1) {
		printf("Unable to set USB device configuration; are you running as root?\n");
		exit(0);
	}

	/* Initialize "USB stuff" */
	ret = usb_claim_interface(h,0);
	if (ret) printf("claim_stat=%d\n",ret);

	ret = usb_set_altinterface(h,0);
	if (ret) printf("alt_stat=%d\n",ret);
	
	ret = usb_clear_halt(h, 1);
	if (ret) printf("usb_clear_halt(1)=%d (%s)\n",ret, usb_strerror());

	/* What does this do? */
	ret = usb_control_msg(h, USB_TYPE_VENDOR + USB_RECIP_DEVICE, 
		2, 2, 0, (char *)buf, 0, 1000);
	if (ret) printf("usb_control_msg(2)=%d\n", ret);

	/* Send Infectus reset command */
	ret = 0;
	while(ret == 0) {
		memcpy(buf, "\x45\x15\x00\x00\x00\x00\x00\x00", 8);
		ret = infectus_sendcommand(buf, 8, 128);
	}
	return 0;
}

/* Version part 1: ? */
int infectus_get_version(void) {
	u8 buf[128];
	int ret;
	memcpy(buf, "\x45\x13\x01\x00\x00\x00\x00\x00", 8);

	ret=infectus_sendcommand(buf, 8, 128);
	printf("Infectus version (?) = %hhx\n", buf[1]);
//	hexdump(buf, ret);
	return 0;
}

/* Version part 2: ? */
int infectus_get_loader_version(void) {
	u8 buf[128];
	int ret;
	memcpy(buf, "\x4c\x07\x00\x00\x00\x00\x00\x00", 8);

	ret = infectus_sendcommand(buf, 8, 128);
	printf("Infectus Loader version = %hhu.%hhu", buf[1], buf[2]);
//	hexdump(buf, ret);
	return 0;
}

int infectus_check_pld_id(void) {
	u8 buf[128];
	int ret;
	memcpy(buf, "\x4c\x15\x00\x00\x00\x00\x00\x00", 8);

	ret = infectus_sendcommand(buf, 8, 128);
	if (ret < 0) return ret;
	if (buf[1] > (sizeof pld_ids)/4) {
		fprintf(stderr, "Unknown PLD ID %d\n", buf[1]);
	} else {
		printf("PLD ID: %s\n", pld_ids[buf[1]]);
	}
	if (buf[1] != 7) {
		fprintf(stderr, "WARNING: This program has only been tested with the \"XDowngrader\" PLD firmware.  Good luck.\n");
	}
	return 0;
}

/* In a dual-NAND configuration, select one of the chips (generally 0 or 1) */
int infectus_selectflash(int which) {
	u8 buf[128];
	memcpy(buf, "\x45\x14\x00\x00\x00\x00\x00\x00", 8);
	buf[2] = which;
	
	infectus_sendcommand(buf, 8, 128);
	return 0;
}

/* Get NAND flash chip status */
int infectus_getstatus(void) {
	u8 buf[128];
	int ret,len;
	
	len=infectus_nand_command(buf, 0, NAND_GETSTATUS);
	ret=infectus_sendcommand(buf, len, 128);

	infectus_nand_receive(buf, 1);
	
	return buf[1];
}

/* Wait for NAND flash to be ready */
void wait_flash(void) {
	int status=0;
	while (status != 0xc0) {
		status = infectus_getstatus();
		if(status != 0xc0) printf("Status = %x\n", status);
	}
}

/* Query the first two bytes of the NAND flash chip ID.
   Eventually, this should be used to select the appropriate parameters
   to be used when talking to this chip. */

int infectus_getflashid(void) {
	u8 buf[128];
	int ret,len;

	len=infectus_nand_command(buf, 0, NAND_RESET);
	ret=infectus_sendcommand(buf, len, 128);

	len=infectus_nand_command(buf, 1, NAND_CHIPID, 0);
	ret=infectus_sendcommand(buf, len, 128);

	infectus_nand_receive(buf, 2);
	
	return buf[1] << 8 | buf[2];
}

/* Erase a block of flash memory. */
int infectus_eraseblock(unsigned int blockno) {
	u8 buf[128];
	unsigned int pageno = blockno * 0x40;
	int ret, len;

	if (test_mode) return 0;
	
	len=infectus_nand_command(buf, 3, NAND_ERASE_PRE, pageno, pageno >> 8, pageno >> 16);
	ret = infectus_sendcommand(buf, len, 128);
	if (ret!=1) printf("Erase command returned %d\n", ret);

	len=infectus_nand_command(buf, 0, NAND_ERASE_POST);
	ret = infectus_sendcommand(buf, len, 128);
	
	if (check_status) wait_flash();
	return ret;
}


int infectus_readflashpage(u8 *dstbuf, unsigned int pageno) {
	u8 buf[128];
	u8 flash_buf[4096];
	int ret, len, subpage;
	
	len=infectus_nand_command(buf, 5, NAND_READ_PRE, 0, 
		0, pageno, pageno >> 8, pageno >> 16);
	ret = infectus_sendcommand(buf, len, 128);

	len=infectus_nand_command(buf, 0, NAND_READ_POST);
	ret = infectus_sendcommand(buf, len, 128);
	
	len = 0;
	for(subpage = 0; subpage < ceil(2112.0 / block_size); subpage++) {
		ret = infectus_nand_receive(flash_buf, block_size);
		if (ret!= (block_size+1)) printf("Readpage returned %d\n", ret);
		memcpy(dstbuf + subpage*block_size, flash_buf+1, block_size);
		len += ret-1;
	}
	return len;
}

int file_readflashpage(FILE *fp, u8 *dstbuf, unsigned int pageno) {
	fseeko(fp, pageno * 2112, SEEK_SET);
	return fread(dstbuf, 1, 2112, fp);
}

int file_writeflashpage(FILE *fp, u8 *dstbuf, unsigned int pageno) {
	fseeko(fp, pageno * 2112, SEEK_SET);
	return fwrite(dstbuf, 1, 2112, fp);
}

int mem_compare(u8 *buf1, u8 *buf2, int size) {
	int i;
	for(i=0; i<size; i++) if(buf1[i] != buf2[i]) break;
	return i;
}

int flash_compare(FILE *fp, unsigned int pageno) {
	u8 buf1[2112], buf2[2112], buf3[2112];
	int x;
	file_readflashpage(fp, buf1, pageno);
	infectus_readflashpage(buf2, pageno);

	if((x = memcmp(buf1, buf2, sizeof buf1))) {
//		printf("miscompare on page %d: \n", pageno);
//		infectus_readflashpage(buf3, pageno);
//		if(memcmp(buf2, buf3, sizeof buf3)) {
//			printf("chip is on crack\n");
//		}
	}
	return x;
}

int flash_isFF(u8 *buf, int len) {
	unsigned int *p = (unsigned int *)buf;
	int i;
	len/=4;
	for(i=0;i<len;i++) 	if(p[i]!=0xFFFFFFFF) return 0;
	return 1;
}

int infectus_writeflashpage(u8 *dstbuf, unsigned int pageno) {
	u8 buf[128];
	u8 flash_buf[4096];
	int ret, len, subpage;
	
	if (test_mode) return 0;
	
	for(subpage = 0; subpage < ceil(2112.0/block_size); subpage++) {
			len=infectus_nand_command(buf, 5, NAND_WRITE_PRE, subpage * block_size,
				(subpage * block_size) >> 8 , pageno, pageno >> 8, pageno >> 16);
			ret = infectus_sendcommand(buf, len, 128);

			// XXX cleanup
			memcpy(flash_buf, "\x4e\x01\x00\x00\x00\x00\x02\xc0", 8);
			memcpy(flash_buf+8, dstbuf + subpage * block_size, block_size);

			infectus_sendcommand(flash_buf, block_size+8, 4096);

  			len=infectus_nand_command(buf, 0, NAND_WRITE_POST);
			ret = infectus_sendcommand(buf, len, 128);

		if (check_status) wait_flash();
		}
	return 0;
}

int flash_program_block(FILE *fp, unsigned int blockno) {
	u8 buf[2112];
	unsigned long long usec;
	int pageno, p, miscompares=0;
//	printf("flash_program_block(0x%x)\n", blockno);
	printf("%04x: ", blockno); fflush(stdout);
	timer_start();
	for(pageno = run_fast?2:0; pageno < 0x40; pageno += (run_fast?0x4:1)) {
		p = blockno*0x40 + pageno;
		if (flash_compare(fp, p)) {
			putchar('x');
			miscompares++;
			if (run_fast) break;
			} else putchar('=');
		fflush(stdout);
	}
	usec = timer_end();
	float rate = (float)blocks_done / (time(NULL) - start_time);
	int secs_remaining = (4096 - blockno) / rate;
	if (blocks_done > 2) printf (" %04.1f (%d:%02d) ", p * 100.0 / (4096 * 0x40), secs_remaining / 60, secs_remaining % 60);
	printf ("Read(%.3f)\n", usec / 1000000.0f);
	if (miscompares > 0) {
//		printf("   %d miscompares in block\n", miscompares);
		printf("Erasing...");
		infectus_eraseblock(blockno);
		printf("\nProg: ");
		timer_start();
		for(pageno = 0; pageno < 0x40; pageno++) {
			p = blockno*0x40 + pageno;
			if (file_readflashpage(fp, buf, p)==2112) {
				if(flash_isFF(buf,2112)) {
					putchar('F');
					continue;
				}
				infectus_writeflashpage(buf, p);
				if (verify_after_write) {
					if (flash_compare(fp, p)) {
						putchar('!');
					} else putchar('.');
					fflush(stdout);
				}
			}
		}
		usec = timer_end();
		printf ("Write(%.3f)\n", usec / 1000000.0f);

	}
	blocks_done++;
	return 0;
}

int flash_dump_block(FILE *fp, unsigned int blockno) {
	u8 buf[2112];
	unsigned long long usec;
	int pageno, p, ret;
	printf("%04x: ", blockno); fflush(stdout);

	for(pageno = 0; pageno < 0x40; pageno++) {
		p = blockno*0x40 + pageno;
		ret = infectus_readflashpage(buf, p);
		if (ret==2112) {
			file_writeflashpage(fp, buf, p);
			putchar('.');
			fflush(stdout);
		} else {
			printf("error, short read: %d < %d\n", ret, 2112);
		}
	}
	float rate = (float)blocks_done / (time(NULL) - start_time);
	int secs_remaining = (4096 - blockno) / rate;
	if (blocks_done > 2) 
		printf (" %04.1f%% (%d:%02d) \r", 
			p * 100.0 / (4096 * 0x40), 
			secs_remaining / 60, secs_remaining % 60);
	else putchar('\r');
	fflush(stdout);
	blocks_done++;
	return 0;
}

void usage(char *progname) {
	fprintf(stderr, "Usage: %s -[tvwd] [-b blocksize] [-f filename] command\n", progname);
	fprintf(stderr, "          -t            test mode -- do not erase or write\n");
	fprintf(stderr, "          -v            verify every byte of written data\n");
	fprintf(stderr, "          -w            wait for status after programming\n");
	fprintf(stderr, "          -d            debug (enable debugging output)\n");
	fprintf(stderr, "          -b blocksize  set blocksize; see docs for more info.  Default: 0x%x\n", block_size);
	fprintf(stderr, "          -f filename   file to use for reading or writing data\n");
	fprintf(stderr, "          -s blockno    start block -- skip this number of blocks before proceeding\n");
	fprintf(stderr, "\nValid commands are:\n");
	fprintf(stderr, "\n         dump         read from flash chip and dump to file\n");
	fprintf(stderr, "\n         program      compare file to flash contents, reprogram flash to match file\n");
	exit(1);	
}

int main (int argc,char **argv)
{
	int send_status, retval, argno;
	u8 buf[4096];
	int ret;
	unsigned char send_buffer[8];
	unsigned char recv_buffer[1024];
	char ch;
	char *filename = NULL, *progname = argv[0];

	
	while ((ch = getopt(argc, argv, "b:tvwdf:s:q")) != -1) {
		switch (ch) {
			case 'b': block_size = strtol(optarg, NULL, 0); break;
			case 't': test_mode = 1; break;
			case 'v': verify_after_write = 1; break;
			case 'w': check_status = 1; break;
			case 'd': debug_mode = 1; break;
			case 'f': filename = optarg; break;
			case 's': start_block = strtol(optarg, NULL, 0); break;
			case 'q': quick_check = 1; break;
            case '?':
            default:
                usage(argv[0]);
         }
	}
	argc -= optind;
	argv += optind;
		
	usb_init();
//	usb_set_debug(2);
 	if ((h = locate_infectus())==0) 
	{
		printf("Could not open the infectus device\n");
		return (-1);
	}

	
	  
	unsigned int flashid = 0;
	infectus_reset();
	infectus_get_version();
	usleep(1000);
	infectus_get_loader_version();
	usleep(1000);
	infectus_check_pld_id();
	usleep(1000);
	infectus_selectflash(0);
	usleep(1000);
	flashid = infectus_getflashid();
		
	printf("ID = %x\n", flashid);
	flashid = infectus_getflashid();
		
	printf("ID = %x\n", flashid);
	flashid = infectus_getflashid();
		
	printf("ID = %x\n", flashid);

	switch(flashid) {
		case 0xADDC: printf("Detected Hynix 512Mbyte flash\n"); break;
		case 0xECDC: printf("Detected Samsung 512Mbyte flash\n"); break;
		case 0x98DC: printf("Detected Toshiba 512Mbyte flash\n"); break;
		case 0:
			printf("No flash chip detected; are you sure target device is powered on?\n");
			exit(1);
		default: 
			printf("Unknown flash ID %04x\n", flashid);
			printf("If this is correct, please notify the author.\n");
			exit(1);
	}

	start_time = time(NULL);
	for(argno=0; argno < argc; argno++) {
		if(!strcmp(argv[argno], "program")) {
			int blockno = start_block;

			if (!filename) {
				fprintf(stderr, "Error: you must specify a filename to program\n");
				usage(progname);
			}
			printf("Programming file %s into flash\n", filename);
			FILE *fp = fopen(filename, "rb");
			if(!fp) {
				perror("Couldn't open file: ");
				break;
			}
			fseek(fp, 0, SEEK_END);
			off_t file_length = ftello(fp);
			fseek(fp, 0, SEEK_SET);
			u64 num_pages = file_length / 2112;
			printf("File size: %llu bytes / %llu pages / %llu blocks\n", file_length, num_pages, num_pages / 64);
			for (; blockno < (num_pages / 64); blockno++) {
				flash_program_block(fp, blockno);
			}
			fclose(fp);
			continue;
		}

		if(!strcmp(argv[argno], "dump")) {
			unsigned long long begin, length;
			unsigned int blockno;

			blockno = start_block;
			length = 553648128ULL - blockno * 0x40ULL * 2112ULL;
			printf("Dumping flash @ 0x%llx (0x%llx bytes) into %s\n", 
				blockno*0x40ULL*2112ULL, length, filename);

			FILE *fp = fopen(filename, "w");
			if(!fp) {
				perror("Couldn't open file for writing: ");
				break;
			}
			for(; blockno < 4096; blockno++) {
//				printf("\rDumping block %x", blockno); fflush(stdout);
				flash_dump_block(fp, blockno);
			}
			printf("Done!\n");
			fclose(fp);
			continue;
		}
#if 0		
		if(!strcmp(argv[argno], "erase")) {
			argno++;
			int blockno = strtol(argv[argno], NULL, 0);
			infectus_eraseblock(blockno);
			continue;
		}

		if(!strcmp(argv[argno], "write")) {
			unsigned long long begin, length, offset;
			char *filename;
			argno++;
			filename = argv[argno++];
			begin = strtoll(argv[argno++], NULL, 0);
			length = strtoll(argv[argno++], NULL, 0);

			printf("Programming flash @ 0x%llx (0x%llx bytes) from %s\n", 
				begin, length, filename);
			FILE *fp = fopen(filename, "r");
			if(!fp) {
				perror("Couldn't open file for writing: ");
				break;
			}
			for(offset=0; (offset < length) && !feof(fp); offset+= 0x840) {
				printf("\rwriting page 0x%x", (unsigned int)(begin+offset)/0x840); fflush(stdout);
				
				fread(buf, 1, 0x840, fp);
				infectus_writeflashpage(buf, (begin+offset)/0x840);
			}
			printf("Wrote %llu bytes to %s\n", offset, filename);
			fclose(fp);
			continue;
		}	
#endif
		printf("Unknown command '%s'\n", argv[argno]);
		usage(progname);
	}
	usb_close(h);
	exit(0);
}	

usb_dev_handle *locate_infectus(void) 
{
	unsigned char located = 0;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *device_handle = 0;
 		
	usb_find_busses();
	usb_find_devices();
 
 	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
//			printf ("idVendor=%hx\n", dev->descriptor.idVendor);
			if (dev->descriptor.idVendor == 0x10c4) {	
				located++;
				device_handle = usb_open(dev);
				printf("infectus Device Found @ Address %s \n", dev->filename);
				printf("infectus Vendor ID 0x0%x\n",dev->descriptor.idVendor);
				printf("infectus Product ID 0x0%x\n",dev->descriptor.idProduct);
			}
//			else printf("** usb device %s found **\n", dev->filename);			
		}	
  }

  if (device_handle==0) return (0);
  else return (device_handle);  	
}




   

