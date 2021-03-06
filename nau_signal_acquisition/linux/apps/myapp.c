#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include  <sys/timerfd.h>
#include <pthread.h>

#include "z7spi_slave.h"
#include "lmk048spi.h"
#include "z7nau_IOCMD.h"
#include "ad9266spi.h"
#include "z7nau_system.h"


#define SPI_DEV_LMK048 "/dev/spidev0.0"
#define SPI_DEV_AD9266 "/dev/spidev0.1"

#define DA_DEV_LT1688 "/dev/z7naubram_DA"
#define AD_DEV_AD9266 "/dev/z7naubram_AD"
#define DEV_AXILITE_REG "/dev/axilite_reg"
#define DEV_Z7NAU_AXIGPIO "/dev/z7nau_axigpio"

#define SERVER_IP  "192.168.1.100"
#define HEAD_TAG(x)  ((x >>16) & 0xffff)
#define HEAD_CMD(x)  (x & 0xff)
#define HEAD_LEN(x)  ((x >>8) & 0xff)
#define CMD_HEAD     0xa55a
#define S_HEAD_CREAT(CMD_HEAD,CMD_LEN,CMD) ((CMD_HEAD & 0xffff) <<16) | ((CMD_LEN & 0xff) << 8) | ( CMD & 0xff)
#define S2MS(x)  x*1000

#define  DA_LENGTH_MAX    50000
short da_buffer[DA_LENGTH_MAX] ;

#define  AD_LENGTH_MAX    10000
short ad_buffer[AD_LENGTH_MAX];

int   average_buffer[AD_LENGTH_MAX];
int s_buffer[4] = {0};
int axireg_fd=0,axigpio_fd=0;
//sig_atomic_t sigusr1_count = 0;
sem_t sem;

void ad_sigal_handler(int signum);

struct SPI_Config lmk_spiconfig={
                    LMK_SPI_MODE,
                    LMK_SPI_LSB,
		            LMK_SPI_BITS,
                    LMK_SPI_MAXSPEED
                    };


int main( void )
{   	
 int key_value;
 char buf_lmk[LMK_SPI_WORD_LENGTH];
  int i = 0;

 SPIslave_Config(SPI_DEV_LMK048,lmk_spiconfig,&lmk_reg_val[0][0],LMK_SPI_WORD_LENGTH,buf_lmk);
 usleep(LMK_RESET_LENGTH);

 for(i=1;i<LMK_REG_NUM+1;i++)
        SPIslave_Config(SPI_DEV_LMK048,lmk_spiconfig,&lmk_reg_val[i][0],LMK_SPI_WORD_LENGTH,buf_lmk);


/*****************axi gpio file***************/
	axigpio_fd = open(DEV_Z7NAU_AXIGPIO, O_WRONLY );
	if(axigpio_fd<0)
	 {
	   perror("open device file failed!");
	   printf("z7nau_axigpio:[ERROR] Can't open device.");
           return -ENODEV;
	 }


/*****************axi_lite  reg  file***************/
	axireg_fd = open(DEV_AXILITE_REG, O_WRONLY);
	if(axigpio_fd<0)
	 {
	   perror("open device file failed!");
	   printf("axilite_reg:[ERROR] Can't open device.");
           return -ENODEV;
	 }
   
       //led always on
        ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
        ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);


      while(1){
         key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,0);
         printf("main function:key_value=%d\n",key_value);
         if (key_value == KEY_CLOSE) // 0 --key close,  1 -- key open
             net_task();
         else
             datasample_task();
     
       }

       close(axigpio_fd);
       close(axireg_fd);
 
       return 0;

}

int net_task()
{
  int value = 0,status;
  int sockfd;
  int sockfd2;
  int status1 = -1,status2 = -1;
  struct sockaddr_in s_addr;
  struct sockaddr_in s_addr2;
  int s_head;
 // int s_buffer[4];
  struct timeval timeout = {2,0}; 
  struct info_savedfiles info_savef = {0,0,0,0};
  int file_num;
  char filename[40] = {0};
  
	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<=0){
	   printf("socket  failed,sockfd=%d\n",sockfd);
           goto net_error;
	}
	sockfd2=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd2<=0){
	   printf("socket  failed,sockfd=%d\n",sockfd2);
           close(sockfd);
           goto net_error;
	}

	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval) );

    status = DA_LENGTH_MAX;
	setsockopt(sockfd2,SOL_SOCKET,SO_SNDBUF,(const char*)&status,sizeof(int) );

	memset(&s_addr, 0, sizeof(s_addr));
   	s_addr.sin_port=htons(60400);
   	s_addr.sin_family=AF_INET;
   	s_addr.sin_addr.s_addr= inet_addr(SERVER_IP);

   	memset(&s_addr2, 0, sizeof(s_addr2));
   	s_addr2.sin_port=htons(60500);
   	s_addr2.sin_family=AF_INET;
   	s_addr2.sin_addr.s_addr= inet_addr(SERVER_IP);

      // wait for net connected
	while(status1 != 0 || status2 != 0){
	      if(status1 != 0)
	         status1=connect(sockfd,(struct sockaddr * )&s_addr,sizeof(s_addr));

	      if(status2 != 0)
	         status2=connect(sockfd2,(struct sockaddr * )&s_addr2,sizeof(s_addr2));

             if(status1 != 0 || status2 != 0){
            	 ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_OFF);
                 ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_OFF);
            	 value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(2));
                if(value == KEY_OPEN){
                  ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
                  ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
                  close(sockfd);
                  close(sockfd2);
                  return -2;
                }
             }       
 	 }

       ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
       ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
       value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,0);
       printf("net_task:key_value=%d\n",value);
       while(value == KEY_CLOSE)
       {

    	   status = recv(sockfd,&s_head, sizeof(unsigned int),0);  //主动关闭socket，recv返回-1,errno=9或104
    	   if(status < 0 ) {                                        //被动关闭socket，recv返回0，errno=11
    		   value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,0);  //正常连接的socket，如果有数据recv返回值接收字节数，否则返回-1且errno=11
    		   continue;
    	   }
    	   if(status == 0 && errno==11)
    		   goto net_error;
          if(HEAD_TAG(s_head)!= CMD_HEAD){
          //deal with invalid cmd packet	
	        continue;
          }
 //       if(!HEAD_LEN(s_head)){
        //len=0, peer received package error
 //       continue;
 //       }

       ioctl(axireg_fd,AXILITE_WLEDON_TIME,mSEC2CYCLE(200));
       ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,mSEC2CYCLE(200));

	   switch(HEAD_CMD(s_head)){
          case CMD_UPLOAD_PARAMETER:
        	  status = upload_parameter(sockfd2);
			  s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_UPLOAD_PARAMETER);
			  s_buffer[1] = status;
			  send(sockfd,s_buffer,sizeof(int)*2,0);
               break;	
          case CMD_UPLOAD_DAWAVE:
               status = upload_dawave(sockfd2,da_buffer);
               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_UPLOAD_DAWAVE);
               s_buffer[1] = status;
               send(sockfd,s_buffer,sizeof(int)*2,0);
               break;	
          case CMD_UPLOAD_SWTABLE:
        	     status = upload_switchtable(sockfd2);
				 s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_UPLOAD_SWTABLE);
				 s_buffer[1] = status;
				 send(sockfd,s_buffer,sizeof(int)*2,0);
               break;	
  	  case CMD_UPLOAD_DATAFILE:
  		  	  	  	  status = upload_datafile(sockfd2,&info_savef,ad_buffer,filename);
  		               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_UPLOAD_DATAFILE);
  		               s_buffer[1] = status;
  		               send(sockfd,s_buffer,sizeof(int)*2,0);
  		              send(sockfd,filename,40,0);
               break;
	  case CMD_GET_PARAMETER:
               //delete_file();
               status = get_parameter(sockfd2);
               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_GET_PARAMETER);
               s_buffer[1] = status;
               send(sockfd,s_buffer,sizeof(int)*2,0);
    	       break;
	  case CMD_GET_DAWAVE:
               status = get_dawave(sockfd2,da_buffer);
               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_GET_DAWAVE);
               s_buffer[1] = status;
               send(sockfd,s_buffer,sizeof(int)*2,0);
  	       break;
	  case CMD_GET_SWTABLE:
               status = get_switchtable(sockfd2);
               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_GET_SWTABLE);
               s_buffer[1] = status;
               send(sockfd,s_buffer,sizeof(int)*2,0);
	       break;
    	  case CMD_SELFCHECK:
    		  status =  z7nau_selfcheck(sockfd2);
    		  s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_SELFCHECK);
    		  s_buffer[1] = status;
    		  send(sockfd,s_buffer,sizeof(int)*2,0);
               break;
         case CMD_FILECHECK:
               status = check_savedfiles(&info_savef,&file_num);
               s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_FILECHECK);
               s_buffer[1] = status;
               s_buffer[2] = file_num;
               send(sockfd,s_buffer,sizeof(int)*3,0);
               break;
         case CMD_REQUEST_FILE:
              if(info_savef.switchs_upload == 0) info_savef.switchs_upload =1;
              if(info_savef.turns_upload == 0) info_savef.turns_upload =1;
              if(info_savef.turns_upload ==  info_savef.turns_saved && info_savef.switchs_upload == info_savef.switchs_saved)
              {/* allow upload file repeatly */
                 info_savef.turns_upload =1;
                 info_savef.switchs_upload =1;
              }
              send(sockfd,&s_head,sizeof(int),0);
              break;
         case CMD_BYTEORDERCHECK:
        	 /**********test remote machine byte order***********************/
        	 // little endian : 0xa55a2211
        	 // big endian:  0x11225aa5
        	 //s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,0x22,0x11);
        	 send(sockfd,&s_head,sizeof(int),0);
        	 break;
         case CMD_DELETE_FILE:
        	 	 status = delete_file(&info_savef);
        	 	 s_buffer[0] = S_HEAD_CREAT(CMD_HEAD,sizeof(int)*2,CMD_DELETE_FILE);
        	 	 s_buffer[1] = status;
        	 	 send(sockfd,s_buffer,sizeof(int)*2,0);
        	 	 break;
	 default:
	         break;
	}


	value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,0);
       	ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
       	ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
       	if(value == KEY_OPEN){
      	       	          close(sockfd);
      	       	          close(sockfd2);
      	       	          return 0;
      	}
    }
net_error:
close(sockfd);
  close(sockfd2);
     ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_OFF);
     ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_OFF);
     while(1){
    	 value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(2));
    	 if(value == KEY_OPEN) return -1;
     }
}


/*********************************************************
*  int upload_datafile(...)

*  upload a saved data file to PC after a request CMD
*  sockfd_dt:   socket fd for sending file data
*  *info_f:  pointer to information of saved files
*  buffer:   buffer of adc result data file 
**********************************************************/
int upload_datafile(int sockfd_dt,struct info_savedfiles *info_f,short *buffer,char *filename)
{
  char name[FILENAME_LEN] = {0};
  FILE *fp;
  int status,rswitch;
  short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM] ;


  fp = fopen("/mnt/usr/table_data.txt", "rb");
  if(fp == NULL)
  {
     printf("open param_data.txt file failed!\n");
     return -2;
  }

  status = fread(t_buff, sizeof(short), SWITCHTABLE_COLMAX*SWITCHTABLE_COLNUM, fp);
  //printf("Read %d data from table_data.txt.\n",status);
  fclose(fp);
  if( status > 0)	rswitch = status / SWITCHTABLE_COLNUM;
  else
  {
     printf("read table_data.txt file failed!\n");
     return -3;
  }


  if(info_f->turns_upload <= info_f->turns_saved )
  {
    //last turn
    if((info_f->turns_upload == info_f->turns_saved ) & (info_f->switchs_saved ==0))
      return -1;

    sprintf(name,"/mnt/usr/ad_data_%d_%d_ok.txt",info_f->turns_upload,info_f->switchs_upload);
    fp = fopen(name, "rb");
    status = fread(buffer,sizeof(short),AD_LENGTH_MAX,fp);  //ad_buffer
    fclose(fp);
    status=send( sockfd_dt,buffer,status*sizeof(short),0) ;
    printf("%s send success!\n",name);
//    if(status!=rec_n*sizeof(short))
//    printf("send data failed send_num=%d\n",status);
    //printf("%s send success!\n",name);
    //unlink(name);

    sprintf(filename,"ad_data_%d_%d_ok.txt",info_f->turns_upload,info_f->switchs_upload);
    //status=send( sockfd_dt,name,FILENAME_LEN,0) ;

    if(info_f->turns_upload < info_f->turns_saved)
    {     //not last kturn
       if(info_f->switchs_upload >= rswitch) {
          info_f->switchs_upload =1;
          info_f->turns_upload++;
       }
       else info_f->switchs_upload++;
       return 0;
    }
    // last kturn
    if(info_f->switchs_upload < info_f->switchs_saved)
    {
       info_f->switchs_upload++;
       return 0;
    }
  }
  return -1;
}

int get_parameter(int sockfd_dt)
{
  struct task_parameter s_buff;
  int  rec_n;
  FILE *fp;
 int data_fd;

  rec_n = recv(sockfd_dt,&s_buff, sizeof(struct  task_parameter),0);
  fp = fopen("/mnt/usr/param_data.txt", "wb");
  if(fp == NULL) {
     printf("open param_data.txt file failed!\n");
     return -1;
  }
  fwrite(&s_buff, sizeof(int), rec_n/sizeof(int), fp);
  fflush(fp);
  data_fd = fileno(fp);
  fsync(data_fd);
  fclose(fp);
  printf("parameter  transfer finished!\n");
  return 0;

}

int upload_parameter(int sockfd_dt)
{
  struct task_parameter s_buff;
  int  rec_n;
  FILE *fp;
 
  fp = fopen("/mnt/usr/param_data.txt", "rb");
  if(fp == NULL) {
     printf("open param_data.txt file failed!\n");
     return -1;
  }
  rec_n = fread(&s_buff, sizeof(struct  task_parameter), 1, fp);
  fclose(fp);
  printf("parameter  transfer finished!\n");
  rec_n = send(sockfd_dt,&s_buff, sizeof(struct  task_parameter),0);
  return 0;

}



int get_dawave(int sockfd_dt, short *buffer)
{
  struct task_parameter s_buff;
  int  rec_n,len,count;
  FILE *fp;
  int data_fd;

  fp = fopen("/mnt/usr/param_data.txt", "rb");
  if(fp == NULL) {
     printf("open param_data.txt file failed!\n");
     return -1;
  }
  rec_n = fread(&s_buff, sizeof(struct  task_parameter), 1, fp);
  fclose(fp);

  count = s_buff.da_wavelength;
  rec_n = 0;  len = 0;
  while(count > 0)
  {
    rec_n = recv(sockfd_dt,buffer+len, count*sizeof(short),0);
    len += rec_n/sizeof(short);
    count -= rec_n/sizeof(short);
  }

  count = s_buff.da_wavelength;
  fp = fopen("/mnt/usr/da_data.txt", "wb");
  if(fp == NULL) {
     printf("open da_data.txt file failed!\n");
     return -2;
  }
  rec_n = fwrite(buffer, sizeof(short), count, fp);
  fflush(fp);
  data_fd = fileno(fp);
  fsync(data_fd);
  fclose(fp);
  printf("da data translate finished!\n");
  return 0;
}


int upload_dawave(int sockfd_dt, short *buffer)
{
  struct task_parameter s_buff;
  int  rec_n,len,count;
  FILE *fp;

  fp = fopen("/mnt/usr/param_data.txt", "rb");
  if(fp == NULL) {
     printf("open param_data.txt file failed!\n");
     return -1;
  }
  rec_n = fread(&s_buff, sizeof(struct  task_parameter), 1, fp);
  fclose(fp);

  fp = fopen("/mnt/usr/da_data.txt", "rb");
  if(fp == NULL) {
     printf("open da_data.txt file failed!\n");
     return -2;
  }
  rec_n = fread(buffer, sizeof(short), s_buff.da_wavelength, fp);
  fclose(fp);
  printf("da data translate finished!\n");
 
  count = s_buff.da_wavelength;
  rec_n = 0;  len = 0;
  while(count > 0)
  {
    rec_n = send(sockfd_dt,buffer+len, count*sizeof(short),0);
    len += rec_n/sizeof(short);
    count -= rec_n/sizeof(short);
  }
  return 0;
}

int delete_file(struct info_savedfiles *info_savef)
{
	char name[40] = {0};
	int i=1,j=1;
	int rec_n=0,stauts=0;;
    FILE * fp;
    short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM];
    int rswitch=0;

	  fp = fopen("/mnt/usr/table_data.txt", "rb");
	  if(fp == NULL)
	  {
	     printf("open param_data.txt file failed!\n");
	     return -2;
	  }
	  rec_n = fread(t_buff, sizeof(short), SWITCHTABLE_COLMAX*SWITCHTABLE_COLNUM, fp);
	  printf("Read %d data from table_data.txt.\n",rec_n);
	  fclose(fp);
	  if(rec_n > 0)	rswitch = rec_n / SWITCHTABLE_COLNUM;


	for(i=1;i<info_savef->turns_saved;i++)
	{
		for(j=1;j<=rswitch;j++)
		{
			sprintf(name,"/mnt/usr/ad_data_%d_%d_ok.txt",i,j);
			stauts = unlink(name);
			if(stauts < 0)
					return stauts;
		}
	}
	for(j=1; j<= info_savef->switchs_saved; j++)
	{
		sprintf(name,"/mnt/usr/ad_data_%d_%d_ok.txt",i,j);
		stauts = unlink(name);
		if(stauts < 0)
			return stauts;
	}
	info_savef->turns_saved = 1;
	info_savef->switchs_saved = 0;


	fp = fopen("/mnt/usr/table_data.txt", "rb");
	  if(fp != NULL)
	  {
		    sprintf(name,"/mnt/usr/table_data.txt");
		  	stauts = unlink(name);
		  	if(stauts < 0)
		  				return stauts;
	  }
	  fp = fopen("/mnt/usr/param_data.txt", "rb");
	  	  if(fp != NULL)
	  	  {
	  		    sprintf(name,"/mnt/usr/param_data.txt");
	  		  	stauts = unlink(name);
	  		  	if(stauts < 0)
	  		  				return stauts;
	  	  }
	  	fp = fopen("/mnt/usr/da_data.txt", "rb");
	  		  	  if(fp != NULL)
	  		  	  {
	  		  		    sprintf(name,"/mnt/usr/da_data.txt");
	  		  		  	stauts = unlink(name);
	  		  		  	if(stauts < 0)
	  		  		  				return stauts;
	  		  	  }

	return 0;
}

int get_switchtable(int sockfd_dt)
{
  short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM] ;
  FILE  *fp;
  int data_fd;
  int rec_n;

  rec_n = recv(sockfd_dt,t_buff, SWITCHTABLE_COLMAX * sizeof(short) * SWITCHTABLE_COLNUM,0);
  fp = fopen("/mnt/usr/table_data.txt", "wb");
  if(fp == NULL)
  {
    printf("open table_data.txt file failed!\n");
    return -1;
  }
  fwrite(t_buff, sizeof(short), rec_n/sizeof(short), fp);
  fflush(fp);
  data_fd = fileno(fp);
  fsync(data_fd);
  fclose(fp);

  printf("switch table OK !\n");
  return 0;
}


int upload_switchtable(int sockfd_dt)
{
  short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM];
  FILE  *fp;
  int rec_n;

  fp = fopen("/mnt/usr/table_data.txt", "rb");
  if(fp == NULL)
  {
    printf("open table_data.txt file failed!\n");
    return -1;
  }
  rec_n = fread(t_buff, sizeof(short), SWITCHTABLE_COLMAX * sizeof(short) * SWITCHTABLE_COLNUM, fp);
  fclose(fp);

  rec_n = send(sockfd_dt,t_buff,rec_n*sizeof(short) ,0);
  printf("table data transfer finished!\n");
  return 0;
}


int  z7nau_selfcheck(int sockfd_dt)
{
  struct task_parameter s_buff;
  int rec_n = 0;
    int temp[20] = {0};
    FILE  *fp;

    fp = fopen("/mnt/usr/param_data.txt", "rb");
    if(fp == NULL) {
      printf("open param_data.txt file failed!\n");
      return -1;
    }
    rec_n = fread(&s_buff, sizeof(struct  task_parameter), 1, fp);
    fclose(fp);
    ioctl(axireg_fd,AXILITE_WDALENGTH, s_buff.da_wavelength);
    rec_n = ioctl(axireg_fd,AXILITE_RDALENGTH, 0);
    if(rec_n == s_buff.da_wavelength)
      	temp[0] = 0;
    else
      	temp[0] = 1;
    memcpy(temp+1,(int *)&s_buff,sizeof(struct  task_parameter));
    send(sockfd_dt,temp, sizeof(struct  task_parameter)+4,0);
    return 0;
}

/*********************************************************
*  int savedfiles_check(...)
*  check saved work parameter files and saved data files 
*  turns:    finished turns
*  switch:  finished rswitch
*  return:   0 when all task finished, 1 when not
*            <0 when error
**********************************************************/
int check_savedfiles(struct info_savedfiles *info_f,int* file_num)
{
  struct task_parameter s_buff;
  short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM] ;
  FILE  *fp;
  char name[FILENAME_LEN] = {0};
  int i,j,rec_n;
  int kturn,rswitch;

  info_f->turns_saved = 0;
  info_f->switchs_saved = 0;
  info_f->turns_upload = 0;
  info_f->switchs_upload = 0;

  fp = fopen("/mnt/usr/param_data.txt", "rb");
  if(fp == NULL) {
     printf("open param_data.txt file failed!\n");
     return -1;
   }
   rec_n = fread(&s_buff, sizeof(struct task_parameter), 1, fp);
   fclose(fp);
   printf("Read %d data from param_data.txt.\n",rec_n);
   kturn = s_buff.turns;

  fp = fopen("/mnt/usr/table_data.txt", "rb");
  if(fp == NULL) 
  {
     printf("open param_data.txt file failed!\n");
     return -2;
  }

  rec_n = fread(t_buff, sizeof(short), SWITCHTABLE_COLMAX*SWITCHTABLE_COLNUM, fp);
  printf("Read %d data from table_data.txt.\n",rec_n);
  fclose(fp);
  if(rec_n > 0)	rswitch = rec_n / SWITCHTABLE_COLNUM;
  else 
  {
     printf("read table_data.txt file failed!\n");
     return -3;
  }

  for(i=1;i<=kturn;i++)
  {
    for(j=1;j<=rswitch;j++)
    {
      sprintf(name,"/mnt/usr/ad_data_%d_%d_ok.txt",i,j);
      fp = fopen(name, "rb");
      if(fp == NULL)
      {
        sprintf(name,"/mnt/usr/ad_data_%d_%d.txt",i,j);
        fp = fopen(name, "rb");
        if(fp != NULL) 
        {
           fclose(fp);
           unlink(name);
         }
        break;
      }
      else
        fclose(fp);
    }
    if(j < rswitch+1)
      break;
  }
 if(i == kturn+1 && j == rswitch+1){
    info_f->turns_saved  = kturn;
    info_f->switchs_saved = rswitch;
    *file_num = kturn * rswitch;
    return 0;
  }
  else
  {
    info_f->turns_saved = i;
    info_f->switchs_saved = j-1;
    *file_num = (i-1)*rswitch + j -1;
    return 1;
  }
}


int datasample_task()
{
  int i,value,key_value;
  int da_length,ad_length;
  int kturn,rswitch,swtable_length = 0;
  int total = 0,cap_cnt = 0;
  int data_fd;
  int file_num;
  FILE * fp = NULL;
  char name[FILENAME_LEN] = {0};
  char newname[FILENAME_LEN] = {0};
  short t_buff[SWITCHTABLE_COLMAX][SWITCHTABLE_COLNUM] ;
  int bramDA_fd=0,bramAD_fd=0;
  char buf_ad[AD9266_SPI_WORD_LENGTH];
  //struct info_savedfiles info_savef;
  struct  task_parameter task_param;
  struct info_savedfiles info_savef = {0,0,0,0};

 
  struct SPI_Config ad_spiconfig={
                    AD9266_SPI_MODE,
                    AD9266_SPI_LSB,
                    AD9266_SPI_BITS,
                    AD9266_SPI_MAXSPEED
                    };


  key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,0);
  printf("datasample_task :key_value=%d\n",key_value);

  sem_init(&sem,0,0);
  value = check_savedfiles(&info_savef,&file_num);
  if (value < 0)  /*** no sufficient parameters  ******/
     goto task_stop;
  if (value == 0)  /***  task finished ******/
  {
     kturn = info_savef.turns_saved; // k turns
     goto task_finish;
  }
  //led always on
  ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
  ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);

  fp = fopen("/mnt/usr/param_data.txt", "rb");
  value = fread(&task_param, sizeof(struct task_parameter), 1, fp);
  fclose(fp);
  printf("Read %d data from param_data.txt.\n",value);

  value = param_check(&task_param);  //check parameters read from param_data.txt
  if(value < 0)
	goto task_stop;

  fp = fopen("/mnt/usr/table_data.txt", "rb");
  swtable_length = fread(t_buff, sizeof(short), SWITCHTABLE_COLMAX*SWITCHTABLE_COLNUM, fp);
  printf("Read %d data from table_data.txt.\n",swtable_length);
  swtable_length = swtable_length/SWITCHTABLE_COLNUM;
  fclose(fp);

/***** DA and DA sample frequency config***********/
  lmk_init();    //在没有配置LMK048时钟之前，ltc1668输出是0

/*****************DA block ram file***************/
  bramDA_fd = open("/dev/z7naubram_DA", O_WRONLY );
  if(bramDA_fd<0) 
  {
     perror("open device file failed!");
     printf("z7naubram_DA:[ERROR] Can't open device.");
     goto task_stop;
  }

/*****************AD block ram file***************/
  bramAD_fd = open("/dev/z7naubram_AD", O_RDONLY );
  if(bramAD_fd<0)
  {
     perror("open device file failed!");
     printf("z7naubram_AD:[ERROR] Can't open device.");
     close(bramDA_fd);
     goto task_stop;
  }

/*****************AD chip configuration***************/
  for(i=0;i<AD9266_REG_NUM;i++)
    SPIslave_Config(SPI_DEV_AD9266,ad_spiconfig,&ad_reg_val[i][0],AD9266_SPI_WORD_LENGTH,buf_ad);

  value = ioctl(axireg_fd,AXILITE_WADRSTN, 0);
  value = ioctl(axireg_fd,AXILITE_WDARSTN, 0);
  value = ioctl(axireg_fd,AXILITE_WTRIGINT,0);
  value = ioctl(axireg_fd,AXILITE_WTRIGSEL,0);

  usleep(LMK_RESET_LENGTH);
  value = ioctl(axireg_fd,AXILITE_WADRSTN, 1);
  value = ioctl(axireg_fd,AXILITE_WDARSTN, 1);


  signal(SIGIO,ad_sigal_handler);//set signal process function
  fcntl(axireg_fd,F_SETOWN,getpid());
	                                   
  value = fcntl(axireg_fd,F_GETFL);
  fcntl(axireg_fd,F_SETFL,value | FASYNC);//start asychronous note

  da_length = task_param.da_wavelength;
  value = ioctl(axireg_fd,AXILITE_WDALENGTH, da_length);
  printf(" write DA capture length!\n");

  ad_length = task_param.ad_length;
  value = ioctl(axireg_fd,AXILITE_WADLENGTH, ad_length);
  printf(" write DA capture length!\n");

  memset(da_buffer,0,sizeof(short)*DA_LENGTH_MAX);
  fp = fopen("/mnt/usr/da_data.txt", "rb");
  value = fread(da_buffer, sizeof(short), da_length, fp);
  fclose(fp);

  value = write(bramDA_fd,da_buffer,da_length);

  ioctl(axireg_fd,AXILITE_WLEDON_TIME,SEC2CYCLE(1));
  ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,SEC2CYCLE(1));

  kturn = info_savef.turns_saved; // k turns
 // if (info_savef.switchs_saved == 0)
 //   rswitch = 0;
 // else
    rswitch = info_savef.switchs_saved;//run swtable the rswitch item in 1 turn

  total = (kturn-1)* swtable_length * task_param.ad_avarage + rswitch * task_param.ad_avarage;
  cap_cnt = 0;
  memset(average_buffer,0,AD_LENGTH_MAX * sizeof(int));


  while(key_value == KEY_OPEN) //capture sample data
  {
  // value = ioctl(axireg_fd,AXILITE_WADRSTN, 0);
   value = ioctl(axireg_fd,AXILITE_WDARSTN, 0);
   value = ioctl(axireg_fd,AXILITE_WTRIGINT,0);
   usleep(LMK_RESET_LENGTH);
  // value = ioctl(axireg_fd,AXILITE_WADRSTN, 1);
   value = ioctl(axireg_fd,AXILITE_WDARSTN, 1);
   //switch I/O
   if(cap_cnt == 0) {
   value = ((t_buff[rswitch][0]-1) << 7) & 0x3f80;
   value = value | ((t_buff[rswitch][1]-1) & 0x7f) | SWTABLE_WORD1;
   printf("t_buff[%d][0]=%d,t_buff[%d][1]=%d,%d\n",rswitch,t_buff[rswitch][0],rswitch,t_buff[rswitch][1],value);
   ioctl(axigpio_fd,Z7AXIGPIO_WDATA, value);
   ioctl(axigpio_fd,Z7AXIGPIO_WDATA, SWTABLE_WORD2);
   usleep(task_param.io_delay * 1000);//ms -> us
   }
   /********** clear trigger**************/
      //value = ioctl(axireg_fd,AXILITE_WTRIGINT,0);
      //usleep(1);
  /********** set trigger**************/
    value = ioctl(axireg_fd,AXILITE_WTRIGINT,1);
    printf("write DA trigger!\n");
    //while(sigusr1_count == 0);//wait for ad to end sample data
    //sigusr1_count = 0;
    sem_wait(&sem);


    total++;
    printf("total = %d\n",total);
    value = read(bramAD_fd, ad_buffer, ad_length);

    for(i = 0; i < ad_length;i++)
	average_buffer[i] += ad_buffer[i];

    cap_cnt++;
    if(cap_cnt <  task_param.ad_avarage){
       key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,task_param.avarage_delay); //ms
       printf("datasample_task :key_value=%d\n",key_value);
       continue;
    }
    else
    {/***** finish one I/O switch  ******/
      cap_cnt = 0;
      rswitch++;
      for(i = 0; i < ad_length;i++)
        ad_buffer[i] = average_buffer[i] / task_param.ad_avarage;

      sprintf(name,"/mnt/usr/ad_data_%d_%d.txt",kturn,rswitch);
      fp = fopen(name, "wb");
      if(fp == NULL) {
         printf("open%s file failed!\n",name);
         goto task_stop;
      }
      fwrite(ad_buffer,sizeof(short),ad_length,fp);
      fflush(fp);
      data_fd = fileno(fp);
      fsync(data_fd);
      fclose(fp);
      sprintf(newname,"/mnt/usr/ad_data_%d_%d_ok.txt",kturn,rswitch);
      rename(name,newname);
      memset(average_buffer,0,AD_LENGTH_MAX * sizeof(int));
    }

    if(rswitch < swtable_length){
	//value = ioctl(axireg_fd,AXILITE_WADRSTN, 0);
    	//value = ioctl(axireg_fd,AXILITE_WDARSTN, 0);
    	//value = ioctl(axireg_fd,AXILITE_WTRIGINT,0);
       key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,task_param.switch_interval);//ms
       printf("datasample_task:key_value=%d\n",key_value);
       continue;
    }

    rswitch = 0;
    /*****reset digital I/O ***********/
    ioctl(axigpio_fd,Z7AXIGPIO_WDATA, SWTABLE_WORD3);
    ioctl(axigpio_fd,Z7AXIGPIO_WDATA, SWTABLE_WORD4);

    kturn ++;
    if(kturn <= task_param.turns){
    	 //value = ioctl(axireg_fd,AXILITE_WADRSTN, 0);
    	 //value = ioctl(axireg_fd,AXILITE_WDARSTN, 0);
    	 //value = ioctl(axireg_fd,AXILITE_WTRIGINT,0);
       key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(task_param.turns_interval));//second
       printf("datasample_task:key_value=%d\n",key_value);
       continue;
    }
    break;
   }

   if(kturn > task_param.turns) {
	//info_savef.turns_saved = task_param.turns;
	//info_savef.switchs_saved = swtable_length;
	ioctl(axireg_fd,AXILITE_WLEDON_TIME,SEC2CYCLE(2));
  	ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,SEC2CYCLE(2));
   }
 //  else {
//	info_savef.turns_saved = kturn;
//	info_savef.switchs_saved = rswitch;
//   }

   while(key_value == KEY_OPEN)
     key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(2));

  ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
  ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
  close(bramDA_fd);
  close(bramAD_fd);
  return 0;
task_finish:
	ioctl(axireg_fd,AXILITE_WLEDON_TIME,SEC2CYCLE(2));
	ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,SEC2CYCLE(2));
	while(key_value == KEY_OPEN)
	      key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(2));
    	ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
	ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
	return 0;
task_stop: 
   ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_OFF);
   ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_OFF);
   while(key_value == KEY_OPEN)
     key_value = ioctl(axigpio_fd,Z7AXIGPIO2_RDATA,S2MS(2));
   ioctl(axireg_fd,AXILITE_WLEDON_TIME,LED_ALWAYS_ON);
   ioctl(axireg_fd,AXILITE_WLEDOFF_TIME,LED_ALWAYS_ON);
   return -1;
}	 





void lmk_init()
{
  char buf_lmk[LMK_SPI_WORD_LENGTH];
  struct  task_parameter s_buff;
  FILE *fp;
  int i,value;


   fp = fopen("/mnt/usr/param_data.txt", "rb");

   fread(&s_buff, sizeof(struct  task_parameter), 1, fp);
   fclose(fp);
   
   value = (s_buff.lmk_DIVIDE0_1 << 5) | 0x00000000;
   lmk_reg_val[1][3] =  value & 0x000000ff;
   lmk_reg_val[1][2] =  (value >> 8)& 0x000000ff;
   lmk_reg_val[1][1] =  (value >> 16)& 0x000000ff;
   lmk_reg_val[1][0] =  (value >> 24)& 0x000000ff;
   value = (s_buff.lmk_DIVIDE2_3 << 5) | 0x00000001;
   lmk_reg_val[2][3] =  value & 0x000000ff;
   lmk_reg_val[2][2] =  (value >> 8)& 0x000000ff;
   lmk_reg_val[2][1] =  (value >> 16)& 0x000000ff;
   lmk_reg_val[2][0] =  (value >> 24)& 0x000000ff;
   value = (s_buff.lmk_pll1_n << 6) | (s_buff.lmk_pll2_r << 20) | 0x0000001c;
   lmk_reg_val[22][3] =  value & 0x000000ff;
   lmk_reg_val[22][2] =  (value >> 8)& 0x000000ff;
   lmk_reg_val[22][1] =  (value >> 16)& 0x000000ff;
   lmk_reg_val[22][0] =  (value >> 24)& 0x000000ff;
   value = (s_buff.lmk_pll2_n << 5) | 0x0100001d;
   lmk_reg_val[23][3] =  value & 0x000000ff;
   lmk_reg_val[23][2] =  (value >> 8)& 0x000000ff;
   lmk_reg_val[23][1] =  (value >> 16)& 0x000000ff;
   lmk_reg_val[23][0] =  (value >> 24)& 0x000000ff;
   value = (s_buff.lmk_pll2_n_pre << 24) | (s_buff.lmk_pll2_n << 5) | 0x0000001e;
   lmk_reg_val[24][3] =  value & 0x000000ff;
   lmk_reg_val[24][2] =  (value >> 8)& 0x000000ff;
   lmk_reg_val[24][1] =  (value >> 16)& 0x000000ff;
   lmk_reg_val[24][0] =  (value >> 24)& 0x000000ff;


   SPIslave_Config(SPI_DEV_LMK048,lmk_spiconfig,&lmk_reg_val[0][0],LMK_SPI_WORD_LENGTH,buf_lmk);
   usleep(LMK_RESET_LENGTH);

   for(i=1;i<LMK_REG_NUM+1;i++)
       SPIslave_Config(SPI_DEV_LMK048,lmk_spiconfig,&lmk_reg_val[i][0],LMK_SPI_WORD_LENGTH,buf_lmk);


}

int param_check(struct task_parameter *param)
{
  
  if(param->task_no < 1 || param->task_no > 100)
	return -1;
  if(param->da_wavelength < 5000 || param->da_wavelength > 50000)  
	return -2;
  if(param->ad_avarage < 1 || param->ad_avarage > 100)  
	return -3;
  if(param->ad_length < 1000 || param->ad_length > 10000)  
	return -4;
  if(param->switch_interval < 10 || param->switch_interval > 10000)  
	return -5;
  if(param->turns < 1 || param->turns > 100)  
	return -6;
  if(param->turns_interval < 1 || param->turns_interval > 86400)  
	return -7;
  if(param->io_delay < 10 || param->io_delay > 1000)  
	return -8;
  if(param->avarage_delay < 10 || param->avarage_delay > 10000)  
	return -9;
  return 0;
}

void ad_sigal_handler(int signum)
{
  printf("SIGIO handler\n");
  //sigusr1_count++;
  sem_post(&sem);
}
