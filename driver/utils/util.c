/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
  gcc -Wall meme.c `pkg-config fuse --cflags --libs` -o meme
*/

#define DIR_COUNT 224

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

static char *devfile = "../../MEMEformats/two-files.meme";

unsigned int LitToBig(unsigned int x)
{
    return (((x >> 24) & 0x000000ff) | ((x >> 8) & 0x0000ff00) | ((x << 8) & 0x00ff0000) | ((x << 24) & 0xff000000));
}

// function to convert Hexadecimal to Binary Number
void HexToBin(char *hexdec)
{

    long int i = 0;

    while (hexdec[i])
    {

        switch (hexdec[i])
        {
        case '0':
            printf("0000");
            break;
        case '1':
            printf("0001");
            break;
        case '2':
            printf("0010");
            break;
        case '3':
            printf("0011");
            break;
        case '4':
            printf("0100");
            break;
        case '5':
            printf("0101");
            break;
        case '6':
            printf("0110");
            break;
        case '7':
            printf("0111");
            break;
        case '8':
            printf("1000");
            break;
        case '9':
            printf("1001");
            break;
        case 'A':
        case 'a':
            printf("1010");
            break;
        case 'B':
        case 'b':
            printf("1011");
            break;
        case 'C':
        case 'c':
            printf("1100");
            break;
        case 'D':
        case 'd':
            printf("1101");
            break;
        case 'E':
        case 'e':
            printf("1110");
            break;
        case 'F':
        case 'f':
            printf("1111");
            break;
        default:
            printf("\nInvalid hexadecimal digit %c",
                   hexdec[i]);
        }
        i++;
    }
}

int bcd_to_dec(unsigned char x){
    return x-6*(x>>4);
}

static int meme_readdir()
{
    const char* path = "/TEST.TXT";
    char *buf = "\n\nTHIS IS NEW DATA\n\n";
    size_t size = 20;
    off_t offset = 50;
	struct fuse_file_info *fi;
	
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
	uint8_t stringBuf[512];
    uint8_t FATbuf[2];
	uint8_t loc[2];
	uint8_t fSize[2];
	size_t flag = 0;
    size_t sumLoc = 0;
    size_t sumBlk = 0;
    size_t sizeF = 0;
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

				//If data path is empty return error
				if(sumLoc == 0xFFFF){
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
				sizeF = 0;
				sizeF +=(int)fSize[0]*0x1000000;
				sizeF+=(int)fSize[1]*0x10000;
				sizeF+=(int)fSize[2]*0x100;
				sizeF += (int)fSize[3];

				
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

int main(int argc, char *argv[])
{
    printf("Main return: %d\n",meme_readdir());
    return 0;
}