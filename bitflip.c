/***************************************************************************
 *   Copyright (C) 2017 by Juan Carlos Fabero Jiménez                      *
 *   jcfabero@ucm.es                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/
 // USE: bitflip3 frame offset savefile

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
//#include <sys/time.h>
#include <time.h>
#include <sys/select.h>
#include <fcntl.h> 
#include <errno.h>
#include <arpa/inet.h> /* for htonl */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "bitflip.h"

#define SAVE 0
#define BITFLIP 1
#define RESTORE 2

#ifndef DEBUG
#define DEBUG 6
#endif


int write_32 (int fd, uint32_t data){
  uint32_t d;
  int n;
  d=htonl(data);
  n=write(fd, &d, sizeof(d));
  return n;
}

int write_16 (int fd, uint16_t data){
  uint16_t d;
  int n;
  d=htons(data);
  n=write(fd, &d, sizeof(d));
  return n;
}

int write_section (int fd, uint8_t s, const char* data){
  uint8_t c;
  uint16_t len, data_16;
  char str[512];
  
  write(fd, &(s), sizeof(s));
  
  snprintf(str, 255, "%s\0", data);
  len=strnlen(str, 254)+1;
  data_16=htons(len);
  write(fd, &data_16, sizeof(data_16));
  write(fd, str, len);
}

int write_words(int fd, const uint32_t *data, int len){
  int i;
  for (i=0; i<len; i++) 
    write_32(fd, data[i]);
}

int read_socket(int fd){
  int i, n;
  int fin=0;
  char c;
  
//   printf("read_socket IN\n");
  while (fin==0) {
    n=read(fd, &c, 1);
    if (n<0){
      perror("read");
      return ERROR_FAIL;
    }

    if (n==0) {
      fprintf(stderr, "ERROR: read 0 bytes!\n");
      return ERROR_OK;
    }
   
    if (c=='>')
      fin=1;
     
    printf("%c", c);
  }
//   printf("read_socket OUT\n");
  return ERROR_OK;
}


int openocd_connect(const char *host, const char *port){ // TODO connection to openocd server
  int sfd;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s;
  
 
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_protocol=0;
  hints.ai_flags=0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  
  s=getaddrinfo(host, port, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  /* getaddrinfo() returns a list of address structures.
  Try each address until we successfully connect(2).
  If socket(2) (or connect(2)) fails, we (close the socket
  and) try the next address. */

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype,
      rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;                  /* Success */

    close(sfd);
  }

  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not connect\n");
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(result);           /* No longer needed */

  read_socket(sfd);
  
  return sfd;
}

int openocd_save(int fd, uint32_t address, int size, char *filename, int dev){ 
  // gcapture and save configuration memory from address with size, to filename, from device dev
  char obuf[1024], ibuf[512];
  int n, i;
  
  snprintf(obuf, 512, "virtex2 gcapture %d\n\0", dev);
  write(fd, obuf, strnlen(obuf, 512));

  read_socket(fd);
  
  snprintf(obuf, 511, "pld save %d 0x%08x %d %s\n\0", dev, address, size/**sizeof(uint32_t)*/, filename);
  write(fd, obuf, strnlen(obuf, 512));

  read_socket(fd);
  
  return ERROR_OK;
}

int openocd_partial(int fd, uint32_t address, int size, char *filename, int dev){ 
  // restore original configuration memory from filename to device dev and grestore
  char obuf[512], ibuf[512];
  int n;
  struct timespec waiting;
   
   waiting.tv_sec=0;
   waiting.tv_nsec=500000000;
  
  snprintf(obuf, 512, "virtex2 partial %d %s\n\0", dev, filename);
  write(fd, obuf, strnlen(obuf, 512));
 
  read_socket(fd);
  
  snprintf(obuf, 512, "virtex2 grestore %d\n\0", dev);
  write(fd, obuf, strnlen(obuf, 512));
 //nanosleep(&waiting, NULL);

  read_socket(fd);
  
  return ERROR_OK;
}

int read_saved(char *filename, uint16_t framesize, uint32_t **bso){
  int fd;
  int bslen=0;
  struct stat fdstat;
  int i;
  uint32_t data_32;
  uint32_t *bs;
  
  printf("Reading %s as saved file\n", filename);
  
  if ( stat(filename, &fdstat) == -1 ){
    perror("stat");
    return 0;
  }
  
  bslen=fdstat.st_size/sizeof(uint32_t)-framesize;
  printf("Size of \"%s\" is %ld bytes (%ld words for bitstream)\n", filename, fdstat.st_size, bslen);
  
  if(bslen%framesize!=0){
    fprintf(stderr, "This does not seem to be a correct saved file\n");
    return 0;
  }
 
  fd=open(filename, O_RDONLY);
  if (fd < 0){
    perror("read_saved open");
    return 0;
  }
  
  if (lseek(fd, framesize*sizeof(uint32_t), SEEK_SET) < 0){
    perror("lseek");
    return 0;
  }
  
  bs=calloc(sizeof(uint32_t), bslen);
  if (bs == NULL) {
    fprintf(stderr, "Error calloc\n");
    close(fd);
    return 0;
  }
  
  for (i=0; i<bslen; i++) {
    if (read(fd, &data_32, sizeof(uint32_t)) < sizeof(uint32_t)){
      fprintf(stderr, "Error reading %s at word %d\n", filename, i);
      close(fd);
      return 0;
    }
    bs[i]=ntohl(data_32);
  }
  *bso=bs;
  close(fd);
  
  return bslen;
}

int bitflip(uint32_t *bs, int offset){
  uint32_t mask=0x00000001;
  int word_offset, bit_offset;
  
  word_offset=(int)(offset/32);
  bit_offset=offset%32;
  bs[word_offset]^=mask<<bit_offset;
  
  return ERROR_OK;
}

int write_bitstream(char *filename, const uint32_t *bitstream, int framesize, int bslen,
		uint32_t address, uint32_t idcode, const char *partnumber){
  int filebin;
  uint32_t *bsheader;
  uint32_t *bssaved;
  uint32_t *pad;
  uint32_t *bstail;
  uint32_t bsheadersize, bssize, bstailsize;
  uint32_t streamsize=0;  /* bitstream size including header and tail */
  uint8_t  data_8;
  uint16_t data_16;
  uint32_t data_32;
  uint32_t magic[3];
  int i, j;
  char fecha[200];
  time_t t;
  struct tm *date_tmp;
   
   
  filebin=open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  if (filebin < 0) {
    perror("open");
    return ERROR_FAIL;
  }
  
  bsheadersize=29;
  bstailsize=124;
  bssize=bslen;
  
  bsheader=calloc(sizeof(uint32_t), bsheadersize); // TODO: calcular size;
  if (bsheader==NULL) {
    perror("calloc");
    return ERROR_FAIL;
  }
  pad=calloc(sizeof(uint32_t), framesize);
  if (pad==NULL) {
    perror("calloc");
    return ERROR_FAIL;
  }
  bstail=calloc(sizeof(uint32_t), bstailsize); // TODO: calcular size;
  if (bstail==NULL) {
    perror("calloc");
    return ERROR_FAIL;
  }
  
  /****** bitfile header ******/
    /* Magic Number */
    i=0;
    magic[i++]=MAGIC1;
    magic[i++]=MAGIC2;
    magic[i++]=MAGIC3;
    write_words(filebin, magic, i);
    data_8 = MAGIC4;
    write(filebin, &(data_8), sizeof(data_8));
    
    
    /* file name and user_id (a) */
    write_section(filebin, 'a', "bitflip.ncd;UserID=0x11805469");
     
    
    /* Part number (b) 7a100tcsg324 */
    write_section(filebin, 'b', partnumber);
    
    /* Date (c) */
    t = time(NULL);
    date_tmp = localtime(&t);
    if (date_tmp == NULL) {
        perror("localtime");
        strncpy(fecha, "2021/07/00", 20); // fecha no válida para indicar error
    }
    if (strftime(fecha, sizeof(fecha), "%Y/%m/%d", date_tmp) == 0) {
        fprintf(stderr, "Error en strftime\n");
        strncpy(fecha, "2021/07/00", 20); // fecha no válida para indicar error
    }
    printf("Fecha: %s ", fecha);
    write_section(filebin, 'c', fecha);
    
    /* Time (d) */
    if (strftime(fecha, sizeof(fecha), "%T", date_tmp) == 0) {
        fprintf(stderr, "Error en strftime\n");
        strncpy(fecha, "00:00:00", 20);
    }
    printf("%s\n", fecha);
    write_section(filebin, 'd', fecha);
    
    /* Bitstream length (e) */
    data_8='e';
    write(filebin, &(data_8), sizeof(data_8));
    data_32=htonl(4*(bsheadersize + bssize + framesize + bstailsize)); /* one frame + pad */
    write(filebin, &(data_32), sizeof(uint32_t));
    
    
    /****** bitstream ******/
    /* Header */
    i=0;
    for (j=0; j<8; j++) {
      bsheader[i++]=DUMMY;
    }
    bsheader[i++]=BUS_SYNC;
    bsheader[i++]=BUS_WIDTH_DETECT;
    bsheader[i++]=DUMMY;
    bsheader[i++]=DUMMY;
    bsheader[i++]=SYNC_WORD;
    bsheader[i++]=NOOP;
    bsheader[i++]=W_CMD;
    bsheader[i++]=RCRC;
    bsheader[i++]=NOOP;
    bsheader[i++]=NOOP;
    bsheader[i++]=WR_IDCODE;
    bsheader[i++]=idcode;
    bsheader[i++]=W_CMD;
    bsheader[i++]=NULLCMD;
    bsheader[i++]=W_CMD;
    bsheader[i++]=WCFG;
    bsheader[i++]=NOOP;
    bsheader[i++]=WR_FAR;
    bsheader[i++]=address;
    bsheader[i++]=NOOP;
    bsheader[i++]=WR_FDRI+framesize*2;
    write_words(filebin, bsheader, i);
    free(bsheader);
    

    
    /* bitstream */
    write_words(filebin, bitstream, bssize);
    
    /* pad */
    write_words(filebin, pad, framesize);
    free(pad);
    
    /*  tail */
    i=0;
    bstail[i++]=W_CMD; 
    bstail[i++]=DGHIGH;
    for (j=0; j<100; j++)
      bstail[i++]=NOOP;
    bstail[i++]=WR_FAR;
    bstail[i++]=LASTFRM; // TODO: last frame
    bstail[i++]=W_CMD;
    bstail[i++]=RCRC;
    bstail[i++]=W_CMD;
    bstail[i++]=DESYNCH;
    for (j=0; j<16; j++)
      bstail[i++]=NOOP;
    write_words(filebin, bstail, i);
    free(bstail);
  
  
  return ERROR_OK;
}

/* Read filename with frame addresses and offsets return an array with flip-flop structures
   Return number of members in array */
int read_list(const char *filename, flipflop_t *flipflop){
  FILE *stream;
  int i, n;
  int eof=0;
  
  stream=fopen(filename, "r");
  if (fopen==NULL) {
    perror("fopen");
    return ERROR_FAIL;
  }
  
  i=0;
  while (!eof) {
    eof=(fscanf(stream, "%x %d\n", &(flipflop[i].frame), &(flipflop[i].offset))!=2);
    if (!eof) {
      printf ("Frame=0x%8.8x Offset=%5d\n", flipflop[i].frame, flipflop[i].offset);
      i++;
    }
  }
  
  fclose(stream);
  
  return i;
}

int main(int argc, char** argv) { 
  
   uint32_t data_32;
   uint16_t data_16;
   uint8_t data_8;
   int filebin;
   char filename[256], fraw[256], foriginal[256];
   char data_char[256];
   uint16_t len, bslen;
   uint16_t framesize=101; /* Frame size in words */
   uint32_t bsheadersize, bssize, bstailsize;
   uint32_t streamsize=0;  /* bitstream size including header and tail */
   uint32_t idcode=0x03631093; /* Artix7 xc7a100t */
   uint32_t address=0x00401B9F;
   uint32_t *bsheader;
   uint32_t *bitstream, *bssaved;
   uint32_t *pad;
   uint32_t *bstail;
   int bslen_pos, filelen_pos;
   int i, j;
   char *idcode_str;
   char partnumber[128], *partnumber_env;
   int server_sock;
   char serverport[32];
   char servername[512];
   int command=BITFLIP;
   flipflop_t flipflop[512];
   unsigned int n_bitflips;
   unsigned long int frame, oframe;
   unsigned int offset = 3;
   
   
  // TODO: check arguments
   
//    snprintf(servername, 511, "localhost");
//    snprintf(serverport, 31, "4444");
   
//    n_bitflips=read_list(argv[1], flipflop);
//    printf("Ready for %d bitflips\n", n_bitflips);
     
   frame=strtoll(argv[1], NULL, 0);
   offset=strtoll(argv[2], NULL, 0);
   if (DEBUG >=5)
     fprintf(stderr, "Frame: 0x%08x Offset: %d\n", frame, offset);
   
   
      
  snprintf(fraw, 254, "%s", argv[3]);
  snprintf(filename, 254, "%s.bit", argv[3]);
  if (DEBUG >= 5)
    fprintf(stderr, "Save file: %s\n", fraw);
  
  
  // TODO: getenv
   
  idcode_str=getenv("IDCODE");
  if (idcode_str==NULL)
    idcode=0x13631093; /* Artix7 xc7a100t*/
  else
    idcode=strtoll(idcode_str, NULL, 0);
  
  if (DEBUG >= 5)
    fprintf(stderr, "IDCODE=%s, 0x%08X\n", idcode_str, idcode);

  partnumber_env=getenv("PARTNUMBER");
  if (partnumber_env==NULL)
    strncpy(partnumber, "7a100tcsg324", 127);
  else
    strncpy(partnumber, partnumber_env, 127);
  
  if (DEBUG >= 5)
    fprintf(stderr, "PARTNUMBER=%s\n", partnumber);
    
    
//     bitstream=calloc(sizeof(uint32_t), bssize); // TODO: posible más de un frame?
//     if (bitstream==NULL) {
//       perror("calloc");
//       return ERROR_FAIL;
//     }

    
//   //  connect to openocd server
//     server_sock=openocd_connect(servername, serverport);
//     if (server_sock < 0) {
//       fprintf(stderr, "Error connecting to %s:%d\n", servername, serverport);
//       return server_sock;
//     }
//     
//     
// 	openocd_save(server_sock, frame, framesize+framesize, fraw, 0);
	
	// Read saved 
	bslen=read_saved(fraw, framesize, &bssaved);
	if (bslen==0){
	  fprintf(stderr, "Error reading saved bitstream\n");
	  return ERROR_FAIL;
	}
	if (DEBUG >= 5)
	  printf("Read %d words\n", bslen);
	
	/* bit flip */
	if (command==BITFLIP)
	  bitflip(bssaved, offset); 
	
	/* Write new partial bitstream file */
	filebin=open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	if (filebin < 0) {
	  perror("open");
	  return ERROR_FAIL;
	}
	write_bitstream(filename, bssaved, framesize, bslen, frame, idcode, partnumber);
	close(filebin);
	
// 	//  Write partial stream to device
// 	if(openocd_partial(server_sock, frame, framesize, filename, 0)==ERROR_FAIL){
// 	  fprintf(stderr, "Error writing to device\n");
// 	  return ERROR_FAIL;
// 	}
      
//     shutdown(server_sock, SHUT_RDWR);
    
    return ERROR_OK;
  
 
	
}
