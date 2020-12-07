/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
  gcc -Wall meme.c `pkg-config fuse --cflags --libs` -o meme
*/

#define FUSE_USE_VERSION 26
#define DIR_COUNT 224

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

//var used to hold image file name
static char *devfile = NULL;

//helper func to converd BCD to decimal
int bcd_to_dec(unsigned char x)
{
	return x - 6 * (x >> 4);
}

//FUSE func to collect path attributes
static int meme_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	FILE *fp;
	//open file for reading
	fp = fopen(devfile, "rb");

	//buffer variables for parsing
	uint8_t buffer[32];
	uint8_t typePerm[2];
	uint8_t fname[8];
	uint8_t fext[3];
	uint8_t fSize[4];
	uint8_t UID[2];
	uint8_t GID[2];
	uint8_t BCD[7];
	size_t size;
	size_t uid;
	size_t gid;
	size_t year;
	time_t epochT;
	struct tm t;
	char *nameBuf = malloc(8);

	//ensure path isn't root else return
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0x0;
		stbuf->st_nlink = 2;
		close(fp);
		free(nameBuf);
		return res;
	}

	//loop through directory entries
	for (long i = 0; i < DIR_COUNT; i++)
	{
		//seek to directory entry i and read into buffer
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, sizeof(uint8_t) * 32, 1, fp);
		//if the entry isn't empty process it
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//collect size of dir entry
			size_t j = 0x18;
			size_t h = 0;
			while (j < 0x1C)
			{
				sprintf(&fSize[h], "%c", buffer[j]);
				h++;
				j++;
			}
			//calculate decimal value of size
			size = 0;
			size+=(int)fSize[0]*0x1000000;
			size+=(int)fSize[1]*0x10000;
			size+=(int)fSize[2]*0x100;
			size += (int)fSize[3];

			//collect uid of dir entry
			j = 0x1c;
			h = 0;
			while (j < 0x1E)
			{
				h += sprintf(&UID[h], "%c", buffer[j]);
				j++;
			}
			//convert hex uid into decimal
			uid = 0;
			uid += ((int)UID[0]) * 256;
			uid += (int)UID[1];

			//collect gid of dir entry
			j = 0x1E;
			h = 0;
			while (j < 0x20)
			{
				h += sprintf(&GID[h], "%c", buffer[j]);
				j++;
			}
			//convert gid into decimal
			gid = 0;
			gid += ((int)GID[0]) * 256;
			gid += (int)GID[1];

			//set epoch time since modification
			j = 0x10;
			h = 0;
			while (j < 0x17)
			{
				h += sprintf(&BCD[h], "%c", bcd_to_dec(buffer[j]));
				j++;
			}
			//set time values for epoch conversion
			year = (int)BCD[0] * 100 + (int)BCD[1];
			t.tm_year = year - 1900;
			t.tm_mon = (int)BCD[2] - 1;
			t.tm_mday = BCD[3];
			t.tm_hour = BCD[4];
			t.tm_min = BCD[5];
			t.tm_sec = BCD[6];
			t.tm_isdst = -1;
			epochT = mktime(&t);

			
			
			
			
			//collect file name and copy into nameBuf
			j = 4;
			h = 0;
			while (j < 0x0C)
			{
		                h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//collect file extension and attach to nameBuf
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		                h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}

			//collect typePerm from 1st 9 bits
			sprintf(&typePerm[0], "%c", buffer[0]);
			sprintf(&typePerm[1], "%c", buffer[1]);
			//calc typePerm and save in index 0
			typePerm[0] = typePerm[0]*256+typePerm[1];

			//check if name matches with path and set attributes if so
			if (strcmp(path + 1, nameBuf) == 0)
			{
				stbuf->st_mode = S_IFREG | typePerm[0];
				stbuf->st_nlink = 1;
				stbuf->st_size = size;
				stbuf->st_blocks = size % 4096;
				stbuf->st_blksize = 512;
				stbuf->st_uid = uid;
				stbuf->st_gid = gid;
				stbuf->st_mtime = epochT;
				close(fp);
				free(nameBuf);
				return 0;
			}
		}
	}
	//if no matches return error
	close(fp);
	free(nameBuf);
	return -ENOENT;
}

static int meme_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{

	(void)offset;
	(void)fi;
	FILE *fp;

	//open file for reading
	fp = fopen(devfile, "rb");

	//buffer variables
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
	char* nameBuf = malloc(12);
	char dirNames[DIR_COUNT][8];
	size_t dirsFilled = 0;
	size_t exists = 0;

	if (strcmp(path, "/") != 0){
		close(fp);
		free(nameBuf);
		return -ENOENT;
	}
	//input root and parent dir entries
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//loop through all directories and print
	for (long i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, sizeof(uint8_t) * 32, 1, fp);
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//set nameBuf to filename
			int j = 4;
			int h = 0;
			while (j < 0x0C)
			{
		                h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			
			//make sure all chars are acceptable
			for(size_t i=0;i<sizeof(nameBuf);i++){
				if((nameBuf[i]!=45) && (nameBuf[i]!=61)\
					&& (nameBuf[i] != 94) && (nameBuf[i] != 95)&& (nameBuf[i]!=0)\
					&& (nameBuf[i] != 124) && !(nameBuf[i]>96&&nameBuf[i]<123)\
					&& !(nameBuf[i]>64&&nameBuf[i]<91) && !(nameBuf[i]>47&&nameBuf[i]<58)){
						exists = 1;
					}
			}
			//pull extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
                h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}

			//check for duplicates
			for(size_t i=0;i<dirsFilled;i++){
				if(strcmp(dirNames[i],nameBuf)==0){exists=1;}
			}
			//make sure all chars are acceptable
			for(size_t i=0;i<sizeof(nameBuf);i++){
				if((nameBuf[i]!=45) && (nameBuf[i]!=61) && (nameBuf[i]!=46)\
					&& (nameBuf[i] != 94) && (nameBuf[i] != 95)&& (nameBuf[i]!=0)\
					&& (nameBuf[i] != 124) && !(nameBuf[i]>96&&nameBuf[i]<123)\
					&& !(nameBuf[i]>64&&nameBuf[i]<91) && !(nameBuf[i]>47&&nameBuf[i]<58)){
						exists = 1;
					}
			}

			//if dir doesn't exist and chars are acceptable input to filler
			if(exists==0){
				filler(buf,nameBuf,NULL,0);
				strcpy(dirNames[dirsFilled],nameBuf);
				dirsFilled++;
			}
			//reset nameBuf and exist values
	                memset(nameBuf,0,12);
			exists = 0;
		}
	}

	free(nameBuf);
	close(fp);
	return 0;
}

static int meme_open(const char *path, struct fuse_file_info *fi)
{
	//loop through dirs and flag if one matches
	(void)fi;
	FILE *fp;

	//open file for reading
	fp = fopen(devfile, "rb");

	//buffer variables
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
	size_t flag = 0;
	char *nameBuf = malloc(12);

	//loop through directory entries
	for (long i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, sizeof(uint8_t) * 32, 1, fp);
		//if entry is valid
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//copy name to nameBuf
			int j = 4;
			int h = 0;
			while (buffer[j] != 0x00 && j < 0x0F)
			{
				h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//capture extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		        h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			strcat(nameBuf,".");
			strcat(nameBuf,fext);

			//if path matches file name/extension set flag
			if (strcmp(path + 1, nameBuf) == 0) {
				flag = 1;
				break;
			}
		}
	}

	close(fp);
	//if file doesn't exist return error
	if (flag == 0){
		free(nameBuf);
		return -ENOENT;
	}
	//if file is read only return error
	if ((fi->flags & 3) != O_RDONLY){
		free(nameBuf);
		return -EACCES;
	}
	//else return 0
	free(nameBuf);
	return 0;
}

static int meme_read(const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	size_t len;
	
	//loop through dirs and flag if one matches
	(void)fi;
	FILE *fp;

	//open file for reading
	fp = fopen(devfile, "rb");

	//buffer variables
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
	uint8_t stringBuf[512];
    uint8_t FATbuf[2];
	uint8_t loc[2];
	uint8_t fSize[2];
	size_t flag = 0;
    size_t sumLoc = 0;
    size_t sumBlk = 0;
   	char *nameBuf = malloc(12);
    char *bufBuf;
	
	//loop through dir entries for valid
	for (long i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, sizeof(uint8_t) * 32, 1, fp);
		//if entry is valid
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//copy file name to nameBuf
			int j = 4;
			int h = 0;
			while (buffer[j] != 0x00 && j < 0x0F)
			{
				h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//capture extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		        h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}
		
			//if file name/extension match path, read	
			if (strcmp(path + 1, nameBuf) == 0) {
				//access initial location of FAT
				j = 2;
				h = 0;
				while (j < 0x04)
				{
					h += sprintf(&loc[h], "%c", buffer[j]);
					j++;
				}
				//calculate sum of hex digits
				sumLoc = (int)loc[0]*256+(int)loc[1];

				//If data path is empty return error
				if(sumLoc == 0xFFFF){
					free(nameBuf);
					return -ENOENT;
				}
				//else loop through fat entries
				
				//collect size of dir entry for bufBuf allocation
				flag = 1;
				j = 0x18;
				h = 0;
				while (j < 0x1C)
				{
					sprintf(&fSize[h], "%c", buffer[j]);
					h++;
					j++;
				}
				//calculate size and allocate bufBuf
				size = 0;
				size+=(int)fSize[0]*0x1000000;
				size+=(int)fSize[1]*0x10000;
				size+=(int)fSize[2]*0x100;
				size += (int)fSize[3];

				bufBuf = malloc(size);
				
				//find first user data block and cat into bufBuf
				fseek(fp, 0x200 + 0x200 * (sumLoc-1) , SEEK_SET);
				fread(stringBuf, sizeof(uint8_t)*512, 1, fp);
				strcat(bufBuf,stringBuf);

				//while FAT entries contain next address
				while(sumBlk != 0xFFFF){
					//search for next FAT value 
					fseek(fp, 130048 + (sumLoc)*2, SEEK_SET);
					fread(FATbuf, sizeof(uint8_t)*2, 1, fp);
					sumBlk = (int)FATbuf[0]*256+(int)FATbuf[1];
					//if entry is valid cat user data into bufBuf
					if(sumBlk != 0xFFFF){
						fseek(fp, 0x200 + (sumBlk-1)*0x200, SEEK_SET);
						fread(stringBuf, 0x200, 1, fp);
						strcat(bufBuf,stringBuf);
					}
					sumLoc = sumBlk;
				}
				break;
			}
		}
	}

	close(fp);

	//if flag is still 0 return error since no directory was found
	if (flag == 0){
		if(bufBuf != NULL)
			free(bufBuf);
		free(nameBuf);
		return -ENOENT;
	}

	//copy bufBuf value into buf for FUSE output
	len = strlen(bufBuf);
	if (offset < len){
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, bufBuf, len);
	}
	else{
		size = 0;
	}

	if(bufBuf != NULL){
			free(bufBuf);
	}
	free(nameBuf);
	return size;
}

/*Rename a file*/
int meme_rename(const char *path, const char *newpath)
{
    //change name of existing file in DIR
	//first check if path exists then replace name in DIR if so

	//capture size of extension
	size_t ext_offset = 0;
	for(size_t g=0; newpath[g]!='.' && g<strlen(newpath); g++){
		ext_offset++;
	}
	size_t ext_size = strlen(newpath)-ext_offset;
	
	//if newpath is too long return invalid arguement error or extension greater than 3
    if (strlen(newpath) > 13 || ext_size>4){
        printf("name error(Len), ext_size: %d\n",ext_size);
		return -EINVAL;
    }
    for(size_t i=1;i<strlen(newpath);i++){
		if((newpath[i]!=45) && (newpath[i]!=46) && (newpath[i]!=61)\
			&& (newpath[i] != 94) && (newpath[i] != 95)&& (newpath[i]!=0)\
			&& (newpath[i] != 124) && !(newpath[i]>96&&newpath[i]<123)\
			&& !(newpath[i]>64&&newpath[i]<91) && !(newpath[i]>47&&newpath[i]<58)){
				printf("name error\n");
				return -EINVAL;
			}
	}

    //loop through dirs and flag if one matches
	FILE *fp;

    //open file for reading
    fp = fopen(devfile, "r+");

	//buffer variables
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
	size_t flag = 0;
	char *nameBuf = malloc(12);

	//loop through directory entries
	for (size_t i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, 32, 1, fp);
		//if entry is valid
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//copy name to nameBuf
			int j = 4;
			int h = 0;
			while (buffer[j] != 0x00 && j < 0x0F)
			{
				h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//capture extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		        h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}

            

			//if path matches file name/extension set flag
			if (strcmp(path + 1, nameBuf) == 0) {
                //clear name and extension
                j=4;
                while(j<0x0F){
                    memcpy(buffer+j,"\0",1);
                    j++;
                }

				flag = 1;
                j = 4;
                h = 1;
                //copy over name value
                 while(newpath[h]!='.' && h<strlen(newpath)){
                     memcpy(buffer+j,newpath+h,1);
                     j++;
                     h++;
                }

                j = 12;
                h++;
                //copy over extension value
                 while(h<strlen(newpath)){
                     memcpy(buffer+j,newpath+h,1);
                     j++;
                     h++;
                }
                //open file for writing and overwrite current Directory entry
		        fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		        fwrite(buffer, sizeof(uint8_t), sizeof(buffer), fp);
				break;
			}
		}
	}

	fclose(fp);
	//if file doesn't exist return error
	if (flag == 0){
		free(nameBuf);
		return -ENOENT;
	}
	//else return 0 
	free(nameBuf);
	return 0;

}

meme_unlink(const char* path){
	//FILE variables fp
	FILE *fp;

    //open file for reading
    fp = fopen(devfile, "r+");

	//buffer variables
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
    uint8_t FATbuf[2];
	uint8_t fSize[4];
	uint8_t loc[2];
	size_t flag = 0;
    size_t sumLoc = 0;
    size_t sumBlk = 0;
    size_t size = 0;
	char *nameBuf = malloc(12);

	//loop through directory entries
	for (size_t i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, 32, 1, fp);
		//if entry is valid

		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//copy name to nameBuf
			int j = 4;
			int h = 0;
			while (buffer[j] != 0x00 && j < 0x0F)
			{
				h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//capture extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		        h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}

            

			//if path matches file name/extension set flag
			if (strcmp(path + 1, nameBuf) == 0) {
                //mark dir entry as not used
                j=0;
                while(j<0x02){
                    printf("%x\n",buffer[j]);
                    buffer[j] = 0x00;
                    j++;
                }

                //access initial location of FAT
				j = 2;
				h = 0;
				while (j < 0x04)
				{
					h += sprintf(&loc[h], "%c", buffer[j]);
					j++;
				}


                //open file for writing and overwrite current Directory entry
		        fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
                fwrite(buffer, sizeof(char), sizeof(buffer), fp);

				//calculate sum of hex digits
				sumLoc = (int)loc[0]*256+(int)loc[1];

                //collect size of dir entry for bufBuf allocation
				flag = 1;
				j = 0x18;
				h = 0;
				while (j < 0x1C)
				{
					sprintf(&fSize[h], "%c", buffer[j]);
					h++;
					j++;
				}
				//calculate size and allocate bufBuf
				size = 0;
				size+=(int)fSize[0]*0x1000000;
				size+=(int)fSize[1]*0x10000;
				size+=(int)fSize[2]*0x100;
				size += (int)fSize[3];
				//while FAT entries contain next address
                uint8_t zero[2];
                zero[0] = 0x00; zero[1] = 0x00;

				while(sumBlk != 0xFFFF){
					//search for next FAT value 
					fseek(fp, 130048 + (sumLoc)*2, SEEK_SET);
					fread(FATbuf, sizeof(uint8_t)*2, 1, fp);
                    printf("before write: %d\n",sumLoc);
					fseek(fp, 130048 + (sumLoc)*2, SEEK_SET);
                    fwrite(zero, sizeof(uint8_t)*2, 1, fp);
					sumBlk = (int)FATbuf[0]*256+(int)FATbuf[1];
					sumLoc = sumBlk;
				}
				break;
			}
		}
	}

	close(fp);

	//if file doesn't exist return error
	if (flag == 0){
		free(nameBuf);
		return -ENOENT;
	}
	//else return 0 
	free(nameBuf);
	return 0;
}

meme_link(const char* from, const char* to){
	printf("link\n to: %s, from: %s\n",to,from);
}

void* meme_init(struct fuse_conn_info *conn){
	uint8_t buffer[1];
	buffer[0] = 0xFF;
	FILE* fp;
	
	//if FS is mounted, set superblock mount bit
	fp = fopen(devfile, "r+");
	fseek(fp, 130576, SEEK_SET);
	fwrite(buffer, sizeof(uint8_t), 1, fp);
	fclose(fp);
}

void meme_destroy(void* private_data){
	//set up a buffer to unmount superblock
	uint8_t buffer[1];
	buffer[0] = 0x00;
	FILE* fp;
	
	//unset superblock mount bit
	fp = fopen(devfile, "r+");
	fseek(fp, 130576, SEEK_SET);
	fwrite(buffer, sizeof(uint8_t), 1, fp);
	fclose(fp);
}

meme_write(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi){
	
	//if the length of buf and value of size don't match return error
	if(strlen(buf) != size){
        return -ENOENT;
    }

	//loop through dirs and flag if one matches
	(void)fi;
	FILE *fp; 

	//open file for reading
	fp = fopen(devfile, "r+");

	//buffer variables
    int sizeTemp = size;
	uint8_t buffer[32];
	uint8_t fname[8];
	uint8_t fext[3];
    uint8_t FATbuf[2];
	uint8_t loc[2];
	size_t flag = 0;
    size_t sumLoc = 0;
    size_t sumBlk = 0;
   	char nameBuf[12];
	
	//loop through dir entries for valid
	for (long i = 0; i < DIR_COUNT; i++)
	{
		fseek(fp, 122880 + sizeof(buffer) * i, SEEK_SET);
		fread(buffer, sizeof(uint8_t) * 32, 1, fp);
		//if entry is valid
		if (buffer[0] != 0x00 && buffer[1] != 0x00)
		{
			//copy file name to nameBuf
			int j = 4;
			int h = 0;
			while (buffer[j] != 0x00 && j < 0x0F)
			{
				h += sprintf(&fname[h], "%c", buffer[j]);
				j++;
			}
			strcpy(nameBuf,fname);
			//capture extension
			j = 12;
			h = 0;
			while (j < 0x0F)
			{
		        h += sprintf(&fext[h], "%c", buffer[j]);
				j++;
			}
			if(fext[0]!=0x00){
				strcat(nameBuf,".");
				strcat(nameBuf,fext);
			}
		
			//if file name/extension match path, read	
			if (strcmp(path + 1, nameBuf) == 0) {
                int pos = offset % 512;
                int startBlk = offset / 512;
                int numBlks = (size+pos) / 512;
                if((size+pos)%512 != 0){
                    numBlks++;
                }

				//access initial location of FAT
				j = 2;
				h = 0;
				while (j < 0x04)
				{
					h += sprintf(&loc[h], "%c", buffer[j]);
					j++;
				}
				//calculate sum of hex digits
				sumLoc = (int)loc[0]*256+(int)loc[1];

				//loop through fat entries				
                //if write starts at block 0, write and subtract from size
                if(startBlk == 0){
                    //seek to user data location
				    fseek(fp, 0x200 + 0x200 * (sumLoc-1) + pos, SEEK_SET);
                    sizeTemp = sizeTemp - (512-pos);
                    //if buf is larger than first block copy over (512-pos) else copy over sizeof buf
                    if(sizeTemp > 0){
                       fwrite(buf, 512-pos, 1, fp);
                    } else {
                       fwrite(buf,strlen(buf), 1, fp);
                    }
                    numBlks--;
                    //switch value of position to pos in buf
                    pos = (512-pos);
                } else {
                    //else subtract from start bulk and iterate until start is reached
                    startBlk--;
                }

                

				//while FAT entries contain next address
				while(sumBlk != 0xFFFF && sizeTemp > 0 && numBlks > 0){
                    //search for next FAT value 
                    fseek(fp, 130048 + (sumLoc)*2, SEEK_SET);
                    fread(FATbuf, sizeof(uint8_t)*2, 1, fp);
                    sumBlk = (int)FATbuf[0]*256+(int)FATbuf[1];
                    
                    if(startBlk>0){startBlk--;}

                    if (sumBlk == 0xFFFF){
                        int z = 0;
                        //search for next free FAT entry 
                        while(sumBlk != 0x0000){
                            z++;
                            fseek(fp, 130048 + (sumLoc+z)*2, SEEK_SET);
                            fread(FATbuf, sizeof(uint8_t)*2, 1, fp);
                            sumBlk = (int)FATbuf[0]*256+(int)FATbuf[1];
                        }
                        //write free fat entry location into FAT table
                        fseek(fp, 130048 + (sumLoc)*2, SEEK_SET);
                        fwrite((void*)sumBlk, sizeof(uint8_t)*2, 1, fp);
                    }
                    

                    //if the start block has already been reached, write
                    if(startBlk == 0){
                        //if this is the first block being written into (offset write)
                        if(pos%512 != 0){
                            //seek to user data location
                            fseek(fp, 0x200 + 0x200 * (sumBlk-1) + pos, SEEK_SET);
                            sizeTemp = sizeTemp - (512-pos);
                            //if buf is larger than first block copy over (512-pos) else copy over sizeof buf
                            if(sizeTemp > 0){
                                fwrite(buf, 512-pos, 1, fp);
                            } else {
                                fwrite(buf,strlen(buf), 1, fp);
                            }
                            numBlks--;
                            //switch value of position to pos in buf
                            pos = (512-pos);
                        } else {
                            //else print from beginning of user data block
                            fseek(fp, 0x200 + 0x200 * (sumBlk-1), SEEK_SET);
                            //if there are more than 512 chars left in buf
                            if( numBlks > 1 ){
                                //write 512 and continute
                                fwrite(buf+pos,512,1,fp);
                                pos += 512;
                            } else {
                                //else this is the final write block
                                fwrite(buf+pos,size-pos,1,fp);
                                pos = size;
                            }
                            numBlks--;
                        }
                    sumLoc = sumBlk;
                    }

				}
				break;
			}
		}
	}

	close(fp);

	//if flag is still 0 return error since no directory was found
	if (flag == 0){
		return -ENOENT;
	}

	return size;
}


static struct fuse_operations meme_oper = {
	.getattr = meme_getattr,
	.readdir = meme_readdir,
	.open = meme_open,
	.read = meme_read,
	.rename = meme_rename,
	.init = meme_init,
	.destroy = meme_destroy,
	.write = meme_write,
	.link = meme_link,
	.unlink = meme_unlink,
	//.truncate = meme_truncate,
	//.fgetattr = meme_fgetattr,
	//.chown = meme_chown,
	//.chmod = meme_chmod,
};

int main(int argc, char *argv[])
{
	int i;

	//get the image filename from args
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		;
	{
		if (i < argc)
		{
			devfile = argv[i];
			memcpy(&argv[i], &argv[i + 1], (argc - i) * sizeof(argv[0]));
			argc--;
		}
	}


	
	

	return fuse_main(argc, argv, &meme_oper, NULL);
}
